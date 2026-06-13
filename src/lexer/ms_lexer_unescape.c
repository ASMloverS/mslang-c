#include "mslang/ms_lexer.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Encode codepoint cp as UTF-8 into buf (must have >= 4 bytes).
// Returns byte count written, or 0 if cp is invalid.
static int encodeUtf8(uint32_t cp, char* buf) {
  if (cp <= 0x7F) {
    buf[0] = (char)cp;
    return 1;
  }
  if (cp <= 0x7FF) {
    buf[0] = (char)(0xC0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  }
  if (cp <= 0xFFFF) {
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  }
  // cp <= 0x10FFFF
  buf[0] = (char)(0xF0 | (cp >> 18));
  buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
  buf[3] = (char)(0x80 | (cp & 0x3F));
  return 4;
}

// ---------------------------------------------------------------------------
// msUnescapeString
// ---------------------------------------------------------------------------

int msUnescapeString(const char* raw, uint32_t rawLen,
                     char* outBuf, uint32_t outBufLen, uint32_t* outLen,
                     char* errBuf, uint32_t errBufLen) {
  if (rawLen < 2) {
    snprintf(errBuf, errBufLen, "invalid string literal: too short");
    return -1;
  }
  // raw[0] == '"', raw[rawLen-1] == '"'
  uint32_t i = 1;              // skip opening quote
  uint32_t end = rawLen - 1;   // stop before closing quote
  uint32_t out = 0;

  while (i < end) {
    char c = raw[i];

    if (c != '\\') {
      if (out >= outBufLen) {
        snprintf(errBuf, errBufLen, "output buffer too small");
        return -1;
      }
      outBuf[out++] = c;
      i++;
      continue;
    }

    // c == '\\'
    i++;
    if (i >= end) {
      snprintf(errBuf, errBufLen,
               "invalid escape sequence at offset %u", i - 1);
      return -1;
    }

    char esc = raw[i];
    i++;

    if (out >= outBufLen) {
      snprintf(errBuf, errBufLen, "output buffer too small");
      return -1;
    }
    switch (esc) {
      case 'n':  outBuf[out++] = '\x0A'; break;
      case 't':  outBuf[out++] = '\x09'; break;
      case 'r':  outBuf[out++] = '\x0D'; break;
      case '\\': outBuf[out++] = '\\';   break;
      case '"':  outBuf[out++] = '"';    break;
      case 'x': {
        if (i + 1 >= end || hexVal(raw[i]) < 0 || hexVal(raw[i + 1]) < 0) {
          snprintf(errBuf, errBufLen,
                   "invalid escape sequence at offset %u", i - 2);
          return -1;
        }
        // out < outBufLen already checked above the switch
        outBuf[out++] = (char)(uint8_t)(hexVal(raw[i]) << 4 | hexVal(raw[i + 1]));
        i += 2;
        break;
      }
      case 'u': {
        if (i >= end || raw[i] != '{') {
          snprintf(errBuf, errBufLen,
                   "invalid escape sequence at offset %u", i - 2);
          return -1;
        }
        i++; // skip '{'
        uint32_t cp = 0;
        uint32_t digitCount = 0;
        while (i < end && raw[i] != '}') {
          int dv = hexVal(raw[i]);
          if (dv < 0) {
            snprintf(errBuf, errBufLen,
                     "invalid escape sequence at offset %u", i);
            return -1;
          }
          cp = cp * 16 + (uint32_t)dv;
          digitCount++;
          i++;
        }
        if (i >= end || raw[i] != '}' || digitCount == 0) {
          snprintf(errBuf, errBufLen,
                   "invalid escape sequence at offset %u", i - 2);
          return -1;
        }
        i++; // skip '}'
        if (cp > 0x10FFFF) {
          snprintf(errBuf, errBufLen,
                   "unicode codepoint out of range at offset %u", i);
          return -1;
        }
        if (cp >= 0xD800 && cp <= 0xDFFF) {
          snprintf(errBuf, errBufLen,
                   "unicode surrogate codepoint at offset %u", i);
          return -1;
        }
        char utf8[4];
        int bytes = encodeUtf8(cp, utf8);
        if (out + (uint32_t)bytes > outBufLen) {
          snprintf(errBuf, errBufLen, "output buffer too small");
          return -1;
        }
        memcpy(outBuf + out, utf8, (size_t)bytes);
        out += (uint32_t)bytes;
        break;
      }
      default: {
        snprintf(errBuf, errBufLen,
                 "invalid escape sequence at offset %u", i - 2);
        return -1;
      }
    }
  }

  *outLen = out;
  return 0;
}
