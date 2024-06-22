#ifndef __DIE_H__
#define __DIE_H__

#include <cstdio>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

template <typename... Args>
void die [[noreturn]] (const std::string &format, Args &&...args) {
  int size_s = std::snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...) + 1;
  if (size_s <= 0) {
    std::cerr << "Failed to format error message.";
    std::terminate();
  }
  auto buf = std::make_unique<char[]>(size_s);
  std::snprintf(buf.get(), size_s, format.c_str(), std::forward<Args>(args)...);
  std::cerr << buf.get() << std::endl;
  std::terminate();
}

#define die_if(cond, fmt, ...)                                                           \
  do {                                                                                   \
    if (cond) {                                                                          \
      die("Failure condition \"%s\": " fmt, #cond, ##__VA_ARGS__);                       \
    }                                                                                    \
  } while (0)

#endif // __DIE_H__
