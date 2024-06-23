#ifndef __MEM_EXTRACT_H__
#define __MEM_EXTRACT_H__

#include <absl/types/span.h>
#include <cinttypes>
#include <cstring>

namespace amt {
void ExtractRaw(void *to, absl::Span<uint8_t> from) {
  std::memcpy(to, from.data(), from.size());
}

template <typename T> void Extract(T *to, absl::Span<uint8_t> from) {
  static_assert(std::is_pod<T>::value == true);
  die_if(sizeof(T) != from.size(), "size mismatch");
  ExtractRaw(to, from);
}

template <typename T> T Extract(absl::Span<uint8_t> from) {
  T ret{};
  Extract(&ret, from);
  return ret;
}

std::string ExtractString(absl::Span<uint8_t> data) {
  return std::string(reinterpret_cast<char *>(data.data()), data.size());
}

void FillRaw(absl::Span<uint8_t> to, const void *from) {
  std::memcpy(to.data(), from, to.size());
}

template <typename T> void Fill(absl::Span<uint8_t> to, const T &from) {
  static_assert(std::is_pod<T>::value == true);
  die_if(sizeof(T) != to.size(), "size mismatch");
  FillRaw(to, &from);
}

} // namespace amt

#endif // __MEM_EXTRACT_H__
