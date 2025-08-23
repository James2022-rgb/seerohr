#pragma once

#include <cstddef>
#include <cstring>
#include <cassert>

#include <type_traits>
#include <iterator>
#include <algorithm>
#include <stdexcept>

#include "mbase/memory.h"

namespace mbase {

namespace detail {

template<class TIterator>
struct construct_strategy final {
  using value_type = typename std::iterator_traits<TIterator>::value_type;

  template<class T = value_type, class V = std::enable_if_t<std::is_trivially_default_constructible_v<T>>>
  static void on_element([[maybe_unused]]  TIterator it) {
    static_assert(std::is_trivially_default_constructible_v<value_type>);
    // No-op for TriviallyDefaultConstructible types.
  }
  template<class T = value_type, class V = std::enable_if_t<std::is_trivially_default_constructible_v<T>>>
  static void on_range([[maybe_unused]]  TIterator first, [[maybe_unused]] TIterator last) {
    static_assert(std::is_trivially_default_constructible_v<value_type>);
    // No-op for TriviallyDefaultConstructible types.
  }

  template<class ... Args>
  static void on_element(TIterator it, Args&& ... args) {
    new(std::addressof(*it)) value_type(std::forward<Args>(args)...);
  }

  template<class ... Args>
  static void on_range(TIterator first, TIterator last, Args&& ... args) {
    for (auto it = first; it != last; ++it) {
      new(std::addressof(*it)) value_type(std::forward<Args>(args)...);
    }
  }
};

template<class TIterator, class U = void>
struct destruct_strategy final {
  using value_type = typename std::iterator_traits<TIterator>::value_type;

  static void on_element(TIterator it) {
    std::addressof(*it)->~value_type();
  }

  static void on_range(TIterator first, TIterator last) {
    for (auto it = first; it != last; ++it) {
      std::addressof(*it)->~value_type();
    }
  }
};

template<class TIterator>
struct destruct_strategy<TIterator, std::enable_if_t<std::is_trivially_destructible_v<typename std::iterator_traits<TIterator>::value_type>>> final {
  static void on_element([[maybe_unused]] TIterator it) {
    // No-op for TriviallyDestructible types.
  }
  static void on_range([[maybe_unused]]  TIterator first, [[maybe_unused]] TIterator last) {
    // No-op for TriviallyDestructible types.
  }
};

template<class TInputIterator, class TOutputIterator, class U = void>
struct non_overlapping_copy_strategy final {
  static void call(TInputIterator first, TInputIterator last, TOutputIterator position) {
    for (auto it = first; it != last; ++it, ++position) {
      construct_strategy<TOutputIterator>::on_element(position, *it);
    }
  }
};

template<class TInputIterator, class TOutputIterator, class U = void>
struct non_overlapping_move_strategy final {
  static void call(TInputIterator first, TInputIterator last, TOutputIterator position) {
    for (auto it = first, out = position; it != last; ++it, ++out) {
      construct_strategy<TOutputIterator>::on_element(out, std::move(*it));
    }
  }
};

template<class TValue>
[[nodiscard]] TValue RoundtoNextPowerOf2(TValue value) {
  --value;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  if constexpr (8 <= sizeof(TValue)) {
    value |= value >> 32;
  }
  ++value;
  return value;
}

} // namespace detail

template<class TValue>
class VectorBase {
public:
  using value_type = TValue;
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using iterator = value_type*;
  using const_iterator = value_type const*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() noexcept { return get_storage_pointer_impl(); }
  const_iterator begin() const noexcept { return get_storage_pointer_impl(); }

  iterator end() noexcept { return get_storage_pointer_impl() + size_; }
  const_iterator end() const noexcept { return get_storage_pointer_impl() + size_; }

  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

  [[nodiscard]] size_type size() const noexcept { return size_; }
  [[nodiscard]] size_type size_in_bytes() const noexcept { return sizeof(value_type) * size_; }
  [[nodiscard]] size_type max_size() const noexcept { return max_size_impl(); }
  [[nodiscard]] size_type capacity() const noexcept { return capacity_impl(); }

  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

