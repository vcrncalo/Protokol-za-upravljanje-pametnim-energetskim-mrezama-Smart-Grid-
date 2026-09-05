#include <iostream>
#include <vector>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <mutex>
#include <cstring>
#include <arpa/inet.h>
#include "../protocol/smart_grid_protocol.hpp"
#include "../database/database.hpp"

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

Database centralDatabase("database/central.db");
std::mutex centralDatabaseMutex;

using tls_socket = ssl::stream<tcp::socket>;


// Obrada jednog regionalnog servera kroz TLS
void handleRegionalServer(std::shared_ptr<tls_socket> socket)
{
    try
    {
        std::cout
            << "TLS konekcija sa regionalnim serverom uspostavljena."
            << std::endl;

        while (true)
        {
            // 1. Primamo header - 4 bajta kroz TLS
            std::vector<uint8_t> headerBuffer(4);

            boost::asio::read(
                *socket,
                boost::asio::buffer(headerBuffer)
            );

            uint8_t version = headerBuffer[0];
            uint8_t type = headerBuffer[1];

            uint16_t payloadLengthNetwork;

            std::memcpy(
                &payloadLengthNetwork,
                headerBuffer.data() + 2,
                sizeof(uint16_t)
            );

            uint16_t payloadLength =
                ntohs(payloadLengthNetwork);


            // 2. Primamo payload kroz TLS
            std::vector<uint8_t> messageBuffer(
                4 + payloadLength
            );

            std::copy(
                headerBuffer.begin(),
                headerBuffer.end(),
                messageBuffer.begin()
            );

            boost::asio::read(
                *socket,
                boost::asio::buffer(
                    messageBuffer.data() + 4,
                    payloadLength
                )
            );


            // 3. Provjeravamo da li je REGION_SYNC
            if (type ==
                static_cast<uint8_t>(
                    MessageType::REGION_SYNC))
            {
                RegionSync sync =
                    deserializeRegionSync(
                        messageBuffer
                    );

                {
                    std::lock_guard<std::mutex> lock(
                        centralDatabaseMutex
                    );

                    centralDatabase.insertCentralConsumption(
                        sync.source_region,
                        std::string(sync.device_uri),
                        sync.timestamp,
                        sync.consumption_kwh,
                        sync.current_power_kw
                    );

                    centralDatabase.printCentralAggregation();
                }

                RegionSyncAck ack{};
                ack.status = 1;

                std::vector<uint8_t> ackData =
                    serializeRegionSyncAck(ack);

                boost::asio::write(
                    *socket,
                    boost::asio::buffer(ackData)
                );

                std::cout
                    << "REGION_SYNC_ACK poslan regionalnom serveru kroz TLS."
                    << std::endl;

                std::cout
                    << "\n=== REGION_SYNC ==="
                    << std::endl;

                std::cout
                    << "Verzija protokola: "
                    << static_cast<int>(version)
                    << std::endl;

                std::cout
                    << "Regija: "
                    << sync.source_region
                    << std::endl;

                std::cout
                    << "Uredjaj: "
                    << sync.device_uri
                    << std::endl;

                std::cout
                    << "Potrosnja: "
                    << sync.consumption_kwh
                    << " kWh"
                    << std::endl;

                std::cout
                    << "Trenutna snaga: "
                    << sync.current_power_kw
                    << " kW"
                    << std::endl;

                std::cout
                    << "==================="
                    << std::endl;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cout
            << "Regionalni server je prekinuo TLS konekciju: "
            << e.what()
            << std::endl;
    }
}


int main()
{
    try
    {
        if (!centralDatabase.initialize())
        {
            std::cerr
                << "Centralna baza nije uspjesno inicijalizovana."
                << std::endl;
            return 1;
        }

        boost::asio::io_context io_context;

        ssl::context sslContext(
            ssl::context::tls_server
        );

        sslContext.set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2 |
            ssl::context::no_sslv3
        );

        // Za sada koristimo isti serverski certifikat potpisan
        // SmartGrid CA certifikatom. Kasnije mozemo izdvojiti
        // poseban certifikat centralnog servera.
        sslContext.use_certificate_chain_file(
            "certs/regional_server.crt"
        );

        sslContext.use_private_key_file(
            "certs/regional_server.key",
            ssl::context::pem
        );

        tcp::acceptor acceptor(
            io_context,
            tcp::endpoint(
                tcp::v4(),
                6000
            )
        );

        std::cout
            << "Centralni TLS server pokrenut."
            << std::endl;

        std::cout
            << "Ceka regionalne servere na TLS portu 6000..."
            << std::endl;


        // Centralni server moze prihvatiti vise regionalnih servera
        while (true)
        {
            auto socket =
                std::make_shared<tls_socket>(
                    io_context,
                    sslContext
                );

            // Prvo se uspostavlja TCP konekcija.
            acceptor.accept(
                socket->next_layer()
            );

            std::cout
                << "TCP konekcija regionalnog servera prihvacena. "
                << "Pokrecem TLS handshake..."
                << std::endl;

            try
            {
                // Zatim se preko TCP konekcije uspostavlja TLS.
                socket->handshake(
                    ssl::stream_base::server
                );

                std::cout
                    << "TLS handshake sa regionalnim serverom uspjesan."
                    << std::endl;

                std::thread(
                    handleRegionalServer,
                    socket
                ).detach();
            }
            catch (const std::exception& e)
            {
                std::cout
                    << "TLS handshake sa regionalnim serverom nije uspio: "
                    << e.what()
                    << std::endl;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "Greska: "
            << e.what()
            << std::endl;
    }

    return 0;
}
