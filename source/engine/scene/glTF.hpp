#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Geometry.hpp"

namespace ptvc {

struct glTF_Options
{
  std::vector<std::string> includeNodes;
  std::vector<std::string> excludeNodes;
};

class glTF : public Geometry
{
public:
  glTF(const std::filesystem::path& path, const glTF_Options options = {});

  [[nodiscard]] glm::mat4 nodeWorldMatrix(const std::string& name) const noexcept;

private:
  std::unordered_map<std::string, glm::mat4> mNodeWorld;
};

}  // namespace ptvc
