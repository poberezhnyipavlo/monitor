#include "LmdbStore.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include "NodeKeys.h"

namespace {

constexpr unsigned int kTables = 3;

node_keys::DevBytes devBytes(std::string_view dev) {
    const auto bytes = node_keys::devFromString(dev);
    if (!bytes) {
        throw std::invalid_argument("invalid dev: " + std::string(dev));
    }

    return *bytes;
}

MDB_dbi openTable(const LmdbTxn& txn, const char* name) {
    MDB_dbi dbi = 0;
    lmdbCheck(mdb_dbi_open(txn.get(), name, MDB_CREATE, &dbi), std::string("mdb_dbi_open") + name);
    return dbi;
}

// A double as its 8 raw bytes. Values are never compared by LMDB, so byte order does not matter
// here.
std::string encodeDouble(double value) {
    std::string out(sizeof(value), '\0');
    std::memcpy(out.data(), &value, sizeof(value));
    return out;
}

double decodeDouble(std::string_view bytes) {
    double value = 0;
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

// false: the key already exists (MDB_NOOVERWRITE).
bool putNew(const LmdbTxn& txn, MDB_dbi dbi, std::string_view key, std::string_view value) {
    MDB_val k = toVal(key);
    MDB_val v = toVal(value);
    const int rc = mdb_put(txn.get(), dbi, &k, &v, MDB_NOOVERWRITE);
    if (rc == MDB_KEYEXIST) {
        return false;
    }
    lmdbCheck(rc, "mdb_put");
    return true;
}

void put(const LmdbTxn& txn, MDB_dbi dbi, std::string_view key, std::string_view value) {
    MDB_val k = toVal(key);
    MDB_val v = toVal(value);
    lmdbCheck(mdb_put(txn.get(), dbi, &k, &v, 0), "mdb_put");
}

}  // namespace

LmdbStore::LmdbStore(const std::filesystem::path& file, std::size_t map_size_bytes)
    : env_(file, kTables, map_size_bytes) {
    LmdbTxn txn(env_, LmdbTxn::Mode::kWrite);
    samples_ = openTable(txn, "samples");
    series_ = openTable(txn, "series");
    logs_ = openTable(txn, "logs");
    txn.commit();
}

PutResult LmdbStore::putMetric(const metric::Message& message, uint64_t ts_ms) {
    const node_keys::DevBytes dev = devBytes(message.dev);
    std::string ts;
    node_keys::appendBigEndian64(ts, ts_ms);

    LmdbTxn txn(env_, LmdbTxn::Mode::kWrite);
    if (!putNew(txn, samples_, node_keys::recordKey(dev, message.boot, message.seq), ts)) {
        return PutResult::kDuplicate;
    }
    put(txn, series_, node_keys::seriesKey(dev, message.name, ts_ms), encodeDouble(message.value));
    txn.commit();
    return PutResult::kStored;
}

PutResult LmdbStore::putLog(const logproto::Record& record, uint64_t ts_ms) {
    const node_keys::DevBytes dev = devBytes(record.dev);

    std::string value;
    node_keys::appendBigEndian64(value, ts_ms);
    value.push_back(static_cast<char>(record.level));
    value.append(record.src);
    value.push_back('\0');
    value.append(record.text);

    LmdbTxn txn(env_, LmdbTxn::Mode::kWrite);
    if (!putNew(txn, logs_, node_keys::recordKey(dev, record.boot, record.seq), value)) {
        return PutResult::kDuplicate;
    }
    txn.commit();
    return PutResult::kStored;
}

std::vector<SeriesPoint> LmdbStore::series(
    std::string_view dev,
    std::string_view name,
    uint64_t from_ms,
    uint64_t to_ms
) const {
    const node_keys::DevBytes dev_bytes = devBytes(dev);
    const std::string prefix = node_keys::seriesPrefix(dev_bytes, name);
    const std::string start = node_keys::seriesKey(dev_bytes, name, from_ms);

    LmdbTxn txn(env_, LmdbTxn::Mode::kRead);
    LmdbCursor cursor(txn, series_);

    std::vector<SeriesPoint> points;
    MDB_val key = toVal(start);
    MDB_val value{};
    // MDB_SET_RANGE: the first key >= start. Then MDB_NEXT walks keys in order.
    int rc = mdb_cursor_get(cursor.get(), &key, &value, MDB_SET_RANGE);
    while (rc == MDB_SUCCESS) {
        const std::string_view k = fromVal(key);
        if (!k.starts_with(prefix)) {
            break;  // next series (other name or other device)
        }
        const uint64_t ts_ms = node_keys::readBigEndian64(k.substr(prefix.size()));
        if (ts_ms > to_ms) {
            break;
        }
        points.push_back({.ts_ms = ts_ms, .value = decodeDouble(fromVal(value))});
        rc = mdb_cursor_get(cursor.get(), &key, &value, MDB_NEXT);
    }
    if (rc != MDB_NOTFOUND && rc != MDB_SUCCESS) {
        lmdbCheck(rc, "mdb_cursor_get");
    }
    return points;
}

std::vector<SeriesId> LmdbStore::listSeries() const {
    LmdbTxn txn(env_, LmdbTxn::Mode::kRead);
    LmdbCursor cursor(txn, series_);

    MDB_val key{};
    MDB_val value{};

    int rc = mdb_cursor_get(cursor.get(), &key, &value, MDB_FIRST);

    std::vector<SeriesId> series;
    while (rc == MDB_SUCCESS) {
        std::string_view k = fromVal(key);

        node_keys::DevBytes dev{};
        std::copy_n(k.begin(), node_keys::kDevBytes, dev.begin());

        std::size_t name_end = k.find('\0', node_keys::kDevBytes);
        const std::string_view name =
            k.substr(node_keys::kDevBytes, name_end - node_keys::kDevBytes);

        series.push_back({.device = metric::formatDev(dev).data(), .name = std::string(name)});

        const std::string next = node_keys::seriesEnd(dev, name);
        key = toVal(next);
        rc = mdb_cursor_get(cursor.get(), &key, &value, MDB_SET_RANGE);
    }

    if (rc != MDB_NOTFOUND) {
        lmdbCheck(rc, "mdb_cursor_get");
    }

    return series;
}
