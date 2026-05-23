#include "VulkanContext.hpp"

#include <map>
#include <string>
#include <lib/ranges.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/color.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "Device.hpp"
#include "IWindow.hpp"
#include "Swapchain.hpp"
#include "ext/Extension.hpp"
#include "platform/platform.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

namespace ptvc::rhi
{
    namespace detail
    {
        // Get the list of default instance extensions for this target. (platform, debug)
        [[nodiscard]] static constexpr std::vector<const char*> getDefaultInstanceExtensions() noexcept
        {
            std::vector<const char*> extensions = { "VK_KHR_surface" };
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            extensions.append_range(platform::getInstanceExtensions());
            return extensions;
        }

        // Get the list of default instance layers for this target. (platform, debug)
        [[nodiscard]] static constexpr std::vector<const char*> getDefaultInstanceLayers() noexcept
        {
            #ifndef NDEBUG
            return { gVulkanKhronosValidationLayerName };
            #endif

            return {};
        }

        // Callback function for vk::DebugUtilsMessengerEXT
        static vk::Bool32 VKAPI_CALL debugMessengerCallback(
            const vk::DebugUtilsMessageSeverityFlagBitsEXT           severity,
            [[maybe_unused]] const vk::DebugUtilsMessageTypeFlagsEXT type,
            const vk::DebugUtilsMessengerCallbackDataEXT*            pData,
            [[maybe_unused]] void*                                   pUserData) noexcept
        {
            static std::shared_ptr<spdlog::logger> debugLogger;
            if (!debugLogger)
            {
                debugLogger = spdlog::stdout_color_mt("vk-debug");
                debugLogger->set_pattern("[validation] %v");
            }

            if (!pData)
            {
                return vk::False;
            }
            switch (severity)
            {
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose: {
                    debugLogger->debug("[verbose] {}", pData->pMessage);
                    break;
                }
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo: {
                    debugLogger->info("[info] {}", pData->pMessage);
                    break;
                }
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning: {
                    debugLogger->warn("[warning] {}", STYLE_WARNING(pData->pMessage));
                    break;
                }
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError: {
                    debugLogger->error("[error] {}", STYLE_ERROR(pData->pMessage));
                    break;
                }
            }

            return vk::False;
        }

        // Style the name of the PhysicalDevice based on Vendor ID.
        std::string styledPhysicalDeviceName(const vk::PhysicalDeviceProperties2& physicalDeviceProperties)
        {
            auto name = std::string(physicalDeviceProperties.properties.deviceName.data());
            auto color = fmt::color::white;
            switch (physicalDeviceProperties.properties.vendorID)
            {
                case gVendorID_Apple: {
                    color = fmt::color::antique_white;
                    break;
                }
                case gVendorID_AMD: {
                    color = fmt::color::red;
                    break;
                }
                case gVendorID_Intel: {
                    color = fmt::color::cornflower_blue;
                    break;
                }
                case gVendorID_Nvidia: {
                    color = fmt::color::green_yellow;
                    break;
                }
                default:
                    return name;
            }
            return fmt::format("{}", styled(name, fg(color)));
        }
    }

    SPtr<VulkanContext> VulkanContext::create(const VulkanContextCreateInfo& createInfo) noexcept
    {
        return SPtr<VulkanContext>(std::move(new VulkanContext(createInfo)));
    }

