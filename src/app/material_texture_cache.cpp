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

void MaterialTextureCache::rebuild(const scene::MaterialRegistry& registry,
    const std::unordered_map<std::string, TextureData>* embeddedTextures) {
    m_lookup.clear();
    m_textures.clear();
    m_handles.clear();
    m_descriptorInfos.clear();

    if (embeddedTextures != nullptr) {
        m_embedded = *embeddedTextures;
    }

    const std::size_t materialCount = registry.size();
    m_handles.reserve(materialCount);

    const std::uint32_t baseFallbackIndex = ensure_color_texture(glm::vec4 {1.0F, 1.0F, 1.0F, 1.0F},
        TextureColorSpace::sRGB, "filler-base");
    const std::uint32_t normalFallback = ensure_color_texture(glm::vec4 {0.5F, 0.5F, 1.0F, 1.0F},
        TextureColorSpace::Linear, "filler-normal");
    const std::uint32_t surfaceFallback = ensure_color_texture(glm::vec4 {0.0F, 1.0F, 1.0F, 1.0F},
        TextureColorSpace::Linear, "filler-surface");

    for (std::size_t index {0U}; index < materialCount; ++index) {
        const scene::Material& material = registry.material(scene::MaterialId {static_cast<std::uint32_t>(index)});
        scene::MaterialTextureHandles handles {};

        auto load_texture_handle = [&](const std::string& rawPath, TextureColorSpace space,
                                       std::uint32_t fallback) -> std::uint32_t {
            if (!rawPath.empty()) {
                if (const auto embeddedIt = m_embedded.find(rawPath); embeddedIt != m_embedded.end()) {
                    TextureData data = embeddedIt->second;
                    data.colorSpace = space;
                    const std::string cacheKey = rawPath + (space == TextureColorSpace::sRGB ? "|srgb" : "|lin");
                    return ensure_texture(cacheKey, data);
                }
                const std::string key = canonical_path_key(rawPath);
                try {
                    const TextureData data = load_texture_from_file(key, space);
                    return ensure_texture(key, data);
                } catch (const std::exception&) {
                    return fallback;
                }
            }
            return fallback;
        };

        handles.baseColor = material.useBaseColorTexture
            ? load_texture_handle(material.textures.baseColorPath, TextureColorSpace::sRGB, baseFallbackIndex)
            : baseFallbackIndex;

        handles.normal = material.useNormalTexture
            ? load_texture_handle(material.textures.normalPath, TextureColorSpace::Linear, normalFallback)
            : normalFallback;

        if (material.useSurfacePropertiesTexture && !material.textures.surfacePropertiesPath.empty()) {
            const std::uint32_t handle = load_texture_handle(material.textures.surfacePropertiesPath,
                TextureColorSpace::Linear, surfaceFallback);
            handles.metallicRoughness = handle;
            handles.ambientOcclusion = handle;
        } else {
            handles.metallicRoughness = material.useMetallicRoughnessTexture
                ? load_texture_handle(material.textures.metallicRoughnessPath, TextureColorSpace::Linear, surfaceFallback)
                : surfaceFallback;

            handles.ambientOcclusion = material.useAmbientOcclusionTexture
                ? load_texture_handle(material.textures.ambientOcclusionPath, TextureColorSpace::Linear, surfaceFallback)
                : surfaceFallback;
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
