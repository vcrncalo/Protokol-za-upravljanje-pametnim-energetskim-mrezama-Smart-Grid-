#include <boost/asio.hpp>
#include <iostream>
#include <cstring>
#include <vector>
#include <arpa/inet.h>
#include <ctime>
#include "../protocol/smart_grid_protocol.hpp"
#include <thread>
#include <chrono>
#include <string>
#include <random>

using boost::asio::ip::tcp;

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout
            << "Koristenje: ./client_async_test <grad> <meter_id>"
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
        tcp::socket socket(io);

        tcp::resolver resolver(io);

        auto endpoints =
            resolver.resolve(
                "127.0.0.1",
                "5001"
            );

        boost::asio::connect(
            socket,
            endpoints
        );

        std::cout
            << "Povezan sa serverom!"
            << std::endl;


        // ==========================================
        // REGISTER_REQ
        // ==========================================

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


        std::vector<uint8_t> serialized =
            serializeRegisterRequest(request);


        boost::asio::write(
            socket,
            boost::asio::buffer(serialized)
        );


        std::cout
            << "Serijalizovani REGISTER_REQ poslan."
            << std::endl;

        std::cout
            << "URI registracije: "
            << request.device_uri
            << std::endl;


        // ==========================================
        // REGISTER_ACK HEADER
        // ==========================================

        std::vector<uint8_t> ackHeader(4);

        boost::asio::read(
            socket,
            boost::asio::buffer(ackHeader)
        );


        uint8_t ackType =
            ackHeader[1];


        uint16_t ackPayloadLengthNetwork;

        std::memcpy(
            &ackPayloadLengthNetwork,
            ackHeader.data() + 2,
            sizeof(ackPayloadLengthNetwork)
        );


        uint16_t ackPayloadLength =
            ntohs(ackPayloadLengthNetwork);


        // ==========================================
        // REGISTER_ACK PAYLOAD
        // ==========================================

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
            ackHeader.begin(),
            ackHeader.end()
        );

        fullAck.insert(
            fullAck.end(),
            ackPayload.begin(),
            ackPayload.end()
        );


        // ==========================================
        // PROVJERA REGISTER_ACK
        // ==========================================

        if (ackType ==
            static_cast<uint8_t>(
                MessageType::REGISTER_ACK))
        {
            RegisterAck ack =
                deserializeRegisterAck(fullAck);


            if (ack.status == 1)
            {
                std::cout
                    << "Smart meter je uspjesno registrovan!"
                    << std::endl;


                std::random_device rd;
                std::mt19937 generator(rd());

                std::uniform_real_distribution<double>
                    consumptionDist(1.5, 5.0);

                std::uniform_real_distribution<double>
                    powerDist(0.5, 3.0);


                // ==========================================
                // SALJEMO 5 CONSUMPTION_REPORT PORUKA
                // ==========================================

                for (int i = 0; i < 5; i++)
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


                    report.consumption_kwh =
                        consumptionDist(generator);

                    report.current_power_kw =
                        powerDist(generator);


                    std::cout
                        << "URI koji se salje: "
                        << report.device_uri
                        << std::endl;


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


                    std::cout
                        << "CONSUMPTION_REPORT poslan: "
                        << report.consumption_kwh
                        << " kWh"
                        << std::endl;


                    // ==========================================
                    // CONSUMPTION_ACK HEADER
                    // ==========================================

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


                    // ==========================================
                    // CONSUMPTION_ACK PAYLOAD
                    // ==========================================

                    std::vector<uint8_t>
                        consumptionAckPayload(
                            consumptionAckPayloadLength
                        );


                    boost::asio::read(
                        socket,
                        boost::asio::buffer(
                            consumptionAckPayload
                        )
                    );


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


                    // ==========================================
                    // PROVJERA CONSUMPTION_ACK
                    // ==========================================

                    if (consumptionAckHeader[1] ==
                        static_cast<uint8_t>(
                            MessageType::CONSUMPTION_ACK))
                    {
                        ConsumptionAck consumptionAck =
                            deserializeConsumptionAck(
                                fullConsumptionAck
                            );


                        if (consumptionAck.status == 1)
                        {
                            std::cout
                                << "CONSUMPTION_ACK primljen."
                                << std::endl;
                        }
                        else
                        {
                            std::cout
                                << "CONSUMPTION_ACK nije uspjesan."
                                << std::endl;
                        }
                    }


                    std::this_thread::sleep_for(
                        std::chrono::seconds(5)
                    );
                }


                // ==========================================
                // CEKANJE KOMANDE OD SERVERA
                // ==========================================

                std::cout
                    << "Cekam komandu od regionalnog servera..."
                    << std::endl;


                std::vector<uint8_t> commandHeader(4);


                boost::asio::read(
                    socket,
                    boost::asio::buffer(commandHeader)
                );


                uint16_t commandPayloadLengthNetwork;


                std::memcpy(
                    &commandPayloadLengthNetwork,
                    commandHeader.data() + 2,
                    sizeof(commandPayloadLengthNetwork)
                );


                uint16_t commandPayloadLength =
                    ntohs(commandPayloadLengthNetwork);


                std::vector<uint8_t> commandPayload(
                    commandPayloadLength
                );


                boost::asio::read(
                    socket,
                    boost::asio::buffer(commandPayload)
                );


                std::vector<uint8_t> fullCommand;


                fullCommand.insert(
                    fullCommand.end(),
                    commandHeader.begin(),
                    commandHeader.end()
                );


                fullCommand.insert(
                    fullCommand.end(),
                    commandPayload.begin(),
                    commandPayload.end()
                );


                // ==========================================
                // PROVJERA REDUCE_CONSUMPTION_CMD
                // ==========================================

                if (commandHeader[1] ==
                    static_cast<uint8_t>(
                        MessageType::REDUCE_CONSUMPTION_CMD))
                {
                    ReduceConsumptionCommand command =
                        deserializeReduceConsumptionCommand(
                            fullCommand
                        );


                    std::cout
                        << "Primljena REDUCE_CONSUMPTION_CMD poruka."
                        << std::endl;

                    std::cout
                        << "URI: "
                        << command.device_uri
                        << std::endl;

                    std::cout
                        << "Nova maksimalna snaga: "
                        << command.target_power_kw
                        << " kW"
                        << std::endl;


                    // ==========================================
                    // COMMAND_ACK
                    // ==========================================

                    CommandAck commandAck{};
                    commandAck.status = 1;


                    std::vector<uint8_t> serializedCommandAck =
                        serializeCommandAck(
                            commandAck
                        );


                    boost::asio::write(
                        socket,
                        boost::asio::buffer(
                            serializedCommandAck
                        )
                    );


                    std::cout
                        << "COMMAND_ACK poslan serveru."
                        << std::endl;
                        // ==========================================
// CEKANJE TARIFF_UPDATE PORUKE
// ==========================================

std::cout
    << "Cekam TARIFF_UPDATE od servera..."
    << std::endl;

std::vector<uint8_t> tariffHeader(4);

boost::asio::read(
    socket,
    boost::asio::buffer(tariffHeader)
);

uint16_t tariffPayloadLengthNetwork;

std::memcpy(
    &tariffPayloadLengthNetwork,
    tariffHeader.data() + 2,
    sizeof(tariffPayloadLengthNetwork)
);

uint16_t tariffPayloadLength =
    ntohs(tariffPayloadLengthNetwork);

std::vector<uint8_t> tariffPayload(
    tariffPayloadLength
);

boost::asio::read(
    socket,
    boost::asio::buffer(tariffPayload)
);

std::vector<uint8_t> fullTariff;

fullTariff.insert(
    fullTariff.end(),
    tariffHeader.begin(),
    tariffHeader.end()
);

fullTariff.insert(
    fullTariff.end(),
    tariffPayload.begin(),
    tariffPayload.end()
);

if (tariffHeader[1] ==
    static_cast<uint8_t>(
        MessageType::TARIFF_UPDATE))
{
    TariffUpdate tariff =
        deserializeTariffUpdate(
            fullTariff
        );

    std::cout
        << "TARIFF_UPDATE primljen."
        << std::endl;

    std::cout
        << "Nova cijena elektricne energije: "
        << tariff.price_per_kwh
        << " KM/kWh"
        << std::endl;
}
else
{
    std::cout
        << "Primljena poruka nije TARIFF_UPDATE."
        << std::endl;
}
                }
                else
                {
                    std::cout
                        << "Primljena poruka nije REDUCE_CONSUMPTION_CMD."
                        << std::endl;
                }
            }
            else
            {
                std::cout
                    << "Registracija nije uspjela."
                    << std::endl;
            }
        }
        else
        {
            std::cout
                << "Primljena poruka nije REGISTER_ACK."
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
