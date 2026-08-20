#include "ai_image.h"
#include "config.h"
#include "credentials.h"
#include "logging.h"
#include "sdcard_mgr.h"
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>
#include <stdlib.h>

static bool isBase64Char(uint8_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
}

// Buffered character reader over an fs::File (avoids 1-byte reads).
struct BufReader {
  fs::File& f;
  uint8_t buf[2048];
  size_t pos;
  size_t len;
  explicit BufReader(fs::File& file) : f(file), pos(0), len(0) {}
  bool next(uint8_t& c) {
    if (pos >= len) {
      len = f.read(buf, sizeof(buf));
      pos = 0;
      if (len == 0) return false;
    }
    c = buf[pos++];
    return true;
  }
};

// Decode one base64 chunk (n must be a multiple of 4) and append to the file.
static bool base64DecodeChunk(const uint8_t* b64, size_t n, fs::File& out) {
  if (n % 4 != 0) return false;
  static uint8_t tmp[3072];  // static: keep it off the (8 KB) loop-task stack
  size_t olen = 0;
  const int rc = mbedtls_base64_decode(tmp, sizeof(tmp), &olen, b64, n);
  if (rc != 0) return false;
  return out.write(tmp, olen) == olen;
}

// Scan the JSON response for "b64_json":"..." and write the decoded PNG bytes
// to png. The base64 payload is decoded in chunks (4 KB of chars -> 3 KB).
static bool extractBase64ToFile(fs::File& json, fs::File& png) {
  BufReader r(json);
  // Phase 1: find the "b64_json" key (tolerant of "key":"..." and "key": "...").
  const char key[] = "\"b64_json\"";
  const size_t klen = sizeof(key) - 1;
  size_t ki = 0;
  uint8_t c;
  while (r.next(c)) {
    if (c == (uint8_t)key[ki]) {
      ki++;
      if (ki == klen) break;
    } else {
      ki = (c == (uint8_t)key[0]) ? 1 : 0;
    }
  }
  if (ki != klen) {
    LOG.println("[AI] b64_json key not found in response");
    return false;
  }

  // Phase 2: skip ':' and any whitespace to the value's opening quote.
  bool foundQuote = false;
  while (r.next(c)) {
    if (c == '"') {
      foundQuote = true;
      break;
    }
  }
  if (!foundQuote) {
    LOG.println("[AI] b64_json value has no opening quote");
    return false;
  }

  // Phase 3: decode base64 chars up to the closing quote.
  static uint8_t b64[4096];  // static: keep it off the (8 KB) loop-task stack
  size_t bi = 0;
  while (r.next(c)) {
    if (c == '"') break;  // end of the b64_json value
    if (isBase64Char(c)) {
      b64[bi++] = c;
      if (bi == sizeof(b64)) {
        if (!base64DecodeChunk(b64, bi, png)) {
          LOG.println("[AI] base64 chunk decode failed");
          return false;
        }
        bi = 0;
      }
    }
    // whitespace/newlines inside the JSON string are skipped (defensive)
  }
  if (bi > 0 && !base64DecodeChunk(b64, bi, png)) {
    LOG.println("[AI] base64 tail decode failed");
    return false;
  }
  return true;
}

// Read a single byte from the TLS client, yielding while no data is buffered.
// Returns the byte, or -1 on connection close / deadline.
static int readByte(WiFiClientSecure& client, unsigned long deadline) {
  while (millis() < deadline) {
    const int c = client.read();
    if (c >= 0) return c;
    if (!client.connected() && client.available() == 0) return -1;
    delay(5);
  }
  return -1;
}

// Read one CR/LF-terminated line from the client into `line`. Returns true if
// any data was received (a blank line yields an empty string).
static bool readLine(WiFiClientSecure& client, String& line, unsigned long deadline) {
  line = "";
  while (millis() < deadline) {
    const int c = readByte(client, deadline);
    if (c < 0) return line.length() > 0;
    if (c == '\n') return true;
    line += (char)c;
  }
  return line.length() > 0;
}

