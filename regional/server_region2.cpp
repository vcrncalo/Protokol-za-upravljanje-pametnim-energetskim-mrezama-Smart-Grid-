#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <cstring>
#include <arpa/inet.h>

#include "../protocol/smart_grid_protocol.hpp"
#include "device_registry.hpp"
#include "../database/database.hpp"

using boost::asio::ip::tcp;
std::shared_ptr<tcp::socket> centralSocket;
DeviceRegistry deviceRegistry; //globalni registar
Database database("database/region2.db");
void acceptClient(
    tcp::acceptor& acceptor,
    boost::asio::io_context& io);

void handleClient(
    std::shared_ptr<tcp::socket> socket);



void readConsumptionReport(
    std::shared_ptr<tcp::socket> socket,
    int reportCount = 0);
void readConsumptionReport(
    std::shared_ptr<tcp::socket> socket,
    int reportCount)

{
    auto consumptionHeader =
        std::make_shared<std::vector<uint8_t>>(4);

    boost::asio::async_read(
        *socket,
        boost::asio::buffer(*consumptionHeader),

       [socket, consumptionHeader, reportCount]
        (
            const boost::system::error_code& headerError,
            std::size_t headerBytes
        )
        {
        if (headerError)
        {
            if (headerError == boost::asio::error::eof)
    {
        std::cout
            << "Smart Meter je prekinuo vezu."
            << std::endl;
    }
    else
    {
        std::cout
            << "Greska pri citanju CONSUMPTION headera: "
            << headerError.message()
            << std::endl;
            }

                return;
            }

            std::cout
                << "Primljen CONSUMPTION header."
                << std::endl;

            std::cout
                << "Primljeno header bajtova: "
                << headerBytes
                << std::endl;

            uint8_t consumptionType =
                (*consumptionHeader)[1];

            uint16_t payloadLengthNetwork;

            std::memcpy(
                &payloadLengthNetwork,
                consumptionHeader->data() + 2,
                sizeof(payloadLengthNetwork)
            );

            uint16_t payloadLength =
                ntohs(payloadLengthNetwork);

            std::cout
                << "Payload length: "
                << payloadLength
                << std::endl;


            // Kreiramo buffer za payload
            auto consumptionPayload =
                std::make_shared<std::vector<uint8_t>>(
                    payloadLength
                );


            // Asinhrono citamo payload
            boost::asio::async_read(
                *socket,
                boost::asio::buffer(*consumptionPayload),

                [socket,
 consumptionHeader,
 consumptionPayload,
 consumptionType,
 reportCount]
                (
                    const boost::system::error_code& payloadError,
                    std::size_t payloadBytes
                )
                {
                    if (payloadError)
                    {
                        std::cout
                            << "Greska pri citanju CONSUMPTION payload-a: "
                            << payloadError.message()
                            << std::endl;

                        return;
                    }

                    std::cout
                        << "Primljen CONSUMPTION payload."
                        << std::endl;

                    std::cout
                        << "Primljeno payload bajtova: "
                        << payloadBytes
                        << std::endl;


                    // Spajamo header i payload
                    std::vector<uint8_t> fullConsumptionMessage;

                    fullConsumptionMessage.insert(
                        fullConsumptionMessage.end(),
                        consumptionHeader->begin(),
                        consumptionHeader->end()
                    );

                    fullConsumptionMessage.insert(
                        fullConsumptionMessage.end(),
                        consumptionPayload->begin(),
                        consumptionPayload->end()
                    );


                    // Provjeravamo tip poruke
                    if (consumptionType ==
                        static_cast<uint8_t>(
                            MessageType::CONSUMPTION_REPORT))
                    {
                        ConsumptionReport report =
                            deserializeConsumptionReport(
                                fullConsumptionMessage
                            );

                        std::cout
                            << "Primljen CONSUMPTION_REPORT asinhrono."
                            << std::endl;

                        std::cout
                            << "URI: "
                            << report.device_uri
                            << std::endl;

                        std::cout
                            << "Timestamp: "
                            << report.timestamp
                            << std::endl;

                        std::cout
                            << "Potrosnja: "
                            << report.consumption_kwh
                            << " kWh"
                            << std::endl;

                        std::cout
                            << "Trenutna snaga: "
                            << report.current_power_kw
                            << " kW"
                            << std::endl;
database.insertConsumption(
    report.device_uri,
    report.timestamp,
    report.consumption_kwh,
    report.current_power_kw
);
// Slanje mjerenja centralnom serveru u realnom vremenu
if (centralSocket && centralSocket->is_open())
{
    RegionSync sync{};

    sync.source_region = 2;

    std::strncpy(
        sync.device_uri,
        report.device_uri,
        sizeof(sync.device_uri) - 1
    );

    sync.timestamp = report.timestamp;
    sync.consumption_kwh = report.consumption_kwh;
    sync.current_power_kw = report.current_power_kw;

    std::vector<uint8_t> syncData =
        serializeRegionSync(sync);

    boost::asio::write(
        *centralSocket,
        boost::asio::buffer(syncData)
    );
    std::vector<uint8_t> ackBuffer(5);

boost::asio::read(
    *centralSocket,
    boost::asio::buffer(ackBuffer)
);

uint8_t ackVersion = ackBuffer[0];
uint8_t ackType = ackBuffer[1];
uint8_t ackStatus = ackBuffer[4];

if (ackVersion == 1 &&
    ackType ==
        static_cast<uint8_t>(
            MessageType::REGION_SYNC_ACK
        ) &&
    ackStatus == 1)
{
    std::cout
        << "Centralni server potvrdio REGION_SYNC."
        << std::endl;
}
else
{
    std::cout
        << "Neispravan REGION_SYNC_ACK."
        << std::endl;
}

    std::cout
        << "Mjerenje proslijedjeno centralnom serveru."
        << std::endl;
}

                        // Kreiramo CONSUMPTION_ACK
                        ConsumptionAck consumptionAck{};
                        consumptionAck.status = 1;

                        auto serializedConsumptionAck =
                            std::make_shared<std::vector<uint8_t>>(
                                serializeConsumptionAck(
                                    consumptionAck
                                )
                            );


                        // Asinhrono saljemo ACK
                        boost::asio::async_write(
                            *socket,
                            boost::asio::buffer(
                                *serializedConsumptionAck
                            ),

                         [socket, serializedConsumptionAck, reportCount, report]
                            (
                                const boost::system::error_code& writeError,
                                std::size_t bytesTransferred
                            )
                            {
                                if (writeError)
                                {
                                    std::cout
                                        << "Greska pri slanju CONSUMPTION_ACK: "
                                        << writeError.message()
                                        << std::endl;

                                    return;
                                }

                                std::cout
                                    << "CONSUMPTION_ACK asinhrono poslan."
                                    << std::endl;

                                std::cout
                                    << "Poslano bajtova: "
                                    << bytesTransferred
                                    << std::endl;

                                std::cout
                                    << "--------------------------------"
                                    << std::endl;


                                // Nakon ACK-a ponovo cekamo novi report
                                if (reportCount + 1 < 5)
{
    readConsumptionReport(socket, reportCount + 1);
}
else
{
    std::cout
        << "Primljeno je 5 CONSUMPTION_REPORT poruka."
        << std::endl;

    std::cout
        << "Server je spreman da posalje REDUCE_CONSUMPTION_CMD."
        << std::endl;
        ReduceConsumptionCommand command{};

std::strncpy(
    command.device_uri,
    report.device_uri,
    sizeof(command.device_uri) - 1
);

command.device_uri[
    sizeof(command.device_uri) - 1
] = '\0';

command.target_power_kw = 1.5;
auto serializedCommand =
    std::make_shared<std::vector<uint8_t>>(
        serializeReduceConsumptionCommand(command)
    );
    boost::asio::async_write(
    *socket,
    boost::asio::buffer(*serializedCommand),

    [socket, serializedCommand, command]
    (
        const boost::system::error_code& writeError,
        std::size_t bytesTransferred
    )
    {
        if (writeError)
        {
            std::cout
                << "Greska pri slanju REDUCE_CONSUMPTION_CMD: "
                << writeError.message()
                << std::endl;

            return;
        }

        std::cout
            << "REDUCE_CONSUMPTION_CMD poslan."
            << std::endl;

        std::cout
            << "Poslano bajtova: "
            << bytesTransferred
            << std::endl;
            auto commandAckHeader =
    std::make_shared<std::vector<uint8_t>>(4);

boost::asio::async_read(
    *socket,
    boost::asio::buffer(*commandAckHeader),
    [socket, commandAckHeader, command]
    (
        const boost::system::error_code& readError,
        std::size_t
    )
    {
        if (readError)
        {
            std::cout
                << "Greska pri prijemu COMMAND_ACK headera: "
                << readError.message()
                << std::endl;

            return;
        }

        uint16_t payloadLengthNetwork;

        std::memcpy(
            &payloadLengthNetwork,
            commandAckHeader->data() + 2,
            sizeof(payloadLengthNetwork)
        );

        uint16_t payloadLength =
            ntohs(payloadLengthNetwork);

        auto commandAckPayload =
            std::make_shared<std::vector<uint8_t>>(
                payloadLength
            );

        boost::asio::async_read(
            *socket,
            boost::asio::buffer(*commandAckPayload),
            [socket, commandAckHeader, commandAckPayload, command]
            (
                const boost::system::error_code& payloadError,
                std::size_t
            )
            {
                if (payloadError)
                {
                    std::cout
                        << "Greska pri prijemu COMMAND_ACK payloada: "
                        << payloadError.message()
                        << std::endl;

                    return;
                }

                std::vector<uint8_t> fullCommandAck;

                fullCommandAck.insert(
                    fullCommandAck.end(),
                    commandAckHeader->begin(),
                    commandAckHeader->end()
                );

                fullCommandAck.insert(
                    fullCommandAck.end(),
                    commandAckPayload->begin(),
                    commandAckPayload->end()
                );

                if ((*commandAckHeader)[1] ==
                    static_cast<uint8_t>(
                        MessageType::COMMAND_ACK))
                {
                    CommandAck ack =
                        deserializeCommandAck(
                            fullCommandAck
                        );

                   if (ack.status == 1)
{
    std::cout
        << "COMMAND_ACK primljen od Smart Metera."
        << std::endl;

    std::cout
        << "Komanda je uspjesno prihvacena."
        << std::endl;
        database.insertCommand(
    command.device_uri,
    "REDUCE_CONSUMPTION",
    command.target_power_kw,
    "ACCEPTED"
);


    // ==========================================
    // TARIFF_UPDATE
    // ==========================================

    TariffUpdate tariff{};

    tariff.price_per_kwh = 0.25;
    
    database.insertTariff(
    tariff.price_per_kwh
);

    auto serializedTariff =
        std::make_shared<std::vector<uint8_t>>(
            serializeTariffUpdate(tariff)
        );

    boost::asio::async_write(
        *socket,
        boost::asio::buffer(*serializedTariff),
        [socket, serializedTariff]
        (
            const boost::system::error_code& tariffError,
            std::size_t bytesTransferred
        )
        {
            if (tariffError)
            {
                std::cout
                    << "Greska pri slanju TARIFF_UPDATE: "
                    << tariffError.message()
                    << std::endl;

                return;
            }

            std::cout
                << "TARIFF_UPDATE poslan Smart Meteru."
                << std::endl;

            std::cout
                << "Nova cijena: 0.25 KM/kWh"
                << std::endl;

            std::cout
                << "Poslano bajtova: "
                << bytesTransferred
                << std::endl;
        }
    );
}
                    else
                    {
                        std::cout
                            << "Smart Meter nije prihvatio komandu."
                            << std::endl;
                    }
                }
                else
                {
                    std::cout
                        << "Primljena poruka nije COMMAND_ACK."
                        << std::endl;
                }
            }
        );
    }
);
    }
);
}
                            }
                        );
                    }
                    else
                    {
                        std::cout
                            << "Ocekivan CONSUMPTION_REPORT, "
                            << "ali je primljen drugi tip poruke."
                            << std::endl;
                    }
                }
            );
        }
    );
}

