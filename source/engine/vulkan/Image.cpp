#include "Image.hpp"

#include <format>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_format_traits.hpp>
#include "Device.hpp"

namespace ptvc::rhi
{
    namespace detail
    {
        // Get the aspect flags for a given format.
        [[nodiscard]] static vk::ImageAspectFlags getAspectFlags(const vk::Format format) noexcept
        {
            vk::ImageAspectFlags aspectFlags = {};

            if (vk::isColor(format))
            {
                aspectFlags = vk::ImageAspectFlagBits::eColor;
            }
            if (vk::hasDepthComponent(format))
            {
                aspectFlags |= vk::ImageAspectFlagBits::eDepth;
            }
            if (vk::hasStencilComponent(format))
            {
                aspectFlags |= vk::ImageAspectFlagBits::eStencil;
            }

            return aspectFlags;
        }

        // Get the number of mip levels for a given extent.
        [[nodiscard]] static uint32_t getMipLevels(const vk::Extent2D& extent) noexcept
        {
            return static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
        }

        // Define and compute the immutable properties an image based on the given parameters.
        [[nodiscard]] static ImageProperties makeImageProperties(const ImageCreateInfo& createInfo) noexcept
        {
            ImageProperties result = {
                .format      = createInfo.format,
                .extent      = createInfo.extent,
                .aspectFlags = getAspectFlags(createInfo.format),
                .levelCount  = (createInfo.mipmapping) ? getMipLevels(createInfo.extent) : 1,
                .samples     = createInfo.samples,
            };
            result.maxSubresourceRange = { result.aspectFlags, 0, result.levelCount, 0, 1 };
            result.maxSubresourceLayers = { result.aspectFlags, 0, 0, 1 };

            return result;
        }
    }

    Result<SPtr<Image>> Image::create(const ImageCreateInfo& createInfo) noexcept
    {
        if (!createInfo.device)
        {
            exitWithError("Failed to create Buffer: Device is null");
        }

        auto image = SPtr<Image>(std::move(new Image(createInfo)));
        if (const auto result = image->create_Image(createInfo); !result.has_value())
        {
            return std::unexpected(result.error());
        }
        return image;
    }

    void Image::generateMipmaps(const vk::CommandBuffer& commandBuffer, const std::optional<ImageState>& srcState,
        const std::optional<ImageState>& dstState) const noexcept
    {
        /* Transition base mip level to TransferSrc */ {
            const auto barrier_Start = vk::ImageMemoryBarrier2()
                .setImage(mImage)
                .setSubresourceRange({ mProperties.aspectFlags, 0, 1, 0, 1 })
                .setOldLayout(srcState.has_value() ? srcState->layout : vk::ImageLayout::eUndefined)
                .setSrcAccessMask(srcState.has_value() ? srcState->accessMask : vk::AccessFlagBits2::eNone)
                .setSrcStageMask(srcState.has_value() ? srcState->stageMask : vk::PipelineStageFlagBits2::eNone)
                .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setDstAccessMask(vk::AccessFlagBits2::eTransferRead)
                .setDstStageMask(vk::PipelineStageFlagBits2::eBlit);
            const auto dependencyInfo = vk::DependencyInfo().setImageMemoryBarriers(barrier_Start);
            commandBuffer.pipelineBarrier2(dependencyInfo);
        }

        // Blit mip levels i-1 to i
        const auto w = static_cast<int32_t>(mProperties.extent.width);
        const auto h = static_cast<int32_t>(mProperties.extent.height);
        for (uint32_t i = 1; i < mProperties.levelCount; i++)
        {
            // current mip level to transfer dst
            const auto barrier_Undef_TDst = vk::ImageMemoryBarrier2()
                .setImage(mImage)
                .setSubresourceRange({ mProperties.aspectFlags, i, 1, 0, 1 })
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
                .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                .setDstStageMask(vk::PipelineStageFlagBits2::eBlit);
            const auto dependencyInfo_Pre = vk::DependencyInfo().setImageMemoryBarriers(barrier_Undef_TDst);

            // Blit
            const auto imageBlit = vk::ImageBlit2()
                .setSrcSubresource({ mProperties.aspectFlags, static_cast<uint32_t>(i - 1), 0, 1})
                .setSrcOffsets({
                    vk::Offset3D { 0, 0, 0 },
                    vk::Offset3D { w >> (i - 1), h >> (i - 1), 1 },
                })
                .setDstSubresource({ mProperties.aspectFlags, static_cast<uint32_t>(i), 0, 1})
                .setDstOffsets({
                    vk::Offset3D { 0, 0, 0 },
                    vk::Offset3D { w >> i, h >> i,  1 },
                });

            const auto blitImageInfo = vk::BlitImageInfo2()
                .setSrcImage(mImage)
                .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setDstImage(mImage)
                .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
                .setFilter(vk::Filter::eNearest)
                .setRegions(imageBlit);

            // current mip level to transfer src
            const auto barrier_TDst_TSrc = vk::ImageMemoryBarrier2()
                .setImage(mImage)
                .setSubresourceRange({ mProperties.aspectFlags, i, 1, 0, 1 })
                .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits2::eTransferRead)
                .setSrcStageMask(vk::PipelineStageFlagBits2::eBlit)
                .setDstStageMask(vk::PipelineStageFlagBits2::eBlit);
            const auto dependencyInfo_Post = vk::DependencyInfo().setImageMemoryBarriers(barrier_TDst_TSrc);

            commandBuffer.pipelineBarrier2(dependencyInfo_Pre);
            commandBuffer.blitImage2(blitImageInfo);
            commandBuffer.pipelineBarrier2(dependencyInfo_Post);
        }

