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

auto Buffer::create_vertex_buffer(const VulkanContext& context, std::span<const MeshVertex> vertices) -> Buffer {
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(vertices.size() * sizeof(MeshVertex));
    Buffer buffer {context, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    if(!vertices.empty()) {
        buffer.copy_data(context, vertices.data(), bufferSize);
    }
    return buffer;
}

auto Buffer::create_device_local_vertex_buffer(const VulkanContext& context, std::span<const MeshVertex> vertices) -> Buffer {
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(vertices.size() * sizeof(MeshVertex));
    if(bufferSize == 0U) {
        throw std::runtime_error {"Vertex buffer requires at least one vertex"};
    }

    Buffer staging {context, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    staging.copy_data(context, vertices.data(), bufferSize);

    Buffer deviceBuffer {context, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
    copy_buffer(context, staging.handle(), deviceBuffer.handle(), bufferSize);
    return deviceBuffer;
}

auto Buffer::create_device_local_index_buffer(const VulkanContext& context, std::span<const std::uint32_t> indices) -> Buffer {
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(indices.size() * sizeof(std::uint32_t));
    if(bufferSize == 0U) {
        throw std::runtime_error {"Index buffer requires at least one index"};
    }

    Buffer staging {context, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    staging.copy_data(context, indices.data(), bufferSize);

    Buffer deviceBuffer {context, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
    copy_buffer(context, staging.handle(), deviceBuffer.handle(), bufferSize);
    return deviceBuffer;
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
    context.set_object_name(VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<std::uint64_t>(m_memory), "Buffer Memory");
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

void Buffer::copy_buffer(const VulkanContext& context, VkBuffer source, VkBuffer destination, VkDeviceSize size) {
    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = context.graphics_queue_index();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool commandPool {VK_NULL_HANDLE};
    if(vkCreateCommandPool(context.device(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create command pool for buffer copy"};
    }

    VkCommandBufferAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1U;

    VkCommandBuffer commandBuffer {VK_NULL_HANDLE};
    if(vkAllocateCommandBuffers(context.device(), &allocateInfo, &commandBuffer) != VK_SUCCESS) {
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to allocate command buffer for buffer copy"};
    }

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to begin command buffer for buffer copy"};
    }

    VkBufferCopy copyRegion {};
    copyRegion.srcOffset = 0U;
    copyRegion.dstOffset = 0U;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, source, destination, 1U, &copyRegion);

    if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to record buffer copy command"};
    }

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers = &commandBuffer;

    const VkQueue queue = context.graphics_queue();
    if(vkQueueSubmit(queue, 1U, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to submit buffer copy command"};
    }

    if(vkQueueWaitIdle(queue) != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to wait for queue idle after buffer copy"};
    }

    vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
    vkDestroyCommandPool(context.device(), commandPool, nullptr);
}

} // namespace vulkano
