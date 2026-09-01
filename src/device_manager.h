// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "config.h"
#include "input_device.h"

namespace smooth_scroll
{

class UdevInputSource;

class DeviceMonitor
{
public:
  ~DeviceMonitor();

  DeviceMonitor(const DeviceMonitor&) = delete;
  DeviceMonitor& operator=(const DeviceMonitor&) = delete;

  [[nodiscard]] int stopFd() const noexcept;

private:
  DeviceMonitor(std::unique_ptr<UdevInputSource> source, DeviceConfig config,
                std::vector<unsigned int> relevant_keyboard_keys);

  [[nodiscard]] static std::unique_ptr<DeviceMonitor>
  start(std::unique_ptr<UdevInputSource> source, DeviceConfig config,
        std::vector<unsigned int> relevant_keyboard_keys);
  [[nodiscard]] bool initialize();
  void monitorLoop();

  std::unique_ptr<UdevInputSource> source_;
  DeviceConfig config_;
  std::vector<unsigned int> relevant_keyboard_keys_;
  int stop_fd_{ -1 };
  int worker_stop_fd_{ -1 };
  std::thread worker_;

  friend class DeviceManager;
};

struct SessionResources
{
  SessionDevices devices;
  std::unique_ptr<DeviceMonitor> monitor;
};

enum class AcquireStatus
{
  Success,
  Shutdown,
  FatalError,
};

struct AcquireResult
{
  AcquireStatus status;
  std::optional<SessionResources> resources;
};

class DeviceManager
{
public:
  DeviceManager(DeviceConfig config, const std::vector<unsigned int>& relevant_keyboard_keys,
                const std::atomic_bool& shutdown, int shutdown_fd);

  DeviceManager(const DeviceManager&) = delete;
  DeviceManager& operator=(const DeviceManager&) = delete;

  [[nodiscard]] AcquireResult acquireSessionDevices();

private:
  [[nodiscard]] std::vector<std::string> listEventDevicePaths() const;
  [[nodiscard]] std::vector<InputDeviceHandle> acquireKeyboards(const std::string& mouse_path) const;

  DeviceConfig config_;
  std::vector<unsigned int> relevant_keyboard_keys_;
  const std::atomic_bool& shutdown_;
  int shutdown_fd_;
};

}  // namespace smooth_scroll
