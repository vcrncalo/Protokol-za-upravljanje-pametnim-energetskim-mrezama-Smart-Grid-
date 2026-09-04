#include <iostream>
#include <vector>
#include <thread>
#include <boost/asio.hpp>
#include <mutex>
#include "../protocol/smart_grid_protocol.hpp"
#include "../database/database.hpp"

using boost::asio::ip::tcp;
Database centralDatabase("database/central.db");
std::mutex centralDatabaseMutex;


// Obrada jednog regionalnog servera
void handleRegionalServer(tcp::socket socket)
{
    try
    {
        std::cout << "Regionalni server se povezao na centralni server."
                  << std::endl;

        while (true)
        {
            // 1. Primamo header - 4 bajta
            std::vector<uint8_t> headerBuffer(4);

            boost::asio::read(
                socket,
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

            uint16_t payloadLength = ntohs(payloadLengthNetwork);


            // 2. Primamo payload
            std::vector<uint8_t> messageBuffer(4 + payloadLength);

            std::copy(
                headerBuffer.begin(),
                headerBuffer.end(),
                messageBuffer.begin()
            );

            boost::asio::read(
                socket,
                boost::asio::buffer(
                    messageBuffer.data() + 4,
                    payloadLength
                )
            );


            // 3. Provjeravamo da li je REGION_SYNC
            if (type ==
                static_cast<uint8_t>(MessageType::REGION_SYNC))
            {
                RegionSync sync =
                    deserializeRegionSync(messageBuffer);
                    {
    std::lock_guard<std::mutex> lock(centralDatabaseMutex);

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
    socket,
    boost::asio::buffer(ackData)
);

std::cout
    << "REGION_SYNC_ACK poslan regionalnom serveru."
    << std::endl;

                std::cout << "\n=== REGION_SYNC ===" << std::endl;

                std::cout << "Verzija protokola: "
                          << static_cast<int>(version)
                          << std::endl;

                std::cout << "Regija: "
                          << sync.source_region
                          << std::endl;

                std::cout << "Uredjaj: "
                          << sync.device_uri
                          << std::endl;

                std::cout << "Potrosnja: "
                          << sync.consumption_kwh
                          << " kWh"
                          << std::endl;

                std::cout << "Trenutna snaga: "
                          << sync.current_power_kw
                          << " kW"
                          << std::endl;

                std::cout << "===================" << std::endl;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "Regionalni server je prekinuo konekciju: "
                  << e.what()
                  << std::endl;
    }
}


int main()
{
    try
    {
    centralDatabase.initialize();
        boost::asio::io_context io_context;

        tcp::acceptor acceptor(
            io_context,
            tcp::endpoint(tcp::v4(), 6000)
        );

        std::cout << "Centralni server pokrenut." << std::endl;
        std::cout << "Ceka regionalne servere na portu 6000..."
                  << std::endl;


        // Centralni server moze prihvatiti vise regionalnih servera
        while (true)
        {
            tcp::socket socket(io_context);

            acceptor.accept(socket);

            std::thread(
                handleRegionalServer,
                std::move(socket)
            ).detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Greska: "
                  << e.what()
                  << std::endl;
    }

    return 0;
}
