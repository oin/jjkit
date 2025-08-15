#include <cstdint>
#include <cstddef>

/**
 * Private implementation details for the ring buffer.
 * @see jjring
 */
class jjring_ {
public:
	/**
	 * @param buffer Pointer to the buffer memory (must be aligned).
	 * @param capacity Capacity of the buffer (must be a power of 2).
	 * @param element_size Size of each element in the buffer.
	 * @param alignment Alignment requirement for the buffer.
	 * @warning `capacity` must be a power of 2.
	 */
	jjring_(void* buffer, size_t capacity, size_t element_size, size_t alignment);

	void clear() noexcept;
	bool empty() const noexcept;
	bool full() const noexcept;
	size_t size_approx() const noexcept;
	
	bool push(const void*) noexcept;
	size_t push(const void*, size_t) noexcept;
	void push_overwrite(const void*) noexcept;
	bool pop(void*) noexcept;
	size_t pop(void*, size_t) noexcept;

	size_t write_acquire(void**) noexcept;
	void write_commit(size_t) noexcept;
	size_t read_acquire(const void**) noexcept;
	void read_commit(size_t) noexcept;
private:
	void* buf;
	const size_t elemsize;
	const size_t mask; // N-1
	size_t head;
	size_t tail;
};

/**
 * A lock-free single-producer, single-consumer ring buffer, with fixed 2^N capacity.
 */
template <typename T, size_t N>
class jjring {
public:
	static_assert(N > 1 && (N & (N - 1)) == 0, "N must be a power of 2");
	static_assert(__is_trivially_copyable(T), "T must be trivially copyable");
	static_assert(sizeof(T) % alignof(T) == 0, "sizeof(T) must be multiple of alignof(T)");
	using value_type = T;
public:
	jjring() : _(buffer, N, sizeof(T), alignof(T)) {}

	/**
	 * Clear the ring buffer.
	 * @warning This operation is not thread-safe and should only be called when the buffer is not being accessed by other threads.
	 */
	void clear() noexcept {
		_.clear();
	}
	/**
	 * @return true if the buffer is empty.
	 */
	bool empty() const noexcept {
		return _.empty();
	}
	/**
	 * @return true if the buffer is full.
	 */
	bool full() const noexcept {
		return _.full();
	}
	/**
	 * @return The approximate number of elements in the buffer.
	 * @note The returned value may be off by one when the buffer is being accessed concurrently.
	 */
	size_t size_approx() const noexcept {
		return _.size_approx();
	}
	/**
	 * @return The maximum number of elements that can be stored in the buffer.
	 */
	constexpr size_t capacity() const noexcept {
		return N - 1;
	}

	/**
	 * Push an element into the buffer.
	 * @param item The element to push.
	 * @return true if the element was pushed successfully, false if the buffer is full.
	 */
	bool push(const T& item) noexcept {
		return _.push(&item);
	}
	/**
	 * Push multiple elements into the buffer at once.
	 * @param src Pointer to the first element to push.
	 * @param size The number of elements to push.
	 * @return The number of elements successfully pushed.
	 */
	size_t push(const T* src, size_t size) noexcept {
		return _.push(static_cast<const void*>(src), size);
	}
	/**
	 * Push an element into the buffer, overwriting the oldest element if the buffer is full.
	 * @param item The element to push.
	 * @warning In a rare race with the consumer popping at the same time, another element may be dropped.
	 * @warning Do not use together with long-lived `read_acquire` spans, which can be invalidated.
	 */
	void push_overwrite(const T& item) noexcept {
		_.push_overwrite(&item);
	}
	/**
	 * Pop an element from the buffer.
	 * @param item The element to pop.
	 * @return true if the element was popped successfully, false if the buffer is empty.
	 */
	bool pop(T& item) noexcept {
		return _.pop(&item);
	}
	/**
	 * Pop multiple elements from the buffer.
	 * @param dst Pointer to the destination buffer.
	 * @param size The number of elements to pop.
	 * @return The number of elements successfully popped.
	 */
	size_t pop(T* dst, size_t size) noexcept {
		return _.pop(static_cast<void*>(dst), size);
	}

	/**
	 * @defgroup zero_copy Zero-copy operations
	 * @brief Functions for zero-copy access to the buffer.
	 *
	 * These functions allow direct access to the buffer memory for writing or reading, avoiding unnecessary copies.
	 * First, use *_acquire() to obtain a pointer to the buffer memory, from which they can read or write data directly (up to the acquired size).
	 * Then, use *_commit() to make the changes visible to the other thread.
	 * @note If the number of elements to write or read exceeds the number of elements that can be written in a call to *_acquire(), the operation must be split into two acquire/commit pairs.
	 */

	/**
	 * Acquire a write pointer to the buffer.
	 * @param ptr Pointer to the write pointer.
	 * @return The number of elements that can be written.
	 */
	size_t write_acquire(T** ptr) noexcept {
		void* pv;
		const auto n = _.write_acquire(&pv);
		*ptr = static_cast<T*>(pv);
		return n;
	}
	/**
	 * Commit the written elements.
	 * @note This function must be called after write_acquire() to make the written elements visible to the other thread.
	 */
	void write_commit(size_t n) noexcept {
		_.write_commit(n);
	}
	/**
	 * Acquire a read pointer to the buffer.
	 * @param ptr Pointer to the read pointer.
	 * @return The number of elements that can be read.
	 */
	size_t read_acquire(const T** ptr) noexcept {
		const void* pv;
		const auto n = _.read_acquire(&pv);
		*ptr = static_cast<const T*>(pv);
		return n;
	}
	/**
	 * Commit the read elements.
	 * @note This function must be called after read_acquire() to make the read elements visible to the other thread.
	 */
	void read_commit(size_t n) noexcept {
		_.read_commit(n);
	}
private:
	alignas(alignof(T)) T buffer[N];
	jjring_ _;
};
