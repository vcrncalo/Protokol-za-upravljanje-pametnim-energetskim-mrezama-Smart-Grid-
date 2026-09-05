#include <boost/asio.hpp>
#include <sqlite3.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

using boost::asio::ip::tcp;

const std::string DATABASE_PATH = "database/central.db";

struct MonitoringData
{
    double totalConsumption = 0.0;
    double region1Consumption = 0.0;
    double region2Consumption = 0.0;
    double averagePower = 0.0;
    int measurementCount = 0;
};

MonitoringData loadMonitoringData()
{
    MonitoringData data;
    sqlite3* db = nullptr;

    if (sqlite3_open(DATABASE_PATH.c_str(), &db) != SQLITE_OK)
    {
        std::cerr << "Greska pri otvaranju centralne baze." << std::endl;

        if (db)
            sqlite3_close(db);

        return data;
    }

    const char* sql =
        "SELECT "
        "COALESCE(SUM(consumption_kwh), 0), "
        "COALESCE(SUM(CASE WHEN source_region = 1 "
        "THEN consumption_kwh ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN source_region = 2 "
        "THEN consumption_kwh ELSE 0 END), 0), "
        "COALESCE(AVG(current_power_kw), 0), "
        "COUNT(*) "
        "FROM central_consumption;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            data.totalConsumption =
                sqlite3_column_double(stmt, 0);

            data.region1Consumption =
                sqlite3_column_double(stmt, 1);

            data.region2Consumption =
                sqlite3_column_double(stmt, 2);

            data.averagePower =
                sqlite3_column_double(stmt, 3);

            data.measurementCount =
                sqlite3_column_int(stmt, 4);
        }
    }
    else
    {
        std::cerr
            << "Greska SQL upita: "
            << sqlite3_errmsg(db)
            << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return data;
}

std::string loadLatestMeasurements()
{
    sqlite3* db = nullptr;

    if (sqlite3_open(DATABASE_PATH.c_str(), &db) != SQLITE_OK)
    {
        if (db)
            sqlite3_close(db);

        return "<tr><td colspan='5'>Baza nije dostupna.</td></tr>";
    }

    const char* sql =
        "SELECT source_region, device_uri, timestamp, "
        "consumption_kwh, current_power_kw "
        "FROM central_consumption "
        "ORDER BY id DESC "
        "LIMIT 10;";

    sqlite3_stmt* stmt = nullptr;
    std::ostringstream html;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        bool found = false;

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = true;

            int region =
                sqlite3_column_int(stmt, 0);

            const unsigned char* uri =
                sqlite3_column_text(stmt, 1);

            sqlite3_int64 timestamp =
                sqlite3_column_int64(stmt, 2);

            double consumption =
                sqlite3_column_double(stmt, 3);

            double power =
                sqlite3_column_double(stmt, 4);

            html << "<tr>";

            html << "<td>Region "
                 << region
                 << "</td>";

            html << "<td>"
                 << (uri
                         ? reinterpret_cast<const char*>(uri)
                         : "")
                 << "</td>";

            std::time_t timeValue =
    static_cast<std::time_t>(timestamp);

std::tm localTime{};

localtime_r(&timeValue, &localTime);

char timeBuffer[32];

std::strftime(
    timeBuffer,
    sizeof(timeBuffer),
    "%d.%m.%Y %H:%M:%S",
    &localTime
);

html << "<td>"
     << timeBuffer
     << "</td>";

            html << "<td>"
                 << std::fixed
                 << std::setprecision(2)
                 << consumption
                 << " kWh</td>";

            html << "<td>"
                 << std::fixed
                 << std::setprecision(2)
                 << power
                 << " kW</td>";

            html << "</tr>";
        }

        if (!found)
        {
            html
                << "<tr><td colspan='5'>"
                << "Nema evidentiranih mjerenja."
                << "</td></tr>";
        }
    }
    else
    {
        html
            << "<tr><td colspan='5'>"
            << "Greska pri citanju baze."
            << "</td></tr>";
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return html.str();
}