        /* Transition all mip levels to dstLayout layout */ {
            const auto barrier_End = vk::ImageMemoryBarrier2()
                .setImage(mImage)
                .setSubresourceRange({ mProperties.aspectFlags, 0, mProperties.levelCount, 0, 1 })
                .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setSrcAccessMask(vk::AccessFlagBits2::eTransferRead)
                .setSrcStageMask(vk::PipelineStageFlagBits2::eBlit)
                .setNewLayout(dstState.has_value() ? dstState->layout : vk::ImageLayout::eShaderReadOnlyOptimal)
                .setDstAccessMask(dstState.has_value() ? dstState->accessMask : vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite)
                .setDstStageMask(dstState.has_value() ? dstState->stageMask : vk::PipelineStageFlagBits2::eAllGraphics);
            const auto dependencyInfo_End = vk::DependencyInfo().setImageMemoryBarriers(barrier_End);
            commandBuffer.pipelineBarrier2(dependencyInfo_End);
        }
    }

    vk::Image Image::getHandle() const noexcept
    {
        return mImage;
    }

    vk::ImageView Image::getImageView() const noexcept
    {
        return mImageView;
    }

    const ImageProperties& Image::getProperties() const noexcept
    {
        return mProperties;
    }

    Image::Image(const ImageCreateInfo& createInfo)
    : mProperties(detail::makeImageProperties(createInfo))
    , mLabel(createInfo.label)
    , mDevice(createInfo.device)
    {
    }

    Result<void> Image::create_Image(const ImageCreateInfo& createInfo) noexcept
    {
        const auto usageFlags = createInfo.usageFlags
            | vk::ImageUsageFlagBits::eTransferDst
            | vk::ImageUsageFlagBits::eTransferSrc;

        auto imageCreateInfo = vk::ImageCreateInfo()
            .setFormat(mProperties.format)
            .setExtent({ mProperties.extent.width, mProperties.extent.height, 1 })
            .setSamples(mProperties.samples)
            .setUsage(usageFlags)
            .setTiling(vk::ImageTiling::eOptimal)
            .setArrayLayers(1)
            .setMipLevels(mProperties.levelCount)
            .setImageType(vk::ImageType::e2D)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setInitialLayout(vk::ImageLayout::eUndefined);

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        const auto result = vmaCreateImage(mDevice->getAllocator(), reinterpret_cast<VkImageCreateInfo*>(&imageCreateInfo),
            &allocationInfo, reinterpret_cast<VkImage*>(&mImage), &mAllocation, &mAllocationInfo);
        if (result != VK_SUCCESS)
        {
            return std::unexpected(string_VkResult(result));
        }

        mDevice->setLabel<vk::Image>({
            .name   = mLabel,
            .handle = mImage,
        });

        const auto viewCreateInfo = vk::ImageViewCreateInfo()
            .setViewType(vk::ImageViewType::e2D)
            .setImage(mImage)
            .setFormat(mProperties.format)
            .setSubresourceRange(mProperties.maxSubresourceRange);
        VK_CATCH(mImageView = mDevice->getHandle().createImageView(viewCreateInfo));

        mDevice->setLabel<vk::ImageView>({
            .name   = std::format("{}-View", createInfo.label),
            .handle = mImageView,
        });

        return {};
    }
}
