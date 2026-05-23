#include "GameLayer.hpp"

#include <spdlog/sinks/stdout_color_sinks-inl.h>

#include <render/GraphicsPipeline.hpp>
#include <scene/ArcballCamera.hpp>
#include "ControllableGameObject.hpp"
#include "RotatingGameObject.hpp"
#include "core/IO.hpp"

GameLayer::GameLayer()
{
    const auto* app = ptvc::Application::getApplication();
    mVulkanContext  = app->getVulkanContext();
    mScene          = app->getScene();
    mEngine         = std::mt19937(std::random_device{}());

    mLogger = spdlog::stdout_color_mt("GameLayer");
    mLogger->set_pattern("[\x1b[36mGameLayer\x1b[0m] [%^%l%$] %v");

    mCubeGeometry = makeShared<ptvc::Cube>();
    mCubeGeometry->init(mVulkanContext.get());

    // Before pipeline, this creates mTextureDescriptor
    loadTestTexture();

    createBasicPipelines();

    // Add Camera
    // =============================
    const auto [w, h] = mVulkanContext->getSwapchain()->getExtent();
    mScene->initCamera<ptvc::ArcballCamera>(static_cast<float>(w) / static_cast<float>(h));

    // Add example objects
    // =============================
    auto gameObjectParams = ptvc::GameObjectParams {
        .pipeline = mPipeline,
        .geometry = mCubeGeometry,
    };

    std::uniform_real_distribution dist(-glm::half_pi<float>(), glm::half_pi<float>());
    std::uniform_real_distribution pos(-10.0f, 10.0f);
    std::uniform_real_distribution ax(-1.0f, 1.0f);

    gameObjectParams.pipeline = mPipeline;
    gameObjectParams.initialTransform.translate = glm::vec3(pos(mEngine), pos(mEngine), pos(mEngine));
    gameObjectParams.name = std::format("Object #{}", mScene->getGameObjects().size());
    gameObjectParams.useExampleTexture = true;

    auto* obj = mScene->addGameObject<RotatingGameObject>(
        dist(mEngine), glm::normalize(glm::vec3(ax(mEngine), ax(mEngine), ax(mEngine))), gameObjectParams);

    gameObjectParams.pipeline = mPhongPipeline;
    gameObjectParams.initialTransform = {};
    gameObjectParams.name = "Player";
    gameObjectParams.useExampleTexture = false;
    auto* player = mScene->addGameObject<ControllableGameObject>(gameObjectParams);
}

void GameLayer::onEvent(const SDL_Event& event) noexcept
{
    // Add object on Q pressed
    // =============================
    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        const SDL_KeyboardEvent& keyboardEvent = event.key;
        if (keyboardEvent.scancode == SDL_SCANCODE_Q)
        {
            auto gameObjectParams = ptvc::GameObjectParams {
                .pipeline = mPhongPipeline,
                .geometry = mCubeGeometry,
            };

            std::uniform_real_distribution dist(-glm::half_pi<float>(), glm::half_pi<float>());
            std::uniform_real_distribution pos(-10.0f, 10.0f);
            std::uniform_real_distribution ax(-1.0f, 1.0f);

            gameObjectParams.initialTransform.translate = glm::vec3(pos(mEngine), pos(mEngine), pos(mEngine));
            gameObjectParams.name = std::format("Object #{}", mScene->getGameObjects().size());
            gameObjectParams.useExampleTexture = (pos(mEngine) > 0.0f) ? true : false;

            auto* obj = mScene->addGameObject<RotatingGameObject>(
                dist(mEngine), glm::normalize(glm::vec3(ax(mEngine), ax(mEngine), ax(mEngine))), gameObjectParams);

            mLogger->info("Added new object!");
        }
    }
}

