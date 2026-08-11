#pragma once

#include "RHIVulkanPrivate.inl"

namespace RHI {

RHIVulkan::RHIVulkan() : impl_(std::make_unique<Impl>()) {}
RHIVulkan::~RHIVulkan() { Shutdown(); }

RHIVulkan::RHIVulkan(RHIVulkan&&) noexcept = default;
RHIVulkan& RHIVulkan::operator=(RHIVulkan&&) noexcept = default;

b8 RHIVulkan::Initialize(const RHIVulkanDesc& desc, std::string* errorMessage) {
    try {
        if (IsInitialized()) {
            Shutdown();
        }
        impl_ = std::make_unique<Impl>();
        impl_->initDesc = desc;
        impl_->native.surface = desc.surface.surface;
        impl_->ownsSurface = desc.surface.ownsSurface;

        const b8 wantsValidation = desc.backend.validation != RHIValidationMode::Disabled;
        const RHIRenderFeature requestedFeatures = desc.backend.optionalFeatures | desc.backend.requiredFeatures;
        const b8 wantsDebugUtils = wantsValidation || RHIHasAny(requestedFeatures, RHIRenderFeature::DebugMarkers);
        const auto availableLayers = enumerateInstanceLayer();
        const auto availableExtensions = enumerateInstanceExtensions();

        std::vector<const char*> layers;
        if (wantsValidation && hasLayer(availableLayers, "VK_LAYER_KHRONOS_validation")) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }
        std::vector<const char*> instanceExtensions;
        for (const char* extension : desc.requiredInstanceExtensions) {
            if (!hasExtension(availableExtensions, extension)) {
                throw std::runtime_error(std::string("Missing Vulkan instance extension: ") + extension);
            }
            appendUniqueExtension(instanceExtensions, extension);
        }
        for (const char* extension : desc.optionalInstanceExtensions) {
            if (hasExtension(availableExtensions, extension)) {
                appendUniqueExtension(instanceExtensions, extension);
            }
        }
        if (wantsDebugUtils && hasExtension(availableExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            appendUniqueExtension(instanceExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        else if (RHIHasAny(requestedFeatures, RHIRenderFeature::DebugMarkers)) {
            throw std::runtime_error("Missing Vulkan instance extension: VK_EXT_debug_utils");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = desc.backend.applicationName.empty() ? "Excalibur" : desc.backend.applicationName.c_str();
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = desc.backend.engineName.c_str();
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = makeDebugMessengerCreateInfo();

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledLayerCount = static_cast<u32>(layers.size());
        instanceInfo.ppEnabledLayerNames = layers.data();
        instanceInfo.enabledExtensionCount = static_cast<u32>(instanceExtensions.size());
        instanceInfo.ppEnabledExtensionNames = instanceExtensions.data();
        if (wantsValidation) {
            instanceInfo.pNext = &debugCreateInfo;
        }
        if (vkCreateInstance(&instanceInfo, nullptr, &impl_->native.instance) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateInstance failed");
        }

        auto createDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(impl_->native.instance, "vkCreateDebugUtilsMessengerEXT"));
        if (wantsValidation && createDebugUtilsMessenger != nullptr) {
            createDebugUtilsMessenger(impl_->native.instance, &debugCreateInfo, nullptr, &impl_->debugMessenger);
        }

        if (impl_->native.surface == VK_NULL_HANDLE && desc.surface.createSurface != nullptr) {
            impl_->native.surface = desc.surface.createSurface(impl_->native.instance);
            if (impl_->native.surface == VK_NULL_HANDLE) {
                throw std::runtime_error("The Vulkan surface factory returned a null VkSurfaceKHR");
            }
        }
    }
    catch (...) {

    }
}

} // namespace RHI