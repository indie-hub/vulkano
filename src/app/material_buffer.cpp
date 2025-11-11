#include <vulkano/app/material_buffer.hpp>

#include <cstring>
#include <stdexcept>
#include <vector>

#include <vulkano/app/material_gpu.hpp>
#include <vulkano/app/vulkan_context.hpp>

namespace vulkano::app {
namespace {
[[nodiscard]] std::uint32_t find_memory_type(VkPhysicalDevice physicalDevice, std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (std::uint32_t i {0U}; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1U << i)) != 0U
            && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error {"Failed to find suitable memory type for material buffer"};
}
} // namespace

MaterialBuffer::MaterialBuffer(const VulkanContext& context)
    : m_context {context} {
}

MaterialBuffer::~MaterialBuffer() noexcept {
    destroy();
}

void MaterialBuffer::update(const scene::MaterialRegistry& registry) {
    std::vector<MaterialGpu> gpuMaterials = build_material_gpu_buffer(registry);
    const VkDeviceSize requiredSize {static_cast<VkDeviceSize>(sizeof(MaterialGpu) * gpuMaterials.size())};

    if (requiredSize == 0U) {
        return;
    }

    if (requiredSize > m_capacity || m_buffer == VK_NULL_HANDLE) {
        create_buffer(gpuMaterials.size());
    }

    void* mapped = nullptr;
    if (vkMapMemory(m_context.device(), m_memory, 0U, requiredSize, 0U, &mapped) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map material buffer memory"};
    }
    std::memcpy(mapped, gpuMaterials.data(), static_cast<std::size_t>(requiredSize));
    vkUnmapMemory(m_context.device(), m_memory);

    m_materialCount = gpuMaterials.size();
    m_descriptorInfo.range = requiredSize;
}

VkBuffer MaterialBuffer::buffer() const noexcept {
    return m_buffer;
}

VkDescriptorBufferInfo MaterialBuffer::descriptor_info() const noexcept {
    return m_descriptorInfo;
}

std::size_t MaterialBuffer::material_count() const noexcept {
    return m_materialCount;
}

void MaterialBuffer::create_buffer(std::size_t materialCount) {
    destroy();

    const VkDevice device = m_context.device();
    const VkPhysicalDevice physicalDevice = m_context.physical_device();

    const VkDeviceSize bufferSize {static_cast<VkDeviceSize>(sizeof(MaterialGpu) * materialCount)};

    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create material buffer"};
    }

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(device, m_buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(physicalDevice, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocateInfo, nullptr, &m_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        throw std::runtime_error {"Failed to allocate material buffer memory"};
    }

    if (vkBindBufferMemory(device, m_buffer, m_memory, 0U) != VK_SUCCESS) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        vkFreeMemory(device, m_memory, nullptr);
        m_buffer = VK_NULL_HANDLE;
        m_memory = VK_NULL_HANDLE;
        throw std::runtime_error {"Failed to bind material buffer memory"};
    }

    m_capacity = bufferSize;
    m_descriptorInfo.buffer = m_buffer;
    m_descriptorInfo.offset = 0U;
    m_descriptorInfo.range = bufferSize;
}

void MaterialBuffer::destroy() noexcept {
    const VkDevice device = m_context.device();
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    m_capacity = 0U;
    m_materialCount = 0U;
    m_descriptorInfo = VkDescriptorBufferInfo {};
}
} // namespace vulkano::app

