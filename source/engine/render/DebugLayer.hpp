#pragma once

#include <random>

#include "Image.hpp"
#include "core/Application.hpp"
#include "core/Layer.hpp"
#include "render/GraphicsPipeline.hpp"

namespace ptvc
{
    enum class DebugRenderMode : int32_t
    {
        eNone   = 0,    // Flat shaded render with a per-object constant color
        eNormal = 1,    // Visualize vertex normals
        eUV     = 2,    // Visualize vertex UVs
    };

    // Behaviour and rendering options
    struct DebugRenderConfig
    {
        DebugRenderMode mode      = DebugRenderMode::eNormal;
        bool            enableUI  = true;
        SDL_Keycode     toggleKey = SDL_SCANCODE_K;
    };

    // Push Constants
    struct DebugPipeline_PCS
    {
        glm::mat4 model;
        glm::vec4 solidColor;
        int32_t   renderMode;
        int32_t   objIndex;
    };

    /**
     * A layer that provides debug rendering features. Note that when enabled
     * all other layers "onRender()" calls are skipped.\n
     * Controls:
     * - Toggled with [K]
     * - UI buttons for rendering modes (ObjIndex, Normal, UV)
     */
    class DebugLayer : public ILayer
    {
    public:
        DebugLayer();

        void onEvent(const SDL_Event& event) noexcept override;

        void onUpdate(float deltaTime) noexcept override {}

        void onRender(const rhi::Frame& frame) noexcept override;

        void onDrawUI() noexcept override;

        [[nodiscard]] bool isEnabled() const noexcept;

    private:
        void createDebugPipeline() noexcept;

        bool                        mIsFirstRender = true;
        SPtr<rhi::Image>            mDepthBuffer;
        SPtr<rhi::Pipeline>         mPipeline;

        std::mt19937                mEngine;
        std::array<glm::vec4, 16>   mObjectColors = {};

        bool                        mEnabled = false;
        DebugRenderConfig           mConfig  = {};

        Scene*                      mScene;
        SPtr<rhi::VulkanContext>    mVulkanContext;
    };
}
