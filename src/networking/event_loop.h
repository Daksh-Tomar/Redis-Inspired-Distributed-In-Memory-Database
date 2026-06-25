#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// Event Loop — Platform-Agnostic Interface
// ═══════════════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <functional>
#include <memory>

namespace redisdb {

enum EventMask : int {
  EVENT_NONE = 0,
  EVENT_READABLE = 1,
  EVENT_WRITABLE = 2,
};

using FileEventCallback = std::function<void(int fd, int mask)>;

using TimeEventCallback = std::function<int()>;

// ═══════════════════════════════════════════════════════════════════════════════
// EventLoop Abstract Interface
// ═══════════════════════════════════════════════════════════════════════════════
class EventLoop {
public:
  virtual ~EventLoop() = default;

  virtual bool addFileEvent(int fd, int mask, FileEventCallback callback) = 0;

  virtual void removeFileEvent(int fd, int mask) = 0;

  virtual int64_t addTimeEvent(int64_t milliseconds,
                               TimeEventCallback callback) = 0;

  virtual void removeTimeEvent(int64_t id) = 0;

  virtual void run() = 0;

  virtual void stop() = 0;

  virtual int processEvents() = 0;

  virtual bool isRunning() const = 0;

  static std::unique_ptr<EventLoop> create(int maxfd = 1024);
};

} // namespace redisdb
