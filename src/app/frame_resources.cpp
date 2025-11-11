#include <vulkano/app/frame_resources.hpp>

#include <vulkano/app/vulkan_context.hpp>

#include <stdexcept>

namespace vulkano::app {
FrameResources::FrameResources(const VulkanContext& context)
    : m_context {context} {
    create_command_resources();
    create_synchronization_objects();
}

FrameResources::~FrameResources() noexcept {
    for (std::size_t i {0U}; i < m_imageAvailableSemaphores.size(); ++i) {
        if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_context.device(), m_imageAvailableSemaphores[i], nullptr);
            m_imageAvailableSemaphores[i] = VK_NULL_HANDLE;
        }
        if (m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_context.device(), m_renderFinishedSemaphores[i], nullptr);
            m_renderFinishedSemaphores[i] = VK_NULL_HANDLE;
        }
        if (m_inFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(m_context.device(), m_inFlightFences[i], nullptr);
            m_inFlightFences[i] = VK_NULL_HANDLE;
        }
    }

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_context.device(), m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
}

VkCommandPool FrameResources::command_pool() const noexcept {
    return m_commandPool;
}

const std::vector<VkCommandBuffer>& FrameResources::command_buffers() const noexcept {
    return m_commandBuffers;
}

std::size_t FrameResources::frames_in_flight() const noexcept {
    return FramesInFlight;
}

VkSemaphore FrameResources::image_available_semaphore(std::size_t index) const noexcept {
    return m_imageAvailableSemaphores[index];
}

VkSemaphore FrameResources::render_finished_semaphore(std::size_t index) const noexcept {
    return m_renderFinishedSemaphores[index];
}

VkFence FrameResources::in_flight_fence(std::size_t index) const noexcept {
    return m_inFlightFences[index];
}

VkFence FrameResources::image_in_flight(std::uint32_t imageIndex) const noexcept {
    return m_imagesInFlight.at(imageIndex);
}

void FrameResources::set_image_in_flight(std::uint32_t imageIndex, VkFence fence) noexcept {
    m_imagesInFlight[imageIndex] = fence;
}

void FrameResources::reset_fence(std::size_t index) const {
    const VkFence fence = m_inFlightFences[index];
    if (fence != VK_NULL_HANDLE) {
        if (vkResetFences(m_context.device(), 1U, &fence) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to reset fence"};
        }
    }
}

void FrameResources::create_command_resources() {
    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_context.graphics_queue_family_index();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_context.device(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create command pool"};
    }

    const std::size_t commandBufferCount = m_context.swapchain_image_views().size();
    VkCommandBufferAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = m_commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = static_cast<std::uint32_t>(commandBufferCount);

    m_commandBuffers.resize(commandBufferCount);
    if (vkAllocateCommandBuffers(m_context.device(), &allocateInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate command buffers"};
    }
}

void FrameResources::create_synchronization_objects() {
    m_imageAvailableSemaphores.resize(FramesInFlight, VK_NULL_HANDLE);
    m_renderFinishedSemaphores.resize(FramesInFlight, VK_NULL_HANDLE);
    m_inFlightFences.resize(FramesInFlight, VK_NULL_HANDLE);
    m_imagesInFlight.resize(m_context.swapchain_image_views().size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::size_t i {0U}; i < FramesInFlight; ++i) {
        if (vkCreateSemaphore(m_context.device(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS
            || vkCreateSemaphore(m_context.device(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS
            || vkCreateFence(m_context.device(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create synchronization objects for a frame"};
        }
    }
}
} // namespace vulkano::app
