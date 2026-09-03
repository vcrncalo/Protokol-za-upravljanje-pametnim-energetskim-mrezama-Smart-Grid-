#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <arpa/inet.h>

#include "../protocol/smart_grid_protocol.hpp"

using boost::asio::ip::tcp;

int main()
{
    try
    {
        boost::asio::io_context io;

        tcp::acceptor acceptor(
            io,
            tcp::endpoint(tcp::v4(), 5000)
        );

        std::cout << "Server slusa na portu 5000..." << std::endl;

        tcp::socket socket(io);

        acceptor.accept(socket);

        std::cout << "Klijent se povezao!" << std::endl;

        // 1. Prvo citamo header od 4 bajta
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
            sizeof(payloadLengthNetwork)
        );

        uint16_t payloadLength =
            ntohs(payloadLengthNetwork);

        std::cout << "Version: "
                  << static_cast<int>(version)
                  << std::endl;

        std::cout << "Type: "
                  << static_cast<int>(type)
                  << std::endl;

        std::cout << "Payload length: "
                  << payloadLength
                  << std::endl;

        // 2. Citamo payload
        std::vector<uint8_t> payload(payloadLength);

        boost::asio::read(
            socket,
            boost::asio::buffer(payload)
        );

        // 3. Sastavimo kompletnu poruku
        std::vector<uint8_t> fullMessage;

        fullMessage.insert(
            fullMessage.end(),
            headerBuffer.begin(),
            headerBuffer.end()
        );

        fullMessage.insert(
            fullMessage.end(),
            payload.begin(),
            payload.end()
        );

        // 4. Provjera tipa poruke
        if (type ==
            static_cast<uint8_t>(MessageType::REGISTER_REQ))
        {
            RegisterRequest request =
                deserializeRegisterRequest(fullMessage);

            std::cout << "Primljen REGISTER_REQ" << std::endl;

            std::cout << "URI: "
                      << request.device_uri
                      << std::endl;

            std::cout << "Region ID: "
                      << request.region_id
                      << std::endl;

            std::cout << "User type: "
                      << static_cast<int>(request.user_type)
                      << std::endl;

RegisterAck ack{};
ack.status = 1;

std::vector<uint8_t> serializedAck =
    serializeRegisterAck(ack);

boost::asio::write(
    socket,
    boost::asio::buffer(serializedAck)
);

std::cout << "REGISTER_ACK poslan." << std::endl;
for (int i = 0; i < 5; i++)
{
// Cekamo sljedecu poruku - CONSUMPTION_REPORT
std::vector<uint8_t> consumptionHeader(4);

boost::asio::read(
    socket,
    boost::asio::buffer(consumptionHeader)
);

uint8_t consumptionVersion = consumptionHeader[0];
uint8_t consumptionType = consumptionHeader[1];

uint16_t consumptionPayloadLengthNetwork;

std::memcpy(
    &consumptionPayloadLengthNetwork,
    consumptionHeader.data() + 2,
    sizeof(consumptionPayloadLengthNetwork)
);

uint16_t consumptionPayloadLength =
    ntohs(consumptionPayloadLengthNetwork);

std::vector<uint8_t> consumptionPayload(
    consumptionPayloadLength
);

boost::asio::read(
    socket,
    boost::asio::buffer(consumptionPayload)
);

std::vector<uint8_t> fullConsumptionMessage;

fullConsumptionMessage.insert(
    fullConsumptionMessage.end(),
    consumptionHeader.begin(),
    consumptionHeader.end()
);

fullConsumptionMessage.insert(
    fullConsumptionMessage.end(),
    consumptionPayload.begin(),
    consumptionPayload.end()
);

if (consumptionType ==
    static_cast<uint8_t>(MessageType::CONSUMPTION_REPORT))
{
    ConsumptionReport report =
        deserializeConsumptionReport(fullConsumptionMessage);

    std::cout << "Primljen CONSUMPTION_REPORT" << std::endl;

    std::cout << "URI: "
              << report.device_uri
              << std::endl;

    std::cout << "Timestamp: "
              << report.timestamp
              << std::endl;

    std::cout << "Potrosnja: "
              << report.consumption_kwh
              << " kWh"
              << std::endl;

    std::cout << "Trenutna snaga: "
              << report.current_power_kw
              << " kW"
              << std::endl;

ConsumptionAck consumptionAck{};
consumptionAck.status = 1;

std::vector<uint8_t> serializedConsumptionAck =
    serializeConsumptionAck(consumptionAck);

boost::asio::write(
    socket,
    boost::asio::buffer(serializedConsumptionAck)
);

std::cout << "CONSUMPTION_ACK poslan." << std::endl;
}
else
{
    std::cout
        << "Ocekivan CONSUMPTION_REPORT, ali primljen drugi tip poruke."
        << std::endl;
}
}
        }
        else
        {
            std::cout << "Nepoznat tip poruke!" << std::endl;
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Greska: "
                  << e.what()
                  << std::endl;
    }

    return 0;
}
