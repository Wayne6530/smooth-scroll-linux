// SPDX-License-Identifier: MIT
#include "ipc_client.h"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <new>
#include <sys/mman.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

int main()
{
  using namespace smooth_scroll;
  const auto name = "/smooth-scroll-test-" + std::to_string(getpid());
  const int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0)
    return 1;
  SmoothScrollKWin::IpcClient client(name.c_str());
  if (client.readSnapshot().valid || ftruncate(fd, sizeof(SmoothScrollIPC)) < 0)
    return 1;
  void* memory = mmap(nullptr, sizeof(SmoothScrollIPC), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (memory == MAP_FAILED)
    return 1;
  auto* ipc = new (memory) SmoothScrollIPC{};
  const auto child = fork();
  if (child < 0)
    return 1;
  if (!child)
  {
    pause();
    _exit(0);
  }
  ipc->daemon_pid.store(child);
  ipc->state_bits.store(IPC_STATE_CONNECTED);
  ipc->magic_version.store(IPC_MAGIC_VERSION_EXPECTED);
  const bool connected = client.readSnapshot().connected;
  shm_unlink(name.c_str());
  kill(child, SIGKILL);
  waitpid(child, nullptr, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  const bool detectedExit = !client.readSnapshot().valid;
  ipc->daemon_pid.store(getpid());
  const bool reconnected = client.readSnapshot().connected;
  munmap(memory, sizeof(SmoothScrollIPC));
  close(fd);
  return connected && detectedExit && reconnected ? 0 : 1;
}
