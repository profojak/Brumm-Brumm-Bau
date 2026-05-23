#pragma once

#include <SDL3/SDL_events.h>

namespace ptvc
{
    namespace rhi
    {
        struct Frame;
    }

    class ILayer
    {
    public:
        virtual ~ILayer() = default;

        virtual void onEvent(const SDL_Event& event) noexcept = 0;

        virtual void onUpdate(float deltaTime) noexcept = 0;

        virtual void onRender(const rhi::Frame& frame) noexcept = 0;

        virtual void onDrawUI() noexcept {}
    };
}
