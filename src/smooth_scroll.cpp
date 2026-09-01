// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <csignal>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "config.h"
#include "device_manager.h"
#include "ipc_server.h"
#include "session.h"
#include "version.h"
#include "virtual_device.h"

using namespace std::string_view_literals;
using namespace smooth_scroll;

namespace
{

constexpr std::string_view kHelpStr =
    R"(Smooth Scroll for Linux (https://github.com/Wayne6530/smooth-scroll-linux)

Usage: smooth-scroll [options]

Options:
  -c, --config <file>  Specify config file path (default "./smooth-scroll.toml")
  -h, --help           Show help message
  -v, --version        Show version information
  -d, --debug          Enable debug mode (verbose logging for parameter tuning)
)"sv;

constexpr std::string_view kDefaultConfigPath = "./smooth-scroll.toml"sv;
std::atomic_bool kShutdown{ false };
int kShutdownFd = -1;

void signalHandler(int signal_number)
{
  const int saved_errno = errno;
  if (signal_number == SIGINT || signal_number == SIGTERM)
  {
    kShutdown.store(true, std::memory_order_relaxed);
    if (kShutdownFd >= 0)
    {
      const std::uint64_t value = 1;
      const ssize_t ignored = write(kShutdownFd, &value, sizeof(value));
      static_cast<void>(ignored);
    }
  }
  errno = saved_errno;
}

bool installSignalHandlers()
{
  struct sigaction action
  {
  };
  action.sa_handler = signalHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  if (sigaction(SIGINT, &action, nullptr) < 0 || sigaction(SIGTERM, &action, nullptr) < 0)
  {
    SPDLOG_ERROR("Failed to install shutdown signal handlers: {}", std::strerror(errno));
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char* argv[])
{
  spdlog::set_pattern("[%E.%f] [%^%L%$] %v");

  std::string config_path{ kDefaultConfigPath };
  bool show_help = false;
  bool show_version = false;
  for (int index = 1; index < argc; ++index)
  {
    const std::string_view argument = argv[index];
    if (argument == "-h" || argument == "--help")
    {
      show_help = true;
      break;
    }
    if (argument == "-v" || argument == "--version")
    {
      show_version = true;
      break;
    }
    if (argument == "-d" || argument == "--debug")
    {
      spdlog::set_level(spdlog::level::debug);
      continue;
    }
    if (argument == "-c" || argument == "--config")
    {
      if (index + 1 < argc)
      {
        config_path = argv[++index];
        continue;
      }
    }
    show_help = true;
    break;
  }

  if (show_help)
  {
    fmt::print("{}", kHelpStr);
    return 0;
  }
  if (show_version)
  {
    fmt::print("{}\n", kVersion);
    return 0;
  }

  Config config;
  if (!loadConfig(config_path, config))
    return -1;

  kShutdownFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (kShutdownFd < 0)
  {
    SPDLOG_ERROR("Failed to create shutdown eventfd: {}", std::strerror(errno));
    return -1;
  }
  if (!installSignalHandlers())
    return -1;

  IpcServer ipc;
  if (!ipc.initialize())
    return -1;

  std::vector<unsigned int> relevant_keyboard_keys = config.session.keyboard_braking_keys;
  relevant_keyboard_keys.insert(relevant_keyboard_keys.end(), config.session.keyboard_passthrough_keys.begin(),
                                config.session.keyboard_passthrough_keys.end());

  DeviceManager device_manager{ config.device_manager, relevant_keyboard_keys, kShutdown, kShutdownFd };

  VirtualDevice virtual_device;
  while (!kShutdown.load(std::memory_order_relaxed))
  {
    AcquireResult acquisition = device_manager.acquireSessionDevices();
    if (acquisition.status == AcquireStatus::Shutdown)
      break;
    if (acquisition.status == AcquireStatus::FatalError || !acquisition.resources)
      return -1;

    if (!virtual_device.ensureCapabilities(acquisition.resources->devices.mouse.info().capabilities))
      return -1;

    Session::CreateResult create_result = Session::CreateResult::InputOutputError;
    auto session = Session::create(std::move(*acquisition.resources), config.session, virtual_device, ipc, kShutdown,
                                   kShutdownFd, create_result);
    if (!session)
    {
      virtual_device.reset();
      if (create_result == Session::CreateResult::Shutdown)
        break;
      if (create_result == Session::CreateResult::MouseLost)
        continue;
      return -1;
    }

    const Session::RunResult result = session->run(kShutdown);
    if (result != Session::RunResult::RestartRequested)
      virtual_device.reset();
    session.reset();

    if (result == Session::RunResult::Shutdown)
      break;
    if (result == Session::RunResult::InputOutputError)
      return -1;
  }

  return 0;
}