// Read up to `want` bytes from the client and append them to `out`. Uses a
// static buffer to keep it off the (8 KB) loop-task stack. Returns the number
// of bytes written, or -1 on connection close / deadline.
static int readSome(WiFiClientSecure& client, fs::File& out, size_t want,
                    unsigned long deadline) {
  static uint8_t buf[2048];  // static: keep it off the (8 KB) loop-task stack
  if (want > sizeof(buf)) want = sizeof(buf);
  while (millis() < deadline) {
    const int n = client.read(buf, want);
    if (n > 0) {
      out.write(buf, n);
      return n;
    }
    if (!client.connected() && client.available() == 0) return -1;
    delay(10);
  }
  return -1;
}

bool aiGenerateImage(const String& prompt) {
  // --- Build the JSON request body ---
  String escaped = prompt;
  escaped.replace("\\", "\\\\");   // backslash -> \\ (JSON)
  escaped.replace("\"", "\\\"");   // quote     -> \"
  escaped.replace("\n", "\\n");    // newline   -> \n
  escaped.replace("\r", "");       // strip CR

  String body = "{\"model\":\"";
  body += AI_MODEL;
  body += "\",\"prompt\":\"";
  body += escaped;
  body += "\",\"size\":\"";
  body += AI_SIZE;
  body += "\",\"n\":1";
  if (AI_QUALITY[0] != '\0') {
    body += ",\"quality\":\"";
    body += AI_QUALITY;
    body += "\"";
  }
  body += "}";

  const unsigned long timeoutMs = (unsigned long)AI_TIMEOUT_MS;
  const uint32_t timeoutSec = (uint32_t)(timeoutMs / 1000UL);

  // --- Send the request over a raw TLS connection ---
  // HTTPClient::setTimeout() takes a uint16_t and its connect() path clamps
  // the socket timeout to ~65 s, which is too short for gpt-image-1.
  // WiFiClientSecure::setTimeout() takes seconds (uint32_t), so the full
  // AI_TIMEOUT_MS (120 s) is honored for real.
  for (int attempt = 1; attempt <= AI_MAX_ATTEMPTS; attempt++) {
    if (attempt > 1) {
      LOG.printf("[AI] retry %d/%d in %lu ms\n", attempt, AI_MAX_ATTEMPTS,
                 (unsigned long)AI_RETRY_DELAY_MS);
      delay(AI_RETRY_DELAY_MS);
    }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(timeoutSec);  // read + connect/handshake timeout

    LOG.printf("[AI] attempt %d/%d POST /v1/images/generations ...\n",
               attempt, AI_MAX_ATTEMPTS);
    if (!client.connect("api.openai.com", 443)) {
      LOG.println("[AI] connect failed");
      continue;
    }

    String req;
    req.reserve(body.length() + 256);
    req += "POST /v1/images/generations HTTP/1.1\r\n";
    req += "Host: api.openai.com\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Authorization: Bearer ";
    req += OPENAI_API_KEY;
    req += "\r\nContent-Length: ";
    req += String(body.length());
    req += "\r\nConnection: close\r\n\r\n";
    req += body;

    size_t sent = 0;
    while (sent < req.length()) {
      const size_t n = client.write((const uint8_t*)req.c_str() + sent,
                                    req.length() - sent);
      if (n == 0) break;
      sent += n;
    }
    if (sent != req.length()) {
      LOG.println("[AI] request send failed");
      client.stop();
      continue;
    }

    // --- Read the status line + headers (bounded by AI_TIMEOUT_MS) ---
    const unsigned long headerDeadline = millis() + timeoutMs;
    int httpCode = 0;
    long contentLength = -1;
    bool chunked = false;
    bool firstLine = true;

    while (millis() < headerDeadline) {
      String line;
      if (!readLine(client, line, headerDeadline)) break;
      line.trim();
      if (firstLine) {
        firstLine = false;
        const int sp1 = line.indexOf(' ');
        const int sp2 = line.indexOf(' ', sp1 + 1);
        if (sp1 > 0) {
          httpCode = line.substring(sp1 + 1, sp2 > 0 ? sp2 : line.length()).toInt();
        }
        LOG.printf("[AI] HTTP %d\n", httpCode);
        if (httpCode == 0) break;
      } else if (line.length() == 0) {
        break;  // end of headers
      } else {
        const int colon = line.indexOf(':');
        if (colon > 0) {
          const String name = line.substring(0, colon);
          String value = line.substring(colon + 1);
          value.trim();
          if (name.equalsIgnoreCase("Content-Length")) {
            contentLength = value.toInt();
          } else if (name.equalsIgnoreCase("Transfer-Encoding")) {
            chunked = (value.indexOf("chunked") >= 0);
          }
        }
      }
    }

    if (httpCode == 0) {
      LOG.printf("[AI] no HTTP response within %lu ms (attempt %d/%d)\n",
                 timeoutMs, attempt, AI_MAX_ATTEMPTS);
      client.stop();
      continue;
    }

    if (httpCode != 200) {
      LOG.printf("[AI] image API HTTP %d (attempt %d/%d)\n",
                 httpCode, attempt, AI_MAX_ATTEMPTS);
      String err;
      const unsigned long errDeadline = millis() + 5000UL;
      while (err.length() < 4096 && millis() < errDeadline) {
        const int c = client.read();
        if (c >= 0) {
          err += (char)c;
        } else if (!client.connected() && client.available() == 0) {
          break;
        } else {
          delay(5);
        }
      }
      if (err.length() > 0) LOG.printf("[AI] error body: %s\n", err.c_str());
      client.stop();
      // 4xx (bad request / auth / quota / rate limit) will not improve on retry.
      if (httpCode >= 400 && httpCode < 500) break;
      continue;
    }

    // --- Stream the response body to the SD card (no large RAM buffer) ---
    fs::File resp = SD.open(SD_TMP_RESP, FILE_WRITE);
    if (!resp) {
      LOG.println("[AI] cannot create response file");
      client.stop();
      continue;
    }

    const unsigned long bodyDeadline = millis() + timeoutMs;
    int total = 0;

    if (chunked && contentLength < 0) {
      // Transfer-Encoding: chunked - decode the framing while streaming.
      while (millis() < bodyDeadline) {
        String sizeLine;
        if (!readLine(client, sizeLine, bodyDeadline)) break;
        sizeLine.trim();
        if (sizeLine.length() == 0) continue;
        const int semi = sizeLine.indexOf(';');
        if (semi >= 0) sizeLine = sizeLine.substring(0, semi);
        const long chunkLen = strtol(sizeLine.c_str(), nullptr, 16);
        if (chunkLen <= 0) break;  // terminating 0-length chunk
        long left = chunkLen;
        while (left > 0) {
          const int n = readSome(client, resp, (size_t)left, bodyDeadline);
          if (n <= 0) break;
          total += n;
          left -= n;
        }
        if (left > 0) break;  // chunk was truncated
        for (int i = 0; i < 2; i++) readByte(client, bodyDeadline);  // trailing CRLF
      }
    } else if (contentLength >= 0) {
      // Content-Length: read exactly that many bytes.
      long left = contentLength;
      while (left > 0) {
        const int n = readSome(client, resp, (size_t)left, bodyDeadline);
        if (n <= 0) break;
        total += n;
        left -= n;
      }
    } else {
      // No framing headers: read until the server closes (Connection: close).
      while (true) {
        const int n = readSome(client, resp, 2048, bodyDeadline);
        if (n <= 0) break;
        total += n;
      }
    }

    resp.close();
    client.stop();
    LOG.printf("[AI] response body: %d bytes\n", total);
    if (total <= 0) {
      sdDelete(SD_TMP_RESP);
      continue;
    }

    // --- Extract the base64 PNG payload to SD_TMP_PNG ---
    fs::File js = SD.open(SD_TMP_RESP, FILE_READ);
    fs::File png = SD.open(SD_TMP_PNG, FILE_WRITE);
    bool ok = false;
    if (js && png) {
      // Debug: dump the head of the JSON response to diagnose b64_json
      // parsing. Gated behind AI_DEBUG_DUMP (config.h).
      if (AI_DEBUG_DUMP) {
        uint8_t head[256];
        const int hn = js.read(head, sizeof(head) - 1);
        if (hn > 0) head[hn] = 0; else head[0] = 0;
        LOG.printf("[AI] response head: %s\n", (const char*)head);
        js.seek(0);  // rewind for extractBase64ToFile
      }
      ok = extractBase64ToFile(js, png);
    } else {
      LOG.println("[AI] cannot open temp files");
    }
    if (js) js.close();
    if (png) png.close();
    sdDelete(SD_TMP_RESP);
    if (!ok) sdDelete(SD_TMP_PNG);
    if (ok) return true;

    LOG.println("[AI] response parsing failed - retrying.");
  }

  LOG.println("[AI] giving up after all attempts.");
  return false;
}
