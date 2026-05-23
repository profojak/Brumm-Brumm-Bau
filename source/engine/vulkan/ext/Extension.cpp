#include "Extension.hpp"

namespace ptvc::rhi {
Extension::Extension(const char* extensionName, const FeatureRequest requested)
    : mRequest(requested)
    , mExtensionName(extensionName)
{
}

Extension::Extension(const char* extensionName, const FeatureRequest requested, const std::function<void()>& structInitFn)
    : mRequest(requested)
    , mExtensionName(extensionName)
    , mStructInitFn(structInitFn)
{
}

void Extension::preCreateDevice(vk::DeviceCreateInfo& deviceCreateInfo) const noexcept
{
  if(mFeatureStructPtr != nullptr && isActive())
  {
    mStructInitFn();

    auto* featureStruct  = static_cast<VulkanAnyStruct*>(mFeatureStructPtr);
    featureStruct->pNext = deviceCreateInfo.pNext;
    deviceCreateInfo.setPNext(mFeatureStructPtr);
  }
}

void Extension::setSupported(const bool value) noexcept
{
  mSupported = value;
}

bool Extension::isActive() const noexcept
{
  return mSupported && mRequest != FeatureRequest::Disabled;
}

const char* Extension::getName() const noexcept
{
  return mExtensionName;
}

FeatureRequest Extension::getRequestType() const noexcept
{
  return mRequest;
}

std::string Extension::toString(const size_t width) const noexcept
{
  return fmt::format("{:<{}} [Supported={:<3} | {} | {:>8}]", mExtensionName, width == 0 ? std::strlen(mExtensionName) : width,
                     STYLE_YN(mSupported), styled(rhi::toString(mRequest), fg(getColor(mRequest))),
                     STYLE_BOOL(mSupported && mRequest != FeatureRequest::Disabled, "Active", "Inactive"));
}
}  // namespace ptvc::rhi
