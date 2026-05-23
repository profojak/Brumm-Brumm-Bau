#pragma once

#include <expected>
#include <optional>
#include <string>

template <class T, class ErrorType = std::string>
using Result = std::expected<T, ErrorType>;

template <class T>
using Option = std::optional<T>;
