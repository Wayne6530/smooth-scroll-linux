// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <smooth_scroll/ipc_protocol.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace smooth_scroll
{

class IpcServer
{
public:
  explicit IpcServer(std::string_view shm_name = IPC_SHM_NAME);

  ~IpcServer();

  IpcServer(const IpcServer&) = delete;
  IpcServer& operator=(const IpcServer&) = delete;

  bool initialize();

  void setConnected() noexcept;

  void setDisconnected() noexcept;

  void setPassthrough(bool passthrough) noexcept;

  void setDragView(bool drag_view) noexcept;

  void setFreeSpin(bool free_spin) noexcept;

  void setAutoScroll(bool auto_scroll) noexcept;

  void setAutoScrollAxes(bool horizontal_enabled, bool vertical_enabled) noexcept;

  void setAutoScrollOffset(int64_t horizontal, int64_t vertical) noexcept;

  void setSpeed(double speed, bool positive, bool horizontal) noexcept;

  void resetMotionState() noexcept;

  [[nodiscard]] bool checkBrakeRequest() noexcept;

  [[nodiscard]] bool isForcePassthroughEnabled() const noexcept;

private:
  void cleanup() noexcept;

  std::string shm_name_;
  int shm_fd_{ -1 };
  SmoothScrollIPC* mapped_memory_{ nullptr };

  uint32_t state_{ 0 };
  uint32_t scroll_id_{ 0 };
  bool force_passthrough_enabled_{ false };
};

}  // namespace smooth_scroll
