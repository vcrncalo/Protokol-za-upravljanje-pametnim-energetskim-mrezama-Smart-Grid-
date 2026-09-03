#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <ctime>
#include <mutex>

#include "../protocol/smart_grid_protocol.hpp"

using boost::asio::ip::udp;

int main()
{
    try
    {
        boost::asio::io_context io;

        // UDP server slusa na portu 5002
        udp::socket socket(
            io,
            udp::endpoint(
                udp::v4(),
                5002
            )
        );

        std::cout
            << "UDP Heartbeat server slusa na portu 5002..."
            << std::endl;

        std::unordered_map<std::string, uint64_t> lastHeartbeat;

        std::unordered_set<std::string> activeAlarms;

        std::mutex heartbeatMutex;


        // ==========================================
        // THREAD ZA ONLINE/OFFLINE PROVJERU
        // ==========================================

        std::thread statusThread([&]()
        {
            while (true)
            {
                std::this_thread::sleep_for(
                    std::chrono::seconds(2)
                );

                uint64_t currentTime =
                    static_cast<uint64_t>(
                        std::time(nullptr)
                    );

                std::lock_guard<std::mutex> lock(
                    heartbeatMutex
                );

                std::cout
                    << "\n--- STATUS SMART METERA ---"
                    << std::endl;

                for (const auto& pair : lastHeartbeat)
                {
                    uint64_t difference =
                        currentTime - pair.second;

                    std::cout
                        << pair.first
                        << " -> ";

                    // Ako je heartbeat stigao
                    // u posljednjih 5 sekundi
                    if (difference <= 5)
                    {
                        std::cout
                            << "ONLINE"
                            << std::endl;

                        // Ako je ranije postojao alarm,
                        // sada ga uklanjamo
                        if (activeAlarms.find(pair.first)
                            != activeAlarms.end())
                        {
                            std::cout
                                << "ALARM UKLONJEN: "
                                << pair.first
                                << " je ponovo ONLINE."
                                << std::endl;

                            activeAlarms.erase(
                                pair.first
                            );
                        }
                    }
                    else
                    {
                        std::cout
                            << "OFFLINE"
                            << std::endl;

                        // Alarm kreiramo samo jednom
                        if (activeAlarms.find(pair.first)
                            == activeAlarms.end())
                        {
                            std::cout
                                << "*** ALARM ***"
                                << std::endl;

                            std::cout
                                << "Smart Meter nije dostupan: "
                                << pair.first
                                << std::endl;

                            std::cout
                                << "Nije primljen HEARTBEAT "
                                << "vise od 5 sekundi."
                                << std::endl;

                            activeAlarms.insert(
                                pair.first
                            );
                        }
                    }
                }

                std::cout
                    << "--------------------------"
                    << std::endl;
            }
        });


        statusThread.detach();


        // ==========================================
        // PRIMANJE HEARTBEAT PORUKA
        // ==========================================

        while (true)
        {
            std::vector<uint8_t> buffer(1024);

            udp::endpoint senderEndpoint;

            std::size_t receivedBytes =
                socket.receive_from(
                    boost::asio::buffer(buffer),
                    senderEndpoint
                );

            buffer.resize(receivedBytes);


            Heartbeat heartbeat =
                deserializeHeartbeat(buffer);


            std::string uri =
                heartbeat.device_uri;


            {
                std::lock_guard<std::mutex> lock(
                    heartbeatMutex
                );

                lastHeartbeat[uri] =
                    heartbeat.timestamp;
            }


            std::cout
                << "Primljen HEARTBEAT."
                << std::endl;

            std::cout
                << "URI: "
                << heartbeat.device_uri
                << std::endl;

            std::cout
                << "Timestamp: "
                << heartbeat.timestamp
                << std::endl;

            std::cout
                << "-----------------------------"
                << std::endl;
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
