#include <vulkano/command_allocator.hpp>

#include <stdexcept>

namespace vulkano {

CommandAllocator::CommandAllocator(const VulkanContext& context, std::uint32_t bufferCount) {
    initialise(context, bufferCount);
}

CommandAllocator::CommandAllocator(CommandAllocator&& other) noexcept {
    move_from(std::move(other));
}

auto CommandAllocator::operator=(CommandAllocator&& other) noexcept -> CommandAllocator& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

CommandAllocator::~CommandAllocator() noexcept {
    cleanup();
}

auto CommandAllocator::create(const VulkanContext& context, std::uint32_t bufferCount) -> CommandAllocator {
    return CommandAllocator {context, bufferCount};
}

auto CommandAllocator::pool() const noexcept -> VkCommandPool {
    return m_commandPool;
}

auto CommandAllocator::buffers() const noexcept -> const std::vector<VkCommandBuffer>& {
    return m_commandBuffers;
}

auto CommandAllocator::buffer_count() const noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(m_commandBuffers.size());
}

void CommandAllocator::reset() const {
    if(m_device == VK_NULL_HANDLE || m_commandPool == VK_NULL_HANDLE) {
        return;
    }
    const VkResult result = vkResetCommandPool(m_device, m_commandPool, 0U);
    if(result != VK_SUCCESS) {
        throw std::runtime_error {"Failed to reset command pool"};
    }
}

void CommandAllocator::recreate(const VulkanContext& context, std::uint32_t bufferCount) {
    cleanup();
    initialise(context, bufferCount);
}

void CommandAllocator::initialise(const VulkanContext& context, std::uint32_t bufferCount) {
    m_device = context.device();
    m_queueFamilyIndex = context.graphics_queue_index();

    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    const VkResult poolResult = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
    if(poolResult != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create command pool"};
    }

    allocate_buffers(bufferCount);
}

void CommandAllocator::cleanup() noexcept {
    if(m_device != VK_NULL_HANDLE && m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }
    m_commandBuffers.clear();
    m_commandPool = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_queueFamilyIndex = 0U;
}

void CommandAllocator::allocate_buffers(std::uint32_t bufferCount) {
    if(bufferCount == 0U) {
        m_commandBuffers.clear();
        return;
    }
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = bufferCount;

    m_commandBuffers.resize(bufferCount);
    const VkResult result = vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data());
    if(result != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate command buffers"};
    }
}

void CommandAllocator::move_from(CommandAllocator&& other) noexcept {
    m_device = other.m_device;
    m_commandPool = other.m_commandPool;
    m_commandBuffers = std::move(other.m_commandBuffers);
    m_queueFamilyIndex = other.m_queueFamilyIndex;

    other.m_device = VK_NULL_HANDLE;
    other.m_commandPool = VK_NULL_HANDLE;
    other.m_commandBuffers.clear();
    other.m_queueFamilyIndex = 0U;
}

} // namespace vulkano