  reference operator[](size_type i) noexcept { return *(get_storage_pointer_impl() + i); }
  const_reference operator[](size_type i) const noexcept { return *(get_storage_pointer_impl() + i); }

  reference at(size_type i) {
    if (size_ <= i) {
      throw std::out_of_range("Subscript out of range!");
    }
    return operator[](i);
  }
  const_reference at(size_type i) const {
    if (size_ <= i) {
      throw std::out_of_range("Subscript out of range!");
    }
    return operator[](i);
  }

  value_type* data() noexcept { return get_storage_pointer_impl(); }
  value_type const* data() const noexcept { return get_storage_pointer_impl(); }

  reference front() noexcept { return *begin(); }
  const_reference front() const noexcept { return *begin(); }
  reference back() noexcept { return *(end() - 1); }
  const_reference back() const noexcept { return *(end() - 1); }

  void clear() {
    detail::destruct_strategy<iterator>::on_range(begin(), end());
    size_ = 0;
  }

  void reserve(size_type new_capacity) noexcept {
    reserve_impl(new_capacity);
  }

  template<class ... Args>
  void resize(size_type new_size, Args&& ... args) {
    if (size_ <= new_size) {
      // Growing resize.
      auto old_size = size_;
      ensure_size_impl(new_size);
      detail::construct_strategy<iterator>::on_range(begin() + old_size, end(), std::forward<Args>(args)...);
    }
    else {
      // Shrinking resize.
      detail::destruct_strategy<iterator>::on_range(begin() + new_size, end());
      size_ = new_size;
    }
  }

  template<class TInputIterator>
  void assign(TInputIterator first, TInputIterator last) {
    if (size_ > 0) {
      clear();
    }

    ensure_size_impl(std::distance(first, last));
    detail::non_overlapping_copy_strategy<TInputIterator, iterator>::call(first, last, begin());
  }
  void assign(size_type n, value_type const& value) {
    if (size_ > 0) {
      clear();
    }

    ensure_size_impl(n);
    detail::construct_strategy<iterator>::on_range(begin(), end(), value);
  }
  void assign(std::initializer_list<value_type> const& values) {
    assign(std::begin(values), std::end(values));
  }

  void push_back(value_type const& value) {
    ensure_size_impl(size_ + 1);
    detail::construct_strategy<iterator>::on_element(end() - 1, value);
  }
  void push_back(value_type&& value) {
    ensure_size_impl(size_ + 1);
    detail::construct_strategy<iterator>::on_element(end() - 1, std::move(value));
  }

  template<class ... Args>
  reference emplace_back(Args&& ... args) {
    ensure_size_impl(size_ + 1);
    detail::construct_strategy<iterator>::on_element(end() - 1, std::forward<Args>(args)...);
    return back();
  }

  void pop_back() {
    detail::destruct_strategy<iterator>::on_element(end() - 1);
    --size_;
  }

  template<class TInputIterator>
  iterator insert(iterator position, TInputIterator first, TInputIterator last) {
    position = ensure_size_and_make_room(position, std::distance(first, last));;
    detail::non_overlapping_copy_strategy<TInputIterator, iterator>::call(first, last, position);
    return position;
  }
  template<class TInputIterator>
  TInputIterator insert_memcpyable(iterator position, TInputIterator first, TInputIterator last) {
    auto input_range_size = std::distance(first, last);
    position = ensure_size_and_make_room(position, input_range_size);
    memcpy(std::addressof(*position), std::addressof(*first), sizeof(value_type) * input_range_size);
    return position;
  }

  void insert(iterator position, size_type n, value_type const& x) {
    position = ensure_size_and_make_room(position, n);
    detail::construct_strategy<iterator>::on_range(position, position + n, x);
  }