void handleClient(
    std::shared_ptr<tcp::socket> socket)
{
    auto headerBuffer =
        std::make_shared<std::vector<uint8_t>>(4);

    boost::asio::async_read(
        *socket,
        boost::asio::buffer(*headerBuffer),

        [socket, headerBuffer]
        (
            const boost::system::error_code& readError,
            std::size_t bytesTransferred
        )
        {
            if (readError)
            {
                std::cout
                    << "Greska pri citanju REGISTER headera: "
                    << readError.message()
                    << std::endl;

                return;
            }

            uint8_t messageType =
                (*headerBuffer)[1];

            uint16_t payloadLengthNetwork;

            std::memcpy(
                &payloadLengthNetwork,
                headerBuffer->data() + 2,
                sizeof(payloadLengthNetwork)
            );

            uint16_t payloadLength =
                ntohs(payloadLengthNetwork);

            auto payloadBuffer =
                std::make_shared<std::vector<uint8_t>>(
                    payloadLength
                );

            boost::asio::async_read(
                *socket,
                boost::asio::buffer(*payloadBuffer),

                [socket,
                 headerBuffer,
                 payloadBuffer,
                 messageType]
                (
                    const boost::system::error_code& payloadError,
                    std::size_t payloadBytes
                )
                {
                    if (payloadError)
                    {
                        std::cout
                            << "Greska pri citanju REGISTER payload-a: "
                            << payloadError.message()
                            << std::endl;

                        return;
                    }

                    std::vector<uint8_t> fullMessage;

                    fullMessage.insert(
                        fullMessage.end(),
                        headerBuffer->begin(),
                        headerBuffer->end()
                    );

                    fullMessage.insert(
                        fullMessage.end(),
                        payloadBuffer->begin(),
                        payloadBuffer->end()
                    );

                    if (messageType ==
                        static_cast<uint8_t>(
                            MessageType::REGISTER_REQ))
                    {
                       RegisterRequest request =
    deserializeRegisterRequest(
        fullMessage
    );

std::cout
    << "Primljen zahtjev za registraciju:"
    << std::endl;

std::cout
    << "URI: "
    << request.device_uri
    << std::endl;

std::cout
    << "Region ID: "
    << request.region_id
    << std::endl;


// Dodajemo Smart Meter u registry
bool registered =
    deviceRegistry.registerDevice(
        request.device_uri,
        request.region_id,
        request.user_type
    );


// Ispisujemo sve trenutno registrovane Smart Metere
deviceRegistry.printDevices();
if (registered)
{
    database.insertDevice(
        request.device_uri,
        request.region_id,
        request.user_type
    );
}


// Kreiramo REGISTER_ACK
RegisterAck ack{};

if (registered)
{
    ack.status = 1;
}
else
{
    ack.status = 0;
}

                        auto serializedAck =
                            std::make_shared<
                                std::vector<uint8_t>
                            >(
                                serializeRegisterAck(ack)
                            );

                        boost::asio::async_write(
                            *socket,
                            boost::asio::buffer(
                                *serializedAck
                            ),

                            [socket, serializedAck]
                            (
                                const boost::system::error_code&
                                    writeError,
                                std::size_t bytesTransferred
                            )
                            {
                                if (writeError)
                                {
                                    std::cout
                                        << "Greska pri slanju REGISTER_ACK: "
                                        << writeError.message()
                                        << std::endl;

                                    return;
                                }

                                std::cout
                                    << "REGISTER_ACK poslan."
                                    << std::endl;

                                readConsumptionReport(socket);
                            }
                        );
                    }
                }
            );
        }
    );
}


