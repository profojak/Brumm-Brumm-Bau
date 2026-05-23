#pragma once

#include <ranges>
#include <sstream>
#include <string>
#include <version>

template <std::ranges::input_range _Range>
[[nodiscard]] std::string join(_Range&& range, const std::string& delimiter) noexcept
{
    std::stringstream sstr;
    bool isFirst = true;
    for (const auto& elem : range)
    {
        if (!isFirst)
        {
            sstr << delimiter;
        }
        isFirst = false;
        sstr << elem;
    }
    return sstr.str();
}

template <std::ranges::input_range _Range>
[[nodiscard]] bool contains(_Range&& range, const std::ranges::range_value_t<_Range>& val) noexcept
{
    return std::ranges::find(range, val) != std::ranges::end(range);
}

template <std::ranges::input_range _Range, class Pred>
requires std::predicate<Pred, std::ranges::range_reference_t<_Range>>
[[nodiscard]] bool containsIf(_Range&& range, Pred&& pred) noexcept
{
    return std::ranges::find_if(range, std::forward<Pred>(pred)) != std::ranges::end(range);
}

/**
 * std::views::enumerate (C++23) is missing from clang libc++
 * https://en.cppreference.com/w/cpp/compiler_support/23.html#cpp_lib_ranges_enumerate_202302L
 */
struct __enumerate
{
    #if !(__cpp_lib_ranges_enumerate >= 202302L)
    template <std::ranges::viewable_range _Range>
    constexpr auto operator()(_Range&& range) const noexcept {
        return std::ranges::zip_view(
            std::ranges::iota_view<std::size_t>(0),
            range);
    };
    #else
    template <std::ranges::viewable_range _Range>
    constexpr auto operator()(_Range&& range) const noexcept
    {
        return std::views::enumerate(range);
    }
    #endif
};

inline constexpr auto enumerate = __enumerate{};