  template<class TInputIterator>
  void append_memcpyable(TInputIterator first, TInputIterator last) {
    auto input_range_size = std::distance(first, last);
    auto old_end_position = size_;
    ensure_size_impl(size_ + input_range_size);
    auto valid_old_end_it = begin() + old_end_position;
    memcpy(std::addressof(*valid_old_end_it), std::addressof(*first), sizeof(value_type) * input_range_size);
  }
  template<class TInputIterator>
  void append_memcpyable_unsafe(TInputIterator first, TInputIterator last) {
    auto input_range_size = std::distance(first, last);
    auto old_end_position = size_;
    ensure_size_unsafe_impl(size_ + input_range_size);
    auto valid_old_end_it = begin() + old_end_position;
    memcpy(std::addressof(*valid_old_end_it), std::addressof(*first), sizeof(value_type) * input_range_size);
  }

  iterator erase(iterator position) {
    return erase(position, position + 1);
  }
  iterator erase(iterator first, iterator last) {
    detail::destruct_strategy<iterator>::on_range(first, last);
    move_range_to(last, end(), first);
    size_ -= std::distance(first, last);
    return first;
  }

protected:
  VectorBase() = default;
  virtual ~VectorBase() = default;
  [[nodiscard]] virtual value_type* get_storage_pointer_impl() = 0;
  [[nodiscard]] virtual value_type const* get_storage_pointer_impl() const = 0;
  virtual void ensure_size_impl(size_type new_size) = 0;
  virtual void ensure_size_unsafe_impl(size_type new_size) = 0;
  virtual void reserve_impl(size_type new_capacity) = 0;
  [[nodiscard]] virtual size_type max_size_impl() const noexcept = 0;
  [[nodiscard]] virtual size_type capacity_impl() const noexcept = 0;

  size_t size_ = 0;

private:
  iterator ensure_size_and_make_room(iterator position_it, size_type range_size) {
    auto position = std::distance(begin(), position_it);
    auto old_end_position = std::distance(begin(), end());

    ensure_size_impl(size_ + range_size);

    auto valid_position_it = begin() + position;
    auto valid_old_end_position_it = begin() + old_end_position;

    move_range_to_and_destruct(valid_position_it, valid_old_end_position_it, valid_position_it + range_size);
    return valid_position_it;
  }

  void move_range_to_and_destruct(iterator first, iterator last, iterator position) {
    move_range_to(first, last, position);
    detail::destruct_strategy<iterator>::on_range(first, first + std::distance(first, position));
  }
  void move_range_to(iterator first, iterator last, iterator position) {
    if (first < position) {
      detail::non_overlapping_move_strategy<reverse_iterator, reverse_iterator>::call(reverse_iterator(last), reverse_iterator(first), reverse_iterator(position + std::distance(first, last)));
    }
    if (position < first) {
      detail::non_overlapping_move_strategy<iterator, iterator>::call(first, last, position);
    }
  }
};

/// A vector with small size optimization.
template<class TValue, size_t InitialCapacity, size_t Alignment = std::max(alignof(TValue), sizeof(void*))>
class SmallVector final : public VectorBase<TValue> {
public:
  using base_type = VectorBase<TValue>;

  using value_type = typename base_type::value_type;
  using size_type = typename base_type::size_type;
  using difference_type = typename base_type::difference_type;
  using reference = typename base_type::reference;
  using const_reference = typename base_type::const_reference;
  using iterator = typename base_type::iterator;
  using const_iterator = typename base_type::const_iterator;
  using reverse_iterator = typename base_type::reverse_iterator;
  using const_reverse_iterator = typename base_type::const_reverse_iterator;

