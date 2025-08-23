#pragma once

#include <memory>

namespace mbase {

template<class T, class TDeleter, class ... Args>
[[nodiscard]] inline std::shared_ptr<T> MakeSharedWithDeleter(TDeleter const& deleter, Args&& ... args) {
  return std::shared_ptr<T>(new T(std::forward<Args>(args)...), deleter);
}

[[nodiscard]] void* AlignedAlloc(uint64_t size, uint64_t alignment);
void AlignedFree(void* block);

} // namespace mbase
