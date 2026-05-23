#include "RotatingGameObject.hpp"

#include <random>

static std::mt19937 sEngine = {};

RotatingGameObject::RotatingGameObject(const float rps, const glm::vec3& axis, const ptvc::GameObjectParams& params)
: GameObject(params)
, mRotationAxis(axis)
, mRadiansPerSec(rps)
{
    // Generate random color
    std::uniform_real_distribution unit(0.0f, 1.0f);
    mColor = { unit(sEngine), unit(sEngine), unit(sEngine), 1.0f };
}

void RotatingGameObject::onUpdate(const float dt, const ptvc::rhi::Frame& frame) noexcept
{
    const auto deltaRot = glm::angleAxis(mRadiansPerSec * dt, mRotationAxis);
    mTransform.rotation *= deltaRot;
}

void RotatingGameObject::onRender(const ptvc::rhi::Frame& frame) noexcept
{
    const ptvc::GPUGameObjectData pushConstant = {
        .model      = mTransform.getModel(),
        .solidColor = mColor,
        .materialProperties = { 0.1f, 0.7f, 0.2f, 10.0f },
        .showFresnel = 0,
        .useExampleTexture = mUseExampleTexture,
    };

    mPipeline->bind(frame, frame.commandBuffer);
    mPipeline->pushConstant(&pushConstant, frame.commandBuffer);

    mGeometry->draw(frame.commandBuffer);
}
