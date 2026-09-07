#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <openssl/ssl.h>

#include <arpa/inet.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
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

    // Broj DATA_STREAM_SAMPLE poruka za benchmark.
    constexpr int N = 1000;

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
            << " SMART GRID DATA-STREAM BENCHMARK\n"
            << "========================================\n"
            << "Server: " << host << ":" << port << "\n"
            << "URI: " << deviceUri << "\n"
            << "Broj DATA_STREAM_SAMPLE poruka: "
            << N << "\n";


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

        const char* group =
            SSL_get0_group_name(
                sslHandle
            );

        const char* peerSignature =
            nullptr;

        int signatureResult =
            SSL_get0_peer_signature_name(
                sslHandle,
                &peerSignature
            );

        std::cout
            << "\nTLS verzija: "
            << SSL_get_version(sslHandle)
            << "\n";

        std::cout
            << "Pregovorena grupa: "
            << (group ? group : "nepoznata")
            << "\n";

        std::cout
            << "Potpis TLS handshake-a: "
            << (
                signatureResult == 1 &&
                peerSignature
                    ? peerSignature
                    : "nepoznat"
            )
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
            serializeRegisterRequest(
                request
            );

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
            sizeof(
                ackPayloadLengthNetwork
            )
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
        // DATA-STREAM BENCHMARK
        // =================================================

        std::size_t totalBytes = 0;
        std::size_t messageSize = 0;

        auto streamStart =
            std::chrono::steady_clock::now();

        for (int i = 0; i < N; ++i)
        {
            DataStreamSample sample{};

            std::strncpy(
                sample.device_uri,
                deviceUri.c_str(),
                sizeof(sample.device_uri) - 1
            );

            sample.device_uri[
                sizeof(sample.device_uri) - 1
            ] = '\0';

            sample.timestamp =
                static_cast<uint64_t>(
                    std::time(nullptr)
                );

            sample.sequence_number =
                static_cast<uint32_t>(
                    i + 1
                );

            sample.consumption_kwh =
                2.50 + ((i % 10) * 0.01);

            sample.current_power_kw =
                1.20 + ((i % 5) * 0.01);

            std::vector<uint8_t> serializedSample =
                serializeDataStreamSample(
                    sample
                );

            if (i == 0)
            {
                messageSize =
                    serializedSample.size();
            }

            boost::asio::write(
                socket,
                boost::asio::buffer(
                    serializedSample
                )
            );

            totalBytes +=
                serializedSample.size();
        }

        auto streamEnd =
            std::chrono::steady_clock::now();


        // =================================================
        // VERIFIKACIJA NAKON DATA-STREAM TOKA
        // =================================================
        //
        // DATA_STREAM_SAMPLE nema poseban ACK.
        // Zato nakon svih stream poruka saljemo jedan
        // regularni CONSUMPTION_REPORT.
        //
        // Ako dobijemo CONSUMPTION_ACK, potvrdujemo da je
        // server nakon prijema stream poruka nastavio
        // pravilno obradivati protokol.

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

        report.consumption_kwh = 2.50;
        report.current_power_kw = 1.20;

        std::vector<uint8_t> serializedReport =
            serializeConsumptionReport(
                report
            );

        boost::asio::write(
            socket,
            boost::asio::buffer(
                serializedReport
            )
        );


        // CONSUMPTION_ACK header

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

        if (consumptionAckHeader[1] !=
            static_cast<uint8_t>(
                MessageType::CONSUMPTION_ACK
            ))
        {
            std::cerr
                << "\nFAIL: Server nije vratio "
                << "CONSUMPTION_ACK nakon DATA-STREAM toka.\n";

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


        // =================================================
        // REZULTATI
        // =================================================

        double elapsedMs =
            std::chrono::duration<
                double,
                std::milli
            >(
                streamEnd - streamStart
            ).count();

        double elapsedSeconds =
            elapsedMs / 1000.0;

        double messagesPerSecond =
            static_cast<double>(N) /
            elapsedSeconds;

        double bytesPerSecond =
            static_cast<double>(
                totalBytes
            ) / elapsedSeconds;

        double megabitsPerSecond =
            (
                bytesPerSecond * 8.0
            ) / 1000000.0;

        double averageMs =
            elapsedMs /
            static_cast<double>(N);


        std::cout
            << "\n--- DATA-STREAM REZULTATI ---\n";

        std::cout
            << std::fixed
            << std::setprecision(3);

        std::cout
            << "Broj poslanih poruka: "
            << N
            << "\n";

        std::cout
            << "Velicina jedne poruke: "
            << messageSize
            << " B\n";

        std::cout
            << "Ukupno poslano podataka: "
            << totalBytes
            << " B\n";

        std::cout
            << "Ukupno vrijeme slanja: "
            << elapsedMs
            << " ms\n";

        std::cout
            << "Prosjecno po poruci: "
            << averageMs
            << " ms\n";

        std::cout
            << "Propusnost: "
            << messagesPerSecond
            << " poruka/s\n";

        std::cout
            << "Propusnost podataka: "
            << megabitsPerSecond
            << " Mbit/s\n";

        std::cout
            << "CONSUMPTION_ACK nakon toka: USPJESAN\n";

        std::cout
            << "\nPASS: DATA-STREAM benchmark uspjesan.\n";

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

