#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <openssl/ssl.h>

#include <arpa/inet.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "../../protocol/smart_grid_protocol.hpp"
#include "../../security/pqc_tls.hpp"

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

int main()
{
    const std::string host = "127.0.0.1";
    const std::string port = "5001";

    const std::string deviceUri =
        "smartgrid://sarajevo/meter/001";

    // Koristimo 4 mjerenja.
    // Nakon svakih 5 regularnih CONSUMPTION_REPORT poruka
    // regionalni server pokrece dodatnu tariff/REDUCE logiku.
    constexpr int N = 4;

    try
    {
        boost::asio::io_context io;

        ssl::context sslContext(
            ssl::context::tls_client
        );

        configurePqcTls(sslContext);

        sslContext.load_verify_file(
            "certs/pqc/pqc_ca.crt"
        );

        sslContext.set_verify_mode(
            ssl::verify_peer
        );

        sslContext.use_certificate_chain_file(
            "certs/pqc/meter001.crt"
        );

        sslContext.use_private_key_file(
            "certs/pqc/meter001.key",
            ssl::context::pem
        );

        ssl::stream<tcp::socket> socket(
            io,
            sslContext
        );

        tcp::resolver resolver(io);

        auto endpoints =
            resolver.resolve(
                host,
                port
            );

        std::cout
            << "\n========================================\n"
            << " SMART GRID CONSUMPTION RTT BENCHMARK\n"
            << "========================================\n"
            << "Server: " << host << ":" << port << "\n"
            << "URI: " << deviceUri << "\n"
            << "Broj RTT mjerenja: " << N << "\n";


        // =================================================
        // TCP + PQC TLS
        // =================================================

        boost::asio::connect(
            socket.next_layer(),
            endpoints
        );

        socket.handshake(
            ssl::stream_base::client
        );

        SSL* sslHandle =
            socket.native_handle();

        std::cout
            << "\nTLS verzija: "
            << SSL_get_version(sslHandle)
            << "\n";

        const char* group =
            SSL_get0_group_name(sslHandle);

        std::cout
            << "Pregovorena grupa: "
            << (group ? group : "nepoznata")
            << "\n";


        // =================================================
        // REGISTRACIJA
        // =================================================

        RegisterRequest request{};

        std::strncpy(
            request.device_uri,
            deviceUri.c_str(),
            sizeof(request.device_uri) - 1
        );

        request.device_uri[
            sizeof(request.device_uri) - 1
        ] = '\0';

        request.region_id = 1;
        request.user_type = 1;

        std::vector<uint8_t> serializedRequest =
            serializeRegisterRequest(request);

        boost::asio::write(
            socket,
            boost::asio::buffer(
                serializedRequest
            )
        );


        // REGISTER_ACK header

        std::vector<uint8_t> ackHeader(4);

        boost::asio::read(
            socket,
            boost::asio::buffer(
                ackHeader
            )
        );

        uint16_t ackPayloadLengthNetwork;

        std::memcpy(
            &ackPayloadLengthNetwork,
            ackHeader.data() + 2,
            sizeof(ackPayloadLengthNetwork)
        );

        uint16_t ackPayloadLength =
            ntohs(
                ackPayloadLengthNetwork
            );


        // REGISTER_ACK payload

        std::vector<uint8_t> ackPayload(
            ackPayloadLength
        );

        if (ackPayloadLength > 0)
        {
            boost::asio::read(
                socket,
                boost::asio::buffer(
                    ackPayload
                )
            );
        }

        std::vector<uint8_t> fullAck;

        fullAck.insert(
            fullAck.end(),
            ackHeader.begin(),
            ackHeader.end()
        );

        fullAck.insert(
            fullAck.end(),
            ackPayload.begin(),
            ackPayload.end()
        );

        if (ackHeader[1] !=
            static_cast<uint8_t>(
                MessageType::REGISTER_ACK
            ))
        {
            std::cerr
                << "\nFAIL: REGISTER_ACK nije primljen.\n";

            return 1;
        }

        RegisterAck registerAck =
            deserializeRegisterAck(
                fullAck
            );

        if (registerAck.status != 1)
        {
            std::cerr
                << "\nFAIL: Registracija odbijena.\n";

            return 1;
        }

        std::cout
            << "Registracija uspjesna.\n";


        // =================================================
        // CONSUMPTION_REPORT -> CONSUMPTION_ACK RTT
        // =================================================

        std::vector<double> rttResults;

        for (int i = 0; i < N; ++i)
        {
            ConsumptionReport report{};

            std::strncpy(
                report.device_uri,
                deviceUri.c_str(),
                sizeof(report.device_uri) - 1
            );

            report.device_uri[
                sizeof(report.device_uri) - 1
            ] = '\0';

            report.timestamp =
                static_cast<uint64_t>(
                    std::time(nullptr)
                );

            // Stabilne testne vrijednosti.
            // Snaga je niska kako se ne bi aktivirao REDUCE.
            report.consumption_kwh =
                2.50 + (i * 0.10);

            report.current_power_kw =
                1.20 + (i * 0.10);

            std::vector<uint8_t> serializedReport =
                serializeConsumptionReport(
                    report
                );


            // =============================================
            // RTT MJERENJE POCINJE NEPOSREDNO PRIJE SLANJA
            // =============================================

            auto start =
                std::chrono::steady_clock::now();

            boost::asio::write(
                socket,
                boost::asio::buffer(
                    serializedReport
                )
            );


            // =============================================
            // CONSUMPTION_ACK HEADER
            // =============================================

            std::vector<uint8_t>
                consumptionAckHeader(4);

            boost::asio::read(
                socket,
                boost::asio::buffer(
                    consumptionAckHeader
                )
            );

            uint16_t
                consumptionAckPayloadLengthNetwork;

            std::memcpy(
                &consumptionAckPayloadLengthNetwork,
                consumptionAckHeader.data() + 2,
                sizeof(
                    consumptionAckPayloadLengthNetwork
                )
            );

            uint16_t
                consumptionAckPayloadLength =
                    ntohs(
                        consumptionAckPayloadLengthNetwork
                    );


            // =============================================
            // CONSUMPTION_ACK PAYLOAD
            // =============================================

            std::vector<uint8_t>
                consumptionAckPayload(
                    consumptionAckPayloadLength
                );

            if (consumptionAckPayloadLength > 0)
            {
                boost::asio::read(
                    socket,
                    boost::asio::buffer(
                        consumptionAckPayload
                    )
                );
            }

            auto end =
                std::chrono::steady_clock::now();


            // =============================================
            // PROVJERA TIPA PORUKE
            // =============================================

            if (consumptionAckHeader[1] !=
                static_cast<uint8_t>(
                    MessageType::CONSUMPTION_ACK
                ))
            {
                std::cerr
                    << "\nFAIL: Ocekivan "
                    << "CONSUMPTION_ACK za mjerenje #"
                    << (i + 1)
                    << ".\n";

                return 1;
            }


            std::vector<uint8_t>
                fullConsumptionAck;

            fullConsumptionAck.insert(
                fullConsumptionAck.end(),
                consumptionAckHeader.begin(),
                consumptionAckHeader.end()
            );

            fullConsumptionAck.insert(
                fullConsumptionAck.end(),
                consumptionAckPayload.begin(),
                consumptionAckPayload.end()
            );

            ConsumptionAck consumptionAck =
                deserializeConsumptionAck(
                    fullConsumptionAck
                );

            if (consumptionAck.status != 1)
            {
                std::cerr
                    << "\nFAIL: CONSUMPTION_ACK status nije 1.\n";

                return 1;
            }


            double rttMs =
                std::chrono::duration<
                    double,
                    std::milli
                >(
                    end - start
                ).count();

            rttResults.push_back(
                rttMs
            );

            std::cout
                << std::fixed
                << std::setprecision(3)
                << "Mjerenje #"
                << (i + 1)
                << " RTT: "
                << rttMs
                << " ms\n";
        }


        // =================================================
        // STATISTIKA
        // =================================================

        double minRtt =
            *std::min_element(
                rttResults.begin(),
                rttResults.end()
            );

        double maxRtt =
            *std::max_element(
                rttResults.begin(),
                rttResults.end()
            );

        double sumRtt =
            std::accumulate(
                rttResults.begin(),
                rttResults.end(),
                0.0
            );

        double avgRtt =
            sumRtt /
            static_cast<double>(
                rttResults.size()
            );


        // =================================================
        // REZULTATI
        // =================================================

        std::cout
            << "\n--- RTT REZULTATI ---\n";

        std::cout
            << std::fixed
            << std::setprecision(3);

        std::cout
            << "Broj uspjesnih mjerenja: "
            << rttResults.size()
            << "\n";

        std::cout
            << "Minimalni RTT: "
            << minRtt
            << " ms\n";

        std::cout
            << "Prosjecni RTT: "
            << avgRtt
            << " ms\n";

        std::cout
            << "Maksimalni RTT: "
            << maxRtt
            << " ms\n";

        std::cout
            << "\nPASS: CONSUMPTION_REPORT -> "
            << "CONSUMPTION_ACK RTT benchmark uspjesan.\n";

        std::cout
            << "========================================\n";


        boost::system::error_code ec;

        socket.shutdown(ec);

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "\nBENCHMARK GRESKA: "
            << e.what()
            << "\n";

        return 1;
    }
}
