/// HTTP/2 streaming implementation for CloudWatch Logs StartLiveTail
/// This implements the actual event stream protocol used by the StartLiveTail API
///
/// The StartLiveTail API uses:
/// - HTTP/2 for streaming
/// - AWS Event Stream binary protocol for framing
/// - JSON for event payloads
///
/// Event Stream frame format:
/// +-----------+-----------+-------------+----------------+------------+
/// | Total Len | Headers   | Prelude CRC | Headers +      | Message    |
/// | (4 bytes) | Len       | (4 bytes)   | Payload        | CRC        |
/// |           | (4 bytes) |             | (variable)     | (4 bytes)  |
/// +-----------+-----------+-------------+----------------+------------+

#include "live_tail_client.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/utils/DateTime.h>
#include <aws/core/utils/HashingUtils.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>

#include <curl/curl.h>

#ifdef __linux__
#include <arpa/inet.h>  // for ntohl/htonl on Linux
#else
#include <winsock2.h>   // for ntohl/htonl on Windows
#endif

#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <vector>

namespace tenzir {
namespace livetail {

/// AWS Event Stream message
struct EventStreamMessage {
  std::string event_type;
  std::string content_type;
  std::string message_type;  // "event" or "exception"
  std::vector<uint8_t> payload;
};

/// Parse a single Event Stream frame from binary data
/// Returns bytes consumed, or 0 if incomplete
static size_t parse_event_stream_frame(const uint8_t* data, size_t length,
                                        EventStreamMessage& msg) {
  // Minimum frame size: 4 + 4 + 4 + 0 + 0 + 4 = 16 bytes
  if (length < 16) {
    return 0;  // Incomplete
  }

  // Read prelude
  uint32_t total_length = ntohl(*reinterpret_cast<const uint32_t*>(data));
  uint32_t headers_length = ntohl(*reinterpret_cast<const uint32_t*>(data + 4));

  if (length < total_length) {
    return 0;  // Incomplete
  }

  // Parse headers
  size_t offset = 12;  // After prelude (total_len + headers_len + prelude_crc)
  while (offset < 12 + headers_length && offset + 1 < length) {
    // Header name length (1 byte)
    uint8_t name_len = data[offset++];
    if (offset + name_len >= length) break;

    std::string name(reinterpret_cast<const char*>(data + offset), name_len);
    offset += name_len;

    if (offset >= length) break;

    // Header value type (1 byte)
    uint8_t value_type = data[offset++];

    // Header value (type-dependent)
    if (value_type == 7) {  // String type
      if (offset + 2 > length) break;
      uint16_t value_len = ntohs(*reinterpret_cast<const uint16_t*>(data + offset));
      offset += 2;
      if (offset + value_len > length) break;

      std::string value(reinterpret_cast<const char*>(data + offset), value_len);
      offset += value_len;

      if (name == ":event-type") {
        msg.event_type = value;
      } else if (name == ":content-type") {
        msg.content_type = value;
      } else if (name == ":message-type") {
        msg.message_type = value;
      }
    } else {
      // Skip other types for now
      break;
    }
  }

  // Extract payload
  size_t payload_offset = 12 + headers_length;
  size_t payload_length = total_length - headers_length - 16;  // -16 for prelude + CRCs
  if (payload_offset + payload_length <= length) {
    msg.payload.assign(data + payload_offset, data + payload_offset + payload_length);
  }

  return total_length;
}

/// Curl write callback context for streaming data
struct StreamContext {
  std::mutex mutex;
  std::vector<uint8_t> buffer;
  std::atomic<bool> done{false};
  std::string error;

  // Callbacks
  std::function<void(const cloudwatch_event&)> on_event;
  std::function<void(const std::string&, const std::string&)> on_session_start;
  std::function<void(const std::string&)> on_error;
};

static size_t curl_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* ctx = static_cast<StreamContext*>(userdata);
  size_t total = size * nmemb;

  std::lock_guard<std::mutex> lock(ctx->mutex);

  // Append to buffer
  ctx->buffer.insert(ctx->buffer.end(), ptr, ptr + total);

