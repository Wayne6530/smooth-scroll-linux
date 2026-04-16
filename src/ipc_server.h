// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <cstdint>
#include <atomic>
#include <string>
#include <string_view>

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

class IpcServer
{
public:
  explicit IpcServer(std::string_view shm_name = "/smooth_scroll_shm");

  ~IpcServer();

  IpcServer(const IpcServer&) = delete;
  IpcServer& operator=(const IpcServer&) = delete;

  bool initialize();

  void setConnected() noexcept;

  void setPassthrough(bool passthrough) noexcept;

  void setDragView(bool drag_view) noexcept;

  void setFreeSpin(bool free_spin) noexcept;

  void setSpeed(double speed, bool positive, bool horizontal) noexcept;

  [[nodiscard]] bool checkBrakeSignal() noexcept;

  [[nodiscard]] bool isForcePassthroughEnabled() const noexcept;

private:
  static constexpr uint32_t MAGIC_VERSION_EXPECTED = 0x53530001;

  void cleanup() noexcept;

  std::string shm_name_;
  int shm_fd_{ -1 };
  SmoothScrollIPC* mapped_memory_{ nullptr };

  uint32_t state_{ 0 };
  uint32_t scroll_id_{ 0 };
};

}  // namespace smooth_scroll
