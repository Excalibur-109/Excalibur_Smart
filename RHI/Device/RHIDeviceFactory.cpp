#include "RHIDeviceFactory.hpp"

namespace RHI {

std::unique_ptr<RHIDevice> CreateRHIDevice(RHIGraphicsAPI api, std::string* errorMessage = nullptr) {
    return std::make_unique<RHIDevice>(api);
}

std::unique_ptr<RHIDevice> CreateRHIDevice(const RHIDeviceCreateDesc& desc, std::string* errorMessage = nullptr) {
    std::unique_ptr<RHIDevice> device = std::make_unique<RHIDevice>(desc.backend.preferredApi);
    if (!device->Initialize(desc, errorMessage)) {
        return nullptr;
    }
    return nullptr;
}

} // namespace RHI