  [[nodiscard]] SmallVector() = default;
  [[nodiscard]] explicit SmallVector(size_t n) {
    ensure_size_impl(n);
    detail::construct_strategy<iterator>::on_range(this->begin(), this->end());
  }
  [[nodiscard]] SmallVector(size_t n, value_type const& value) {
    base_type::assign(n, value);
  }
  [[nodiscard]] SmallVector(SmallVector const& rhs) {
    base_type::assign(std::begin(rhs), std::end(rhs));
  }
  [[nodiscard]] SmallVector(SmallVector&& rhs) noexcept {
    move_construct_from(std::move(rhs));
  }
  template<size_t C2, size_t A2>
  [[nodiscard]] SmallVector(SmallVector<TValue, C2, A2> const& rhs) {
    base_type::assign(std::begin(rhs), std::end(rhs));
  }
  template<class TInputIterator>
  [[nodiscard]] SmallVector(TInputIterator first, TInputIterator last) {
    base_type::assign(first, last);
  }
  [[nodiscard]] SmallVector(std::initializer_list<value_type> const& values) {
    base_type::assign(std::begin(values), std::end(values));
  }
  ~SmallVector() override {
    if (base_type::size_ > 0) {
      base_type::clear();
    }

    if (storage_) {
      AlignedFree(storage_);
    }
  }

  SmallVector& operator=(SmallVector const& rhs) {
    if (this != &rhs) {
      base_type::assign(std::begin(rhs), std::end(rhs));
    }
    return *this;
  }
  SmallVector& operator=(SmallVector&& rhs) noexcept {
    base_type::clear();
    move_construct_from(std::move(rhs));
    return *this;
  }

  template<size_t C2, size_t A2>
  SmallVector& operator=(SmallVector<TValue, C2, A2> const& rhs) {
    base_type::assign(std::begin(rhs), std::end(rhs));
    return *this;
  }

protected:
  [[nodiscard]] value_type* get_storage_pointer_impl() override {
    return static_cast<value_type*>(storage_pointer_);
  }
  [[nodiscard]] value_type const* get_storage_pointer_impl() const override {
    return static_cast<value_type const*>(storage_pointer_);
  }
  void ensure_size_impl(size_type new_size) override {
    ensure_size_unsafe_impl(new_size);
  }
  void ensure_size_unsafe_impl(size_type new_size) override {
    reserve_capacity(new_size);
    base_type::size_ = new_size;
  }
  void reserve_impl(size_type new_capacity) override {
    reserve_capacity(new_capacity);
  }
  [[nodiscard]] size_type max_size_impl() const noexcept override {
    return std::numeric_limits<size_type>::max();
  }
  [[nodiscard]] size_type capacity_impl() const noexcept override {
    return capacity_;
  }

private:
  void reserve_capacity(size_type new_capacity) {
    if (new_capacity == 0) return;

    auto effective_new_capacity = detail::RoundtoNextPowerOf2(new_capacity);

    if (capacity_ <= InitialCapacity && effective_new_capacity <= InitialCapacity) {
      storage_pointer_ = std::launder(&initial_storage_);
    }

    if (capacity_ < effective_new_capacity) {
      if (capacity_ <= InitialCapacity) {
        if (InitialCapacity < effective_new_capacity) {
          storage_ = AlignedAlloc(sizeof(value_type) * effective_new_capacity, Alignment);

          auto src_first = reinterpret_cast<value_type*>(std::launder(&initial_storage_));
          auto dst_first = static_cast<value_type*>(storage_);

          detail::non_overlapping_move_strategy<iterator, iterator>::call(src_first, src_first + base_type::size_, dst_first);
          detail::destruct_strategy<iterator>::on_range(src_first, src_first + base_type::size_);

          storage_pointer_ = static_cast<value_type*>(storage_);
        }
      }
      else {
        auto new_storage = AlignedAlloc(sizeof(value_type) * effective_new_capacity, Alignment);

        auto src_first = static_cast<value_type*>(storage_);
        auto dst_first = reinterpret_cast<value_type*>(new_storage);

        detail::non_overlapping_move_strategy<iterator, iterator>::call(src_first, src_first + base_type::size_, dst_first);
        detail::destruct_strategy<iterator>::on_range(src_first, src_first + base_type::size_);

        AlignedFree(storage_);
        storage_ = new_storage;

        storage_pointer_ = static_cast<value_type*>(storage_);
      }

      capacity_ = effective_new_capacity;
    }
  }