void acceptClient(
    tcp::acceptor& acceptor,
    boost::asio::io_context& io)
{
    auto socket =
        std::make_shared<tcp::socket>(io);

    acceptor.async_accept(
        *socket,
        [&acceptor, &io, socket]
        (const boost::system::error_code& error)
        {
            if (error)
            {
                std::cout
                    << "Greska pri povezivanju klijenta: "
                    << error.message()
                    << std::endl;

                return;
            }

            std::cout
                << "Novi Smart Meter se povezao!"
                << std::endl;

            
	    handleClient(socket);
            acceptClient(acceptor, io);
        }
    );
}
void connectToCentralServer()
{
    try
    {
        static boost::asio::io_context centralIoContext;

        tcp::resolver resolver(centralIoContext);
        auto endpoints = resolver.resolve("127.0.0.1", "6000");

        centralSocket = std::make_shared<tcp::socket>(centralIoContext);

        boost::asio::connect(*centralSocket, endpoints);

        std::cout
            << "Regionalni server 2 povezan sa centralnim serverom."
            << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "Greska pri povezivanju sa centralnim serverom: "
            << e.what()
            << std::endl;
    }
}

int main()
{
connectToCentralServer();
if (!database.initialize())
{
    std::cerr
        << "Baza podataka nije uspjesno inicijalizovana."
        << std::endl;

    return 1;
}
    try
    {
        boost::asio::io_context io;

        tcp::acceptor acceptor(
            io,
            tcp::endpoint(
                tcp::v4(),
                5003
            )
        );

        std::cout
            << "Asinhroni server slusa na portu 5003..."
            << std::endl;

	 acceptClient(acceptor, io);
	 

        io.run();
      
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
