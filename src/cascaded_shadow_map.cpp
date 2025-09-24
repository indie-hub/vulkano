#include <vulkano/cascaded_shadow_map.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace {
    constexpr float epsilon {1e-6F};
    constexpr float defaultShadowDirectionY {-1.0F};
    constexpr float depthPaddingScale {2.0F};
    constexpr float extentPaddingFactor {1.05F};

    auto has_stencil_component(VkFormat format) -> bool {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    auto find_memory_type(
        VkPhysicalDevice physicalDevice,
        std::uint32_t typeFilter,
        VkMemoryPropertyFlags properties) -> std::uint32_t {
        VkPhysicalDeviceMemoryProperties memoryProperties {};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        for(std::uint32_t index {0U}; index < memoryProperties.memoryTypeCount; ++index) {
            const bool suitable = (typeFilter & (1U << index)) != 0U;
            const bool hasProperties = (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
            if(suitable && hasProperties) {
                return index;
            }
        }
        throw std::runtime_error {"Failed to find suitable memory type for cascaded shadow map"};
    }

    auto default_light_direction() -> glm::vec3 {
        return glm::normalize(glm::vec3 {0.0F, defaultShadowDirectionY, 1.0F});
    }

    auto compute_frustum_corners(const glm::mat4& inverseViewProjection)
        -> std::array<glm::vec3, 8U> {
        const std::array<glm::vec4, 8U> ndcCorners {
            glm::vec4 {-1.0F, 1.0F, -1.0F, 1.0F},
            glm::vec4 {1.0F, 1.0F, -1.0F, 1.0F},
            glm::vec4 {1.0F, -1.0F, -1.0F, 1.0F},
            glm::vec4 {-1.0F, -1.0F, -1.0F, 1.0F},
            glm::vec4 {-1.0F, 1.0F, 1.0F, 1.0F},
            glm::vec4 {1.0F, 1.0F, 1.0F, 1.0F},
            glm::vec4 {1.0F, -1.0F, 1.0F, 1.0F},
            glm::vec4 {-1.0F, -1.0F, 1.0F, 1.0F}
        };

        std::array<glm::vec3, 8U> corners {};
        for(std::size_t index {0U}; index < ndcCorners.size(); ++index) {
            const glm::vec4 cornerWorld = inverseViewProjection * ndcCorners.at(index);
            corners.at(index) = glm::vec3 {cornerWorld} / std::max(cornerWorld.w, epsilon);
        }
        return corners;
    }

    auto choose_up_vector(const glm::vec3& lightDirection) -> glm::vec3 {
        const glm::vec3 worldUp {0.0F, 1.0F, 0.0F};
        const float dotUp = glm::abs(glm::dot(worldUp, lightDirection));
        if(dotUp > 0.95F) {
            return glm::vec3 {0.0F, 0.0F, 1.0F};
        }
        return worldUp;
    }
}

namespace vulkano {

CascadedShadowSettings::CascadedShadowSettings()
    : enabled {true}
    , stabilize {true}
    , visualizeCascades {false}
    , showShadowAtlas {false}
    , cascadeCount {maxShadowCascades}
    , resolution {2048U}
    , splitLambda {0.5F}
    , biasConstant {0.0025F}
    , biasSlope {1.5F}
    , normalBias {0.0025F}
    , shadowStrength {0.75F}
    , pcfRadius {1.0F} {
}

CascadedShadowUniform::CascadedShadowUniform()
    : lightViewProjection {}
    , cascadeData {}
    , cascadeSplits {0.0F, 0.0F, 0.0F, 0.0F}
    , shadowParams {1.0F, 0.75F, 1.0F, static_cast<float>(maxShadowCascades)}
    , biasParams {0.0025F, 1.5F, 0.0025F, 0.0F}
    , atlasSize {2048.0F, 1.0F / 2048.0F, 0.0F, 0.0F}
    , debugParams {0.0F, 0.0F, 1.0F, 0.0F} {
    for(glm::mat4& matrix : lightViewProjection) {
        matrix = glm::mat4 {1.0F};
    }
    for(glm::vec4& cascade : cascadeData) {
        cascade = glm::vec4 {0.0F};
    }
}

ShadowComputationInput::ShadowComputationInput()
    : view {1.0F}
    , projection {1.0F}
    , lightDirection {default_light_direction()}
    , nearPlane {0.1F}
    , farPlane {100.0F} {
}

ShadowCascadeData::ShadowCascadeData()
    : uniform {}
    , cascadeTexelSizes {}
    , cascadeRadii {}
    , cascadeCenters {}
    , lightPositions {} {
    cascadeTexelSizes.fill(0.0F);
    cascadeRadii.fill(0.0F);
    cascadeCenters.fill(glm::vec3 {0.0F});
    lightPositions.fill(glm::vec3 {0.0F});
}

auto compute_cascaded_shadow_data(
    const ShadowComputationInput& input,
    const CascadedShadowSettings& settings,
    std::uint32_t cascadeCount,
    std::uint32_t resolution) -> ShadowCascadeData {
    ShadowCascadeData data {};
    const std::uint32_t clampedCascadeCount = std::clamp(cascadeCount, 1U, maxShadowCascades);
    const std::uint32_t clampedResolution = std::max(resolution, 1U);

    if(!settings.enabled) {
        data.uniform.shadowParams = glm::vec4 {0.0F, settings.shadowStrength, settings.pcfRadius, static_cast<float>(clampedCascadeCount)};
        data.uniform.cascadeSplits = glm::vec4 {input.farPlane, input.farPlane, input.farPlane, input.farPlane};
        data.uniform.biasParams = glm::vec4 {settings.biasConstant, settings.biasSlope, settings.normalBias, settings.stabilize ? 1.0F : 0.0F};
        data.uniform.atlasSize = glm::vec4 {
            static_cast<float>(clampedResolution),
            1.0F / static_cast<float>(clampedResolution),
            0.0F,
            0.0F
        };
        data.uniform.debugParams = glm::vec4 {settings.visualizeCascades ? 1.0F : 0.0F, settings.showShadowAtlas ? 1.0F : 0.0F, settings.stabilize ? 1.0F : 0.0F, 0.0F};
        return data;
    }

    const glm::mat4 view = input.view;
    const glm::mat4 projection = input.projection;
    const glm::mat4 viewProjection = projection * view;
    const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);

    const auto frustumCorners = compute_frustum_corners(inverseViewProjection);
    const auto splits = detail::compute_cascade_splits(
        input.nearPlane,
        input.farPlane,
        clampedCascadeCount,
        settings.splitLambda);

    glm::vec3 lightDirection = settings.enabled ? glm::normalize(input.lightDirection) : default_light_direction();
    if(glm::length(lightDirection) <= epsilon) {
        lightDirection = default_light_direction();
    }

    const float range = input.farPlane - input.nearPlane;
    const glm::vec3 up = choose_up_vector(lightDirection);

    for(std::uint32_t cascadeIndex {0U}; cascadeIndex < maxShadowCascades; ++cascadeIndex) {
        if(cascadeIndex >= clampedCascadeCount) {
            const std::size_t arrayIndex = static_cast<std::size_t>(cascadeIndex);
            data.uniform.lightViewProjection.at(arrayIndex) = glm::mat4 {1.0F};
            data.uniform.cascadeData.at(arrayIndex) = glm::vec4 {0.0F};
            data.uniform.cascadeSplits[static_cast<int>(cascadeIndex)] = input.farPlane;
            data.cascadeTexelSizes.at(cascadeIndex) = 0.0F;
            data.cascadeRadii.at(cascadeIndex) = 0.0F;
            data.cascadeCenters.at(cascadeIndex) = glm::vec3 {0.0F};
            data.lightPositions.at(cascadeIndex) = glm::vec3 {0.0F};
            continue;
        }

        const float splitDepth = splits.at(cascadeIndex);
        const float previousSplit = (cascadeIndex == 0U) ? input.nearPlane : splits.at(cascadeIndex - 1U);

        const float previousRatio = (previousSplit - input.nearPlane) / range;
        const float splitRatio = (splitDepth - input.nearPlane) / range;

        std::array<glm::vec3, 8U> cascadeCorners {};
        for(std::uint32_t cornerIndex {0U}; cornerIndex < 4U; ++cornerIndex) {
            const glm::vec3 cornerNear = frustumCorners.at(cornerIndex);
            const glm::vec3 cornerFar = frustumCorners.at(cornerIndex + 4U);
            const glm::vec3 cornerSliceNear = cornerNear + (cornerFar - cornerNear) * previousRatio;
            const glm::vec3 cornerSliceFar = cornerNear + (cornerFar - cornerNear) * splitRatio;
            cascadeCorners.at(cornerIndex) = cornerSliceNear;
            cascadeCorners.at(cornerIndex + 4U) = cornerSliceFar;
        }

        glm::vec3 cascadeCenter {0.0F, 0.0F, 0.0F};
        for(const glm::vec3& corner : cascadeCorners) {
            cascadeCenter += corner;
        }
        cascadeCenter /= static_cast<float>(cascadeCorners.size());

        float radius {0.0F};
        for(const glm::vec3& corner : cascadeCorners) {
            const float distance = glm::length(corner - cascadeCenter);
            radius = std::max(radius, distance);
        }
        radius = std::ceil(radius * 16.0F) / 16.0F;

        const float lightDistance = std::max(radius * depthPaddingScale, epsilon);
        const glm::vec3 lightPosition = cascadeCenter - (lightDirection * lightDistance);
        const glm::mat4 lightView = glm::lookAt(lightPosition, cascadeCenter, up);

        glm::vec3 minBounds {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        glm::vec3 maxBounds {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        };

        for(const glm::vec3& corner : cascadeCorners) {
            const glm::vec3 cornerLightSpace = glm::vec3 {lightView * glm::vec4 {corner, 1.0F}};
            minBounds = glm::min(minBounds, cornerLightSpace);
            maxBounds = glm::max(maxBounds, cornerLightSpace);
        }

        const float minLightSpaceZ = minBounds.z;
        const float maxLightSpaceZ = maxBounds.z;

        const glm::vec3 cascadeCenterLightSpace = (minBounds + maxBounds) * 0.5F;
        float halfExtentX = (maxBounds.x - minBounds.x) * 0.5F;
        float halfExtentY = (maxBounds.y - minBounds.y) * 0.5F;
        float halfExtent = std::max({halfExtentX, halfExtentY, epsilon});
        halfExtent *= extentPaddingFactor;

        const float cascadeWidth = halfExtent * 2.0F;
        float texelSize = cascadeWidth / static_cast<float>(clampedResolution);
        texelSize = std::max(texelSize, epsilon);

        glm::vec3 stabilisedCenterLightSpace = cascadeCenterLightSpace;
        if(settings.stabilize && texelSize > epsilon) {
            stabilisedCenterLightSpace.x = std::round(stabilisedCenterLightSpace.x / texelSize) * texelSize;
            stabilisedCenterLightSpace.y = std::round(stabilisedCenterLightSpace.y / texelSize) * texelSize;
        }

        const float minX = stabilisedCenterLightSpace.x - halfExtent;
        const float maxX = stabilisedCenterLightSpace.x + halfExtent;
        const float minY = stabilisedCenterLightSpace.y - halfExtent;
        const float maxY = stabilisedCenterLightSpace.y + halfExtent;

        const float depthPadding = range * 0.5F;
        const float nearPlane = minLightSpaceZ - depthPadding;
        const float farPlane = maxLightSpaceZ + depthPadding;

        glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, nearPlane, farPlane);
        glm::mat4 viewProjectionMatrix = lightProjection * lightView;

        const std::size_t arrayIndex = static_cast<std::size_t>(cascadeIndex);
        data.uniform.lightViewProjection.at(arrayIndex) = viewProjectionMatrix;
        data.uniform.cascadeData.at(arrayIndex) = glm::vec4 {texelSize, halfExtent, farPlane - nearPlane, 0.0F};
        data.uniform.cascadeSplits[static_cast<int>(cascadeIndex)] = splitDepth;
        data.cascadeTexelSizes.at(cascadeIndex) = texelSize;
        data.cascadeRadii.at(cascadeIndex) = halfExtent;
        data.cascadeCenters.at(cascadeIndex) = cascadeCenter;
        data.lightPositions.at(cascadeIndex) = lightPosition;
    }

    data.uniform.shadowParams = glm::vec4 {
        settings.enabled ? 1.0F : 0.0F,
        settings.shadowStrength,
        settings.pcfRadius,
        static_cast<float>(clampedCascadeCount)
    };
    data.uniform.biasParams = glm::vec4 {settings.biasConstant, settings.biasSlope, settings.normalBias, settings.stabilize ? 1.0F : 0.0F};
    data.uniform.atlasSize = glm::vec4 {
        static_cast<float>(clampedResolution),
        1.0F / static_cast<float>(clampedResolution),
        0.0F,
        0.0F
    };
    data.uniform.debugParams = glm::vec4 {
        settings.visualizeCascades ? 1.0F : 0.0F,
        settings.showShadowAtlas ? 1.0F : 0.0F,
        settings.stabilize ? 1.0F : 0.0F,
        0.0F
    };

    return data;
}

CascadedShadowMapResources::CascadedShadowMapResources(
    const VulkanContext& context,
    const ShadowRenderPass& renderPass,
    VkFormat format,
    std::uint32_t cascadeCount,
    std::uint32_t resolution) {
    initialise(context, renderPass, format, cascadeCount, resolution);
}

CascadedShadowMapResources::CascadedShadowMapResources(CascadedShadowMapResources&& other) noexcept {
    move_from(std::move(other));
}

auto CascadedShadowMapResources::operator=(CascadedShadowMapResources&& other) noexcept -> CascadedShadowMapResources& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

CascadedShadowMapResources::~CascadedShadowMapResources() noexcept {
    cleanup();
}

auto CascadedShadowMapResources::create(
    const VulkanContext& context,
    const ShadowRenderPass& renderPass,
    VkFormat format,
    std::uint32_t cascadeCount,
    std::uint32_t resolution) -> CascadedShadowMapResources {
    return CascadedShadowMapResources {context, renderPass, format, cascadeCount, resolution};
}

void CascadedShadowMapResources::recreate(
    const VulkanContext& context,
    const ShadowRenderPass& renderPass,
    VkFormat format,
    std::uint32_t cascadeCount,
    std::uint32_t resolution) {
    cleanup();
    initialise(context, renderPass, format, cascadeCount, resolution);
}

void CascadedShadowMapResources::cleanup() noexcept {
    if(m_device == VK_NULL_HANDLE) {
        return;
    }

    for(VkFramebuffer framebuffer : m_framebuffers) {
        if(framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
    }
    m_framebuffers.clear();

    for(VkImageView view : m_layerViews) {
        if(view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, view, nullptr);
        }
    }
    m_layerViews.clear();

    if(m_arrayView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_arrayView, nullptr);
        m_arrayView = VK_NULL_HANDLE;
    }

    if(m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if(m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }

    if(m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }

    m_device = VK_NULL_HANDLE;
    m_format = VK_FORMAT_D32_SFLOAT;
    m_extent = VkExtent2D {0U, 0U};
    m_cascadeCount = 0U;
}

auto CascadedShadowMapResources::cascade_count() const noexcept -> std::uint32_t {
    return m_cascadeCount;
}

auto CascadedShadowMapResources::extent() const noexcept -> VkExtent2D {
    return m_extent;
}

auto CascadedShadowMapResources::image() const noexcept -> VkImage {
    return m_image;
}

auto CascadedShadowMapResources::image_view_array() const noexcept -> VkImageView {
    return m_arrayView;
}

auto CascadedShadowMapResources::layer_view(std::size_t index) const noexcept -> VkImageView {
    if(index >= m_layerViews.size()) {
        return VK_NULL_HANDLE;
    }
    return m_layerViews.at(index);
}

auto CascadedShadowMapResources::framebuffer(std::size_t index) const noexcept -> VkFramebuffer {
    if(index >= m_framebuffers.size()) {
        return VK_NULL_HANDLE;
    }
    return m_framebuffers.at(index);
}

auto CascadedShadowMapResources::sampler() const noexcept -> VkSampler {
    return m_sampler;
}

void CascadedShadowMapResources::initialise(
    const VulkanContext& context,
    const ShadowRenderPass& renderPass,
    VkFormat format,
    std::uint32_t cascadeCount,
    std::uint32_t resolution) {
    if(cascadeCount == 0U) {
        throw std::invalid_argument {"Cascade count must be greater than zero"};
    }
    if(resolution == 0U) {
        throw std::invalid_argument {"Shadow map resolution must be greater than zero"};
    }

    m_device = context.device();
    m_format = format;
    m_cascadeCount = cascadeCount;
    m_extent = VkExtent2D {resolution, resolution};

    VkImageCreateInfo imageInfo {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_extent.width;
    imageInfo.extent.height = m_extent.height;
    imageInfo.extent.depth = 1U;
    imageInfo.mipLevels = 1U;
    imageInfo.arrayLayers = cascadeCount;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateImage(m_device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create cascaded shadow image"};
    }
    context.set_object_name(
        VK_OBJECT_TYPE_IMAGE,
        reinterpret_cast<std::uint64_t>(m_image),
        "Cascaded Shadow Image");

    VkMemoryRequirements requirements {};
    vkGetImageMemoryRequirements(m_device, m_image, &requirements);

    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(
        context.physical_device(),
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_memory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate cascaded shadow image memory"};
    }

    if(vkBindImageMemory(m_device, m_image, m_memory, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to bind cascaded shadow image memory"};
    }

    VkImageViewCreateInfo arrayViewInfo {};
    arrayViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    arrayViewInfo.image = m_image;
    arrayViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    arrayViewInfo.format = format;
    arrayViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if(has_stencil_component(format)) {
        arrayViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    arrayViewInfo.subresourceRange.baseMipLevel = 0U;
    arrayViewInfo.subresourceRange.levelCount = 1U;
    arrayViewInfo.subresourceRange.baseArrayLayer = 0U;
    arrayViewInfo.subresourceRange.layerCount = cascadeCount;

    if(vkCreateImageView(m_device, &arrayViewInfo, nullptr, &m_arrayView) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create cascaded shadow array view"};
    }
    context.set_object_name(
        VK_OBJECT_TYPE_IMAGE_VIEW,
        reinterpret_cast<std::uint64_t>(m_arrayView),
        "Cascaded Shadow Array View");

    m_layerViews.resize(cascadeCount, VK_NULL_HANDLE);
    m_framebuffers.resize(cascadeCount, VK_NULL_HANDLE);

    for(std::uint32_t index {0U}; index < cascadeCount; ++index) {
        VkImageViewCreateInfo layerViewInfo = arrayViewInfo;
        layerViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        layerViewInfo.subresourceRange.baseArrayLayer = index;
        layerViewInfo.subresourceRange.layerCount = 1U;

        if(vkCreateImageView(m_device, &layerViewInfo, nullptr, &m_layerViews.at(index)) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create cascaded shadow layer view"};
        }
        const std::string viewName = "Cascaded Shadow Layer View " + std::to_string(index);
        context.set_object_name(
            VK_OBJECT_TYPE_IMAGE_VIEW,
            reinterpret_cast<std::uint64_t>(m_layerViews.at(index)),
            viewName);

        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass.handle();
        framebufferInfo.attachmentCount = 1U;
        framebufferInfo.pAttachments = &m_layerViews.at(index);
        framebufferInfo.width = m_extent.width;
        framebufferInfo.height = m_extent.height;
        framebufferInfo.layers = 1U;

        if(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers.at(index)) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create cascaded shadow framebuffer"};
        }
        const std::string framebufferName = "Cascaded Shadow Framebuffer " + std::to_string(index);
        context.set_object_name(
            VK_OBJECT_TYPE_FRAMEBUFFER,
            reinterpret_cast<std::uint64_t>(m_framebuffers.at(index)),
            framebufferName);
    }

    VkSamplerCreateInfo samplerInfo {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.mipLodBias = 0.0F;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0F;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerInfo.minLod = 0.0F;
    samplerInfo.maxLod = 0.0F;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create cascaded shadow sampler"};
    }
    context.set_object_name(
        VK_OBJECT_TYPE_SAMPLER,
        reinterpret_cast<std::uint64_t>(m_sampler),
        "Cascaded Shadow Sampler");
}

void CascadedShadowMapResources::move_from(CascadedShadowMapResources&& other) noexcept {
    m_device = other.m_device;
    m_format = other.m_format;
    m_image = other.m_image;
    m_memory = other.m_memory;
    m_arrayView = other.m_arrayView;
    m_layerViews = std::move(other.m_layerViews);
    m_framebuffers = std::move(other.m_framebuffers);
    m_sampler = other.m_sampler;
    m_extent = other.m_extent;
    m_cascadeCount = other.m_cascadeCount;

    other.m_device = VK_NULL_HANDLE;
    other.m_format = VK_FORMAT_D32_SFLOAT;
    other.m_image = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_arrayView = VK_NULL_HANDLE;
    other.m_layerViews.clear();
    other.m_framebuffers.clear();
    other.m_sampler = VK_NULL_HANDLE;
    other.m_extent = VkExtent2D {0U, 0U};
    other.m_cascadeCount = 0U;
}

namespace detail {

auto compute_cascade_splits(
    float nearPlane,
    float farPlane,
    std::uint32_t cascadeCount,
    float lambda) -> std::array<float, maxShadowCascades> {
    const std::uint32_t clampedCascades = std::clamp(cascadeCount, 1U, maxShadowCascades);
    std::array<float, maxShadowCascades> splits {};
    splits.fill(farPlane);

    const float range = farPlane - nearPlane;
    const float ratio = farPlane / nearPlane;

    for(std::uint32_t index {0U}; index < clampedCascades; ++index) {
        const float fraction = static_cast<float>(index + 1U) / static_cast<float>(clampedCascades);
        const float logSplit = nearPlane * std::pow(ratio, fraction);
        const float uniformSplit = nearPlane + (range * fraction);
        const float split = glm::mix(uniformSplit, logSplit, lambda);
        splits.at(index) = split;
    }

    return splits;
}

} // namespace detail

} // namespace vulkano
