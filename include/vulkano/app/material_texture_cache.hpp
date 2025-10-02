#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec4.hpp>

#include <vulkan/vulkan.h>

#include <vulkano/app/texture_image.hpp>
#include <vulkano/scene/material.hpp>

namespace vulkano::app {
class VulkanContext;

class MaterialTextureCache final {
public:
    explicit MaterialTextureCache(const VulkanContext& context);
    ~MaterialTextureCache() noexcept;

    MaterialTextureCache(const MaterialTextureCache&) = delete;
    MaterialTextureCache& operator=(const MaterialTextureCache&) = delete;
    MaterialTextureCache(MaterialTextureCache&&) noexcept = delete;
    MaterialTextureCache& operator=(MaterialTextureCache&&) noexcept = delete;

    void rebuild(const scene::MaterialRegistry& registry,
        const std::unordered_map<std::string, TextureData>* embeddedTextures = nullptr);

    [[nodiscard]] const std::vector<scene::MaterialTextureHandles>& handles() const noexcept;
    [[nodiscard]] const std::vector<VkDescriptorImageInfo>& descriptor_infos() const noexcept;

private:
    std::uint32_t ensure_texture(const std::string& key, const TextureData& textureData);
    std::uint32_t ensure_color_texture(const glm::vec4& color, TextureColorSpace space, const std::string& prefix);
    static std::string make_color_key(const std::string& prefix, const glm::vec4& color, TextureColorSpace space);

    const VulkanContext& m_context;

    struct TextureSlot final {
        TextureImage image;
        VkDescriptorImageInfo descriptor {};
        std::string key {};
    };

    std::vector<TextureSlot> m_textures;
    std::unordered_map<std::string, std::uint32_t> m_lookup;
    std::unordered_map<std::string, TextureData> m_embedded;
    std::vector<scene::MaterialTextureHandles> m_handles;
    mutable std::vector<VkDescriptorImageInfo> m_descriptorInfos;
};
} // namespace vulkano::app
