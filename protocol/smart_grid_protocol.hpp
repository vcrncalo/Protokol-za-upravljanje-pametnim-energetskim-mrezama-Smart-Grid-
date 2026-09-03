
#include <endian.h>
#include <vector>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#ifndef SMART_GRID_PROTOCOL_HPP
#define SMART_GRID_PROTOCOL_HPP

#include <cstdint>

enum class MessageType : uint8_t
{
    REGISTER_REQ = 0x01,
    REGISTER_ACK = 0x02,

    CONSUMPTION_REPORT = 0x03,
    CONSUMPTION_ACK = 0x04,

    TARIFF_UPDATE = 0x05,

    REDUCE_CONSUMPTION_CMD = 0x06,
    COMMAND_ACK = 0x07,

    HEARTBEAT = 0x08,
    ALARM = 0x09,

    REGION_SYNC = 0x20,
    REGION_SYNC_ACK = 0x21
};

struct MessageHeader
{
    uint8_t version;
    uint8_t type;
    uint16_t payload_length;
};

struct RegisterRequest
{
    char device_uri[64];
    uint32_t region_id;
    uint8_t user_type;
};
struct RegisterAck
{
    uint8_t status;
};
struct ConsumptionReport
{
    char device_uri[64];
    uint64_t timestamp;
    double consumption_kwh;
    double current_power_kw;
};
struct ConsumptionAck
{
    uint8_t status;
};

struct Heartbeat
{
    char device_uri[64];
    uint64_t timestamp;
};
struct ReduceConsumptionCommand
{
    char device_uri[64];
    double target_power_kw;
};

struct CommandAck
{
    uint8_t status;
};
struct TariffUpdate
{
    double price_per_kwh;
};

inline std::vector<uint8_t> serializeHeartbeat(
    const Heartbeat& heartbeat)
{
    std::vector<uint8_t> buffer;

    MessageHeader header;
    header.version = 1;
    header.type =
        static_cast<uint8_t>(MessageType::HEARTBEAT);

    uint16_t payloadSize =
        sizeof(heartbeat.device_uri) +
        sizeof(heartbeat.timestamp);

    header.payload_length =
        htons(payloadSize);

    // Header
    buffer.push_back(header.version);
    buffer.push_back(header.type);

    uint8_t* lengthPtr =
        reinterpret_cast<uint8_t*>(
            &header.payload_length
        );

    buffer.push_back(lengthPtr[0]);
    buffer.push_back(lengthPtr[1]);

    // URI Smart Metera
    buffer.insert(
        buffer.end(),
        heartbeat.device_uri,
        heartbeat.device_uri +
            sizeof(heartbeat.device_uri)
    );

    // Timestamp
    uint64_t timestampNetwork =
        htobe64(heartbeat.timestamp);

    uint8_t* timestampPtr =
        reinterpret_cast<uint8_t*>(
            &timestampNetwork
        );

    buffer.insert(
        buffer.end(),
        timestampPtr,
        timestampPtr +
            sizeof(timestampNetwork)
    );

    return buffer;
}

inline Heartbeat deserializeHeartbeat(
    const std::vector<uint8_t>& buffer)
{
    Heartbeat heartbeat{};

    std::size_t offset = 4;

    // URI Smart Metera
    std::memcpy(
        heartbeat.device_uri,
        buffer.data() + offset,
        sizeof(heartbeat.device_uri)
    );

    offset += sizeof(heartbeat.device_uri);

    // Timestamp
    uint64_t timestampNetwork;

    std::memcpy(
        &timestampNetwork,
        buffer.data() + offset,
        sizeof(timestampNetwork)
    );

    heartbeat.timestamp =
        be64toh(timestampNetwork);

    return heartbeat;
}

