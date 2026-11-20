#pragma once

#include <lmdb.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

class LmdbError : public std::runtime_error {
public:
    LmdbError(const std::string& what, int code);

    [[nodiscard]] int code() const {
        return code_;
    }

private:
    int code_;
};

void lmdbCheck(int rc, const std::string& what);

MDB_val toVal(std::string_view bytes);
std::string_view fromVal(const MDB_val& val);

class LmdbEnv {
public:
    LmdbEnv(const std::filesystem::path& file, unsigned int max_dbs, std::size_t map_size_butes);
    ~LmdbEnv();

    LmdbEnv(const LmdbEnv&) = delete;
    LmdbEnv& operator=(const LmdbEnv&) = delete;
    LmdbEnv(LmdbEnv&& other) noexcept;
    LmdbEnv& operator=(LmdbEnv&& other) noexcept;

    [[nodiscard]] MDB_env* get() const {
        return env_;
    };

private:
    MDB_env* env_ = nullptr;
};

class LmdbTxn {
public:
    enum class Mode {
        kRead,
        kWrite,
    };

    LmdbTxn(const LmdbEnv& env, Mode mode);
    ~LmdbTxn();

    LmdbTxn(const LmdbTxn&) = delete;
    LmdbTxn& operator=(const LmdbTxn&) = delete;
    LmdbTxn(LmdbTxn&&) = delete;
    LmdbTxn& operator=(LmdbTxn&&) = delete;

    void commit();

    [[nodiscard]] MDB_txn* get() const {
        return txn_;
    }

private:
    MDB_txn* txn_ = nullptr;
};

class LmdbCursor {
public:
    LmdbCursor(const LmdbTxn& txn, MDB_dbi dbi);
    ~LmdbCursor();

    LmdbCursor(const LmdbCursor&) = delete;
    LmdbCursor& operator=(const LmdbCursor&) = delete;
    LmdbCursor(LmdbCursor&&) = delete;
    LmdbCursor& operator=(LmdbCursor&&) = delete;

    [[nodiscard]] MDB_cursor* get() const {
        return cursor_;
    }

private:
    MDB_cursor* cursor_ = nullptr;
};