  // Try to parse complete frames
  while (true) {
    EventStreamMessage msg;
    size_t consumed = parse_event_stream_frame(ctx->buffer.data(),
                                                ctx->buffer.size(), msg);
    if (consumed == 0) {
      break;  // Incomplete frame
    }

    // Remove consumed bytes
    ctx->buffer.erase(ctx->buffer.begin(), ctx->buffer.begin() + consumed);

    // Process message
    if (msg.message_type == "exception") {
      ctx->error = std::string(msg.payload.begin(), msg.payload.end());
      if (ctx->on_error) {
        ctx->on_error(ctx->error);
      }
    } else if (msg.event_type == "sessionStart") {
      // Parse JSON payload
      std::string json_str(msg.payload.begin(), msg.payload.end());
      Aws::Utils::Json::JsonValue json(json_str);
      if (json.WasParseSuccessful()) {
        auto view = json.View();
        if (ctx->on_session_start) {
          ctx->on_session_start(view.GetString("sessionId"),
                                 view.GetString("requestId"));
        }
      }
    } else if (msg.event_type == "sessionUpdate") {
      // Parse JSON payload containing log events
      std::string json_str(msg.payload.begin(), msg.payload.end());
      Aws::Utils::Json::JsonValue json(json_str);
      if (json.WasParseSuccessful()) {
        auto view = json.View();
        if (view.KeyExists("sessionResults")) {
          auto results = view.GetArray("sessionResults");
          for (size_t i = 0; i < results.GetLength(); ++i) {
            auto item = results.GetItem(i);
            cloudwatch_event event;
            event.log_stream_name = item.GetString("logStreamName");
            event.message = item.GetString("message");
            event.timestamp = item.GetInt64("timestamp");
            event.ingestion_time = item.GetInt64("ingestionTime");

            if (ctx->on_event) {
              ctx->on_event(event);
            }
          }
        }
      }
    }
  }