  void move_construct_from(SmallVector&& rhs) {
    assert(this->empty());

    // TODO: Reuse already-allocated storage ?
    if (InitialCapacity < this->capacity_) {
      AlignedFree(this->storage_);
      this->storage_ = nullptr;

      this->capacity_ = InitialCapacity;
    }

    if (InitialCapacity < rhs.capacity_) {
      // Take rhs's guts.

      this->size_ = rhs.size_;
      this->capacity_ = rhs.capacity_;
      this->storage_ = rhs.storage_;
      this->storage_pointer_ = static_cast<value_type*>(this->storage_);

      rhs.size_ = 0;
      rhs.capacity_ = InitialCapacity;
      rhs.storage_ = nullptr;
      rhs.storage_pointer_ = std::launder(&rhs.initial_storage_);
    }
    else {
      if constexpr (std::is_trivially_copyable_v<TValue>) {
        memcpy(&this->initial_storage_, &rhs.initial_storage_, rhs.size_in_bytes());
        this->size_ = rhs.size_;
      }
      else {
        for (auto& v : rhs) {
          this->emplace_back(std::move(v));
        }
      }

      rhs.clear();
    }
  }

  size_type capacity_ = InitialCapacity;

  using InitialStorageType = std::byte[sizeof(value_type) * InitialCapacity];
  static_assert(std::is_trivially_destructible_v<InitialStorageType>);

  alignas(Alignment) InitialStorageType initial_storage_ {};

  void* storage_ = nullptr;

  void* storage_pointer_ = std::launder(&initial_storage_);
};

template<class TValue, size_t Capacity, size_t Alignment>
class StaticVectorImpl : public VectorBase<TValue> {
public:
  using base_type = VectorBase<TValue>;

  using value_type = typename base_type::value_type;
  using size_type = typename base_type::size_type;
  using difference_type = typename base_type::difference_type;
  using reference = typename base_type::reference;
  using const_reference = typename base_type::const_reference;
  using iterator = typename base_type::iterator;
  using const_iterator = typename base_type::const_iterator;
  using reverse_iterator = typename base_type::reverse_iterator;
  using const_reverse_iterator = typename base_type::const_reverse_iterator;

  [[nodiscard]] explicit StaticVectorImpl(size_t n) {
    ensure_size_impl(n);
    detail::construct_strategy<iterator>::on_range(this->begin(), this->end());
  }
  [[nodiscard]] StaticVectorImpl(size_t n, value_type const& value) {
    base_type::assign(n, value);
  }
  [[nodiscard]] StaticVectorImpl(StaticVectorImpl const& rhs) {
    base_type::assign(std::begin(rhs), std::end(rhs));
  }
  [[nodiscard]] StaticVectorImpl(StaticVectorImpl&& rhs) noexcept {
    move_construct_from(std::move(rhs));
  }
  template<size_t C2, size_t A2>
  [[nodiscard]] StaticVectorImpl(StaticVectorImpl<TValue, C2, A2> const& rhs) {
    base_type::assign(std::begin(rhs), std::end(rhs));
  }
  template<class TInputIterator>
  [[nodiscard]] StaticVectorImpl(TInputIterator first, TInputIterator last) {
    base_type::assign(first, last);
  }
  [[nodiscard]] StaticVectorImpl(std::initializer_list<value_type> const& values) {
    base_type::assign(std::begin(values), std::end(values));
  }

  StaticVectorImpl& operator=(StaticVectorImpl const& rhs) {
    base_type::assign(std::begin(rhs), std::end(rhs));
    return *this;
  }
  StaticVectorImpl& operator=(StaticVectorImpl&& rhs) noexcept {
    base_type::clear();
    move_construct_from(std::move(rhs));
    return *this;
  }

