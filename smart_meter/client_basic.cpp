#include <boost/asio.hpp>
#include <iostream>
#include <cstring>
#include <vector>
#include <arpa/inet.h>
#include <ctime>
#include "../protocol/smart_grid_protocol.hpp"
#include <thread>
#include <chrono>
using boost::asio::ip::tcp;

int main()
{
    try
    {
        boost::asio::io_context io;
        tcp::socket socket(io);

        tcp::resolver resolver(io);
        auto endpoints = resolver.resolve("127.0.0.1", "5000");

        boost::asio::connect(socket, endpoints);

        std::cout << "Povezan sa serverom!" << std::endl;

        RegisterRequest request{};

        std::strncpy(
            request.device_uri,
            "smartgrid://sarajevo/meter/001",
            sizeof(request.device_uri) - 1
        );

        request.region_id = 1;
        request.user_type = 1;

        // Serijalizacija REGISTER_REQ poruke
        std::vector<uint8_t> serialized =
            serializeRegisterRequest(request);

        boost::asio::write(
            socket,
            boost::asio::buffer(serialized)
        );

        std::cout << "Serijalizovani REGISTER_REQ poslan." << std::endl;

// Citamo REGISTER_ACK header
std::vector<uint8_t> ackHeader(4);

boost::asio::read(
    socket,
    boost::asio::buffer(ackHeader)
);

uint8_t ackType = ackHeader[1];

uint16_t ackPayloadLengthNetwork;

std::memcpy(
    &ackPayloadLengthNetwork,
    ackHeader.data() + 2,
    sizeof(ackPayloadLengthNetwork)
);

uint16_t ackPayloadLength =
    ntohs(ackPayloadLengthNetwork);

// Citamo ACK payload
std::vector<uint8_t> ackPayload(ackPayloadLength);

boost::asio::read(
    socket,
    boost::asio::buffer(ackPayload)
);

// Spajamo header i payload
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

if (ackType ==
    static_cast<uint8_t>(MessageType::REGISTER_ACK))
{
    RegisterAck ack =
        deserializeRegisterAck(fullAck);

if (ack.status == 1)
{
    std::cout
        << "Smart meter je uspjesno registrovan!"
        << std::endl;

    for (int i = 0; i < 5; i++)
    {
        ConsumptionReport report{};

        std::strncpy(
            report.device_uri,
            "smartgrid://sarajevo/meter/001",
            sizeof(report.device_uri) - 1
        );

        report.timestamp =
            static_cast<uint64_t>(std::time(nullptr));

        report.consumption_kwh =
            2.35 + i * 0.10;

        report.current_power_kw =
            1.20 + i * 0.05;

        std::vector<uint8_t> serializedReport =
            serializeConsumptionReport(report);

        boost::asio::write(
            socket,
            boost::asio::buffer(serializedReport)
        );

        std::cout
            << "CONSUMPTION_REPORT poslan: "
            << report.consumption_kwh
            << " kWh"
            << std::endl;

        std::vector<uint8_t> consumptionAckHeader(4);

        boost::asio::read(
            socket,
            boost::asio::buffer(consumptionAckHeader)
        );

        uint16_t ackPayloadLengthNetwork;

        std::memcpy(
            &ackPayloadLengthNetwork,
            consumptionAckHeader.data() + 2,
            sizeof(ackPayloadLengthNetwork)
        );

        uint16_t ackPayloadLength =
            ntohs(ackPayloadLengthNetwork);

        std::vector<uint8_t> ackPayload(
            ackPayloadLength
        );

        boost::asio::read(
            socket,
            boost::asio::buffer(ackPayload)
        );

        std::vector<uint8_t> fullAck;

        fullAck.insert(
            fullAck.end(),
            consumptionAckHeader.begin(),
            consumptionAckHeader.end()
        );

        fullAck.insert(
            fullAck.end(),
            ackPayload.begin(),
            ackPayload.end()
        );

        if (consumptionAckHeader[1] ==
            static_cast<uint8_t>(
                MessageType::CONSUMPTION_ACK))
        {
            ConsumptionAck ack =
                deserializeConsumptionAck(fullAck);

            if (ack.status == 1)
            {
                std::cout
                    << "CONSUMPTION_ACK primljen."
                    << std::endl;
            }
        }

        std::this_thread::sleep_for(
            std::chrono::seconds(5)
        );
    }
}
}
	else	
	{
	
        std::cout
            << "Registracija nije uspjela."
            << std::endl;
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
