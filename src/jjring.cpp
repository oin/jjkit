#include "jjring.hpp"
#include <cassert>
#include <cstring>

static inline size_t load_acquire(const size_t& x) {
	return __atomic_load_n(&x, __ATOMIC_ACQUIRE);
}
static inline size_t load_relaxed(const size_t& x) {
	return __atomic_load_n(&x, __ATOMIC_RELAXED);
}
static inline void store_release(size_t& x, size_t v) {
	__atomic_store_n(&x, v, __ATOMIC_RELEASE);
}

jjring_::jjring_(void* buffer, size_t capacity, size_t element_size, size_t alignment) : buf(buffer), elemsize(element_size), mask(capacity - 1), head(0), tail(0) {
	assert((capacity & (capacity - 1)) == 0 && "Capacity must be a power of 2");
	assert(buffer != nullptr && "Buffer must not be null");
	assert(reinterpret_cast<uintptr_t>(buffer) % alignment == 0 && "Buffer must be aligned");
}

void jjring_::clear() noexcept {
	store_release(head, 0);
	store_release(tail, 0);
}

bool jjring_::empty() const noexcept {
	const auto h = load_acquire(head);
	const auto t = load_relaxed(tail);
	return h == t;
}

bool jjring_::full() const noexcept {
	const auto h = load_relaxed(head);
	const auto next = (h + 1) & mask;
	const auto t = load_acquire(tail);
	return next == t;
}

size_t jjring_::size_approx() const noexcept {
	const auto h = load_acquire(head);
	const auto t = load_relaxed(tail);
	const auto N = mask + 1;
	return (h + N - t) & mask;
}

bool jjring_::push(const void* src) noexcept {
	const auto h = load_relaxed(head);
	const auto t = load_acquire(tail);
	const auto next = (h + 1) & mask;
	if(next == t) {
		// The buffer is full
		return false;
	}
	std::memcpy(static_cast<char*>(buf) + h * elemsize, src, elemsize);
	store_release(head, next);
	return true;
}

size_t jjring_::push(const void* src, size_t size) noexcept {
	const auto h = load_relaxed(head);
	const auto t = load_acquire(tail);
	const auto N = mask + 1;
	const auto space = (t + N - 1 - h) & mask; // keep one empty slot
	if(space == 0) {
		return 0;
	}
	if(size > space) {
		size = space;
	}

	// First contiguous chunk until wrap
	size_t c1 = N - h;
	if(c1 > size) {
		c1 = size;
	}
	std::memcpy(static_cast<char*>(buf) + h * elemsize, src, c1 * elemsize);

	// Second chunk from start, if needed
	if(size > c1) {
		std::memcpy(static_cast<char*>(buf), static_cast<const char*>(src) + c1 * elemsize, (size - c1) * elemsize);
	}

	store_release(head, (h + size) & mask);
	return size;
}

void jjring_::push_overwrite(const void* src) noexcept {
	const auto h = load_relaxed(head);
	const auto t = load_acquire(tail);
	const auto next = (h + 1) & mask;
	if(next == t) {
		// The buffer is full, overwrite the oldest element
		store_release(tail, (t + 1) & mask);
	}
	std::memcpy(static_cast<char*>(buf) + h * elemsize, src, elemsize);
	store_release(head, next);
}

bool jjring_::pop(void* dst) noexcept {
	const auto t = load_relaxed(tail);
	const auto h = load_acquire(head);
	if(h == t) {
		// The buffer is empty
		return false;
	}
	std::memcpy(dst, static_cast<char*>(buf) + t * elemsize, elemsize);
	store_release(tail, (t + 1) & mask);
	return true;
}

size_t jjring_::pop(void* dst, size_t size) noexcept {
	const auto t = load_relaxed(tail);
	const auto h = load_acquire(head);
	const auto N = mask + 1;
	const auto avail = (h + N - t) & mask;
	if(avail == 0) {
		// The buffer is empty
		return 0;
	}
	if(size > avail) {
		size = avail;
	}

	// First contiguous chunk until wrap
	size_t c1 = N - t;
	if(c1 > size) {
		c1 = size;
	}
	std::memcpy(dst, static_cast<char*>(buf) + t * elemsize, c1 * elemsize);

	// Second chunk from start, if needed
	if(size > c1) {
		std::memcpy(static_cast<char*>(dst) + c1 * elemsize, static_cast<char*>(buf), (size - c1) * elemsize);
	}

	store_release(tail, (t + size) & mask);
	return size;
}

size_t jjring_::write_acquire(void** p) noexcept {
	const auto h = load_relaxed(head);
	const auto t = load_acquire(tail);
	const auto N = mask + 1;
	const auto space = (t + N - 1 - h) & mask; // keep one empty slot
	if(space == 0) {
		// The buffer is full
		*p = nullptr;
		return 0;
	}
	const auto until_wrap = N - h;
	const auto n = (space < until_wrap)? space : until_wrap;
	*p = static_cast<void*>(static_cast<char*>(buf) + h * elemsize);
	return n;
}

void jjring_::write_commit(size_t n) noexcept {
	const auto h = load_relaxed(head);
	store_release(head, (h + n) & mask);
}

size_t jjring_::read_acquire(const void** p) noexcept {
	const auto t = load_relaxed(tail);
	const auto h = load_acquire(head);
	const auto N = mask + 1;
	const auto avail = (h + N - t) &mask;
	if(avail == 0) {
		// The buffer is empty
		*p = nullptr;
		return 0;
	}

	const auto until_wrap = N - t;
	const auto n = (avail < until_wrap)? avail : until_wrap;
	*p = static_cast<const void*>(static_cast<const char*>(buf) + t * elemsize);
	return n;
}

void jjring_::read_commit(size_t n) noexcept {
	const auto t = load_relaxed(tail);
	store_release(tail, (t + n) & mask);
}
