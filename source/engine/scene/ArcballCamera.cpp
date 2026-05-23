#include "ArcballCamera.hpp"

namespace ptvc
{
    ArcballCamera::ArcballCamera(const float aspect, const float fov, const float near, const float far)
    : mMouseX(0.0f)
    , mMouseY(0.0f)
    , mYaw(0.0f)
    , mPitch(0.0f)
    , mStrafe(glm::vec3(0.0f))
    , mViewMatrix(glm::mat4(1))
    {
        mProjMatrix = glm::perspective(glm::radians(fov), aspect, near, far);
    }

    void ArcballCamera::setYaw(const float yaw) noexcept
    {
        mYaw = yaw;
    }

    void ArcballCamera::setPitch(const float pitch) noexcept
    {
        mPitch = pitch;
    }

    CameraData ArcballCamera::getCameraData(const float aspect) noexcept
    {
        mProjMatrix = glm::perspective(glm::radians(mFOV), aspect, mNear, mFar);
        mProjMatrix[1][1] *= -1.0f;

        return {
            .view = mViewMatrix,
            .proj = mProjMatrix,
            .viewInverse = glm::inverse(mViewMatrix),
            .projInverse = glm::inverse(mProjMatrix),
            .eye = { mPosition.x, mPosition.y, mPosition.z, 1.0f },
            .nearPlane = mNear,
            .farPlane = mFar,
        };
    }

    void ArcballCamera::onEvent(const SDL_Event& event) noexcept
    {
        static bool isDragging = false;
        static bool isStrafing = false;

        switch (event.type)
        {
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    isDragging = true;
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    isStrafing = true;
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    isDragging = false;
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    isStrafing = false;
                }
                break;
            }
            case SDL_EVENT_MOUSE_MOTION: {
                update(event.motion.x, event.motion.y, isDragging, isStrafing);
                break;
            }
            case SDL_EVENT_MOUSE_WHEEL: {
                mDistance *= (event.wheel.y > 0) ? 0.9f : 1.1f;
                mDistance = glm::clamp(mDistance, 0.1f, 500.0f);
                update(mMouseX, mMouseY, false, false);
                break;
            }
            default: {
                break;
            }
        }
    }

    void ArcballCamera::update(const float x, const float y, const bool isDragging,
        const bool isStrafing) noexcept
    {
        const     float dx    = x - mMouseX;
        const     float dy    = y - mMouseY;
        constexpr float speed = 0.005f;
        glm::vec3       pos;

        if (isDragging)
        {
            mYaw   -= dx * speed;
            mPitch -= dy * speed;

            mPitch = glm::min(mPitch, glm::pi<float>() * 0.5f - 0.01f);
            mPitch = glm::max(mPitch, -glm::pi<float>() * 0.5f + 0.01f);
        }

        pos.x     = mDistance * glm::cos(mPitch) * -glm::sin(mYaw);
        pos.y     = mDistance * glm::sin(mPitch);
        pos.z     = mDistance * glm::cos(mPitch) * glm::cos(mYaw);
        mPosition = pos;

        if (isStrafing) {
            glm::vec3 up    = glm::vec3(0, 1, 0);
            glm::vec3 right = glm::normalize(glm::cross(-pos, up));
            up              = glm::normalize(glm::cross(right, -pos));

            mStrafe += up * dy * speed + right * -dx * speed;
        }

        mPosition = mPosition + mStrafe;

        mViewMatrix = glm::inverse(glm::translate(glm::mat4(1.0f), mPosition))
            * glm::yawPitchRoll(-mYaw, -mPitch, 0.0f);

        mMouseX = x;
        mMouseY = y;
    }
}