    VulkanContext::VulkanContext(const VulkanContextCreateInfo& createInfo)
    : mWindow(createInfo.window)
    {
        #ifndef NDEBUG
        spdlog::set_level(spdlog::level::debug);
        #endif

        if (!mWindow)
        {
            exitWithError("No window was specified to the Vulkan context.");
        }

        const vk::detail::DynamicLoader dynamicLoader;
        const auto vkGetInstanceProcAddr = dynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        if (const auto instanceResult = createVulkanInstance(); !instanceResult.has_value())
        {
            exitWithError("Failed to create Vulkan Instance: {}", STYLE_ERROR(instanceResult.error()));
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(mInstance);

        if (const auto surfaceResult = mWindow->createVulkanSurfaceKHR(mInstance, &mSurface); !surfaceResult.has_value())
        {
            exitWithError("Failed to create Vulkan Surface: {}", STYLE_ERROR(surfaceResult.error()));
        }

        createInfo.options.extensions(mDeviceExtensions);

        const auto physicalDeviceResult = selectPhysicalDevice();
        if (!physicalDeviceResult.has_value())
        {
            exitWithError("Failed to find a suitable physical device! (Available devices: {})", STYLE_ERROR(physicalDeviceResult.error()));
        }
        mPhysicalDevice = physicalDeviceResult.value();
        mPhysicalDeviceProperties = mPhysicalDevice.getProperties2();
        spdlog::info("Using PhysicalDevice: {}", detail::styledPhysicalDeviceName(mPhysicalDeviceProperties));

        mDeviceExtensions.postPhysicalDeviceSelection(mPhysicalDevice);
        spdlog::debug("GPU ({}) Extension support:\n{}", detail::styledPhysicalDeviceName(mPhysicalDeviceProperties), mDeviceExtensions.toString());

        const auto deviceResult = Device::create({ mInstance, mPhysicalDevice, mDeviceExtensions });
        if (!deviceResult.has_value())
        {
            exitWithError("Failed to find a create Vulkan Device: {}", STYLE_ERROR(deviceResult.error()));
        }
        mDevice = std::move(deviceResult.value());
        VULKAN_HPP_DEFAULT_DISPATCHER.init(mDevice->getHandle());

        if (!mPhysicalDevice.getSurfaceSupportKHR(mDevice->getGraphicsQueue().familyIndex, mSurface))
        {
            exitWithError("The queue family [{}] does not support presentation.", mDevice->getGraphicsQueue().familyIndex);
        }

       createSwapchain();

        mFrameSync = makeUnique<FrameSync>(mDevice, mSwapchain->getImageCount());

        /* CommandPool */ {
            const auto commandPoolInfo = vk::CommandPoolCreateInfo()
                .setQueueFamilyIndex(mDevice->getGraphicsQueue().familyIndex)
                .setFlags(vk::CommandPoolCreateFlagBits::eTransient);
            try {
                mCommandPool = mDevice->getHandle().createCommandPool(commandPoolInfo);
            } catch (const vk::SystemError& e) {
                exitWithError("Failed to create CommandPool: {}", STYLE_ERROR(e.what()));
            }
        }
    }

    Result<Frame> VulkanContext::beginFrame() const noexcept
    {
        Frame frame = mFrameSync->getNextFrame();
        VK_RESULT(mDevice->getHandle().waitForFences(frame.fPresentFinished, true, std::numeric_limits<uint64_t>::max()));
        mDevice->getHandle().resetFences(frame.fPresentFinished);

        const auto acquireInfo = vk::AcquireNextImageInfoKHR()
            .setSwapchain(mSwapchain->getHandle())
            .setTimeout(std::numeric_limits<uint64_t>::max())
            .setSemaphore(frame.sImageAvailable)
            .setDeviceMask(1);

        VK_CATCH(frame.acquiredImageIndex = mDevice->getHandle().acquireNextImage2KHR(acquireInfo).value);

        const auto allocInfo = vk::CommandBufferAllocateInfo()
            .setCommandPool(mCommandPool)
            .setCommandBufferCount(1)
            .setLevel(vk::CommandBufferLevel::ePrimary);
        const auto buffers = mDevice->getHandle().allocateCommandBuffers(allocInfo);
        frame.commandBuffer = buffers[0];

        constexpr auto beginInfo = vk::CommandBufferBeginInfo();
        VK_CATCH(frame.commandBuffer.begin(beginInfo));

        return frame;
    }

    void VulkanContext::endFrame_submitAndPresent(const Frame& frame) const noexcept
    {
        frame.commandBuffer.end();

        const auto commandBufferSubmitInfo = vk::CommandBufferSubmitInfo()
            .setCommandBuffer(frame.commandBuffer);

        const auto waitSemaphoreInfo = vk::SemaphoreSubmitInfo()
                .setSemaphore(frame.sImageAvailable)
                .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

        const auto signalSemaphoreInfo = vk::SemaphoreSubmitInfo()
                .setSemaphore(frame.sRenderingFinished)
                .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

        const auto vkSubmitInfo = vk::SubmitInfo2()
            .setCommandBufferInfos(commandBufferSubmitInfo)
            .setCommandBufferInfoCount(1)
            .setWaitSemaphoreInfos(waitSemaphoreInfo)
            .setWaitSemaphoreInfoCount(1)
            .setSignalSemaphoreInfos(signalSemaphoreInfo)
            .setSignalSemaphoreInfoCount(1);

        mDevice->getGraphicsQueue()
            .getHandle(0)
            .submit2(vkSubmitInfo, frame.fPresentFinished);

        const auto swapchain = mSwapchain->getHandle();
        const auto presentInfo = vk::PresentInfoKHR()
            .setPWaitSemaphores(&frame.sRenderingFinished)
            .setWaitSemaphoreCount(1)
            .setPSwapchains(&swapchain)
            .setSwapchainCount(1)
            .setImageIndices(frame.acquiredImageIndex)
            .setPResults(nullptr);

        const auto result = mDevice->getGraphicsQueue()
            .getHandle(0)
            .presentKHR(presentInfo);

        mDevice->getGraphicsQueue().getHandle(0).waitIdle();

        mDevice->getHandle().freeCommandBuffers(mCommandPool, 1, &frame.commandBuffer);

        mFrameSync->advance();
    }

    void VulkanContext::executeImmediateCommand(const std::function<void (const vk::CommandBuffer&)>& lambda) const noexcept
    {
        const auto allocateInfo = vk::CommandBufferAllocateInfo()
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandPool(mCommandPool)
            .setCommandBufferCount(1);

        vk::CommandBuffer commandBuffer;
        VK_CHECK(mDevice->getHandle().allocateCommandBuffers(&allocateInfo, &commandBuffer));

        constexpr auto beginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        VK_CHECK(commandBuffer.begin(&beginInfo));

        lambda(commandBuffer);

        commandBuffer.end();

        const auto queue = mDevice->getGraphicsQueue().getHandle();
        const auto submitInfo = vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&commandBuffer);
        VK_CHECK(queue.submit(1, &submitInfo, nullptr));

        queue.waitIdle();
        mDevice->getHandle().freeCommandBuffers(mCommandPool, 1, &commandBuffer);
    }