inline std::vector<uint8_t> serializeRegisterRequest(
    const RegisterRequest& request)
{
    std::vector<uint8_t> buffer;

    MessageHeader header;
    header.version = 1;
    header.type = static_cast<uint8_t>(MessageType::REGISTER_REQ);

    uint16_t payloadSize =
        sizeof(request.device_uri) +
        sizeof(request.region_id) +
        sizeof(request.user_type);

    header.payload_length = htons(payloadSize);

    buffer.push_back(header.version);
    buffer.push_back(header.type);

    uint8_t* lengthPtr =
        reinterpret_cast<uint8_t*>(&header.payload_length);

    buffer.push_back(lengthPtr[0]);
    buffer.push_back(lengthPtr[1]);

    buffer.insert(
        buffer.end(),
        request.device_uri,
        request.device_uri + sizeof(request.device_uri)
    );

    uint32_t regionNetworkOrder =
        htonl(request.region_id);

    uint8_t* regionPtr =
        reinterpret_cast<uint8_t*>(&regionNetworkOrder);

    buffer.insert(
        buffer.end(),
        regionPtr,
        regionPtr + sizeof(regionNetworkOrder)
    );

    buffer.push_back(request.user_type);

    return buffer;
}
inline RegisterRequest deserializeRegisterRequest(
    const std::vector<uint8_t>& buffer)
{
    RegisterRequest request{};

    std::size_t offset = 4;

    std::memcpy(
        request.device_uri,
        buffer.data() + offset,
        sizeof(request.device_uri)
    );

    offset += sizeof(request.device_uri);

    uint32_t regionNetworkOrder;

    std::memcpy(
        &regionNetworkOrder,
        buffer.data() + offset,
        sizeof(regionNetworkOrder)
    );

    request.region_id =
        ntohl(regionNetworkOrder);

    offset += sizeof(regionNetworkOrder);

    request.user_type =
        buffer[offset];

    return request;
}
inline std::vector<uint8_t> serializeRegisterAck(
    const RegisterAck& ack)
{
    std::vector<uint8_t> buffer;

    MessageHeader header;
    header.version = 1;
    header.type = static_cast<uint8_t>(MessageType::REGISTER_ACK);
    header.payload_length = htons(1);

    buffer.push_back(header.version);
    buffer.push_back(header.type);

    uint8_t* lengthPtr =
        reinterpret_cast<uint8_t*>(&header.payload_length);

    buffer.push_back(lengthPtr[0]);
    buffer.push_back(lengthPtr[1]);

    buffer.push_back(ack.status);

    return buffer;
}
inline std::vector<uint8_t> serializeConsumptionReport(
    const ConsumptionReport& report)
{
    std::vector<uint8_t> buffer;

    MessageHeader header;
    header.version = 1;
    header.type = static_cast<uint8_t>(MessageType::CONSUMPTION_REPORT);

    uint16_t payloadSize =
        sizeof(report.device_uri) +
        sizeof(report.timestamp) +
        sizeof(report.consumption_kwh) +
        sizeof(report.current_power_kw);

    header.payload_length = htons(payloadSize);

    // Header
    buffer.push_back(header.version);
    buffer.push_back(header.type);

    uint8_t* lengthPtr =
        reinterpret_cast<uint8_t*>(&header.payload_length);

    buffer.push_back(lengthPtr[0]);
    buffer.push_back(lengthPtr[1]);

    // Device URI
    buffer.insert(
        buffer.end(),
        report.device_uri,
        report.device_uri + sizeof(report.device_uri)
    );

    // Timestamp
    uint64_t timestampNetwork =
        htobe64(report.timestamp);

    uint8_t* timestampPtr =
        reinterpret_cast<uint8_t*>(&timestampNetwork);

    buffer.insert(
        buffer.end(),
        timestampPtr,
        timestampPtr + sizeof(timestampNetwork)
    );

    // Consumption kWh
    const uint8_t* consumptionPtr =
        reinterpret_cast<const uint8_t*>(&report.consumption_kwh);

    buffer.insert(
        buffer.end(),
        consumptionPtr,
        consumptionPtr + sizeof(report.consumption_kwh)
    );

    // Current power kW
    const uint8_t* powerPtr =
        reinterpret_cast<const uint8_t*>(&report.current_power_kw);

    buffer.insert(
        buffer.end(),
        powerPtr,
        powerPtr + sizeof(report.current_power_kw)
    );

    return buffer;
}

