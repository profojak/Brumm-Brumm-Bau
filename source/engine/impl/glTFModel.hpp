#pragma once

#include <cgltf.h>

#include <cstddef>
#include <filesystem>
#include <span>

namespace ptvc::gltf {
/**
 * Tiny glTF model wrapper.
 */
class GltfModel
{
public:
  GltfModel() = default;
  ~GltfModel();
  GltfModel(const GltfModel&)            = delete;
  GltfModel& operator=(const GltfModel&) = delete;
  GltfModel(GltfModel&& other) noexcept;
  GltfModel& operator=(GltfModel&& other) noexcept;

  /// Load a glTF model from the given file path.
  [[nodiscard]] bool loadFromFile(const std::filesystem::path& filePath) noexcept;

  [[nodiscard]] bool                         valid() const noexcept { return m_data != nullptr; }
  [[nodiscard]] const cgltf_data*            data() const noexcept { return m_data; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }
  [[nodiscard]] std::span<const std::byte>   buffer(const cgltf_buffer* buffer) const noexcept
  {
    if(buffer == nullptr || buffer->data == nullptr)
      return {};
    return {reinterpret_cast<const std::byte*>(buffer->data), buffer->size};
  }

private:
  void release() noexcept;

  cgltf_data*           m_data{};
  std::filesystem::path m_path;
};
}  // namespace ptvc::gltf
