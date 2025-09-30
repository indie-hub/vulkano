#include <vulkano/app/material_texture_cache.hpp>

#include <vulkano/app/material_gpu.hpp>
#include <vulkano/app/texture_loader.hpp>
#include <vulkano/app/texture_image.hpp>
#include <vulkano/app/vulkan_context.hpp>

#include <glm/vec4.hpp>

#include <cmath>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace vulkano::app {
namespace {
[[nodiscard]] glm::vec4 to_vec4(const glm::vec3& v, float a = 1.0F) noexcept {
    return glm::vec4 {v, a};
}

[[nodiscard]] std::string canonical_path_key(const std::string& path) {
    try {
        if (path.empty()) {
            return {};
        }
        const std::filesystem::path original {path};
        const std::filesystem::path canonical = std::filesystem::weakly_canonical(original);
        return canonical.string();
    } catch (...) {
        return path;
    }
}
} // namespace

MaterialTextureCache::MaterialTextureCache(const VulkanContext& context)
    : m_context {context} {
}

MaterialTextureCache::~MaterialTextureCache() noexcept = default;

void MaterialTextureCache::rebuild(const scene::MaterialRegistry& registry) {
    m_lookup.clear();
    m_textures.clear();
    m_handles.clear();

    const std::size_t materialCount = registry.size();
    m_handles.reserve(materialCount);

    for (std::size_t index {0U}; index < materialCount; ++index) {
        const scene::Material& material = registry.material(scene::MaterialId {static_cast<std::uint32_t>(index)});
        scene::MaterialTextureHandles handles {};

        if (!material.textures.baseColorPath.empty()) {
            const std::string key = canonical_path_key(material.textures.baseColorPath);
            try {
                const TextureData data = load_texture_from_file(key, TextureColorSpace::sRGB);
                handles.baseColor = ensure_texture(key, data);
            } catch (const std::exception&) {
                handles.baseColor = ensure_color_texture(to_vec4(material.properties.baseColor), TextureColorSpace::sRGB,
                    "fallback-base");
            }
        } else {
            handles.baseColor = ensure_color_texture(to_vec4(material.properties.baseColor), TextureColorSpace::sRGB,
                "base");
        }

        if (!material.textures.normalPath.empty()) {
            const std::string key = canonical_path_key(material.textures.normalPath);
            try {
                const TextureData data = load_texture_from_file(key, TextureColorSpace::Linear);
                handles.normal = ensure_texture(key, data);
            } catch (const std::exception&) {
                handles.normal = ensure_color_texture(glm::vec4 {0.5F, 0.5F, 1.0F, 1.0F}, TextureColorSpace::Linear,
                    "fallback-normal");
            }
        } else {
            handles.normal = ensure_color_texture(glm::vec4 {0.5F, 0.5F, 1.0F, 1.0F}, TextureColorSpace::Linear,
                "normal");
        }

        if (!material.textures.metallicRoughnessPath.empty()) {
            const std::string key = canonical_path_key(material.textures.metallicRoughnessPath);
            try {
                const TextureData data = load_texture_from_file(key, TextureColorSpace::Linear);
                handles.metallicRoughness = ensure_texture(key, data);
            } catch (const std::exception&) {
                const float metallic = material.properties.metallic;
                const float roughness = material.properties.roughness;
                handles.metallicRoughness = ensure_color_texture(glm::vec4 {metallic, roughness, 0.0F, 1.0F},
                    TextureColorSpace::Linear, "fallback-metalrough");
            }
        } else {
            const float metallic = material.properties.metallic;
            const float roughness = material.properties.roughness;
            handles.metallicRoughness = ensure_color_texture(glm::vec4 {metallic, roughness, 0.0F, 1.0F},
                TextureColorSpace::Linear, "metalrough");
        }

        if (!material.textures.ambientOcclusionPath.empty()) {
            const std::string key = canonical_path_key(material.textures.ambientOcclusionPath);
            try {
                const TextureData data = load_texture_from_file(key, TextureColorSpace::Linear);
                handles.ambientOcclusion = ensure_texture(key, data);
            } catch (const std::exception&) {
                handles.ambientOcclusion = ensure_color_texture(glm::vec4 {1.0F, 1.0F, 1.0F, 1.0F}, TextureColorSpace::Linear,
                    "fallback-ao");
            }
        } else {
            handles.ambientOcclusion = ensure_color_texture(glm::vec4 {1.0F, 1.0F, 1.0F, 1.0F}, TextureColorSpace::Linear,
                "ao");
        }

        m_handles.push_back(handles);
    }
}

const std::vector<scene::MaterialTextureHandles>& MaterialTextureCache::handles() const noexcept {
    return m_handles;
}

const std::vector<VkDescriptorImageInfo>& MaterialTextureCache::descriptor_infos() const noexcept {
    m_descriptorInfos.clear();
    m_descriptorInfos.reserve(m_textures.size());
    for (const TextureSlot& slot : m_textures) {
        m_descriptorInfos.push_back(slot.descriptor);
    }
    return m_descriptorInfos;
}

std::uint32_t MaterialTextureCache::ensure_texture(const std::string& key, const TextureData& data) {
    const std::string cacheKey = key;
    const auto it = m_lookup.find(cacheKey);
    if (it != m_lookup.end()) {
        return it->second;
    }

    TextureSlot slot {};
    slot.image = TextureImage::create(m_context, data);
    slot.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    slot.descriptor.imageView = slot.image.view();
    slot.descriptor.sampler = slot.image.sampler();
    slot.key = cacheKey;
    m_textures.push_back(std::move(slot));
    const std::uint32_t index {static_cast<std::uint32_t>(m_textures.size() - 1U)};
    m_lookup.emplace(cacheKey, index);
    return index;
}

std::uint32_t MaterialTextureCache::ensure_color_texture(
    const glm::vec4& color, TextureColorSpace space, const std::string& prefix) {
    const std::string key = make_color_key(prefix, color, space);
    const auto it = m_lookup.find(key);
    if (it != m_lookup.end()) {
        return it->second;
    }

    TextureData data = make_solid_texture(color, space);
    return ensure_texture(key, data);
}
std::string MaterialTextureCache::make_color_key(
    const std::string& prefix, const glm::vec4& color, TextureColorSpace space) {
    std::stringstream stream;
    stream << prefix << ':' << static_cast<int>(std::round(color.r * 255.0F)) << ':'
           << static_cast<int>(std::round(color.g * 255.0F)) << ':' << static_cast<int>(std::round(color.b * 255.0F))
           << ':' << static_cast<int>(std::round(color.a * 255.0F)) << ':' << static_cast<std::uint32_t>(space);
    return stream.str();
}
} // namespace vulkano::app
