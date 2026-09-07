#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <openssl/ssl.h>

#include <arpa/inet.h>

#include <chrono>
#include <cstring>
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

    try
    {
        boost::asio::io_context io;

        ssl::context sslContext(
            ssl::context::tls_client
        );

        configurePqcTls(sslContext);

        // CA certifikat
        sslContext.load_verify_file(
            "certs/pqc/pqc_ca.crt"
        );

        sslContext.set_verify_mode(
            ssl::verify_peer
        );

        // mTLS Smart Meter certifikat
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
            << "\n========================================\n";

        std::cout
            << " SMART GRID REGISTRATION BENCHMARK\n";

        std::cout
            << "========================================\n";

        std::cout
            << "Server: "
            << host
            << ":"
            << port
            << "\n";

        std::cout
            << "URI: "
            << deviceUri
            << "\n";


        // =================================================
        // UKUPNO MJERENJE POCINJE
        // =================================================

        auto totalStart =
            std::chrono::steady_clock::now();


        // =================================================
        // 1. TCP CONNECT
        // =================================================

        auto tcpStart =
            std::chrono::steady_clock::now();

        boost::asio::connect(
            socket.next_layer(),
            endpoints
        );

        auto tcpEnd =
            std::chrono::steady_clock::now();


        // =================================================
        // 2. PQC TLS HANDSHAKE
        // =================================================

        auto tlsStart =
            std::chrono::steady_clock::now();

        socket.handshake(
            ssl::stream_base::client
        );

        auto tlsEnd =
            std::chrono::steady_clock::now();


        // Informacije o uspostavljenoj TLS vezi

        SSL* sslHandle =
            socket.native_handle();

        const char* tlsVersion =
            SSL_get_version(
                sslHandle
            );

        const char* negotiatedGroup =
            SSL_get0_group_name(
                sslHandle
            );

        const SSL_CIPHER* cipher =
            SSL_get_current_cipher(
                sslHandle
            );

        const char* peerSignature =
            nullptr;

        int signatureResult =
            SSL_get0_peer_signature_name(
                sslHandle,
                &peerSignature
            );


        // =================================================
        // 3. REGISTER_REQ
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

        // Sarajevo = Region 1
        request.region_id = 1;

        // Domacinstvo = user_type 1
        request.user_type = 1;

        std::vector<uint8_t> serialized =
            serializeRegisterRequest(
                request
            );


        // Mjerimo vrijeme od slanja REGISTER_REQ
        // do prijema kompletnog REGISTER_ACK.

        auto registrationStart =
            std::chrono::steady_clock::now();

        boost::asio::write(
            socket,
            boost::asio::buffer(
                serialized
            )
        );


        // =================================================
        // 4. REGISTER_ACK HEADER
        // =================================================

        std::vector<uint8_t> ackHeader(4);

        boost::asio::read(
            socket,
            boost::asio::buffer(
                ackHeader
            )
        );

        uint8_t ackType =
            ackHeader[1];

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


        // =================================================
        // 5. REGISTER_ACK PAYLOAD
        // =================================================

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

        auto registrationEnd =
            std::chrono::steady_clock::now();

        auto totalEnd =
            registrationEnd;


        // =================================================
        // PROVJERA ODGOVORA
        // =================================================

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

        if (ackType !=
            static_cast<uint8_t>(
                MessageType::REGISTER_ACK
            ))
        {
            std::cerr
                << "\nFAIL: Ocekivan REGISTER_ACK.\n";

            return 1;
        }

        RegisterAck ack =
            deserializeRegisterAck(
                fullAck
            );


        // =================================================
        // IZRAČUN VREMENA
        // =================================================

        double tcpMs =
            std::chrono::duration<
                double,
                std::milli
            >(
                tcpEnd - tcpStart
            ).count();

        double tlsMs =
            std::chrono::duration<
                double,
                std::milli
            >(
                tlsEnd - tlsStart
            ).count();

        double registrationMs =
            std::chrono::duration<
                double,
                std::milli
            >(
                registrationEnd -
                registrationStart
            ).count();

        double totalMs =
            std::chrono::duration<
                double,
                std::milli
            >(
                totalEnd -
                totalStart
            ).count();


        // =================================================
        // REZULTATI
        // =================================================

        std::cout
            << "\n--- PQC TLS INFORMACIJE ---\n";

        std::cout
            << "TLS verzija: "
            << (tlsVersion
                    ? tlsVersion
                    : "nepoznata")
            << "\n";

        std::cout
            << "Pregovorena grupa: "
            << (negotiatedGroup
                    ? negotiatedGroup
                    : "nepoznata")
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

        std::cout
            << "Cipher suite: "
            << (
                cipher
                    ? SSL_CIPHER_get_name(
                          cipher
                      )
                    : "nepoznat"
            )
            << "\n";


        std::cout
            << "\n--- REGISTRACIJA ---\n";

        std::cout
            << "REGISTER_ACK status: "
            << static_cast<int>(
                   ack.status
               )
            << "\n";


        std::cout
            << std::fixed
            << std::setprecision(3);

        std::cout
            << "\n--- LATENCY REZULTATI ---\n";

        std::cout
            << "TCP connect: "
            << tcpMs
            << " ms\n";

        std::cout
            << "PQC TLS handshake: "
            << tlsMs
            << " ms\n";

        std::cout
            << "REGISTER_REQ -> REGISTER_ACK: "
            << registrationMs
            << " ms\n";

        std::cout
            << "Ukupno TCP + TLS + registracija: "
            << totalMs
            << " ms\n";


        if (ack.status == 1)
        {
            std::cout
                << "\nPASS: PQC TLS registracija "
                << "uspjesno izmjerena.\n";
        }
        else
        {
            std::cout
                << "\nFAIL: Regionalni server "
                << "je odbio registraciju.\n";

            return 1;
        }

        std::cout
            << "========================================\n";


        // Pokusaj urednog TLS zatvaranja.
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
