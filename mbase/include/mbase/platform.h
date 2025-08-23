#pragma once

#if defined(_MSC_VER)
# define MBASE_PLATFORM_WINDOWS 1
# define MBASE_PLATFORM_LINUX   0
# define MBASE_PLATFORM_ANDROID 0
# define MBASE_PLATFORM_WEB     0
#elif defined(__ANDROID__)
# define MBASE_PLATFORM_WINDOWS 0
# define MBASE_PLATFORM_LINUX   0
# define MBASE_PLATFORM_ANDROID 1
# define MBASE_PLATFORM_WEB     0
// __linux__ is also defined on Android, hence the order of the checks.
#elif __linux__
# define MBASE_PLATFORM_WINDOWS 0
# define MBASE_PLATFORM_LINUX   1
# define MBASE_PLATFORM_ANDROID 0
# define MBASE_PLATFORM_WEB     0
#elif defined(__EMSCRIPTEN__)
# define MBASE_PLATFORM_WINDOWS 0
# define MBASE_PLATFORM_LINUX   0
# define MBASE_PLATFORM_ANDROID 0
# define MBASE_PLATFORM_WEB     1
#endif

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
# define MBASE_PLATFORM_64_BIT 1
# define MBASE_PLATFORM_32_BIT 0
#else
# define MBASE_PLATFORM_64_BIT 0
# define MBASE_PLATFORM_32_BIT 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
# define MBASE_PLATFORM_X86_32  0
# define MBASE_PLATFORM_X86_64  1
# define MBASE_PLATFORM_AARCH32 0
# define MBASE_PLATFORM_AARCH64 0
#elif defined(__i386__) || defined(_M_IX86)
# define MBASE_PLATFORM_X86_32  1
# define MBASE_PLATFORM_X86_64  0
# define MBASE_PLATFORM_AARCH32 0
# define MBASE_PLATFORM_AARCH64 0
#elif defined(__aarch64__) || defined(_M_ARM64)
# define MBASE_PLATFORM_X86_32  0
# define MBASE_PLATFORM_X86_64  0
# define MBASE_PLATFORM_AARCH32 0
# define MBASE_PLATFORM_AARCH64 1
#elif defined(__arm__) || defined(_M_ARM)
# define MBASE_PLATFORM_X86_32  0
# define MBASE_PLATFORM_X86_64  0
# define MBASE_PLATFORM_AARCH32 1
# define MBASE_PLATFORM_AARCH64 0
#endif

#define MBASE_PLATFORM_X86 (MBASE_PLATFORM_X86_32 || MBASE_PLATFORM_X86_64)
#define MBASE_PLATFORM_AARCH (MBASE_PLATFORM_AARCH32 || MBASE_PLATFORM_AARCH64)
