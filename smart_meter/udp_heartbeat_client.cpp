#include <boost/asio.hpp>
#include <iostream>
#include <cstring>
#include <ctime>
#include <vector>
#include <thread>
#include <chrono>

#include "../protocol/smart_grid_protocol.hpp"

using boost::asio::ip::udp;

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout
            << "Koristenje: ./udp_heartbeat_client <grad> <meter_id>"
            << std::endl;

        return 1;
    }

    std::string city = argv[1];
    std::string meterId = argv[2];

    std::string deviceUri =
        "smartgrid://" + city + "/meter/" + meterId;

    try
    {
        boost::asio::io_context io;

        udp::socket socket(io);
        socket.open(udp::v4());

        udp::endpoint serverEndpoint(
            boost::asio::ip::make_address("127.0.0.1"),
            5002
        );

        for (int i = 0; i < 5; i++)
        {
            Heartbeat heartbeat{};

            std::strncpy(
                heartbeat.device_uri,
                deviceUri.c_str(),
                sizeof(heartbeat.device_uri) - 1
            );

            heartbeat.device_uri[
                sizeof(heartbeat.device_uri) - 1
            ] = '\0';

            heartbeat.timestamp =
                static_cast<uint64_t>(
                    std::time(nullptr)
                );

            std::vector<uint8_t> serializedHeartbeat =
                serializeHeartbeat(heartbeat);

            socket.send_to(
                boost::asio::buffer(serializedHeartbeat),
                serverEndpoint
            );

            std::cout
                << "HEARTBEAT poslan."
                << std::endl;

            std::cout
                << "URI: "
                << heartbeat.device_uri
                << std::endl;

            std::this_thread::sleep_for(
                std::chrono::seconds(5)
            );
        }
    }
    catch (std::exception& e)
    {
        std::cerr
            << "Greska: "
            << e.what()
            << std::endl;
    }

    return 0;
}
