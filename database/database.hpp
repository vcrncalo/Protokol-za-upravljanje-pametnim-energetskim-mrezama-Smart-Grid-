#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <sqlite3.h>
#include <iostream>
#include <string>
#include <ctime>

class Database
{
private:
    sqlite3* db;

public:
    Database(const std::string& databasePath)
        : db(nullptr)
    {
        int result =
            sqlite3_open(databasePath.c_str(), &db);

        if (result != SQLITE_OK)
        {
            std::cerr
                << "Greska pri otvaranju baze: "
                << sqlite3_errmsg(db)
                << std::endl;
        }
        else
        {
            std::cout
                << "Baza podataka uspjesno otvorena."
                << std::endl;
        }
    }

    ~Database()
    {
        if (db != nullptr)
        {
            sqlite3_close(db);
        }
    }


    bool initialize()
    {
        const char* sql = R"(

            CREATE TABLE IF NOT EXISTS devices (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                uri TEXT UNIQUE NOT NULL,
                region_id INTEGER NOT NULL,
                user_type INTEGER NOT NULL,
                registered_at INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS consumption (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_uri TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                consumption_kwh REAL NOT NULL,
                current_power_kw REAL NOT NULL,
                FOREIGN KEY(device_uri)
                    REFERENCES devices(uri)
            );

            CREATE TABLE IF NOT EXISTS tariffs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                price_per_kwh REAL NOT NULL,
                timestamp INTEGER NOT NULL
            );

        )";

        char* errorMessage = nullptr;

        int result =
            sqlite3_exec(
                db,
                sql,
                nullptr,
                nullptr,
                &errorMessage
            );

        if (result != SQLITE_OK)
        {
            std::cerr
                << "Greska pri kreiranju tabela: "
                << errorMessage
                << std::endl;

            sqlite3_free(errorMessage);

            return false;
        }

        std::cout
            << "Tabele baze podataka su spremne."
            << std::endl;

        return true;
    }
    bool insertDevice(
    const std::string& uri,
    uint32_t regionId,
    uint8_t userType)
{
    const char* sql =
        "INSERT OR IGNORE INTO devices "
        "(uri, region_id, user_type, registered_at) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* statement = nullptr;

    int result =
        sqlite3_prepare_v2(
            db,
            sql,
            -1,
            &statement,
            nullptr
        );

    if (result != SQLITE_OK)
    {
        std::cerr
            << "Greska pri pripremi INSERT upita: "
            << sqlite3_errmsg(db)
            << std::endl;

        return false;
    }

    sqlite3_bind_text(
        statement,
        1,
        uri.c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_int(
        statement,
        2,
        static_cast<int>(regionId)
    );

    sqlite3_bind_int(
        statement,
        3,
        static_cast<int>(userType)
    );

    sqlite3_bind_int64(
        statement,
        4,
        static_cast<sqlite3_int64>(
            std::time(nullptr)
        )
    );

    result =
        sqlite3_step(statement);

    sqlite3_finalize(statement);

    if (result != SQLITE_DONE)
    {
        std::cerr
            << "Greska pri upisu uredjaja: "
            << sqlite3_errmsg(db)
            << std::endl;

        return false;
    }

    std::cout
        << "Uredjaj upisan u bazu: "
        << uri
        << std::endl;

    return true;
}
};

#endif

