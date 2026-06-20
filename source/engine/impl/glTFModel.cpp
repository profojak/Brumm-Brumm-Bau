#include "GltfModel.hpp"

#include <spdlog/spdlog.h>

namespace ptvc::gltf {
GltfModel::~GltfModel()
{
  release();
}

GltfModel::GltfModel(GltfModel&& other) noexcept
    : m_data(other.m_data)
    , m_path(std::move(other.m_path))
{
  other.m_data = nullptr;
}

GltfModel& GltfModel::operator=(GltfModel&& other) noexcept
{
  if(this != &other)
  {
    release();
    m_data       = other.m_data;
    m_path       = std::move(other.m_path);
    other.m_data = nullptr;
  }
  return *this;
}

void GltfModel::release() noexcept
{
  if(m_data != nullptr)
  {
    cgltf_free(m_data);
    m_data = nullptr;
  }
}

bool GltfModel::loadFromFile(const std::filesystem::path& filePath) noexcept
{
  release();
  m_path = filePath;

  cgltf_options options{};
  options.type = cgltf_file_type_invalid;

  const std::string pathStr = filePath.string();
  cgltf_result      result  = cgltf_parse_file(&options, pathStr.c_str(), &m_data);
  if(result != cgltf_result_success)
  {
    spdlog::error("cgltf: failed to parse '{}': error {}", pathStr, static_cast<int>(result));
    m_data = nullptr;
    return false;
  }

  result = cgltf_load_buffers(&options, m_data, pathStr.c_str());
  if(result != cgltf_result_success)
  {
    spdlog::error("cgltf: failed to load buffers for '{}': error {}", pathStr, static_cast<int>(result));
    cgltf_free(m_data);
    m_data = nullptr;
    return false;
  }

  return true;
}
}  // namespace ptvc::gltf
