#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

/**
 * @file
 * Typed value registry system for structured storage of configuration data over a byte array.
 */

#define JJREG__ENUM(name, val) _index_##name,
#define JJREG__SIZE(name, val) decltype(val)::field_size,
#define JJREG__DECL(name, val) decltype(val) name = val;
#define JJREG__DECL_VIEW(name, val) jjreg_proxy<decltype(val)> name;
#define JJREG__INIT_VIEW(name, val) , name{ptr + jjreg_offset(field_size, _index_##name), meta.name}
#define JJREG__RESET_CALL(name, val) name.reset();

/**
 * Create a registry with the given name and schema.
 *
 * @param schema_name Name of the generated registry struct.
 * @param LIST X-macro listing the fields of the schema, such as : `#define SCHEMA(_) _(field1, (jjreg_u8{0, 100, 50})) _(field2, (jjreg_string<16>{"default"})) )`
 */
#define JJREG(schema_name, LIST) \
	struct schema_name { \
		enum { LIST(JJREG__ENUM) _index__count }; \
		/** The size of each field, in bytes. */ \
		static constexpr size_t field_size[_index__count] = { LIST(JJREG__SIZE) }; \
		/** The total size of the registry, in bytes. */ \
		static constexpr size_t size = jjreg_offset(field_size, _index__count); \
		LIST(JJREG__DECL) \
		struct view_t { \
			const schema_name& meta; \
			uint8_t* ptr; \
			LIST(JJREG__DECL_VIEW) \
			view_t(const schema_name& meta, uint8_t* ptr) : meta(meta), ptr(ptr) LIST(JJREG__INIT_VIEW) {} \
			void reset() { \
				LIST(JJREG__RESET_CALL) \
			} \
		}; \
		view_t operator()(uint8_t* ptr) const { \
			return view_t{*this, ptr}; \
		} \
	}; \
	constexpr size_t schema_name::field_size[]; \
	constexpr size_t schema_name::size; \

#pragma mark - Types

/**
 * @return The byte offset of the given index in a schema with the given field sizes.
 */
constexpr size_t jjreg_offset(const size_t* sizes, size_t index) {
	size_t offset = 0;
	for(size_t i=0; i<index; ++i) {
		offset += sizes[i];
	}
	return offset;
}

/**
 * A proxy for a registry field of the given meta type.
 *
 * @tparam Meta The meta type of the field.
 */
template <typename Meta>
struct jjreg_proxy {
	/**
	 * The field type represented by the proxy.
	 */
	using field_t = typename Meta::field_type;

	/**
	 * A pointer to the raw data of the field.
	 */
	uint8_t* ptr;
	/**
	 * The meta information for the field.
	 */
	const Meta& meta;

	void set(field_t&& v) {
		meta.write(v, ptr);
	}
	field_t get() const {
		return meta.read(ptr);
	}
	operator field_t() const {
		return get();
	}
	void operator=(field_t&& v) {
		set(std::forward<field_t>(v));
	}
	void reset() {
		meta.write(meta.default_value, ptr);
	}
};

#pragma mark Unsigned and signed integers

/**
 * A meta type for an 8-bit unsigned integer with clamping.
 */
struct jjreg_u8 {
	using field_type = uint8_t;
	static constexpr size_t field_size = 1;

	uint8_t min;
	uint8_t max;
	uint8_t default_value;

	constexpr uint8_t clamped(uint8_t v) const {
		if(v < min) return min;
		if(v > max) return max;
		return v;
	}
	constexpr void write(uint8_t v, uint8_t* out) const {
		*out = clamped(v);
	}
	constexpr uint8_t read(const uint8_t* in) const {
		return *in;
	}
};

/**
 * A meta type for an 8-bit signed integer with clamping.
 */
struct jjreg_i8 {
	using field_type = int8_t;
	static constexpr size_t field_size = 1;

	int8_t min;
	int8_t max;
	int8_t default_value;

	constexpr int8_t clamped(int8_t v) const {
		if(v < min) return min;
		if(v > max) return max;
		return v;
	}
	constexpr void write(int8_t v, uint8_t* out) const {
		*out = static_cast<uint8_t>(clamped(v));
	}
	constexpr int8_t read(const uint8_t* in) const {
		return static_cast<int8_t>(*in);
	}
};

#pragma mark Enum

/**
 * A meta type for an enum stored as an 8-bit value with clamping.
 *
 * @tparam Enum The enum type.
 */
template <typename Enum>
struct jjreg_e8 {
	using field_type = Enum;
	static constexpr size_t field_size = 1;
	static_assert(std::is_same<typename std::underlying_type<Enum>::type, uint8_t>::value, "");

	size_t size;
	Enum default_value;

