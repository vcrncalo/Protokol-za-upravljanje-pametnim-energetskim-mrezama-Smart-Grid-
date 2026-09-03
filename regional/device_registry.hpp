#ifndef DEVICE_REGISTRY_HPP
#define DEVICE_REGISTRY_HPP

#include <string>
#include <unordered_map>
#include <iostream>

struct DeviceInfo
{
    std::string uri;
    uint32_t region_id;
    uint8_t user_type;
};

class DeviceRegistry
{
private:
    std::unordered_map<std::string, DeviceInfo> devices;

public:
    bool registerDevice(
        const std::string& uri,
        uint32_t region_id,
        uint8_t user_type)
    {
        if (devices.find(uri) != devices.end())
        {
            std::cout
                << "Smart Meter je vec registrovan: "
                << uri
                << std::endl;

            return false;
        }

        DeviceInfo info;

        info.uri = uri;
        info.region_id = region_id;
        info.user_type = user_type;

        devices[uri] = info;

        std::cout
            << "Smart Meter dodat u registry: "
            << uri
            << std::endl;

        return true;
    }

    bool isRegistered(
        const std::string& uri) const
    {
        return devices.find(uri) != devices.end();
    }

    void printDevices() const
    {
        std::cout
            << "\n--- REGISTROVANI SMART METERI ---"
            << std::endl;

        if (devices.empty())
        {
            std::cout
                << "Nema registrovanih uredjaja."
                << std::endl;

            return;
        }

        for (const auto& pair : devices)
        {
            const DeviceInfo& device =
                pair.second;

            std::cout
                << "URI: "
                << device.uri
                << " | Region: "
                << device.region_id
                << " | User type: "
                << static_cast<int>(
                    device.user_type)
                << std::endl;
        }

        std::cout
            << "--------------------------------"
            << std::endl;
    }
};

#endif
