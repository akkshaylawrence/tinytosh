#ifndef HARNESS_ARDUINO_H
#define HARNESS_ARDUINO_H

// Just enough of the Arduino core to build the UI library, the screens and Adafruit_GFX on a host.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <algorithm>
#include <string>

using std::max;
using std::min;

typedef bool boolean;
typedef uint8_t byte;

#define PROGMEM
#define F(text) (text)
#define radians(deg) ((deg) * 0.017453292519943295)
#define constrain(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

class __FlashStringHelper;

unsigned long millis();
unsigned long micros();

class String {
public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned int v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  String(long long v) : s_(std::to_string(v)) {}
  String(double v, int decimals = 2) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    s_ = buf;
  }

  unsigned int length() const { return s_.size(); }
  const char* c_str() const { return s_.c_str(); }
  char operator[](size_t i) const { return s_[i]; }
  void reserve(size_t n) { s_.reserve(n); }

  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* o) { s_ += o; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* o) const { return s_ == o; }
  bool operator!=(const String& o) const { return s_ != o.s_; }
  bool operator!=(const char* o) const { return s_ != o; }

  int indexOf(char c, unsigned int from = 0) const { size_t p = s_.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const char* t) const { size_t p = s_.find(t); return p == std::string::npos ? -1 : (int)p; }
  int lastIndexOf(char c) const { size_t p = s_.rfind(c); return p == std::string::npos ? -1 : (int)p; }
  String substring(unsigned int from) const { return from >= s_.size() ? String() : String(s_.substr(from)); }
  String substring(unsigned int from, unsigned int to) const { return from >= s_.size() || to <= from ? String() : String(s_.substr(from, to - from)); }
  void remove(unsigned int index) { if (index < s_.size()) s_.erase(index); }
  void toUpperCase() { for (char& c : s_) c = toupper((unsigned char)c); }
  void trim() {
    size_t a = s_.find_first_not_of(" \t\r\n"), b = s_.find_last_not_of(" \t\r\n");
    s_ = a == std::string::npos ? "" : s_.substr(a, b - a + 1);
  }
  void replace(const char* from, const char* to) {
    const size_t n = strlen(from);
    for (size_t p = 0; n && (p = s_.find(from, p)) != std::string::npos; p += strlen(to)) s_.replace(p, n, to);
  }
  bool equalsIgnoreCase(const String& o) const { return strcasecmp(s_.c_str(), o.s_.c_str()) == 0; }

private:
  std::string s_;
};

inline String operator+(const String& a, const String& b) { String r = a; r += b; return r; }
inline String operator+(const String& a, const char* b) { String r = a; r += b; return r; }
inline String operator+(const char* a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, char b) { String r = a; r += b; return r; }

#include "Print.h"

#endif
