#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// select()-Based Event Loop Implementation
// ═══════════════════════════════════════════════════════════════════════════════

#include "networking/event_loop.h"
#include "networking/platform/socket_compat.h"

#include <atomic>
#include <chrono>
#include <unordered_map>
#include <vector>


namespace redisdb {

struct FileEvent {
  int mask = EVENT_NONE;
  FileEventCallback readCallback;
  FileEventCallback writeCallback;
};

struct TimeEvent {
  int64_t id;
  std::chrono::steady_clock::time_point fireTime;
  TimeEventCallback callback;
};

class SelectEventLoop : public EventLoop {
public:
  explicit SelectEventLoop(int maxfd);
  ~SelectEventLoop() override;

  bool addFileEvent(int fd, int mask, FileEventCallback callback) override;
  void removeFileEvent(int fd, int mask) override;

  int64_t addTimeEvent(int64_t milliseconds,
                       TimeEventCallback callback) override;
  void removeTimeEvent(int64_t id) override;

  void run() override;
  void stop() override;
  int processEvents() override;
  bool isRunning() const override { return running_.load(); }

private:
  struct timeval *calculateTimeout(struct timeval *tv);

  int processTimeEvents();

  int maxfd_;
  int highestFd_ = -1;
  std::unordered_map<int, FileEvent> fileEvents_;
  std::vector<TimeEvent> timeEvents_;
  int64_t nextTimeEventId_ = 1;
  std::atomic<bool> running_{false};
};

} // namespace redisdb
