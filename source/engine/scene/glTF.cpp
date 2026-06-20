#include "glTF.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <cgltf.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

namespace ptvc {

namespace {

[[nodiscard]] glm::mat4 nodeMatrix(const cgltf_node* node) noexcept
{
  if(node->has_matrix)
    return glm::make_mat4(node->matrix);

  glm::mat4 T(1.0f);
  glm::mat4 R(1.0f);
  glm::mat4 S(1.0f);

  if(node->has_translation)
    T = glm::translate(glm::mat4(1.0f), glm::make_vec3(node->translation));
  if(node->has_rotation)
    R = glm::mat4_cast(glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]));
  if(node->has_scale)
    S = glm::scale(glm::mat4(1.0f), glm::make_vec3(node->scale));

  return T * R * S;
}

[[nodiscard]] bool listed(const std::vector<std::string>& names, const char* name) noexcept
{
  if(name == nullptr)
    return false;
  return std::find(names.begin(), names.end(), name) != names.end();
}

void bakeNode(const cgltf_node*                           node,
              const glm::mat4&                            parent,
              bool                                        included,
              bool                                        excluded,
              const glTF_Options&                         opts,
              std::vector<Vertex>&                        vertices,
              std::vector<std::uint32_t>&                 indices,
              std::unordered_map<std::string, glm::mat4>& nodeWorld) noexcept
{
  const glm::mat4 M = parent * nodeMatrix(node);
  if(node->name != nullptr)
    nodeWorld.emplace(node->name, M);

  const bool nodeExcluded = excluded || listed(opts.excludeNodes, node->name);
  if(nodeExcluded)
    return;

  const bool nodeIncluded = included || listed(opts.includeNodes, node->name);
  const bool emitMesh     = opts.includeNodes.empty() || nodeIncluded;

  if(emitMesh && node->mesh != nullptr)
  {
    const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(M)));

    for(cgltf_size p = 0; p < node->mesh->primitives_count; ++p)
    {
      const cgltf_primitive& prim = node->mesh->primitives[p];

      const cgltf_accessor* posAcc = nullptr;
      const cgltf_accessor* nrmAcc = nullptr;
      const cgltf_accessor* uvAcc  = nullptr;
      for(cgltf_size a = 0; a < prim.attributes_count; ++a)
      {
        const cgltf_attribute& attr = prim.attributes[a];
        if(attr.type == cgltf_attribute_type_position)
          posAcc = attr.data;
        else if(attr.type == cgltf_attribute_type_normal)
          nrmAcc = attr.data;
        else if(attr.type == cgltf_attribute_type_texcoord && attr.index == 0)
          uvAcc = attr.data;
      }
      if(posAcc == nullptr)
        continue;

      const std::uint32_t baseVertex = static_cast<std::uint32_t>(vertices.size());
      vertices.reserve(vertices.size() + posAcc->count);

      for(cgltf_size v = 0; v < posAcc->count; ++v)
      {
        Vertex    vtx{};
        glm::vec3 pos{};
        cgltf_accessor_read_float(posAcc, v, &pos[0], 3);
        vtx.position = glm::vec3(M * glm::vec4(pos, 1.0f));

        if(nrmAcc != nullptr)
        {
          glm::vec3 nrm{};
          cgltf_accessor_read_float(nrmAcc, v, &nrm[0], 3);
          vtx.normal = glm::normalize(normalMat * nrm);
        }

        if(uvAcc != nullptr)
        {
          glm::vec2 uv{};
          cgltf_accessor_read_float(uvAcc, v, &uv[0], 2);
          vtx.uv = uv;
        }

        vertices.push_back(vtx);
      }

      if(prim.indices != nullptr)
      {
        indices.reserve(indices.size() + prim.indices->count);
        for(cgltf_size i = 0; i < prim.indices->count; ++i)
          indices.push_back(baseVertex + static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, i)));
      }
      else
      {
        indices.reserve(indices.size() + posAcc->count);
        for(cgltf_size i = 0; i < posAcc->count; ++i)
          indices.push_back(baseVertex + static_cast<std::uint32_t>(i));
      }
    }
  }

  for(cgltf_size c = 0; c < node->children_count; ++c)
    bakeNode(node->children[c], M, nodeIncluded, nodeExcluded, opts, vertices, indices, nodeWorld);
}
}  // namespace

glTF::glTF(const std::filesystem::path& path, const glTF_Options options)
    : Geometry(path.stem().string())
{
  cgltf_data*       data    = nullptr;
  cgltf_options     opts    = {};
  const std::string pathStr = path.string();

  if(cgltf_parse_file(&opts, pathStr.c_str(), &data) != cgltf_result_success)
  {
    spdlog::error("glTF: failed to parse '{}'", pathStr);
    return;
  }

  if(cgltf_load_buffers(&opts, data, pathStr.c_str()) != cgltf_result_success)
  {
    spdlog::error("glTF: failed to load buffers for '{}'", pathStr);
    cgltf_free(data);
    return;
  }

  if(data->scenes_count == 0 || data->scene == nullptr)
  {
    cgltf_free(data);
    return;
  }

  for(cgltf_size n = 0; n < data->scene->nodes_count; ++n)
    bakeNode(data->scene->nodes[n], glm::mat4(1.0f), false, false, options, mVertices, mIndices, mNodeWorld);

  cgltf_free(data);
}

glm::mat4 glTF::nodeWorldMatrix(const std::string& name) const noexcept
{
  if(const auto it = mNodeWorld.find(name); it != mNodeWorld.end())
    return it->second;
  return glm::mat4(1.0f);
}
}  // namespace ptvc