std::string generatePage()
{
    MonitoringData data = loadMonitoringData();
    std::string measurements = loadLatestMeasurements();

    std::ostringstream html;

    html << std::fixed << std::setprecision(2);

    html <<
        "<!DOCTYPE html>"
        "<html lang='bs'>"

        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' "
        "content='width=device-width, initial-scale=1.0'>"

        "<meta http-equiv='refresh' content='5'>"

        "<title>Smart Grid Monitoring</title>"

        "<style>"

        "*{box-sizing:border-box;}"

        "body{"
        "font-family:Arial,sans-serif;"
        "background:#f4f6f8;"
        "margin:0;"
        "color:#263238;"
        "}"

        ".header{"
        "background:#263238;"
        "color:white;"
        "padding:25px;"
        "text-align:center;"
        "}"

        ".header h1{"
        "margin:0 0 8px 0;"
        "}"

        ".container{"
        "max-width:1200px;"
        "margin:30px auto;"
        "padding:20px;"
        "}"

        ".cards{"
        "display:grid;"
        "grid-template-columns:repeat(auto-fit,minmax(200px,1fr));"
        "gap:20px;"
        "}"

        ".card{"
        "background:white;"
        "padding:25px;"
        "border-radius:12px;"
        "box-shadow:0 2px 8px rgba(0,0,0,0.08);"
        "text-align:center;"
        "}"

        ".card h3{"
        "margin-top:0;"
        "font-size:16px;"
        "}"

        ".value{"
        "font-size:28px;"
        "font-weight:bold;"
        "margin-top:15px;"
        "}"

        ".table-box{"
        "background:white;"
        "margin-top:30px;"
        "padding:25px;"
        "border-radius:12px;"
        "box-shadow:0 2px 8px rgba(0,0,0,0.08);"
        "overflow-x:auto;"
        "}"

        "table{"
        "width:100%;"
        "border-collapse:collapse;"
        "}"

        "th,td{"
        "padding:12px;"
        "border-bottom:1px solid #ddd;"
        "text-align:left;"
        "}"

        "th{"
        "background:#eceff1;"
        "}"

        ".status{"
        "margin-top:20px;"
        "font-size:14px;"
        "text-align:center;"
        "}"

        "</style>"
        "</head>"

        "<body>"

        "<div class='header'>"
        "<h1>Smart Grid Monitoring</h1>"
        "<div>"
        "Univerzitet u Sarajevu - Elektrotehnicki fakultet"
        "</div>"
        "</div>"

        "<div class='container'>"

        "<div class='cards'>"

        "<div class='card'>"
        "<h3>Ukupna potrosnja</h3>"
        "<div class='value'>"
        << data.totalConsumption <<
        " kWh</div>"
        "</div>"

        "<div class='card'>"
        "<h3>Region 1 - Sarajevo</h3>"
        "<div class='value'>"
        << data.region1Consumption <<
        " kWh</div>"
        "</div>"

        "<div class='card'>"
        "<h3>Region 2 - Mostar</h3>"
        "<div class='value'>"
        << data.region2Consumption <<
        " kWh</div>"
        "</div>"

        "<div class='card'>"
        "<h3>Prosjecna snaga</h3>"
        "<div class='value'>"
        << data.averagePower <<
        " kW</div>"
        "</div>"

        "<div class='card'>"
        "<h3>Broj mjerenja</h3>"
        "<div class='value'>"
        << data.measurementCount <<
        "</div>"
        "</div>"

        "</div>"

        "<div class='table-box'>"

        "<h2>Posljednja mjerenja</h2>"

        "<table>"

        "<thead>"
        "<tr>"
        "<th>Region</th>"
        "<th>Smart Meter URI</th>"
        "<th>Timestamp</th>"
        "<th>Potrosnja</th>"
        "<th>Snaga</th>"
        "</tr>"
        "</thead>"

        "<tbody>"
        << measurements <<
        "</tbody>"

        "</table>"

        "</div>"

        "<div class='status'>"
        "Podaci se automatski osvjezavaju svakih 5 sekundi."
        "</div>"

        "</div>"

        "</body>"
        "</html>";

    return html.str();
}

class WebSession :
    public std::enable_shared_from_this<WebSession>
{
private:
    tcp::socket socket_;
    boost::asio::streambuf request_;

public:
    explicit WebSession(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        readRequest();
    }

private:
    void readRequest()
    {
        auto self = shared_from_this();

        boost::asio::async_read_until(
            socket_,
            request_,
            "\r\n\r\n",
            [this, self](
                const boost::system::error_code& ec,
                std::size_t)
            {
                if (!ec)
                    sendResponse();
            });
    }

    void sendResponse()
    {
        std::string page = generatePage();

        auto response =
            std::make_shared<std::string>(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Content-Length: " +
                std::to_string(page.size()) +
                "\r\n"
                "Connection: close\r\n"
                "\r\n" +
                page);

        auto self = shared_from_this();

        boost::asio::async_write(
            socket_,
            boost::asio::buffer(*response),
            [this, self, response](
                const boost::system::error_code&,
                std::size_t)
            {
                boost::system::error_code ignored;

                socket_.shutdown(
                    tcp::socket::shutdown_both,
                    ignored);

                socket_.close(ignored);
            });
    }
};

class WebServer
{
private:
    tcp::acceptor acceptor_;

public:
    WebServer(
        boost::asio::io_context& io,
        unsigned short port)
        : acceptor_(
              io,
              tcp::endpoint(tcp::v4(), port))
    {
        accept();
    }

private:
    void accept()
    {
        acceptor_.async_accept(
            [this](
                const boost::system::error_code& ec,
                tcp::socket socket)
            {
                if (!ec)
                {
                    std::cout
                        << "Web monitoring zahtjev primljen."
                        << std::endl;

                    std::make_shared<WebSession>(
                        std::move(socket))->start();
                }

                accept();
            });
    }
};

int main()
{
    try
    {
        boost::asio::io_context io;

        const unsigned short port = 8080;

        WebServer server(io, port);

        std::cout
            << "Smart Grid Web Monitoring pokrenut."
            << std::endl;

        std::cout
            << "Centralna baza: "
            << DATABASE_PATH
            << std::endl;

        std::cout
            << "Otvori: http://localhost:"
            << port
            << std::endl;

        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "Greska: "
            << e.what()
            << std::endl;

        return 1;
    }

    return 0;
}