    void VulkanContext::rebuildSwapchain() noexcept
    {
        const auto oldSwapchain = std::move(mSwapchain);
        auto swapchainResult = Swapchain::create({
            mDevice, mWindow, mSurface, oldSwapchain.get()
        });
        if (!swapchainResult.has_value())
        {
            exitWithError("Failed to create Swapchain: {}", STYLE_ERROR(swapchainResult.error()));
        }
        mSwapchain = std::move(swapchainResult.value());
    }

    SPtr<Device> VulkanContext::getDevice() const noexcept
    {
        return mDevice;
    }

    Swapchain* VulkanContext::getSwapchain() const noexcept
    {
        return mSwapchain.get();
    }

    vk::Instance VulkanContext::getInstance() const noexcept
    {
        return mInstance;
    }

    Result<void> VulkanContext::createVulkanInstance() noexcept
    {
        constexpr auto applicationInfo = vk::ApplicationInfo()
            .setApiVersion(VK_API_VERSION_1_4)
            .setPApplicationName("ptvc-project")
            .setPEngineName("ptvc-runtime");

        const auto windowInstanceExtResult = mWindow->getRequiredInstanceExtensionsWSI();
        if (!windowInstanceExtResult.has_value())
        {
            exitWithError("{}", STYLE_ERROR(windowInstanceExtResult.error()));
        }

        auto instanceExtensions = detail::getDefaultInstanceExtensions();
        instanceExtensions.append_range(windowInstanceExtResult.value());

        if (const auto extensionSupportResult = evaluateFeatureSupport(vk::enumerateInstanceExtensionProperties(), instanceExtensions);
            !extensionSupportResult.has_value())
        {
            exitWithError("The following required Vulkan Instance extensions are missing: {}",
                join(extensionSupportResult.error(), ", "));
        }

        const auto instanceLayers = detail::getDefaultInstanceLayers();
        if (const auto layerSupportResult = evaluateFeatureSupport(vk::enumerateInstanceLayerProperties(), instanceLayers);
            !layerSupportResult.has_value())
        {
            exitWithError("The following required Vulkan Instance layers are missing: {}",
                join(layerSupportResult.error(), ", "));
        }

        const auto instanceCreateInfo = vk::InstanceCreateInfo()
            .setFlags(platform::getInstanceFlags())
            .setEnabledExtensionCount(instanceExtensions.size())
            .setPpEnabledExtensionNames(instanceExtensions.data())
            .setEnabledLayerCount(instanceLayers.size())
            .setPpEnabledLayerNames(instanceLayers.data())
            .setPApplicationInfo(&applicationInfo);

        VK_CATCH(mInstance = vk::createInstance(instanceCreateInfo));

        return {};
    }

