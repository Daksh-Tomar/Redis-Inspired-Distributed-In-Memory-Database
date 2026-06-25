#include "networking/event_loop.h"
#include "networking/platform/event_loop_select.h"

#ifdef REDIS_PLATFORM_LINUX
#include "networking/platform/event_loop_epoll.h"
#endif

namespace redisdb {

std::unique_ptr<EventLoop> EventLoop::create(int maxfd) {

#ifdef REDIS_PLATFORM_LINUX
  return std::make_unique<EpollEventLoop>(maxfd);
#else
  return std::make_unique<SelectEventLoop>(maxfd);
#endif
}

} // namespace redisdb
