#pragma once

#include <memory>

template <class T>
using UPtr = std::unique_ptr<T>;

template <class T, class... Args>
constexpr UPtr<T> makeUnique(Args&&... args)
{
  return std::make_unique<T>(std::forward<Args>(args)...);
}

template <class T>
using SPtr = std::shared_ptr<T>;

template <class T, class... Args>
constexpr SPtr<T> makeShared(Args&&... args)
{
  return std::make_shared<T>(std::forward<Args>(args)...);
}
