/// Prototype implementation of from_cloudwatch operator
/// Uses AWS C++ SDK to stream events from CloudWatch Logs
///
/// This prototype demonstrates two approaches:
/// 1. Live Tail using StartLiveTail API (HTTP/2 event streaming)
/// 2. Polling fallback using FilterLogEvents API
///
/// Note: The C++ SDK may not have full StartLiveTail bindings yet.
/// This implementation provides both approaches.

#include "from_cloudwatch.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/Outcome.h>
#include <aws/logs/CloudWatchLogsClient.h>
#include <aws/logs/model/FilterLogEventsRequest.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

// Include Live Tail streaming client when enabled
#ifdef USE_LIVE_TAIL_STREAMING
#include "live_tail_client.hpp"
#endif

namespace tenzir {

namespace {

/// Get current time in milliseconds since epoch
int64_t now_ms() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             now.time_since_epoch())
      .count();
}

/// AWS Event Stream frame types
enum class EventStreamFrameType {
  Unknown = 0,
  SessionStart = 1,
  SessionUpdate = 2,
  Exception = 3
};

/// Parse event stream message type from headers
/// Event stream uses a binary protocol with headers indicating message type
EventStreamFrameType parse_event_type(const std::string& event_type_header) {
  if (event_type_header.find("sessionStart") != std::string::npos) {
    return EventStreamFrameType::SessionStart;
  }
  if (event_type_header.find("sessionUpdate") != std::string::npos) {
    return EventStreamFrameType::SessionUpdate;
  }
  if (event_type_header.find("exception") != std::string::npos) {
    return EventStreamFrameType::Exception;
  }
  return EventStreamFrameType::Unknown;
}

} // namespace

from_cloudwatch_operator::from_cloudwatch_operator(cloudwatch_config config)
    : config_(std::move(config)) {
  Aws::Client::ClientConfiguration client_config;
  if (config_.region) {
    client_config.region = *config_.region;
  }

  client_ =
      std::make_unique<Aws::CloudWatchLogs::CloudWatchLogsClient>(client_config);
}

from_cloudwatch_operator::~from_cloudwatch_operator() { stop(); }

void from_cloudwatch_operator::start(event_callback on_event,
                                     session_start_callback on_session_start,
                                     error_callback on_error) {
  if (running_.load()) {
    return;
  }

  on_event_ = std::move(on_event);
  on_session_start_ = std::move(on_session_start);
  on_error_ = std::move(on_error);

  running_.store(true);

  worker_thread_ = std::thread([this]() {
#ifdef USE_LIVE_TAIL_STREAMING
    // Use HTTP/2 streaming with StartLiveTail
    run_live_tail();
#else
    // Use polling fallback
    run_polling_fallback();
#endif
  });
}

void from_cloudwatch_operator::stop() {
  running_.store(false);
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

/// Live Tail implementation using HTTP/2 streaming
/// This is the preferred approach - uses libcurl for HTTP/2 and AWS Event Stream protocol
void from_cloudwatch_operator::run_live_tail() {
#ifdef USE_LIVE_TAIL_STREAMING
  // Use the LiveTailClient from live_tail_stream.cpp
  // This implements full HTTP/2 streaming with AWS SigV4 signing
  // and AWS Event Stream binary protocol parsing

  livetail::LiveTailClient live_tail_client(config_);

  live_tail_client.start(
    on_event_,
    on_session_start_,
    on_error_
  );

  // Wait while running
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  live_tail_client.stop();
#else
  // Fall back to polling if streaming not available
  if (on_error_) {
    on_error_("Live Tail streaming requires USE_LIVE_TAIL_STREAMING, "
              "falling back to polling");
  }
  run_polling_fallback();
#endif
}

/// Polling fallback using FilterLogEvents
void from_cloudwatch_operator::run_polling_fallback() {
  // Notify session start (simulated for polling mode)
  if (on_session_start_) {
    on_session_start_("polling-session", "polling-request");
  }

  // Set initial start time
  if (config_.start_time == 0) {
    last_event_time_ = now_ms() - 60000; // Start from 1 minute ago
  } else {
    last_event_time_ = config_.start_time;
  }

  while (running_.load()) {
    try {
      Aws::CloudWatchLogs::Model::FilterLogEventsRequest request;
      request.SetLogGroupName(config_.log_group_identifier);

      if (!config_.log_stream_names.empty()) {
        Aws::Vector<Aws::String> streams;
        for (const auto& name : config_.log_stream_names) {
          streams.push_back(name);
        }
        request.SetLogStreamNames(std::move(streams));
      }

      if (!config_.log_stream_name_prefixes.empty() &&
          !config_.log_stream_name_prefixes[0].empty()) {
        request.SetLogStreamNamePrefix(config_.log_stream_name_prefixes[0]);
      }

      if (!config_.filter_pattern.empty()) {
        request.SetFilterPattern(config_.filter_pattern);
      }

      // Get events since last check
      request.SetStartTime(last_event_time_);
      request.SetEndTime(now_ms());

      if (!next_token_.empty()) {
        request.SetNextToken(next_token_);
      }

      auto outcome = client_->FilterLogEvents(request);

      if (outcome.IsSuccess()) {
        const auto& result = outcome.GetResult();
        const auto& events = result.GetEvents();

        for (const auto& event : events) {
          if (!running_.load()) {
            break;
          }

          cloudwatch_event cw_event;
          cw_event.log_stream_name = event.GetLogStreamName();
          cw_event.message = event.GetMessage();
          cw_event.timestamp = event.GetTimestamp();
          cw_event.ingestion_time = event.GetIngestionTime();
          cw_event.event_id = event.GetEventId();

          // Update last event time
          if (event.GetTimestamp() > last_event_time_) {
            last_event_time_ = event.GetTimestamp() + 1;
          }

          if (on_event_) {
            on_event_(cw_event);
          }
        }

        // Handle pagination
        if (result.GetNextToken().empty()) {
          next_token_.clear();
        } else {
          next_token_ = result.GetNextToken();
          // If there's more data, don't sleep - fetch it immediately
          continue;
        }
      } else {
        if (on_error_) {
          on_error_(outcome.GetError().GetMessage());
        }
      }
    } catch (const std::exception& e) {
      if (on_error_) {
        on_error_(e.what());
      }
    }

    // Sleep before next poll
    std::this_thread::sleep_for(
        std::chrono::milliseconds(config_.poll_interval_ms));
  }
}

void from_cloudwatch_operator::process_event(const std::string& event_data) {
  // Parse JSON event data from the stream
  // This is used for the live tail implementation
  Aws::Utils::Json::JsonValue json(event_data);
  if (!json.WasParseSuccessful()) {
    if (on_error_) {
      on_error_("Failed to parse event JSON");
    }
    return;
  }

  auto view = json.View();

  // Check if this is a session start event
  if (view.KeyExists("sessionId")) {
    if (on_session_start_) {
      on_session_start_(view.GetString("sessionId"),
                        view.GetString("requestId"));
    }
    return;
  }

  // Check if this is a session update event
  if (view.KeyExists("sessionResults")) {
    auto results = view.GetArray("sessionResults");
    for (size_t i = 0; i < results.GetLength(); ++i) {
      auto item = results.GetItem(i);
      cloudwatch_event event;
      event.log_stream_name = item.GetString("logStreamName");
      event.message = item.GetString("message");
      event.timestamp = item.GetInt64("timestamp");
      event.ingestion_time = item.GetInt64("ingestionTime");
      // event_id is generated for live tail events

      if (on_event_) {
        on_event_(event);
      }
    }
  }
}

} // namespace tenzir

