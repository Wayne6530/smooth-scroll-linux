# Smooth Scroll Linux - Shared Memory IPC Protocol (v2)

## 1. Overview

The `smooth-scroll-linux` daemon communicates with external UI or CLI tools via a POSIX Shared Memory segment. This protocol uses a strict, lock-free (mutex-free) design, relying exclusively on C++11 atomic operations (`std::atomic`) to ensure high-performance, zero-latency state synchronization without disrupting the daemon's input event loop.

- **Shared Memory Name:** `/smooth_scroll_shm` (Typically mapped to `/dev/shm/smooth_scroll_shm`)
- **Total Size:** 32 Bytes
- **Endianness:** Host Endianness (Typically Little-Endian)
- **Canonical C++ Header:** `include/smooth_scroll/ipc_protocol.h`

## 2. Memory Layout

The memory structure is strictly 32-bit (4-byte) aligned. All concurrent read/write operations to this memory block **MUST** use 32-bit atomic instructions.

```cpp
#include <cstdint>
#include <atomic>

// Ensure the struct is exactly 32 bytes and aligned to the cache line boundary
struct alignas(32) SmoothScrollIPC {
    // [0x00] Header
    std::atomic<uint32_t> magic_version; 
    
    // [0x04] Daemon Lifecycle
    std::atomic<uint32_t> daemon_pid;    
    
    // [0x08] Status (Daemon -> UI)
    std::atomic<uint32_t> state_bits;    
    
    // [0x0C] Control: Scroll ID (UI -> Daemon)
    std::atomic<uint32_t> scroll_id;     
    
    // [0x10] Control: Force Passthrough (UI -> Daemon)
    std::atomic<uint32_t> force_passthrough; 
    
    // [0x14] Status: packed signed Auto Scroll offsets (Daemon -> UI)
    std::atomic<uint32_t> auto_scroll_offset;

    // [0x18 - 0x1F] Reserved for future use
    std::atomic<uint32_t> reserved[2];
};

static_assert(sizeof(SmoothScrollIPC) == 32, "IPC struct size mismatch");
```

## 3. Field Definitions

### 3.1 `magic_version` (Offset: 0x00)

- **Purpose:** Protocol validation and versioning.
- **High 16-bits:** Magic Number, strictly `0x5353` (ASCII: 'S', 'S').
- **Low 16-bits:** Protocol Version, currently `0x0002`.
- **Expected Value:** `0x53530002`.
- **Client Behavior:** Upon mapping the shared memory, clients must verify this field. If it does not match, the daemon is either initializing or running an incompatible version, and the client must not proceed.

### 3.2 `daemon_pid` (Offset: 0x04)

- **Purpose:** Tracks the operating system PID of the active daemon. Used for zero-overhead health monitoring.
- **Daemon Behavior:** Writes its PID upon successful initialization. Writes `0` during a graceful shutdown (`SIGINT`/`SIGTERM`).

### 3.3 `state_bits` (Offset: 0x08)

- **Purpose:** Real-time daemon state broadcast. Compressed into a single 32-bit bitfield.
- **Bit Layout:**
  - `Bit 0`: **Connected** (1 = Mouse device acquired, 0 = Lost/Searching)
  - `Bit 1`: **Passthrough** (1 = Currently in passthrough mode, 0 = Active interception)
  - `Bit 2`: **DragView** (1 = Drag View mode active)
  - `Bit 3`: **FreeSpin** (1 = Free Spin mode active)
  - `Bit 4`: **Horizontal** (ordinary scrolling only; 1 = horizontal, 0 = vertical)
  - `Bit 5`: **Direction** (ordinary scrolling only; 1 = positive/up/right, 0 = negative/down/left)
  - `Bit 6`: **AutoScroll** (1 = Auto Scroll owns the pointer, including while paused)
  - `Bit 7`: **AutoScrollHorizontalEnabled** (1 = the configured Auto Scroll mode enables the horizontal axis)
  - `Bit 8`: **AutoScrollVerticalEnabled** (1 = the configured Auto Scroll mode enables the vertical axis)
  - `Bits 9-15`: *Reserved*
  - `Bits 16-31`: **Speed** (unsigned 16-bit ordinary continuous-scroll speed; zero while Auto Scroll is active)

The Auto Scroll axis bits form these modes:

