#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <sqlite3.h>
#include <iostream>
#include <string>
#include <ctime>
#include <cstdint>

class Database
{
private:
    sqlite3* db;

    // Pretvara Unix timestamp u:
    // YYYY-MM-DD HH:MM:SS
    std::string formatTimestamp(uint64_t timestamp)
    {
        std::time_t timeValue =
            static_cast<std::time_t>(timestamp);

        std::tm* localTime =
            std::localtime(&timeValue);

        char timeBuffer[20];

        std::strftime(
            timeBuffer,
            sizeof(timeBuffer),
            "%Y-%m-%d %H:%M:%S",
            localTime
        );

        return std::string(timeBuffer);
    }

public:

    // ==========================================
    // KONSTRUKTOR
    // ==========================================

    Database(const std::string& databasePath)
        : db(nullptr)
    {
        int result =
            sqlite3_open(
                databasePath.c_str(),
                &db
            );

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


    // ==========================================
    // DESTRUKTOR
    // ==========================================

    ~Database()
    {
        if (db != nullptr)
        {
            sqlite3_close(db);
        }
    }


    // ==========================================
    // INICIJALIZACIJA BAZE
    // ==========================================

    bool initialize()
    {
        const char* sql = R"(

            CREATE TABLE IF NOT EXISTS devices (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                uri TEXT UNIQUE NOT NULL,
                region_id INTEGER NOT NULL,
                user_type INTEGER NOT NULL,
                registered_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS consumption (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_uri TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                consumption_kwh REAL NOT NULL,
                current_power_kw REAL NOT NULL,
                FOREIGN KEY(device_uri)
                    REFERENCES devices(uri)
            );

            CREATE TABLE IF NOT EXISTS tariffs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                price_per_kwh REAL NOT NULL,
                timestamp TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS commands (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_uri TEXT NOT NULL,
                command_type TEXT NOT NULL,
                target_power_kw REAL,
                status TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                FOREIGN KEY(device_uri)
                    REFERENCES devices(uri)
            );

            CREATE TABLE IF NOT EXISTS alarms (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_uri TEXT NOT NULL,
                alarm_type TEXT NOT NULL,
                message TEXT NOT NULL,
                resolved INTEGER NOT NULL DEFAULT 0,
                timestamp TEXT NOT NULL,
                FOREIGN KEY(device_uri)
                    REFERENCES devices(uri)
            );

                       CREATE TABLE IF NOT EXISTS region_sync (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                source_region INTEGER NOT NULL,
                target_region INTEGER NOT NULL,
                sync_type TEXT NOT NULL,
                status TEXT NOT NULL,
                timestamp TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS central_consumption (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                source_region INTEGER NOT NULL,
                device_uri TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                consumption_kwh REAL NOT NULL,
                current_power_kw REAL NOT NULL
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


    // ==========================================
    // UPIS SMART METERA
    // ==========================================

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

        uint64_t currentTimestamp =
            static_cast<uint64_t>(
                std::time(nullptr)
            );

        std::string registrationTime =
            formatTimestamp(currentTimestamp);

        sqlite3_bind_text(
            statement,
            4,
            registrationTime.c_str(),
            -1,
            SQLITE_TRANSIENT
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


    // ==========================================
    // UPIS POTROSNJE
    // ==========================================

    bool insertConsumption(
        const std::string& deviceUri,
        uint64_t timestamp,
        double consumptionKwh,
        double currentPowerKw)
    {
        const char* sql =
            "INSERT INTO consumption "
            "(device_uri, timestamp, consumption_kwh, current_power_kw) "
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
                << "Greska pri pripremi INSERT upita za potrosnju: "
                << sqlite3_errmsg(db)
                << std::endl;

            return false;
        }

        sqlite3_bind_text(
            statement,
            1,
            deviceUri.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        std::string measurementTime =
            formatTimestamp(timestamp);

        sqlite3_bind_text(
            statement,
            2,
            measurementTime.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        sqlite3_bind_double(
            statement,
            3,
            consumptionKwh
        );

        sqlite3_bind_double(
            statement,
            4,
            currentPowerKw
        );

        result =
            sqlite3_step(statement);

        sqlite3_finalize(statement);

        if (result != SQLITE_DONE)
        {
            std::cerr
                << "Greska pri upisu potrosnje: "
                << sqlite3_errmsg(db)
                << std::endl;

            return false;
        }

        std::cout
            << "Potrosnja upisana u bazu za: "
            << deviceUri
            << std::endl;

        return true;
    }


    // ==========================================
    // UPIS TARIFE
    // ==========================================

    bool insertTariff(
        double pricePerKwh)
    {
        const char* sql =
            "INSERT INTO tariffs "
            "(price_per_kwh, timestamp) "
            "VALUES (?, ?);";

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
                << "Greska pri pripremi INSERT upita za tarifu: "
                << sqlite3_errmsg(db)
                << std::endl;

            return false;
        }

        sqlite3_bind_double(
            statement,
            1,
            pricePerKwh
        );

        uint64_t currentTimestamp =
            static_cast<uint64_t>(
                std::time(nullptr)
            );

        std::string tariffTime =
            formatTimestamp(currentTimestamp);

        sqlite3_bind_text(
            statement,
            2,
            tariffTime.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        result =
            sqlite3_step(statement);

        sqlite3_finalize(statement);

        if (result != SQLITE_DONE)
        {
            std::cerr
                << "Greska pri upisu tarife: "
                << sqlite3_errmsg(db)
                << std::endl;

            return false;
        }

        std::cout
            << "Tarifa upisana u bazu: "
            << pricePerKwh
            << " KM/kWh"
            << std::endl;

        return true;
    }
    
       // ==========================================
    // UPIS POTROSNJE U CENTRALNU BAZU
    // ==========================================

    void insertCentralConsumption(
        uint32_t source_region,
        const std::string& device_uri,
        uint64_t timestamp,
        double consumption_kwh,
        double current_power_kw)
    {
        const char* sql =
            "INSERT INTO central_consumption "
            "(source_region, device_uri, timestamp, "
            "consumption_kwh, current_power_kw) "
            "VALUES (?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(
                db,
                sql,
                -1,
                &stmt,
                nullptr) != SQLITE_OK)
        {
            std::cerr
                << "Greska pri pripremi centralnog upisa: "
                << sqlite3_errmsg(db)
                << std::endl;

            return;
        }

        sqlite3_bind_int(
            stmt,
            1,
            static_cast<int>(source_region)
        );

        sqlite3_bind_text(
            stmt,
            2,
            device_uri.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        sqlite3_bind_int64(
            stmt,
            3,
            static_cast<sqlite3_int64>(timestamp)
        );

        sqlite3_bind_double(
            stmt,
            4,
            consumption_kwh
        );

        sqlite3_bind_double(
            stmt,
            5,
            current_power_kw
        );

        int result = sqlite3_step(stmt);

        if (result != SQLITE_DONE)
        {
            std::cerr
                << "Greska pri upisu u centralnu bazu: "
                << sqlite3_errmsg(db)
                << std::endl;
        }
        else
        {
            std::cout
                << "Mjerenje upisano u centralnu bazu za regiju "
                << source_region
                << std::endl;
        }

        sqlite3_finalize(stmt);
    }


    // ==========================================
    // AGREGACIJA CENTRALNE POTROSNJE PO REGIJAMA
    // ==========================================

    void printCentralAggregation()
    {
        const char* sql =
            "SELECT "
            "source_region, "
            "COUNT(*) AS broj_mjerenja, "
            "SUM(consumption_kwh) AS ukupna_potrosnja, "
            "AVG(consumption_kwh) AS prosjecna_potrosnja, "
            "AVG(current_power_kw) AS prosjecna_snaga "
            "FROM central_consumption "
            "GROUP BY source_region "
            "ORDER BY source_region;";

        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(
                db,
                sql,
                -1,
                &stmt,
                nullptr) != SQLITE_OK)
        {
            std::cerr
                << "Greska pri agregaciji centralnih podataka: "
                << sqlite3_errmsg(db)
                << std::endl;

            return;
        }

        std::cout
            << "\n=== AGREGACIJA PO REGIJAMA ==="
            << std::endl;

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int region =
                sqlite3_column_int(stmt, 0);

            int brojMjerenja =
                sqlite3_column_int(stmt, 1);

            double ukupnaPotrosnja =
                sqlite3_column_double(stmt, 2);

            double prosjecnaPotrosnja =
                sqlite3_column_double(stmt, 3);

            double prosjecnaSnaga =
                sqlite3_column_double(stmt, 4);

            std::cout
                << "\nRegija: "
                << region
                << std::endl;

            std::cout
                << "Broj mjerenja: "
                << brojMjerenja
                << std::endl;

            std::cout
                << "Ukupna potrosnja: "
                << ukupnaPotrosnja
                << " kWh"
                << std::endl;

            std::cout
                << "Prosjecna potrosnja: "
                << prosjecnaPotrosnja
                << " kWh"
                << std::endl;

            std::cout
                << "Prosjecna snaga: "
                << prosjecnaSnaga
                << " kW"
                << std::endl;

            std::cout
                << "------------------------"
                << std::endl;
        }

        sqlite3_finalize(stmt);

        std::cout
            << "================================\n"
            << std::endl;
    }

    // ==========================================
    // UPIS KOMANDE
    // ==========================================

    bool insertCommand(
        const std::string& deviceUri,
        const std::string& commandType,
        double targetPowerKw,
        const std::string& status)
    {
        const char* sql =
            "INSERT INTO commands "
            "(device_uri, command_type, target_power_kw, status, timestamp) "
            "VALUES (?, ?, ?, ?, ?);";

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
                << "Greska pri pripremi INSERT upita za komandu: "
                << sqlite3_errmsg(db)
                << std::endl;

            return false;
        }

        sqlite3_bind_text(
            statement,
            1,
            deviceUri.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        sqlite3_bind_text(
            statement,
            2,
            commandType.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        sqlite3_bind_double(
            statement,
            3,
            targetPowerKw
        );

        sqlite3_bind_text(
            statement,
            4,
            status.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        uint64_t currentTimestamp =
            static_cast<uint64_t>(
                std::time(nullptr)
            );

        std::string commandTime =
            formatTimestamp(currentTimestamp);

        sqlite3_bind_text(
            statement,
            5,
            commandTime.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        result =
            sqlite3_step(statement);

        sqlite3_finalize(statement);

        if (result != SQLITE_DONE)
        {
            std::cerr
                << "Greska pri upisu komande: "
                << sqlite3_errmsg(db)
                << std::endl;

            return false;
        }

        std::cout
            << "Komanda upisana u bazu za: "
            << deviceUri
            << std::endl;

        return true;
    }


    // ==========================================
    // UPIS ALARMA
    // ==========================================

    bool insertAlarm(
        const std::string& deviceUri,
        const std::string& alarmType,
        const std::string& message)
    {
        const char* sql =
            "INSERT INTO alarms "
            "(device_uri, alarm_type, message, resolved, timestamp) "
            "VALUES (?, ?, ?, 0, ?);";

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
                << "Greska pri pripremi INSERT upita za alarm: "
                << sqlite3_errmsg(db)
                << std::endl;

            return false;
        }

        sqlite3_bind_text(
            statement,
            1,
            deviceUri.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        sqlite3_bind_text(
            statement,
            2,
            alarmType.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        sqlite3_bind_text(
            statement,
            3,
            message.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        uint64_t currentTimestamp =
            static_cast<uint64_t>(
                std::time(nullptr)
            );

        std::string alarmTime =
            formatTimestamp(currentTimestamp);

        sqlite3_bind_text(
            statement,
            4,
            alarmTime.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        result =
            sqlite3_step(statement);

        sqlite3_finalize(statement);

        if (result != SQLITE_DONE)
        {
            std::cerr
                << "Greska pri upisu alarma: "
                << sqlite3_errmsg(db)
                << std::endl;

            return false;
        }

        std::cout
            << "Alarm upisan u bazu za: "
            << deviceUri
            << std::endl;

        return true;
    }
    // ==========================================
// RJESAVANJE ALARMA
// ==========================================

bool resolveAlarm(
    const std::string& deviceUri)
{
    const char* sql =
        "UPDATE alarms "
        "SET resolved = 1 "
        "WHERE device_uri = ? "
        "AND alarm_type = 'DEVICE_OFFLINE' "
        "AND resolved = 0;";

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
            << "Greska pri pripremi UPDATE upita za alarm: "
            << sqlite3_errmsg(db)
            << std::endl;

        return false;
    }

    sqlite3_bind_text(
        statement,
        1,
        deviceUri.c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    result = sqlite3_step(statement);

    sqlite3_finalize(statement);

    if (result != SQLITE_DONE)
    {
        std::cerr
            << "Greska pri rjesavanju alarma: "
            << sqlite3_errmsg(db)
            << std::endl;

        return false;
    }

    std::cout
        << "Alarm oznacen kao rijesen za: "
        << deviceUri
        << std::endl;

    return true;
}


    // ==========================================
    // POTROSNJA PO REGIJAMA
    // ==========================================

    void printConsumptionByRegion()
    {
        const char* sql =
            "SELECT "
            "d.region_id, "
            "SUM(c.consumption_kwh), "
            "AVG(c.current_power_kw), "
            "COUNT(c.id) "
            "FROM consumption c "
            "JOIN devices d "
            "ON c.device_uri = d.uri "
            "GROUP BY d.region_id "
            "ORDER BY d.region_id;";

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
                << "Greska pri izracunu potrosnje po regijama: "
                << sqlite3_errmsg(db)
                << std::endl;

            return;
        }

        std::cout
            << "\n=== POTROSNJA PO REGIJAMA ==="
            << std::endl;

        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            int regionId =
                sqlite3_column_int(statement, 0);

            double totalConsumption =
                sqlite3_column_double(statement, 1);

            double averagePower =
                sqlite3_column_double(statement, 2);

            int measurementCount =
                sqlite3_column_int(statement, 3);

            std::cout
                << "Regija: "
                << regionId
                << std::endl;

            std::cout
                << "Ukupna potrosnja: "
                << totalConsumption
                << " kWh"
                << std::endl;

            std::cout
                << "Prosjecna snaga: "
                << averagePower
                << " kW"
                << std::endl;

            std::cout
                << "Broj mjerenja: "
                << measurementCount
                << std::endl;

            std::cout
                << "------------------------"
                << std::endl;
        }

        sqlite3_finalize(statement);
    }


    // ==========================================
    // HISTORIJA POTROSNJE I TROSAK
    // ==========================================

    void printConsumptionHistory(
        const std::string& deviceUri)
    {
        const char* sql =
            "SELECT "
            "c.timestamp, "
            "c.consumption_kwh, "
            "c.current_power_kw, "
            "(SELECT price_per_kwh "
            " FROM tariffs "
            " ORDER BY id DESC "
            " LIMIT 1) AS tariff "
            "FROM consumption c "
            "WHERE c.device_uri = ? "
            "ORDER BY c.timestamp ASC;";

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
                << "Greska pri citanju historije potrosnje: "
                << sqlite3_errmsg(db)
                << std::endl;

            return;
        }

        sqlite3_bind_text(
            statement,
            1,
            deviceUri.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        std::cout
            << "\n=== HISTORIJA POTROSNJE ==="
            << std::endl;

        std::cout
            << "Uredjaj: "
            << deviceUri
            << std::endl;

        double totalConsumption = 0.0;
        double totalCost = 0.0;

        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            const unsigned char* timestamp =
                sqlite3_column_text(statement, 0);

            double consumption =
                sqlite3_column_double(statement, 1);

            double power =
                sqlite3_column_double(statement, 2);

            double tariff =
                sqlite3_column_double(statement, 3);

            double cost =
                consumption * tariff;

            totalConsumption += consumption;
            totalCost += cost;

            std::cout
                << "\nVrijeme: "
                << timestamp
                << std::endl;

            std::cout
                << "Potrosnja: "
                << consumption
                << " kWh"
                << std::endl;

            std::cout
                << "Snaga: "
                << power
                << " kW"
                << std::endl;

            std::cout
                << "Tarifa: "
                << tariff
                << " KM/kWh"
                << std::endl;

            std::cout
                << "Trosak: "
                << cost
                << " KM"
                << std::endl;
        }

        sqlite3_finalize(statement);

        std::cout
            << "\n------------------------"
            << std::endl;

        std::cout
            << "Ukupna potrosnja: "
            << totalConsumption
            << " kWh"
            << std::endl;

        std::cout
            << "Ukupan trosak: "
            << totalCost
            << " KM"
            << std::endl;
    }
};

#endif