  return total;
}

// LiveTailClient implementation

LiveTailClient::LiveTailClient(const cloudwatch_config& config)
    : config_(config), ctx_(std::make_unique<StreamContext>()) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

LiveTailClient::~LiveTailClient() {
  stop();
  curl_global_cleanup();
}

void LiveTailClient::start(
    std::function<void(const cloudwatch_event&)> on_event,
    std::function<void(const std::string&, const std::string&)> on_session_start,
    std::function<void(const std::string&)> on_error) {
  ctx_->on_event = std::move(on_event);
  ctx_->on_session_start = std::move(on_session_start);
  ctx_->on_error = std::move(on_error);
  ctx_->done.store(false);

  worker_ = std::thread([this]() { run(); });
}

void LiveTailClient::stop() {
  ctx_->done.store(true);
  if (worker_.joinable()) {
    worker_.join();
  }
}

bool LiveTailClient::is_running() const {
  return !ctx_->done.load();
}

void LiveTailClient::run() {
  CURL* curl = curl_easy_init();
  if (!curl) {
    if (ctx_->on_error) {
      ctx_->on_error("Failed to initialize curl");
    }
    return;
  }

  // Build endpoint URL
  std::string region = config_.region.value_or("us-east-1");
  std::string endpoint = "https://streaming-logs." + region +
                         ".amazonaws.com/StartLiveTail";

  // Build request body
  Aws::Utils::Json::JsonValue request_body;
  Aws::Utils::Array<Aws::Utils::Json::JsonValue> log_groups(1);
  log_groups[0] = Aws::Utils::Json::JsonValue(config_.log_group_identifier);
  request_body.WithArray("logGroupIdentifiers", std::move(log_groups));

  if (!config_.log_stream_names.empty()) {
    Aws::Utils::Array<Aws::Utils::Json::JsonValue> streams(
        config_.log_stream_names.size());
    for (size_t i = 0; i < config_.log_stream_names.size(); ++i) {
      streams[i] = Aws::Utils::Json::JsonValue(config_.log_stream_names[i]);
    }
    request_body.WithArray("logStreamNames", std::move(streams));
  }

  if (!config_.log_stream_name_prefixes.empty()) {
    Aws::Utils::Array<Aws::Utils::Json::JsonValue> prefixes(
        config_.log_stream_name_prefixes.size());
    for (size_t i = 0; i < config_.log_stream_name_prefixes.size(); ++i) {
      prefixes[i] = Aws::Utils::Json::JsonValue(config_.log_stream_name_prefixes[i]);
    }
    request_body.WithArray("logStreamNamePrefixes", std::move(prefixes));
  }

  if (!config_.filter_pattern.empty()) {
    request_body.WithString("logEventFilterPattern", config_.filter_pattern);
  }

  std::string body = request_body.View().WriteCompact();

  // Get AWS credentials
  Aws::Auth::DefaultAWSCredentialsProviderChain cred_provider;
  auto creds = cred_provider.GetAWSCredentials();

  if (creds.GetAWSAccessKeyId().empty()) {
    if (ctx_->on_error) {
      ctx_->on_error("No AWS credentials available");
    }
    curl_easy_cleanup(curl);
    return;
  }

  // Sign the request (AWS SigV4)
  auto now = Aws::Utils::DateTime::Now();
  std::string date_str = now.ToGmtString("%Y%m%dT%H%M%SZ");
  std::string date_short = now.ToGmtString("%Y%m%d");

  // Calculate content hash
  auto content_hash = Aws::Utils::HashingUtils::HexEncode(
      Aws::Utils::HashingUtils::CalculateSHA256(body));

  // Build canonical request
  std::string host = "streaming-logs." + region + ".amazonaws.com";
  std::string canonical_uri = "/StartLiveTail";
  std::string canonical_query = "";
  std::string canonical_headers =
      "content-type:application/json\n"
      "host:" + host + "\n"
      "x-amz-content-sha256:" + content_hash + "\n"
      "x-amz-date:" + date_str + "\n";
  std::string signed_headers = "content-type;host;x-amz-content-sha256;x-amz-date";

  std::string canonical_request =
      "POST\n" + canonical_uri + "\n" + canonical_query + "\n" +
      canonical_headers + "\n" + signed_headers + "\n" + content_hash;

  // Build string to sign
  std::string algorithm = "AWS4-HMAC-SHA256";
  std::string credential_scope = date_short + "/" + region + "/logs/aws4_request";
  auto canonical_hash = Aws::Utils::HashingUtils::HexEncode(
      Aws::Utils::HashingUtils::CalculateSHA256(canonical_request));
  std::string string_to_sign =
      algorithm + "\n" + date_str + "\n" + credential_scope + "\n" +
      canonical_hash;

  // Calculate signature using HMAC chain
  auto k_date = Aws::Utils::HashingUtils::CalculateSHA256HMAC(
      Aws::Utils::ByteBuffer(reinterpret_cast<const unsigned char*>(date_short.c_str()),
                             date_short.length()),
      Aws::Utils::ByteBuffer(reinterpret_cast<const unsigned char*>(
                                 ("AWS4" + creds.GetAWSSecretKey()).c_str()),
                             4 + creds.GetAWSSecretKey().length()));
  auto k_region = Aws::Utils::HashingUtils::CalculateSHA256HMAC(
      Aws::Utils::ByteBuffer(reinterpret_cast<const unsigned char*>(region.c_str()),
                             region.length()),
      k_date);
  auto k_service = Aws::Utils::HashingUtils::CalculateSHA256HMAC(
      Aws::Utils::ByteBuffer(reinterpret_cast<const unsigned char*>("logs"), 4),
      k_region);
  auto k_signing = Aws::Utils::HashingUtils::CalculateSHA256HMAC(
      Aws::Utils::ByteBuffer(reinterpret_cast<const unsigned char*>("aws4_request"), 12),
      k_service);
  auto signature_bytes = Aws::Utils::HashingUtils::CalculateSHA256HMAC(
      Aws::Utils::ByteBuffer(reinterpret_cast<const unsigned char*>(string_to_sign.c_str()),
                             string_to_sign.length()),
      k_signing);
  std::string signature = Aws::Utils::HashingUtils::HexEncode(signature_bytes);

  // Build authorization header
  std::string authorization =
      algorithm + " Credential=" + creds.GetAWSAccessKeyId() + "/" +
      credential_scope + ", SignedHeaders=" + signed_headers +
      ", Signature=" + signature;

  // Set curl options
  curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.length()));

  // Enable HTTP/2
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);

  // Set headers
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: application/vnd.amazon.eventstream");
  headers = curl_slist_append(headers, ("Host: " + host).c_str());
  headers = curl_slist_append(headers, ("X-Amz-Date: " + date_str).c_str());
  headers = curl_slist_append(headers, ("X-Amz-Content-Sha256: " + content_hash).c_str());
  headers = curl_slist_append(headers, ("Authorization: " + authorization).c_str());

  if (!creds.GetSessionToken().empty()) {
    headers = curl_slist_append(headers,
        ("X-Amz-Security-Token: " + creds.GetSessionToken()).c_str());
  }

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  // Set write callback for streaming response
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx_.get());

  // Perform request (will stream until done or error)
  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK && !ctx_->done.load()) {
    if (ctx_->on_error) {
      ctx_->on_error(std::string("curl error: ") + curl_easy_strerror(res));
    }
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
}

} // namespace livetail
} // namespace tenzir
