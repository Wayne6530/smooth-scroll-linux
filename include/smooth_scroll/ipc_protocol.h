// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <atomic>
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
  std::atomic<uint32_t> reserved[3];
};

static_assert(sizeof(SmoothScrollIPC) == 32, "IPC struct size must be exactly 32 bytes");

inline constexpr uint32_t IPC_MAGIC_VERSION_EXPECTED = 0x53530001;
inline constexpr const char* IPC_SHM_NAME = "/smooth_scroll_shm";

inline constexpr uint32_t IPC_STATE_CONNECTED = 1u << 0;
inline constexpr uint32_t IPC_STATE_PASSTHROUGH = 1u << 1;
inline constexpr uint32_t IPC_STATE_DRAG_VIEW = 1u << 2;
inline constexpr uint32_t IPC_STATE_FREE_SPIN = 1u << 3;
inline constexpr uint32_t IPC_STATE_HORIZONTAL = 1u << 4;
inline constexpr uint32_t IPC_STATE_DIRECTION = 1u << 5;
inline constexpr uint32_t IPC_STATE_SPEED_SHIFT = 16;

}  // namespace smooth_scroll
