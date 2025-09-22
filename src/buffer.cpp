#include <vulkano/buffer.hpp>

#include <cstring>
#include <stdexcept>

namespace vulkano {

Buffer::Buffer(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    initialise(context, size, usage, properties);
}

Buffer::Buffer(Buffer&& other) noexcept {
    move_from(std::move(other));
}

auto Buffer::operator=(Buffer&& other) noexcept -> Buffer& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

Buffer::~Buffer() noexcept {
    cleanup();
}

auto Buffer::create_vertex_buffer(const VulkanContext& context, std::span<const Vertex> vertices) -> Buffer {
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));
    Buffer buffer {context, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    buffer.copy_data(context, vertices.data(), bufferSize);
    context.set_object_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(buffer.m_buffer), "Vertex Buffer");
    return buffer;
}

auto Buffer::handle() const noexcept -> VkBuffer {
    return m_buffer;
}

auto Buffer::size() const noexcept -> VkDeviceSize {
    return m_size;
}

void Buffer::initialise(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    m_device = context.device();
    m_size = size;

    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create buffer"};
    }

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(m_device, m_buffer, &requirements);

    VkMemoryAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(context, requirements.memoryTypeBits, properties);

    if(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate buffer memory"};
    }

    if(vkBindBufferMemory(m_device, m_buffer, m_memory, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to bind buffer memory"};
    }
}

void Buffer::copy_data(const VulkanContext& context, const void* data, VkDeviceSize size) {
    if(m_memory == VK_NULL_HANDLE) {
        throw std::runtime_error {"Buffer memory must be allocated before copying data"};
    }
    void* mappedMemory {nullptr};
    if(vkMapMemory(m_device, m_memory, 0U, size, 0U, &mappedMemory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map buffer memory"};
    }
    std::memcpy(mappedMemory, data, static_cast<std::size_t>(size));
    vkUnmapMemory(m_device, m_memory);
    context.set_object_name(VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<std::uint64_t>(m_memory), "Vertex Buffer Memory");
}

void Buffer::cleanup() noexcept {
    if(m_device != VK_NULL_HANDLE && m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_buffer, nullptr);
    }
    if(m_device != VK_NULL_HANDLE && m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
    }
    m_buffer = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_size = 0U;
}

void Buffer::move_from(Buffer&& other) noexcept {
    m_device = other.m_device;
    m_buffer = other.m_buffer;
    m_memory = other.m_memory;
    m_size = other.m_size;

    other.m_device = VK_NULL_HANDLE;
    other.m_buffer = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_size = 0U;
}

auto Buffer::find_memory_type(const VulkanContext& context, std::uint32_t typeFilter, VkMemoryPropertyFlags properties) -> std::uint32_t {
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vkGetPhysicalDeviceMemoryProperties(context.physical_device(), &memoryProperties);

    for(std::uint32_t index {0U}; index < memoryProperties.memoryTypeCount; ++index) {
        const bool suitable = (typeFilter & (1U << index)) != 0U;
        const bool hasProperties = (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
        if(suitable && hasProperties) {
            return index;
        }
    }
    throw std::runtime_error {"Failed to find suitable memory type"};
}

} // namespace vulkano
