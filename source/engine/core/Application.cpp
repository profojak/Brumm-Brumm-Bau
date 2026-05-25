#include "Application.hpp"

#include <chrono>
#include <imgui.h>

#include "render/DebugLayer.hpp"

namespace ptvc {
static Application* sApplication = nullptr;

Application::Application(ApplicationOptions options)
{
  sApplication = this;

  mWindow        = makeShared<wsi::SDLWindow>(options.windowOptions);
  mVulkanContext = rhi::VulkanContext::create({
      .window  = mWindow,
      .options = options.vulkanOptions,
  });

  init_UserInterface();
}

Application::~Application()
{
  sApplication = nullptr;
}

void Application::run()
{
  registerLayer<DebugLayer>();
  mDebugLayer = getLayer<DebugLayer>();

  mRunning      = true;
  auto lastTime = std::chrono::high_resolution_clock::now();

  while(mRunning)
  {
    const auto                         currentTime = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<float> delta       = currentTime - lastTime;
    const float                        deltaTime   = std::clamp(delta.count(), 0.001f, 0.1f);
    ;
    lastTime = currentTime;

    // Handle Events
    // =============================
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
      switch(event.type)
      {
        // Exit on [ESC]
        case SDL_EVENT_KEY_DOWN: {
          const SDL_KeyboardEvent& keyboardEvent = event.key;
          if(keyboardEvent.key == SDLK_ESCAPE)
          {
            mRunning = false;
          }
          if(keyboardEvent.scancode == SDL_SCANCODE_I)
          {
            mShowImGui = !mShowImGui;
            spdlog::debug("Toggled ImGUI: {}", STYLE_BOOL(mShowImGui, "ON", "OFF"));
          }
          break;
        }
        case SDL_EVENT_QUIT: {
          mRunning = false;
          break;
        }
        // Pause application while minimized
        case SDL_EVENT_WINDOW_MINIMIZED: {
          mMinimized = true;
          spdlog::info("Paused application loop.");
          break;
        }
        // Handle rebuilding of the swapchain
        case SDL_EVENT_WINDOW_RESTORED: {
          mVulkanContext->rebuildSwapchain();
          mMinimized = false;
          spdlog::info("Resuming application loop.");
          break;
        }
        default: {
        }
      }

      // Let ImGui process events
      mImGui->onEvent(event);
      // If ImGui didn't want to consume any input continue with Layer handlers.
      if(!mImGui->wantCaptureInput())
      {
        for(const auto& layer : std::views::reverse(mLayers))
        {
          layer->onEvent(event);
        }
        mScene->onEvent(event);
      }
    }

    // If the Application is minimized, do not do updates or any rendering
    if(mMinimized)
    {
      continue;
    }

    // Begin Frame
    // =============================
    const auto beginFrameResult = mVulkanContext->beginFrame();
    if(!beginFrameResult.has_value())
    {
      exitWithError("Failed to begin new frame: {}", STYLE_ERROR(beginFrameResult.error()));
    }
    const auto frame = beginFrameResult.value();

    // Handle Updates
    // =============================
    for(const auto& layer : mLayers)
    {
      layer->onUpdate(deltaTime);
    }

    mScene->onUpdate(deltaTime, frame);

    // Rendering
    // =============================
    {
      if(mDebugLayer->isEnabled())
      {
        mDebugLayer->onRender(frame);
      }
      else
      {
        for(const auto& layer : mLayers)
        {
          layer->onRender(frame);
        }
      }

      // Render UI
      mImGui->render(frame, [&]() -> void {
        if(mShowImGui)
        {
          ImGui::Begin("PTVC Framework");
          {
            const ImGuiIO& io = ImGui::GetIO();

            ImGui::Text("GPU: %s", mVulkanContext->getDevice()->getName().c_str());
            ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);
            ImGui::Text("Position: %.2f, %.2f, %.2f", mScene->getCamera().getPosition().x,
                        mScene->getCamera().getPosition().y, mScene->getCamera().getPosition().z);
          }
          ImGui::End();

          for(const auto& layer : mLayers)
          {
            layer->onDrawUI();
          }
        }
      });

      /* Barrier: Swapchain Image to PresentSrc */ {
        const auto imageToPresentBarrier =
            mVulkanContext->getSwapchain()->getBarrier(frame.acquiredImageIndex,
                                                       {
                                                           .layout     = vk::ImageLayout::ePresentSrcKHR,
                                                           .accessMask = vk::AccessFlagBits2::eNone,
                                                           .stageMask  = vk::PipelineStageFlagBits2::eNone,
                                                       });
        const auto dependencyInfo =
            vk::DependencyInfo().setImageMemoryBarrierCount(1).setPImageMemoryBarriers(&imageToPresentBarrier);
        frame.commandBuffer.pipelineBarrier2(dependencyInfo);
      }
    }

    // Submit Frame and Present to Swapchain
    mVulkanContext->endFrame_submitAndPresent(frame);
  }
}

Scene* Application::getScene() const noexcept
{
  return mScene.get();
}

const SPtr<rhi::VulkanContext>& Application::getVulkanContext() const noexcept
{
  return mVulkanContext;
}

Application* Application::getApplication() noexcept
{
  assert(sApplication);
  return sApplication;
}

void Application::init_UserInterface() noexcept
{
  auto imGuiInitResult = ui::ImGuiIntegration::create({
      .window        = mWindow,
      .vulkanContext = mVulkanContext.get(),
  });
  if(!imGuiInitResult.has_value())
  {
    exitWithError("Failed to initialize ImGui: {}", STYLE_ERROR(imGuiInitResult.error()));
  }
  mImGui = std::move(imGuiInitResult.value());
}
}  // namespace ptvc