//////////////////////////////////////////////////////////////////////////////
// Main entry point - demonstrates the operator
//////////////////////////////////////////////////////////////////////////////

void print_usage(const char* program) {
  std::cerr << "Usage: " << program << " <log-group-name> [options]\n"
            << "\nOptions:\n"
            << "  --stream <name>     Filter by log stream name\n"
            << "  --prefix <prefix>   Filter by log stream name prefix\n"
            << "  --filter <pattern>  Filter pattern (CloudWatch syntax)\n"
            << "  --region <region>   AWS region\n"
            << "  --poll-interval <ms> Polling interval in milliseconds "
               "(default: 1000)\n"
            << "\nExamples:\n"
            << "  " << program << " /aws/lambda/my-function\n"
            << "  " << program
            << " /aws/lambda/my-function --filter \"ERROR\"\n"
            << "  " << program
            << " /aws/lambda/my-function --stream my-stream --region "
               "us-west-2\n";
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  // Parse arguments
  tenzir::cloudwatch_config config;
  config.log_group_identifier = argv[1];

  for (int i = 2; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--stream" && i + 1 < argc) {
      config.log_stream_names.push_back(argv[++i]);
    } else if (arg == "--prefix" && i + 1 < argc) {
      config.log_stream_name_prefixes.push_back(argv[++i]);
    } else if (arg == "--filter" && i + 1 < argc) {
      config.filter_pattern = argv[++i];
    } else if (arg == "--region" && i + 1 < argc) {
      config.region = argv[++i];
    } else if (arg == "--poll-interval" && i + 1 < argc) {
      config.poll_interval_ms = std::stoi(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      print_usage(argv[0]);
      return 0;
    }
  }

  // Initialize AWS SDK
  Aws::SDKOptions options;
  options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Warn;
  Aws::InitAPI(options);

  {
    // Create operator
    tenzir::from_cloudwatch_operator op(config);

    // Event counter for statistics
    std::atomic<size_t> event_count{0};
    auto start_time = std::chrono::steady_clock::now();

    // Start streaming
    std::cout << "Starting CloudWatch log stream from: "
              << config.log_group_identifier << "\n";
    if (!config.filter_pattern.empty()) {
      std::cout << "Filter pattern: " << config.filter_pattern << "\n";
    }
    std::cout << "Press Ctrl+C to stop\n\n";

    op.start(
        // Event callback
        [&event_count](const tenzir::cloudwatch_event& event) {
          event_count++;

          // Convert timestamp to human readable
          auto ts =
              std::chrono::system_clock::time_point(
                  std::chrono::milliseconds(event.timestamp));
          auto time_t = std::chrono::system_clock::to_time_t(ts);

          // Print event as JSON-like output
          std::cout << "{\n"
                    << "  \"timestamp\": " << event.timestamp << ",\n"
                    << "  \"time\": \"" << std::ctime(&time_t)
                    << "  \"logStreamName\": \"" << event.log_stream_name
                    << "\",\n"
                    << "  \"message\": \"" << event.message << "\"\n"
                    << "}\n";
        },
        // Session start callback
        [](const std::string& session_id, const std::string& request_id) {
          std::cout << "Session started: " << session_id
                    << " (request: " << request_id << ")\n";
        },
        // Error callback
        [](const std::string& error) {
          std::cerr << "Error: " << error << "\n";
        });

    // Wait for interrupt (simple version - just sleep)
    // In a real implementation, you'd use proper signal handling
    while (op.is_running()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));

      // Print statistics every 10 seconds
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                         now - start_time)
                         .count();
      if (elapsed > 0 && elapsed % 10 == 0) {
        std::cerr << "[Stats] Events received: " << event_count.load()
                  << " (elapsed: " << elapsed << "s)\n";
      }
    }
  }

  Aws::ShutdownAPI(options);
  return 0;
}