  template<size_t C2, size_t A2>
  StaticVectorImpl& operator=(StaticVectorImpl<TValue, C2, A2> const& rhs) {
    base_type::assign(std::begin(rhs), std::end(rhs));
    return *this;
  }

protected:
  StaticVectorImpl() = default;
  ~StaticVectorImpl() override = default;

  [[nodiscard]] value_type* get_storage_pointer_impl() override {
    return reinterpret_cast<value_type*>(&storage_);
  }
  [[nodiscard]] value_type const* get_storage_pointer_impl() const override {
    return reinterpret_cast<value_type const*>(&storage_);
  }
  void ensure_size_impl(size_type new_size) override {
    if (Capacity < new_size) {
      throw std::bad_alloc();
    }
    ensure_size_unsafe_impl(new_size);
  }
  void ensure_size_unsafe_impl(size_type new_size) override {
    base_type::size_ = std::max(base_type::size_, new_size);
  }
  void reserve_impl([[maybe_unused]] size_type new_capacity) override {
  }
  [[nodiscard]] size_type max_size_impl() const noexcept override {
    return Capacity;
  }
  [[nodiscard]] size_type capacity_impl() const noexcept override {
    return Capacity;
  }
  
private:
  void move_construct_from(StaticVectorImpl&& rhs) {
    if constexpr (std::is_trivially_copyable_v<TValue>) {
      memcpy(&storage_, &rhs.storage_, sizeof(storage_));
      base_type::size_ = rhs.size_;
    }
    else {
      for (auto& v : rhs) {
        this->emplace_back(std::move(v));
      }
    }

    rhs.clear();
  }

  using StorageType = std::byte[sizeof(value_type) * Capacity];
  static_assert(std::is_trivially_destructible_v<StorageType>);

  alignas(Alignment) StorageType storage_ {};
};

template<class TValue, size_t Capacity, size_t Alignment, class U = void>
class ConditionallyTriviallyDestructibleStaticVector final : public StaticVectorImpl<TValue, Capacity, Alignment> {
public:
  using StaticVectorImpl<TValue, Capacity, Alignment>::StaticVectorImpl;

  ~ConditionallyTriviallyDestructibleStaticVector() override {
    this->clear();
  }
private:
};

template<class TValue, size_t Capacity, size_t Alignment>
class ConditionallyTriviallyDestructibleStaticVector<TValue, Capacity, Alignment, std::enable_if_t<std::is_trivially_destructible_v<TValue>>> final : public StaticVectorImpl<TValue, Capacity, Alignment> {
public:
  using StaticVectorImpl<TValue, Capacity, Alignment>::StaticVectorImpl;

  ~ConditionallyTriviallyDestructibleStaticVector() override = default;
private:
};

/// A vector with a static capacity. `TriviallyDestructible` if TValue is `TriviallyDestructible`.
template<class TValue, size_t Capacity, size_t Alignment = alignof(TValue)>
using StaticVector = ConditionallyTriviallyDestructibleStaticVector<TValue, Capacity, Alignment>;

template<class TValue, size_t C1, size_t A1, size_t C2, size_t A2>
bool operator==(StaticVector<TValue, C1, A1> const& lhs, StaticVector<TValue, C2, A2> const& rhs) {
  return lhs.size() == rhs.size() && std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs));
}
template<class TValue, size_t C1, size_t A1, size_t C2, size_t A2>
bool operator!=(StaticVector<TValue, C1, A1> const& lhs, StaticVector<TValue, C2, A2> const& rhs) {
  return !operator==(lhs, rhs);
}

template<class TValue, size_t C1, size_t A1, size_t C2, size_t A2>
bool operator==(SmallVector<TValue, C1, A1> const& lhs, SmallVector<TValue, C2, A2> const& rhs) {
  return lhs.size() == rhs.size() && std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs));
}
template<class TValue, size_t C1, size_t A1, size_t C2, size_t A2>
bool operator!=(SmallVector<TValue, C1, A1> const& lhs, SmallVector<TValue, C2, A2> const& rhs) {
  return !operator==(lhs, rhs);
}

} // namespace mbase