void GameLayer::onRender(const ptvc::rhi::Frame& frame) noexcept
{
    // Begin debug label region
    frame.commandBuffer.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT().setPLabelName("GameLayer::onRender()"));
    // =================================

    // Barriers for render targets: Swapchain and Depth Buffer
    #pragma region
    std::array<vk::ImageMemoryBarrier2, 2> barriers;

    barriers[0] = mVulkanContext->getSwapchain()->getBarrier(frame.acquiredImageIndex, {
        .layout     = vk::ImageLayout::eColorAttachmentOptimal,
        .accessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
        .stageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    });

    barriers[1] = vk::ImageMemoryBarrier2()
        .setImage(mDepthBuffer->getHandle())
        .setSubresourceRange({ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 })
        .setOldLayout(mFirstRender ? vk::ImageLayout::eUndefined : vk::ImageLayout::eDepthAttachmentOptimal)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
        .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests);

    const auto dependencyInfo = vk::DependencyInfo().setImageMemoryBarriers(barriers);
    #pragma endregion
    frame.commandBuffer.pipelineBarrier2(dependencyInfo);

    frame.commandBuffer.setScissor(0, mVulkanContext->getSwapchain()->getScissor());
    frame.commandBuffer.setViewport(0, mVulkanContext->getSwapchain()->getViewport());

    // Setup rendering attachments
    #pragma region
    const auto colorAttachment = vk::RenderingAttachmentInfo()
        .setClearValue(vk::ClearValue().setColor({0.0f, 0.0f, 0.0f, 1.0f}))
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(mVulkanContext->getSwapchain()->getImageView(frame.acquiredImageIndex))
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    const auto depthAttachment = vk::RenderingAttachmentInfo()
        .setClearValue(vk::ClearValue().setDepthStencil({1.0f, 0}))
        .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setImageView(mDepthBuffer->getImageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    const auto renderingInfo = vk::RenderingInfo()
        .setColorAttachments(colorAttachment)
        .setPDepthAttachment(&depthAttachment)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D{{0, 0}, mVulkanContext->getSwapchain()->getExtent()});
    #pragma endregion

    frame.commandBuffer.beginRendering(renderingInfo);

    for (const auto& obj : mScene->getGameObjects())
    {
        obj->onRender(frame);
    }

    frame.commandBuffer.endRendering();

    // End debug label region
    frame.commandBuffer.endDebugUtilsLabelEXT();
    // =================================

    if (mFirstRender)
    {
        mFirstRender = false;
    }
}

void GameLayer::createBasicPipelines() noexcept
{
    using enum vk::ShaderStageFlagBits;
    mDepthBuffer = ptvc::rhi::Image::create({
        .extent     = mVulkanContext->getSwapchain()->getExtent(),
        .usageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment,
        .format     = vk::Format::eD32Sfloat,
        .mipmapping = false,
        .samples    = vk::SampleCountFlagBits::e1,
        .label      = "BasicPipeline_DepthBuffer",
        .device     = mVulkanContext->getDevice(),
    }).value();

    mPipeline = ptvc::rhi::GraphicsPipelineBuilder()
        .addDescriptor(0, mScene->getDescriptor())
        .addDescriptor(1, mTextureDescriptor)
        .addPushConstantRange({ eVertex | eFragment, 0, sizeof(ptvc::GPUGameObjectData) })
        .addVertexType<ptvc::Vertex>()
        .addShader({ "assets/shaders/basic.vert.glsl", eVertex })
        .addShader({ "assets/shaders/basic.frag.glsl", eFragment })
        .addAttachment(vk::Format::eB8G8R8A8Unorm)
        .setDepthFormat(vk::Format::eD32Sfloat)
        .setName("BasicPipeline")
        .create(mVulkanContext->getDevice());

    mPhongPipeline = ptvc::rhi::GraphicsPipelineBuilder()
        .addDescriptor(0, mScene->getDescriptor())
        .addDescriptor(1, mTextureDescriptor)
        .addPushConstantRange({ eVertex | eFragment, 0, sizeof(ptvc::GPUGameObjectData) })
        .addVertexType<ptvc::Vertex>()
        .addShader({ "assets/shaders/phong.vert.glsl", eVertex })
        .addShader({ "assets/shaders/phong.frag.glsl", eFragment })
        .addAttachment(vk::Format::eB8G8R8A8Unorm)
        .setDepthFormat(vk::Format::eD32Sfloat)
        .setName("PhongPipeline")
        .create(mVulkanContext->getDevice());
}

