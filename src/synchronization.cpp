#include <vulkano/synchronization.hpp>

#include <stdexcept>
#include <string>

namespace vulkano {

SyncManager::SyncManager(const VulkanContext& context, std::uint32_t framesInFlight) {
    initialise(context, framesInFlight);
}

SyncManager::SyncManager(SyncManager&& other) noexcept {
    move_from(std::move(other));
}

auto SyncManager::operator=(SyncManager&& other) noexcept -> SyncManager& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

SyncManager::~SyncManager() noexcept {
    cleanup();
}

auto SyncManager::create(const VulkanContext& context, std::uint32_t framesInFlight) -> SyncManager {
    return SyncManager {context, framesInFlight};
}

auto SyncManager::frames() noexcept -> std::vector<FrameSync>& {
    return m_frames;
}

auto SyncManager::frames() const noexcept -> const std::vector<FrameSync>& {
    return m_frames;
}

auto SyncManager::frame_count() const noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(m_frames.size());
}

void SyncManager::initialise(const VulkanContext& context, std::uint32_t framesInFlight) {
    m_device = context.device();
    m_frames.resize(framesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(std::size_t index {0U}; index < m_frames.size(); ++index) {
        FrameSync& frame = m_frames.at(index);
        if(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create image-available semaphore"};
        }
        if(vkCreateFence(m_device, &fenceInfo, nullptr, &frame.inFlight) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create in-flight fence"};
        }
        const std::string imageSemaphoreName = "Frame " + std::to_string(index) + " Image Available Semaphore";
        context.set_object_name(VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<std::uint64_t>(frame.imageAvailable), imageSemaphoreName);
        const std::string fenceName = "Frame " + std::to_string(index) + " In Flight Fence";
        context.set_object_name(VK_OBJECT_TYPE_FENCE, reinterpret_cast<std::uint64_t>(frame.inFlight), fenceName);
    }
}

void SyncManager::cleanup() noexcept {
    if(m_device == VK_NULL_HANDLE) {
        return;
    }
    for(FrameSync& frame : m_frames) {
        if(frame.imageAvailable != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, frame.imageAvailable, nullptr);
            frame.imageAvailable = VK_NULL_HANDLE;
        }
        if(frame.inFlight != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, frame.inFlight, nullptr);
            frame.inFlight = VK_NULL_HANDLE;
        }
    }
    m_frames.clear();
    m_device = VK_NULL_HANDLE;
}

void SyncManager::move_from(SyncManager&& other) noexcept {
    m_device = other.m_device;
    m_frames = std::move(other.m_frames);
    other.m_device = VK_NULL_HANDLE;
    other.m_frames.clear();
}

} // namespace vulkano