std::vector<uint8_t> serializeReduceConsumptionCommand(
    const ReduceConsumptionCommand& command)
{
    std::vector<uint8_t> buffer;

    MessageHeader header{};
    header.version = 1;
    header.type =
        static_cast<uint8_t>(
            MessageType::REDUCE_CONSUMPTION_CMD
        );

    header.payload_length =
        htons(
            sizeof(command.device_uri)
            + sizeof(command.target_power_kw)
        );

    buffer.resize(
        sizeof(MessageHeader)
        + sizeof(command.device_uri)
        + sizeof(command.target_power_kw)
    );

    std::memcpy(
        buffer.data(),
        &header,
        sizeof(MessageHeader)
    );

    std::size_t offset =
        sizeof(MessageHeader);

    std::memcpy(
        buffer.data() + offset,
        command.device_uri,
        sizeof(command.device_uri)
    );

    offset += sizeof(command.device_uri);

    std::memcpy(
        buffer.data() + offset,
        &command.target_power_kw,
        sizeof(command.target_power_kw)
    );

    return buffer;
}
ReduceConsumptionCommand deserializeReduceConsumptionCommand(
    const std::vector<uint8_t>& buffer)
{
    ReduceConsumptionCommand command{};

    std::size_t offset =
        sizeof(MessageHeader);

    std::memcpy(
        command.device_uri,
        buffer.data() + offset,
        sizeof(command.device_uri)
    );

    offset += sizeof(command.device_uri);

    std::memcpy(
        &command.target_power_kw,
        buffer.data() + offset,
        sizeof(command.target_power_kw)
    );

    return command;
}
inline ConsumptionReport deserializeConsumptionReport(
    const std::vector<uint8_t>& buffer)
{
    ConsumptionReport report{};

    std::size_t offset = 4;

    // Device URI
    std::memcpy(
        report.device_uri,
        buffer.data() + offset,
        sizeof(report.device_uri)
    );

    offset += sizeof(report.device_uri);

    // Timestamp
    uint64_t timestampNetwork;

    std::memcpy(
        &timestampNetwork,
        buffer.data() + offset,
        sizeof(timestampNetwork)
    );

    report.timestamp =
        be64toh(timestampNetwork);

    offset += sizeof(timestampNetwork);

    // Consumption kWh
    std::memcpy(
        &report.consumption_kwh,
        buffer.data() + offset,
        sizeof(report.consumption_kwh)
    );

    offset += sizeof(report.consumption_kwh);

    // Current power kW
    std::memcpy(
        &report.current_power_kw,
        buffer.data() + offset,
        sizeof(report.current_power_kw)
    );

    return report;
}

inline RegisterAck deserializeRegisterAck(
    const std::vector<uint8_t>& buffer)
{
    RegisterAck ack{};

    ack.status = buffer[4];

    return ack;
}
inline std::vector<uint8_t> serializeConsumptionAck(
    const ConsumptionAck& ack)
{
    std::vector<uint8_t> buffer;

    MessageHeader header;
    header.version = 1;
    header.type = static_cast<uint8_t>(MessageType::CONSUMPTION_ACK);
    header.payload_length = htons(1);

    buffer.push_back(header.version);
    buffer.push_back(header.type);

    uint8_t* lengthPtr =
        reinterpret_cast<uint8_t*>(&header.payload_length);

    buffer.push_back(lengthPtr[0]);
    buffer.push_back(lengthPtr[1]);

    buffer.push_back(ack.status);

    return buffer;
}

inline ConsumptionAck deserializeConsumptionAck(
    const std::vector<uint8_t>& buffer)
{
    ConsumptionAck ack{};
    ack.status = buffer[4];
    return ack;
}
std::vector<uint8_t> serializeCommandAck(
    const CommandAck& ack)
{
    std::vector<uint8_t> buffer;

    MessageHeader header{};
    header.version = 1;
    header.type =
        static_cast<uint8_t>(
            MessageType::COMMAND_ACK
        );

    header.payload_length =
        htons(sizeof(ack.status));

    buffer.resize(
        sizeof(MessageHeader)
        + sizeof(ack.status)
    );

    std::memcpy(
        buffer.data(),
        &header,
        sizeof(MessageHeader)
    );

    std::memcpy(
        buffer.data() + sizeof(MessageHeader),
        &ack.status,
        sizeof(ack.status)
    );

    return buffer;
}
CommandAck deserializeCommandAck(
    const std::vector<uint8_t>& buffer)
{
    CommandAck ack{};

    std::size_t offset =
        sizeof(MessageHeader);

    std::memcpy(
        &ack.status,
        buffer.data() + offset,
        sizeof(ack.status)
    );

    return ack;
}
std::vector<uint8_t> serializeTariffUpdate(
    const TariffUpdate& tariff)
{
    std::vector<uint8_t> buffer;

    MessageHeader header{};

    header.version = 1;

    header.type =
        static_cast<uint8_t>(
            MessageType::TARIFF_UPDATE
        );

    header.payload_length =
        htons(sizeof(tariff.price_per_kwh));

    buffer.resize(
        sizeof(MessageHeader)
        + sizeof(tariff.price_per_kwh)
    );

    std::memcpy(
        buffer.data(),
        &header,
        sizeof(MessageHeader)
    );

    std::memcpy(
        buffer.data() + sizeof(MessageHeader),
        &tariff.price_per_kwh,
        sizeof(tariff.price_per_kwh)
    );

    return buffer;
}
TariffUpdate deserializeTariffUpdate(
    const std::vector<uint8_t>& buffer)
{
    TariffUpdate tariff{};

    std::size_t offset =
        sizeof(MessageHeader);

    std::memcpy(
        &tariff.price_per_kwh,
        buffer.data() + offset,
        sizeof(tariff.price_per_kwh)
    );

    return tariff;
}
#endif
