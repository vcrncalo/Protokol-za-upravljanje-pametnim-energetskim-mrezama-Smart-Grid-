#include <sqlite3.h>

#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>

int main()
{
    sqlite3* db = nullptr;

    // Testna baza u RAM-u.
    // Ne mijenja central.db, region1.db ni region2.db.
    if (sqlite3_open(":memory:", &db) != SQLITE_OK)
    {
        std::cerr << "Nije moguce otvoriti testnu bazu.\n";
        return 1;
    }

    const char* createSql =
        "CREATE TABLE central_consumption ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source_region INTEGER NOT NULL,"
        "device_uri TEXT NOT NULL,"
        "timestamp INTEGER NOT NULL,"
        "consumption_kwh REAL NOT NULL,"
        "current_power_kw REAL NOT NULL"
        ");";

    if (sqlite3_exec(
            db,
            createSql,
            nullptr,
            nullptr,
            nullptr) != SQLITE_OK)
    {
        std::cerr
            << "Nije moguce kreirati testnu tabelu.\n";

        sqlite3_close(db);
        return 1;
    }

    constexpr int N = 10000;

    // =====================================================
    // 1. INSERT BENCHMARK
    // =====================================================

    const char* insertSql =
        "INSERT INTO central_consumption "
        "(source_region, device_uri, timestamp, "
        "consumption_kwh, current_power_kw) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* insertStmt = nullptr;

    if (sqlite3_prepare_v2(
            db,
            insertSql,
            -1,
            &insertStmt,
            nullptr) != SQLITE_OK)
    {
        std::cerr
            << "Nije moguce pripremiti INSERT upit.\n";

        sqlite3_close(db);
        return 1;
    }

    sqlite3_exec(
        db,
        "BEGIN TRANSACTION;",
        nullptr,
        nullptr,
        nullptr);

    auto insertStart =
        std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i)
    {
        int region = (i % 2) + 1;

        std::string uri =
            region == 1
                ? "smartgrid://sarajevo/meter/001"
                : "smartgrid://mostar/meter/002";

        sqlite3_bind_int(
            insertStmt,
            1,
            region);

        sqlite3_bind_text(
            insertStmt,
            2,
            uri.c_str(),
            -1,
            SQLITE_TRANSIENT);

        sqlite3_bind_int64(
            insertStmt,
            3,
            1700000000LL + i);

        sqlite3_bind_double(
            insertStmt,
            4,
            2.50 + (i % 10) * 0.1);

        sqlite3_bind_double(
            insertStmt,
            5,
            1.20 + (i % 5) * 0.1);

        if (sqlite3_step(insertStmt) != SQLITE_DONE)
        {
            std::cerr
                << "Greska pri INSERT operaciji.\n";

            sqlite3_finalize(insertStmt);
            sqlite3_close(db);
            return 1;
        }

        sqlite3_reset(insertStmt);
        sqlite3_clear_bindings(insertStmt);
    }

    sqlite3_exec(
        db,
        "COMMIT;",
        nullptr,
        nullptr,
        nullptr);

    auto insertEnd =
        std::chrono::steady_clock::now();

    sqlite3_finalize(insertStmt);

    double insertElapsedMs =
        std::chrono::duration<double, std::milli>(
            insertEnd - insertStart).count();

    double insertAvgMs =
        insertElapsedMs / N;

    double insertsPerSecond =
        N / (insertElapsedMs / 1000.0);


    // =====================================================
    // 2. SELECT / READ BENCHMARK
    // =====================================================

    // Svaki SELECT trazi jedan konkretan zapis prema ID-u.
    const char* selectSql =
        "SELECT "
        "source_region, "
        "device_uri, "
        "timestamp, "
        "consumption_kwh, "
        "current_power_kw "
        "FROM central_consumption "
        "WHERE id = ?;";

    sqlite3_stmt* selectStmt = nullptr;

    if (sqlite3_prepare_v2(
            db,
            selectSql,
            -1,
            &selectStmt,
            nullptr) != SQLITE_OK)
    {
        std::cerr
            << "Nije moguce pripremiti SELECT upit.\n";

        sqlite3_close(db);
        return 1;
    }

    int successfullyRead = 0;

    auto selectStart =
        std::chrono::steady_clock::now();

    for (int i = 1; i <= N; ++i)
    {
        sqlite3_bind_int(
            selectStmt,
            1,
            i);

        int result =
            sqlite3_step(selectStmt);

        if (result == SQLITE_ROW)
        {
            // Citamo vrijednosti da benchmark zaista
            // izvrsi pristup podacima iz rezultata.
            volatile int region =
                sqlite3_column_int(
                    selectStmt,
                    0);

            const unsigned char* uri =
                sqlite3_column_text(
                    selectStmt,
                    1);

            volatile sqlite3_int64 timestamp =
                sqlite3_column_int64(
                    selectStmt,
                    2);

            volatile double consumption =
                sqlite3_column_double(
                    selectStmt,
                    3);

            volatile double power =
                sqlite3_column_double(
                    selectStmt,
                    4);

            (void)region;
            (void)uri;
            (void)timestamp;
            (void)consumption;
            (void)power;

            ++successfullyRead;
        }
        else
        {
            std::cerr
                << "Greska pri SELECT operaciji za ID "
                << i
                << ".\n";

            sqlite3_finalize(selectStmt);
            sqlite3_close(db);
            return 1;
        }

        sqlite3_reset(selectStmt);
        sqlite3_clear_bindings(selectStmt);
    }

    auto selectEnd =
        std::chrono::steady_clock::now();

    sqlite3_finalize(selectStmt);

    double selectElapsedMs =
        std::chrono::duration<double, std::milli>(
            selectEnd - selectStart).count();

    double selectAvgMs =
        selectElapsedMs / N;

    double selectsPerSecond =
        N / (selectElapsedMs / 1000.0);


    // =====================================================
    // 3. REZULTATI
    // =====================================================

    std::cout
        << "\n========================================\n";

    std::cout
        << " SMART GRID DATABASE BENCHMARK\n";

    std::cout
        << "========================================\n";

    std::cout
        << "Testna SQLite baza: :memory:\n";

    std::cout
        << "Broj testnih zapisa: "
        << N
        << "\n";

    std::cout
        << std::fixed
        << std::setprecision(3);


    std::cout
        << "\n--- INSERT BENCHMARK ---\n";

    std::cout
        << "Broj INSERT operacija: "
        << N
        << "\n";

    std::cout
        << "Ukupno vrijeme: "
        << insertElapsedMs
        << " ms\n";

    std::cout
        << "Prosjecno vrijeme po INSERT-u: "
        << insertAvgMs
        << " ms\n";

    std::cout
        << "INSERT propusnost: "
        << insertsPerSecond
        << " operacija/s\n";


    std::cout
        << "\n--- SELECT/READ BENCHMARK ---\n";

    std::cout
        << "Broj SELECT operacija: "
        << N
        << "\n";

    std::cout
        << "Uspjesno procitano zapisa: "
        << successfullyRead
        << "\n";

    std::cout
        << "Ukupno vrijeme: "
        << selectElapsedMs
        << " ms\n";

    std::cout
        << "Prosjecno vrijeme po SELECT-u: "
        << selectAvgMs
        << " ms\n";

    std::cout
        << "SELECT propusnost: "
        << selectsPerSecond
        << " operacija/s\n";

    std::cout
        << "========================================\n";

    sqlite3_close(db);

    return 0;
}
