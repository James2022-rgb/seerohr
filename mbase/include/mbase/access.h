#pragma once

#define MBASE_DISALLOW_COPY(T) \
  T(T const& rhs) = delete; \
  T& operator=(T const& rhs) = delete;

#define MBASE_DISALLOW_MOVE(T) \
  T(T&& rhs) = delete; \
  T& operator=(T&& rhs) = delete;

#define MBASE_DEFAULT_COPY(T) \
  T(T const& rhs) = default; \
  T& operator=(T const& rhs) = default;

#define MBASE_DEFAULT_MOVE(T) \
  T(T&& rhs) = default; \
  T& operator=(T&& rhs) = default;

#define MBASE_DEFAULT_COPY_MOVE(T) \
  MBASE_DEFAULT_COPY(T) \
  MBASE_DEFAULT_MOVE(T)

#define MBASE_DISALLOW_COPY_MOVE(T) \
  MBASE_DISALLOW_COPY(T) \
  MBASE_DISALLOW_MOVE(T)

#define MBASE_DEFAULT_COPY_DISALLOW_MOVE(T) \
  MBASE_DEFAULT_COPY(T) \
  MBASE_DISALLOW_MOVE(T)

#define MBASE_DISALLOW_COPY_DEFAULT_MOVE(T) \
  MBASE_DISALLOW_COPY(T) \
  MBASE_DEFAULT_MOVE(T)