void GameLayer::loadTestTexture() noexcept
{
    using namespace ptvc::rhi;

    // Format used for the example
    constexpr auto textureFormat = vk::Format::eR8G8B8A8Srgb;

    // 1. - Load texture from disk.
    const auto texture = ptvc::io::loadTextureFromFile("assets/textures/example.png");

    const auto device = mVulkanContext->getDevice();

    auto useHostImageCopy = device->isHostImageCopyAvailable();

    // If using HostImageCopy, we must check if the image format supports host transfer.
    if (useHostImageCopy)
    {
        auto formatProperties3 = vk::FormatProperties3();
        auto formatProperties2 = vk::FormatProperties2();
        formatProperties2.pNext = &formatProperties3;

        device->getPhysicalDevice().getFormatProperties2(textureFormat, &formatProperties2);

        if (!(formatProperties3.optimalTilingFeatures & vk::FormatFeatureFlagBits2::eHostImageTransfer))
        {
            useHostImageCopy = false;
        }
    }

    // NSight Graphics (version 2025.5.0 and before) crashes when HostImageCopy is used, so avoid that for now.
    if (useHostImageCopy)
    {
        for (const auto& tool : device->getPhysicalDevice().getToolProperties())
        {
            if (std::string(tool.name.data()) == "NVIDIA Nsight Graphics")
            {
                useHostImageCopy = false;
                spdlog::warn("Not using ImageHostCopy since the app is running with NVIDIA NSight Graphics. (It doesn't support the extension yet and causes a crash.)");
            }
        }
    }

    // 2.  - Upload texture data to GPU
    // 2.a - Traditional way (Copy data from a staging buffer to an image)
    if (!useHostImageCopy)
    {
        const auto requiredSize = static_cast<vk::DeviceSize>(texture.width * texture.height * texture.channels);

        // Create staging buffer
        const auto stagingBuffer = Buffer::create({
            .size        = requiredSize,
            .hostVisible = true,
            .device      = device,
        }).value();

        // Set data
        stagingBuffer->setData(texture.pixels, requiredSize, 0);

        // Create Image
        mTestTexture = Image::create({
            .extent     = { static_cast<uint32_t>(texture.width), static_cast<uint32_t>(texture.height) },
            .usageFlags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            .format     = textureFormat,
            .mipmapping = true,
            .samples    = vk::SampleCountFlagBits::e1,
            .label      = fmt::format("Texture[{}]", texture.fileName),
            .device     = device,
        }).value();

        // Copy from buffer to image and generate mipmaps
        mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& commandBuffer) -> void
        {
            /* Barrier | Undefined to TransferDst */ {
                const auto imageTransferDst = vk::ImageMemoryBarrier2()
                    .setImage(mTestTexture->getHandle())
                    .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
                    .setOldLayout(vk::ImageLayout::eUndefined)
                    .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                    .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
                    .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                    .setDstStageMask(vk::PipelineStageFlagBits2::eAllTransfer);
                 const auto dependencyInfo = vk::DependencyInfo()
                    .setImageMemoryBarriers(imageTransferDst);
                commandBuffer.pipelineBarrier2(dependencyInfo);
            }

            // copyBufferToImage
            const auto [w, h] = mTestTexture->getProperties().extent;
            const auto copyRegion = vk::BufferImageCopy2()
                .setBufferOffset(0)
                .setBufferRowLength(0)
                .setBufferImageHeight(0)
                .setImageSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                .setImageOffset({0, 0, 0})
                .setImageExtent({ w, h, 1 });
            const auto copyBufferToImageInfo = vk::CopyBufferToImageInfo2()
                .setDstImage(mTestTexture->getHandle())
                .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
                .setSrcBuffer(stagingBuffer->getHandle())
                .setRegions(copyRegion);

            commandBuffer.copyBufferToImage2(copyBufferToImageInfo);

            // ! This leaves the image in ShaderReadOnly layout
            mTestTexture->generateMipmaps(commandBuffer,
                ImageState { vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eAllTransfer },
                ImageState { vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderSampledRead, vk::PipelineStageFlagBits2::eAllGraphics });
        });
    }
    // 2.b - With VK_EXT_host_image_copy available
    // With the extension the layout transition and memory to image copy requires no command buffer.
    else
    {
        const auto w = static_cast<uint32_t>(texture.width);
        const auto h = static_cast<uint32_t>(texture.height);

        // Create Image !!! with HostTransfer usage bit !!!
        // =================================================
        mTestTexture = Image::create({
            .extent     = { w, h },
            .usageFlags = vk::ImageUsageFlagBits::eHostTransfer | vk::ImageUsageFlagBits::eSampled,
            .format     = textureFormat,
            .mipmapping = true,
            .samples    = vk::SampleCountFlagBits::e1,
            .label      = fmt::format("Texture[{}]", texture.fileName),
            .device     = device,
        }).value();

        // Transition image layout
        // =============================
        const auto hostImageLayoutTransitionInfo = vk::HostImageLayoutTransitionInfo()
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setImage(mTestTexture->getHandle())
            .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        device->getHandle().transitionImageLayout(hostImageLayoutTransitionInfo);

        // Copy from memory to Image
        // =============================
        const auto memoryToImageCopy = vk::MemoryToImageCopy()
            .setImageExtent({ w, h, 1 })
            .setImageOffset({ 0, 0, 0 })
            .setImageSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
            .setPHostPointer(texture.pixels);

        const auto copyMemoryToImageInfo = vk::CopyMemoryToImageInfo()
            .setDstImage(mTestTexture->getHandle())
            .setDstImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setRegions(memoryToImageCopy);

        device->getHandle().copyMemoryToImage(copyMemoryToImageInfo);

        // Generate mipmaps
        // =============================
        mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& commandBuffer) -> void
        {
            // ! This leaves the image in ShaderReadOnly layout
            mTestTexture->generateMipmaps(commandBuffer,
                ImageState { vk::ImageLayout::eShaderReadOnlyOptimal },
                ImageState { vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderSampledRead, vk::PipelineStageFlagBits2::eAllGraphics });
        });
    }

    // 3. Create Sampler
    // =============================
    #pragma region
    constexpr auto samplerCreateInfo = vk::SamplerCreateInfo()
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        .setAnisotropyEnable(true)
        .setMaxAnisotropy(8.0)
        .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
        .setUnnormalizedCoordinates(false)
        .setCompareEnable(false)
        .setCompareOp(vk::CompareOp::eAlways)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setMipLodBias(0.0f)
        .setMinLod(0.0f)
        .setMaxLod(0.0f);

    mSampler = device->getHandle().createSampler(samplerCreateInfo);
    #pragma endregion

    // 4. Create Descriptor
    // =============================
    #pragma region
    mTextureDescriptor = Descriptor::create({
        .bindings = {{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, }},
        .setCount = 1,
        .label = "TextureDescriptor",
        .device = device,
    }).value();

    // Write bindings
    const auto imageInfo = vk::DescriptorImageInfo()
        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setImageView(mTestTexture->getImageView())
        .setSampler(mSampler);
    const auto write = vk::WriteDescriptorSet()
        .setImageInfo(imageInfo)
        .setDstBinding(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDstSet(mTextureDescriptor->getSet(0));
    device->getHandle()
        .updateDescriptorSets(write, {});
    #pragma endregion

    // 5. Free texture data memory when done.
    texture.free();

    spdlog::debug("Loaded texture: {} [HostImageCopy={}]",
        texture.fileName, STYLE_YN(useHostImageCopy));
}
