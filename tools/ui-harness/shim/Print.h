#ifndef HARNESS_PRINT_H
#define HARNESS_PRINT_H

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  size_t write(const char* s) { size_t n = 0; while (*s) n += write((uint8_t)*s++); return n; }
  size_t print(const char* s) { return write(s); }
  size_t print(const String& s) { return write(s.c_str()); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v) { return print(String(v)); }
};

#endif
