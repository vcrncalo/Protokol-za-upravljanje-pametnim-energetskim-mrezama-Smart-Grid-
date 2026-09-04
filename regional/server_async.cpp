#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <ctime>
#include <iomanip>
#include <cmath>
#include <unordered_map>
#include <mutex>

#include "../protocol/smart_grid_protocol.hpp"
#include "device_registry.hpp"
#include "../database/database.hpp"

using boost::asio::ip::tcp;
std::shared_ptr<tcp::socket> centralSocket; //trajna konekcija
DeviceRegistry deviceRegistry; //globalni registar
Database database("database/region1.db");
std::unordered_map<std::string, double> latestPowerByDevice;
std::mutex powerMutex;
double updateAndGetTotalNetworkLoad(
    const std::string& deviceUri,
    double currentPowerKw)
{
    std::lock_guard<std::mutex> lock(powerMutex);

    // Azuriramo posljednju poznatu snagu ovog Smart Metera
    latestPowerByDevice[deviceUri] = currentPowerKw;

    double totalLoadKw = 0.0;

    // Sabiramo posljednje vrijednosti svih Smart Metera
    for (const auto& device : latestPowerByDevice)
    {
        totalLoadKw += device.second;
    }

    std::cout
        << "Trenutno ukupno opterecenje regije: "
        << std::fixed << std::setprecision(2)
        << totalLoadKw
        << " kW"
        << std::endl;

    return totalLoadKw;
}
double calculateDynamicTariff(
    uint8_t userType,
    double networkLoadKw,
    bool reductionAccepted)
{
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    int hour = localTime->tm_hour;
    int day = localTime->tm_wday; // 0 = nedjelja
    bool daylightSaving = localTime->tm_isdst > 0;

    bool lowerTariff = false;

    // DOMACINSTVO
    if (userType == 1)
    {
        // Nedjeljom cijeli dan niza tarifa
        if (day == 0)
        {
            lowerTariff = true;
        }
        else if (day >= 1 && day <= 6)
        {
            if (daylightSaving)
            {
                // Ljetno racunanje vremena:
                // 14-17 i 23-08
                lowerTariff =
                    (hour >= 14 && hour < 17) ||
                    (hour >= 23 || hour < 8);
            }
            else
            {
                // Zimsko racunanje vremena:
                // 13-16 i 22-07
                lowerTariff =
                    (hour >= 13 && hour < 16) ||
                    (hour >= 22 || hour < 7);
            }
        }
    }

    // INDUSTRIJA
    else if (userType == 2)
    {
        // Vikendom cijeli dan niza tarifa
        if (day == 0 || day == 6)
        {
            lowerTariff = true;
        }
        else
        {
            if (daylightSaving)
            {
                lowerTariff =
                    (hour >= 23 || hour < 8);
            }
            else
            {
                lowerTariff =
                    (hour >= 22 || hour < 7);
            }
        }
    }

    double price = 0.0;

    if (userType == 1)
    {
        // Domacinstvo
        price = lowerTariff ? 0.10 : 0.20;
    }
    else
    {
        // Industrija
        price = lowerTariff ? 0.14 : 0.24;
    }

    // Dinamicka korekcija prema opterecenju mreze
    if (networkLoadKw >= 10.0)
    {
        // Visoko opterecenje
        price *= 1.20;
    }
    else if (networkLoadKw >= 5.0)
    {
        // Povecano opterecenje
        price *= 1.10;
    }
    // Popust za prihvaceno smanjenje potrosnje
// primjenjuje se samo tokom visokog opterecenja
bool discountApplied = false;

if (networkLoadKw >= 10.0 && reductionAccepted)
{
    price *= 0.90; // 10% popusta
    discountApplied = true;
}

    // Zaokruzivanje na 2 decimale
    price = std::round(price * 100.0) / 100.0;

    std::cout << "\n=== DINAMICKA TARIFA ===" << std::endl;

    std::cout
        << "Tip korisnika: "
        << (userType == 1 ? "DOMACINSTVO" : "INDUSTRIJA")
        << std::endl;

    std::cout
        << "Tarifni period: "
        << (lowerTariff ? "NIZA TARIFA" : "VISA TARIFA")
        << std::endl;

    std::cout
        << "Opterecenje: "
        << networkLoadKw
        << " kW"
        << std::endl;
    std::cout
    << "Popust za smanjenje potrosnje: "
    << (discountApplied ? "10%" : "NEMA")
    << std::endl;

    std::cout
        << "Izracunata cijena: "
        << std::fixed << std::setprecision(2)
        << price
        << " KM/kWh"
        << std::endl;

    std::cout
        << "========================"
        << std::endl;

    return price;
}
void acceptClient(
    tcp::acceptor& acceptor,
    boost::asio::io_context& io);

void handleClient(
    std::shared_ptr<tcp::socket> socket);



void readConsumptionReport(
    std::shared_ptr<tcp::socket> socket,
    uint8_t userType,
    int reportCount = 0);
void readConsumptionReport(
    std::shared_ptr<tcp::socket> socket,
    uint8_t userType,
    int reportCount)

{
    auto consumptionHeader =
        std::make_shared<std::vector<uint8_t>>(4);

    boost::asio::async_read(
        *socket,
        boost::asio::buffer(*consumptionHeader),

       [socket, consumptionHeader, reportCount, userType]
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
 reportCount,
 userType]
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
                            double totalNetworkLoadKw =
    updateAndGetTotalNetworkLoad(
        report.device_uri,
        report.current_power_kw
    );
database.insertConsumption(
    report.device_uri,
    report.timestamp,
    report.consumption_kwh,
    report.current_power_kw
);
// Slanje novog mjerenja centralnom serveru u realnom vremenu
if (centralSocket && centralSocket->is_open())
{
    RegionSync sync{};

    sync.source_region = 1;

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

                         [socket,
 serializedConsumptionAck,
 reportCount,
 report,
 userType,
 totalNetworkLoadKw]
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
    readConsumptionReport(
    socket,
    userType,
    reportCount + 1
);
}
else
{
    std::cout
        << "Primljeno je 5 CONSUMPTION_REPORT poruka."
        << std::endl;

    std::cout
        << "Server je spreman da posalje REDUCE_CONSUMPTION_CMD."
        << std::endl;
       
        double networkLoadKw = totalNetworkLoadKw;
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

    [socket, serializedCommand, command, userType, networkLoadKw]
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
    [socket, commandAckHeader, command, userType, networkLoadKw]
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
            [socket, commandAckHeader, commandAckPayload, command, userType, networkLoadKw]
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

   tariff.price_per_kwh =
    calculateDynamicTariff(
        userType,
        networkLoadKw,
        true
    );

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
        [socket, serializedTariff, tariff]
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
    << "Nova cijena: "
    << tariff.price_per_kwh
    << " KM/kWh"
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

                            [socket, serializedAck, request]
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

                                readConsumptionReport(
    socket,
    request.user_type
);
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

        std::cout << "Regionalni server 1 povezan sa centralnim serverom."
                  << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Greska pri povezivanju sa centralnim serverom: "
                  << e.what() << std::endl;
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
                5001
            )
        );

        std::cout
            << "Asinhroni server slusa na portu 5001..."
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
