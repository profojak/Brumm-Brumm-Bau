#pragma once

#define STYLE_ERROR(MSG)    fmt::styled(MSG, fmt::fg(fmt::color::pale_violet_red))
#define STYLE_WARNING(MSG)  fmt::styled(MSG, fmt::fg(fmt::color::light_yellow))

#define STYLE_BOOL(VALUE, T, F) fmt::styled(VALUE ? T : F, fmt::fg(VALUE ? fmt::color::lawn_green : fmt::color::pale_violet_red))
#define STYLE_TF(VALUE)         STYLE_BOOL(VALUE, "true", "false")
#define STYLE_YN(VALUE)         STYLE_BOOL(VALUE, "Yes", "No")

#define DISABLE_COPY(T)                 \
    T(const T&) = delete;               \
    T& operator=(const T&) = delete;    \
    T(const T&&) = delete;              \
    T& operator=(const T&&) = delete;

#include <spdlog/spdlog.h>

/**
 * Print the given error message and terminate the application.\n
 * When debugging it also triggers a breakpoint.
 */
template <typename... Args>
[[noreturn]] void exitWithError(spdlog::format_string_t<Args...> fmt, Args &&...args)
{
    spdlog::critical(fmt, std::forward<Args>(args)...);

    #if defined(__x86_64__) || defined(_M_X64)
    __debugbreak();
    #else
    // Breakpoint for arm platforms
    asm("brk #0xF000");
    #endif

    exit(EXIT_FAILURE);
}

template <class Integral>
constexpr Integral alignUp(Integral x, size_t a) noexcept
{
    return Integral((x + (Integral(a) - 1)) & ~Integral(a - 1));
}

#include "platform.hpp"
#include "ptr.hpp"
#include "ranges.hpp"
#include "result.hpp"
#include "size.hpp"

#define exitOnError(R) if (!R.has_value()) { exitWithError("{}", STYLE_ERROR(R.error())); }

#define result_moveOrExit(Result, Storage)  \
    exitOnError(Result);                    \
    Storage = std::move(Result.value());
