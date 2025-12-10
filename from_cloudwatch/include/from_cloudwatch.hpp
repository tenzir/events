#pragma once

#include <aws/core/Aws.h>
#include <aws/logs/CloudWatchLogsClient.h>
#include <aws/logs/model/FilterLogEventsRequest.h>
#include <aws/logs/model/FilterLogEventsResult.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace tenzir {

/// A single CloudWatch log event
struct cloudwatch_event {
  std::string log_stream_name;
  std::string message;
  int64_t timestamp;      // milliseconds since epoch
  int64_t ingestion_time; // milliseconds since epoch
  std::string event_id;
};

/// Configuration for the from_cloudwatch operator
struct cloudwatch_config {
  /// Log group ARN or name (required)
  std::string log_group_identifier;

  /// Optional log stream names to filter
  std::vector<std::string> log_stream_names;

  /// Optional log stream name prefixes
  std::vector<std::string> log_stream_name_prefixes;

  /// Optional filter pattern (supports CloudWatch filter syntax)
  std::string filter_pattern;

  /// AWS region (defaults to environment/config)
  std::optional<std::string> region;

  /// Polling interval for fallback mode (milliseconds)
  int poll_interval_ms = 1000;

  /// Start time (milliseconds since epoch, 0 = now)
  int64_t start_time = 0;
};

/// Callback type for receiving events
using event_callback = std::function<void(const cloudwatch_event&)>;

/// Callback for session start
using session_start_callback = std::function<void(const std::string& session_id,
                                                   const std::string& request_id)>;

/// Callback for errors
using error_callback = std::function<void(const std::string& error)>;

/// CloudWatch log stream operator
class from_cloudwatch_operator {
public:
  explicit from_cloudwatch_operator(cloudwatch_config config);
  ~from_cloudwatch_operator();

  /// Start streaming events
  void start(event_callback on_event,
             session_start_callback on_session_start = {},
             error_callback on_error = {});

  /// Stop streaming
  void stop();

  /// Check if running
  bool is_running() const { return running_.load(); }

private:
  /// Live Tail implementation (uses HTTP/2 streaming)
  void run_live_tail();

  /// Polling fallback using FilterLogEvents
  void run_polling_fallback();

  /// Process a raw event from the stream
  void process_event(const std::string& event_data);

  cloudwatch_config config_;
  std::unique_ptr<Aws::CloudWatchLogs::CloudWatchLogsClient> client_;
  std::atomic<bool> running_{false};
  std::thread worker_thread_;

  event_callback on_event_;
  session_start_callback on_session_start_;
  error_callback on_error_;

  // For polling fallback
  std::string next_token_;
  int64_t last_event_time_ = 0;
};

} // namespace tenzir
