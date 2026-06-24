#include "networking/client.h"

#include <cstring>
#include <iostream>


namespace redisdb {

uint64_t Client::nextId_ = 1;

Client::Client(socket_t fd)
    : fd_(fd), id_(nextId_++), createdAt_(std::chrono::steady_clock::now()),
      lastInteraction_(createdAt_) {

  inputBuffer_.reserve(READ_BUFFER_SIZE);
}

Client::~Client() {
  if (fd_ != INVALID_SOCKET_VAL) {
    platform::closeSocket(fd_);
  }
}

int Client::readFromSocket() {

  char buf[READ_BUFFER_SIZE];

  int nread = recv(fd_, buf, sizeof(buf), 0);

  if (nread > 0) {
    inputBuffer_.append(buf, nread);
    updateLastInteraction();

    if (inputBuffer_.size() > MAX_INPUT_BUFFER) {
      std::cerr << "[Client " << id_ << "] Input buffer overflow ("
                << inputBuffer_.size() << " bytes), closing connection"
                << std::endl;
      return 0;
    }

    return nread;
  } else if (nread == 0) {

    return 0;
  } else {

    if (platform::wouldBlock()) {

      return -1;
    }
    // Real error
    return 0;
  }
}

int Client::writeToSocket() {

  if (!hasPendingOutput())
    return 0;

  const char *data = outputBuffer_.data() + outputPos_;
  size_t remaining = outputBuffer_.size() - outputPos_;

  int nwritten = send(fd_, data, static_cast<int>(remaining), 0);

  if (nwritten > 0) {
    outputPos_ += nwritten;

    if (outputPos_ >= outputBuffer_.size()) {
      outputBuffer_.clear();
      outputPos_ = 0;
    }

    return nwritten;
  } else {
    if (platform::wouldBlock()) {
      return -1;
    }
    return 0;
  }
}

void Client::addReply(const std::string &data) { outputBuffer_.append(data); }

void Client::consumeInput(size_t bytes) {

  if (bytes >= inputBuffer_.size()) {
    inputBuffer_.clear();
  } else {
    inputBuffer_.erase(0, bytes);
  }
}

} // namespace redisdb