| Horizontal | Vertical | Mode |
| --- | --- | --- |
| 0 | 1 | Vertical only |
| 1 | 0 | Horizontal only |
| 1 | 1 | Omnidirectional |

The daemon publishes these configuration bits once during initialization. They remain valid whether Auto Scroll is active or inactive, allowing clients to prepare the correct visualization before activation.

### 3.4 `scroll_id` (Offset: 0x0C)

- **Purpose:** Asynchronous brake trigger (UI writes, Daemon reads).
- **Interaction:** To brake scrolling from the UI, the client increments this ID or writes a randomized `uint32_t`. For ordinary inertia this stops motion. For Auto Scroll it re-anchors a held gesture, or exits the mode when it is latched hands-free.

### 3.5 `force_passthrough` (Offset: 0x10)

- **Purpose:** Global override for passthrough mode (UI writes, Daemon reads).
- **Interaction:**
  - `0`: Normal operation (Daemon governs interception).
  - `> 0`: Forced wheel passthrough. Physical vertical and horizontal wheel events are forwarded unchanged. The enabling edge brakes ordinary inertia and latched Auto Scroll. A held Auto Scroll gesture is re-anchored without losing ownership, and held Drag View remains active.

### 3.6 `auto_scroll_offset` (Offset: 0x14)

- **Purpose:** Publishes the two-dimensional virtual pointer displacement used by Auto Scroll. Both components are delivered in one atomic 32-bit snapshot.
- **Bits 0-15:** Signed 16-bit horizontal offset. Positive values point right.
- **Bits 16-31:** Signed 16-bit vertical offset. Positive values point down.
- **Axis filtering:** A component disabled by the configured Auto Scroll axis mode is always zero.
- **Range:** Each component is clamped to `[-32768, 32767]` for transport. The daemon retains its wider internal value.
- **Lifecycle:** The value is zero on entry, changes with consumed pointer movement, survives a held-to-latched transition, returns to zero when a held gesture is re-anchored, and is cleared on exit.

Auto Scroll speed is derived by the daemon from each component, the dead zone, `auto_scroll_speed_factor`, and `auto_scroll_max_speed`. Clients should use the offset vector directly for visualization rather than interpreting the ordinary `Speed`, `Horizontal`, or `Direction` fields.

## 4. Lifecycle & Health Monitoring

To provide a robust user experience, external clients (UI) must monitor the daemon's health without polling at high frequencies. Clients must implement the following two-tier monitoring strategy:

### Step 1: Handling Graceful Exits

The daemon guarantees that it will write `0` to `daemon_pid` upon a clean exit (e.g., standard service stop). The UI should periodically check if `ipc->daemon_pid.load() == 0` to immediately detect a clean shutdown.

### Step 2: Handling Unexpected Crashes (Heartbeat)

If the daemon is killed forcefully (`kill -9`) or crashes (Segfault), `daemon_pid` will remain $> 0$. The UI must detect the death of the process entity.

**Primary Method (Linux 5.3+ via `pidfd`):**
This is the recommended, zero-overhead approach. The UI requests a file descriptor for the daemon's PID and adds it to its native event loop (e.g., `epoll`, Qt, GLib).

```c
int pid = ipc->daemon_pid.load(std::memory_order_relaxed);
if (pid > 0) {
    // Obtain a file descriptor for the process
    int pfd = syscall(SYS_pidfd_open, pid, 0);
    // Add 'pfd' to your UI's event loop. 
    // It will become readable the exact microsecond the daemon terminates.
}
```

**Fallback Method (Legacy via `kill`):**
If `pidfd_open` is unavailable, the UI should use a low-frequency timer (e.g., 1 Hz / once per second) to ping the process.

```c
int pid = ipc->daemon_pid.load(std::memory_order_relaxed);
if (pid > 0) {
    if (kill(pid, 0) == -1 && errno == ESRCH) {
        // Process no longer exists (crashed)
        handle_daemon_offline();
    }
}
```

## 5. Concurrency Guidelines

- **No Locks:** Do not attempt to use mutexes or semaphores.
- **Memory Ordering:** Both the Daemon and the UI should default to using `std::memory_order_relaxed` for reads and writes. The slight (nanosecond) latency in cache coherency is perfectly acceptable for UI rendering and asynchronous control, and omitting memory barriers maximizes overall system throughput.
