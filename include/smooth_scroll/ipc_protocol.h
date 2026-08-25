// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace smooth_scroll
{

struct alignas(32) SmoothScrollIPC
{
  std::atomic<uint32_t> magic_version;
  std::atomic<uint32_t> daemon_pid;
  std::atomic<uint32_t> state_bits;
  std::atomic<uint32_t> scroll_id;
  std::atomic<uint32_t> force_passthrough;
  std::atomic<uint32_t> auto_scroll_offset;
  std::atomic<uint32_t> reserved[2];
};

static_assert(sizeof(SmoothScrollIPC) == 32, "IPC struct size must be exactly 32 bytes");
static_assert(offsetof(SmoothScrollIPC, auto_scroll_offset) == 0x14,
              "Auto Scroll offset must remain at IPC offset 0x14");

inline constexpr uint32_t IPC_MAGIC_VERSION_EXPECTED = 0x53530002;
inline constexpr const char* IPC_SHM_NAME = "/smooth_scroll_shm";

inline constexpr uint32_t IPC_STATE_CONNECTED = 1u << 0;
inline constexpr uint32_t IPC_STATE_PASSTHROUGH = 1u << 1;
inline constexpr uint32_t IPC_STATE_DRAG_VIEW = 1u << 2;
inline constexpr uint32_t IPC_STATE_FREE_SPIN = 1u << 3;
inline constexpr uint32_t IPC_STATE_HORIZONTAL = 1u << 4;
inline constexpr uint32_t IPC_STATE_DIRECTION = 1u << 5;
inline constexpr uint32_t IPC_STATE_AUTO_SCROLL = 1u << 6;
inline constexpr uint32_t IPC_STATE_AUTO_SCROLL_HORIZONTAL_ENABLED = 1u << 7;
inline constexpr uint32_t IPC_STATE_AUTO_SCROLL_VERTICAL_ENABLED = 1u << 8;
inline constexpr uint32_t IPC_STATE_SPEED_SHIFT = 16;

inline constexpr uint32_t packAutoScrollOffset(int16_t horizontal, int16_t vertical) noexcept
{
  return static_cast<uint16_t>(horizontal) | (static_cast<uint32_t>(static_cast<uint16_t>(vertical)) << 16);
}

inline constexpr int16_t decodeAutoScrollOffsetComponent(uint16_t value) noexcept
{
  return value <= 0x7FFFu ? static_cast<int16_t>(value)
                          : static_cast<int16_t>(static_cast<int32_t>(value) - 0x10000);
}

inline constexpr int16_t autoScrollHorizontalOffset(uint32_t packed) noexcept
{
  return decodeAutoScrollOffsetComponent(static_cast<uint16_t>(packed));
}

inline constexpr int16_t autoScrollVerticalOffset(uint32_t packed) noexcept
{
  return decodeAutoScrollOffsetComponent(static_cast<uint16_t>(packed >> 16));
}

static_assert(autoScrollHorizontalOffset(packAutoScrollOffset(-123, 456)) == -123);
static_assert(autoScrollVerticalOffset(packAutoScrollOffset(-123, 456)) == 456);

}  // namespace smooth_scroll