    Result<void> VulkanContext::createDebugMessenger() noexcept
    {
        using S = vk::DebugUtilsMessageSeverityFlagBitsEXT;
        constexpr auto severity = S::eInfo | S::eWarning | S::eError | S::eVerbose;

        using T = vk::DebugUtilsMessageTypeFlagBitsEXT;
        constexpr auto type = T::eGeneral | T::ePerformance | T::eValidation;

        constexpr auto messengerCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT()
            .setMessageSeverity(severity)
            .setMessageType(type)
            .setPfnUserCallback(detail::debugMessengerCallback);

        VK_CATCH(mDebugMessenger = mInstance.createDebugUtilsMessengerEXT(messengerCreateInfo));
        return {};
    }

    Result<vk::PhysicalDevice> VulkanContext::selectPhysicalDevice() const noexcept
    {
        std::vector<std::string> availableDeviceNames;
        std::map<vk::PhysicalDevice, int32_t> scores;

        for (const auto& physicalDevice : mInstance.enumeratePhysicalDevices())
        {
            const auto props2 = physicalDevice.getProperties2();
            const auto properties = props2.properties;

            // Extension scoring
            const auto extensionScore = mDeviceExtensions.evaluateDeviceSupport(physicalDevice);

            // Device Type scoring
            int32_t deviceTypeScore = 0;
            switch (properties.deviceType)
            {
                case vk::PhysicalDeviceType::eIntegratedGpu:
                    deviceTypeScore = gDeviceScore_IsIntegratedGPU;
                    break;
                case vk::PhysicalDeviceType::eDiscreteGpu: {
                    deviceTypeScore = gDeviceScore_IsDedicatedGPU;
                    break;
                }
                default: {
                    deviceTypeScore = 0;
                    break;
                }
            }

            scores[physicalDevice] = extensionScore + deviceTypeScore;
            availableDeviceNames.push_back(std::format("{}[Type={}, Score={}]",std::string(properties.deviceName.data()), to_string(properties.deviceType), scores[physicalDevice]));
        }

        if (scores.empty() || std::ranges::all_of(scores, [](const auto& dsp){return dsp.second <= 0; }))
        {
            return std::unexpected(join(availableDeviceNames, ", "));
        }

        // Return PhysicalDevice with the highest score.
        return std::ranges::max_element(scores, [](const auto& a, const auto& b) -> bool {
            return a.second < b.second;
        })->first;
    }

    void VulkanContext::createSwapchain() noexcept
    {
        auto swapchainResult = Swapchain::create({
            .device  = mDevice,
            .window  = mWindow,
            .surface = mSurface,
        });
        if (!swapchainResult.has_value())
        {
            exitWithError("Failed to create Swapchain: {}", STYLE_ERROR(swapchainResult.error()));
        }
        mSwapchain = std::move(swapchainResult.value());
    }
}