	constexpr Enum clamped(Enum v) const {
		if(static_cast<size_t>(v) >= size) {
			return static_cast<Enum>(size - 1);
		}
		return v;
	}
	constexpr void write(Enum v, uint8_t* out) const {
		*out = static_cast<uint8_t>(clamped(v));
	}
	constexpr Enum read(const uint8_t* in) const {
		return static_cast<Enum>(*in);
	}
};

#pragma mark Struct

/**
 * A default serializer for structs using memcpy.
 */
template <typename T>
struct jjreg_struct_ {
	static constexpr size_t size = sizeof(T);

	constexpr void write(const T& v, uint8_t* out) const {
		std::memcpy(out, &v, sizeof(T));
	}
	constexpr T read(const uint8_t* in) const {
		T v;
		std::memcpy(&v, in, sizeof(T));
		return v;
	}
};

/**
 * A meta type for a struct field with custom serialization.
 *
 * @tparam T The struct type.
 * @tparam Serializer The serializer type for the struct.
 */
template <typename T, typename Serializer = jjreg_struct_<T>>
struct jjreg_struct {
	using field_type = T;
	static constexpr size_t field_size = Serializer::size;

	constexpr void write(const T& v, uint8_t* out) const {
		Serializer::write(v, out);
	}
	constexpr T read(const uint8_t* in) const {
		return Serializer::read(in);
	}
};

#pragma mark Fixed-size array

/**
 * A meta type for a fixed-size array.
 */
template <typename Meta, size_t N>
struct jjreg_array {
	static constexpr size_t field_size = Meta::field_size * N;

	Meta meta;
};

template <typename Meta, size_t N>
struct jjreg_proxy<jjreg_array<Meta, N>> {
	uint8_t* ptr;
	const jjreg_array<Meta, N>& meta;

	jjreg_proxy<Meta> operator[](size_t index) const {
		return { ptr + index * Meta::field_size, meta.meta };
	}
	void set(const typename Meta::field_type* in, size_t size) {
		for(size_t i=0; i<size && i<N; ++i) {
			(*this)[i].set(in[i]);
		}
	}
	void reset() {
		for(size_t i=0; i<N; ++i) {
			(*this)[i].reset();
		}
	}
};

#pragma mark String

/**
 * A meta type for a fixed-size string.
 *
 * @tparam N The maximum length of the string including null terminator.
 */
template <size_t N>
struct jjreg_string {
	using field_type = const char*;
	static constexpr size_t field_size = N;

	const char* default_value;
};

template <size_t N>
struct jjreg_proxy<jjreg_string<N>> {
	uint8_t* ptr;
	const jjreg_string<N>& meta;

	void set(const char* in) {
		size_t i = 0;
		for(; i < N - 1 && in[i] != 0; ++i) {
			reinterpret_cast<char*>(ptr)[i] = in[i];
		}
		ptr[i] = 0;
	}
	const char* get() {
		reinterpret_cast<char*>(ptr)[N - 1] = 0;
		return reinterpret_cast<const char*>(ptr);
	}
	operator const char*() {
		return get();
	}
	void operator=(const char* in) {
		set(in);
	}
	void reset() {
		set(meta.default_value);
	}
};

#pragma mark Variable-size list

/**
 * A meta type for a variable-size list.
 *
 * @tparam Meta The meta type of the list items.
 * @tparam Capacity The maximum number of items in the list.
 * @tparam SizeType The integer type used to store the size of the list.
 */
template <typename Meta, size_t Capacity, typename SizeType = uint8_t>
struct jjreg_list {
	static constexpr size_t field_size = sizeof(SizeType) + Meta::field_size * Capacity;

	Meta meta;
};

template <typename Meta, size_t Capacity, typename SizeType>
struct jjreg_proxy<jjreg_list<Meta, Capacity, SizeType>> {
	static constexpr size_t capacity = Capacity;
	SizeType* size_ptr() const {
		return reinterpret_cast<SizeType*>(ptr);
	}
	uint8_t* item_ptr(size_t index) const {
		return ptr + sizeof(SizeType) + index * Meta::field_size;
	}

	uint8_t* ptr;
	const jjreg_list<Meta, Capacity, SizeType>& meta;

	jjreg_proxy<Meta> operator[](size_t index) const {
		return { item_ptr(index), meta.meta };
	}
	size_t size() const {
		return static_cast<size_t>(*size_ptr());
	}
	void reset() {
		*size_ptr() = 0;
	}
	bool push_back(typename Meta::field_type&& v) {
		auto& current_size = *size_ptr();
		if(current_size >= capacity) {
			return false;
		}

		jjreg_proxy<Meta> proxy{ item_ptr(current_size), meta.meta };
		proxy.set(std::forward<decltype(v)>(v));

		++current_size;
		return true;
	}
};
