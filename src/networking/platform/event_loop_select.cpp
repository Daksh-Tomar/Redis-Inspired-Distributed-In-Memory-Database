#include "networking/platform/event_loop_select.h"

#include <algorithm>
#include <cstring>
#include <iostream>


namespace redisdb {

// Constructor / Destructor

SelectEventLoop::SelectEventLoop(int maxfd) : maxfd_(maxfd) {
  fileEvents_.reserve(maxfd);
}

SelectEventLoop::~SelectEventLoop() { stop(); }

// File Events

bool SelectEventLoop::addFileEvent(int fd, int mask,
                                   FileEventCallback callback) {

  auto &fe = fileEvents_[fd];

  if (mask & EVENT_READABLE) {
    fe.readCallback = callback;
  }
  if (mask & EVENT_WRITABLE) {
    fe.writeCallback = callback;
  }
  fe.mask |= mask;

  if (fd > highestFd_) {
    highestFd_ = fd;
  }

  return true;
}

void SelectEventLoop::removeFileEvent(int fd, int mask) {
  auto it = fileEvents_.find(fd);
  if (it == fileEvents_.end())
    return;

  it->second.mask &= ~mask;

  if (mask & EVENT_READABLE) {
    it->second.readCallback = nullptr;
  }
  if (mask & EVENT_WRITABLE) {
    it->second.writeCallback = nullptr;
  }

  if (it->second.mask == EVENT_NONE) {
    fileEvents_.erase(it);

    if (fd == highestFd_) {
      highestFd_ = -1;
      for (auto &[k, _] : fileEvents_) {
        if (k > highestFd_)
          highestFd_ = k;
      }
    }
  }
}

// Time Events

int64_t SelectEventLoop::addTimeEvent(int64_t milliseconds,
                                      TimeEventCallback callback) {

  int64_t id = nextTimeEventId_++;

  TimeEvent te;
  te.id = id;
  te.fireTime = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(milliseconds);
  te.callback = std::move(callback);

  timeEvents_.push_back(std::move(te));
  return id;
}

void SelectEventLoop::removeTimeEvent(int64_t id) {
  timeEvents_.erase(
      std::remove_if(timeEvents_.begin(), timeEvents_.end(),
                     [id](const TimeEvent &te) { return te.id == id; }),
      timeEvents_.end());
}

// Main Loop

void SelectEventLoop::run() {

  running_.store(true);
  while (running_.load()) {
    processEvents();
  }
}

void SelectEventLoop::stop() { running_.store(false); }

int SelectEventLoop::processEvents() {
  int processed = 0;

  fd_set readfds, writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  int maxfd = -1;

  for (auto &[fd, fe] : fileEvents_) {
    if (fe.mask & EVENT_READABLE) {
      FD_SET(static_cast<unsigned int>(fd), &readfds);
    }
    if (fe.mask & EVENT_WRITABLE) {
      FD_SET(static_cast<unsigned int>(fd), &writefds);
    }
    if (fd > maxfd)
      maxfd = fd;
  }

  struct timeval tv;
  struct timeval *tvp = calculateTimeout(&tv);

  int numready;
  if (maxfd >= 0) {
    numready = select(maxfd + 1, &readfds, &writefds, nullptr, tvp);
  } else {

    if (tvp) {
#ifdef REDIS_PLATFORM_WINDOWS
      Sleep(tvp->tv_sec * 1000 + tvp->tv_usec / 1000);
#else
      select(0, nullptr, nullptr, nullptr, tvp);
#endif
    }
    numready = 0;
  }

  if (numready > 0) {

    struct FiredEvent {
      int fd;
      int mask;
    };
    std::vector<FiredEvent> fired;
    fired.reserve(numready);

    for (auto &[fd, fe] : fileEvents_) {
      int firedMask = 0;
      if ((fe.mask & EVENT_READABLE) && FD_ISSET(fd, &readfds)) {
        firedMask |= EVENT_READABLE;
      }
      if ((fe.mask & EVENT_WRITABLE) && FD_ISSET(fd, &writefds)) {
        firedMask |= EVENT_WRITABLE;
      }
      if (firedMask) {
        fired.push_back({fd, firedMask});
      }
    }

    for (auto &event : fired) {
      auto it = fileEvents_.find(event.fd);
      if (it == fileEvents_.end())
        continue;

      if ((event.mask & EVENT_READABLE) && it->second.readCallback) {
        it->second.readCallback(event.fd, event.mask);
        processed++;
      }

      it = fileEvents_.find(event.fd);
      if (it == fileEvents_.end())
        continue;

      if ((event.mask & EVENT_WRITABLE) && it->second.writeCallback) {
        it->second.writeCallback(event.fd, event.mask);
        processed++;
      }
    }
  }

  processed += processTimeEvents();

  return processed;
}

// Private Helpers

struct timeval *SelectEventLoop::calculateTimeout(struct timeval *tv) {
  if (timeEvents_.empty()) {
    tv->tv_sec = 0;
    tv->tv_usec = 100000; // 100ms
    return tv;
  }

  auto now = std::chrono::steady_clock::now();
  auto nearest = std::min_element(timeEvents_.begin(), timeEvents_.end(),
                                  [](const TimeEvent &a, const TimeEvent &b) {
                                    return a.fireTime < b.fireTime;
                                  });

  auto diff = std::chrono::duration_cast<std::chrono::microseconds>(
      nearest->fireTime - now);

  if (diff.count() <= 0) {
    tv->tv_sec = 0;
    tv->tv_usec = 0;
    return tv;
  }

  tv->tv_sec = static_cast<long>(diff.count() / 1000000);
  tv->tv_usec = static_cast<long>(diff.count() % 1000000);
  return tv;
}

int SelectEventLoop::processTimeEvents() {
  int processed = 0;
  auto now = std::chrono::steady_clock::now();

  size_t i = 0;
  while (i < timeEvents_.size()) {
    if (timeEvents_[i].fireTime <= now) {

      int retval = timeEvents_[i].callback();
      processed++;

      if (retval > 0) {
        timeEvents_[i].fireTime = now + std::chrono::milliseconds(retval);
        i++;
      } else {
        timeEvents_.erase(timeEvents_.begin() + static_cast<ptrdiff_t>(i));
      }
    } else {
      i++;
    }
  }

  return processed;
}

} // namespace redisdb
