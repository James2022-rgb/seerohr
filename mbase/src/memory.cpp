// TU header --------------------------------------------
#include "mbase/memory.h"

// c++ headers ------------------------------------------
#include <cstdlib>
#include <print>

// project headers --------------------------------------
#include "mbase/platform.h"

namespace mbase {

void* AlignedAlloc(uint64_t size, uint64_t alignment) {
#if MBASE_PLATFORM_WINDOWS
  return _aligned_malloc(size, alignment);
#else
  // Assume POSIX
  void* block = nullptr;
  posix_memalign(&block, alignment, size);
  if (block == nullptr) {
    std::print("Failed to allocate memory; size:{}, alignment:{}", size, alignment);
  }
  return block;
#endif
}
void AlignedFree(void* block) {
#if MBASE_PLATFORM_WINDOWS
  _aligned_free(block);
#else
  // Assume POSIX
  free(block);
#endif
}

} // namespace mbase
