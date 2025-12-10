#pragma once

#include "from_cloudwatch.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace tenzir {
namespace livetail {

// Forward declaration - implementation in live_tail_stream.cpp
struct StreamContext;

/// Live Tail client using HTTP/2 streaming
/// Implements the StartLiveTail API with AWS Event Stream protocol
class LiveTailClient {
public:
  explicit LiveTailClient(const cloudwatch_config& config);
  ~LiveTailClient();

  // Non-copyable
  LiveTailClient(const LiveTailClient&) = delete;
  LiveTailClient& operator=(const LiveTailClient&) = delete;

  /// Start streaming logs
  void start(std::function<void(const cloudwatch_event&)> on_event,
             std::function<void(const std::string&, const std::string&)> on_session_start,
             std::function<void(const std::string&)> on_error);

  /// Stop streaming
  void stop();

  /// Check if running
  bool is_running() const;

private:
  void run();

  cloudwatch_config config_;
  std::unique_ptr<StreamContext> ctx_;
  std::thread worker_;
};

} // namespace livetail
} // namespace tenzir
