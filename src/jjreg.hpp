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
#define JJREG__SIZE_SUM(name, val) + decltype(val)::field_size

/**
 * Create a registry with the given name and schema.
 *
 * @param schema_name Name of the generated registry struct.
 * @param LIST X-macro listing the fields of the schema, such as : `#define SCHEMA(_) _(field1, (jjreg_u8{0, 100, 50})) _(field2, (jjreg_string<16>{"default"})) )`
 */
#define JJREG(schema_name, max_size, LIST) \
	struct schema_name { \
		enum { LIST(JJREG__ENUM) _index__count }; \
		/** The size of each field, in bytes. */ \
		static constexpr size_t field_size(size_t index) { \
			constexpr size_t arr[_index__count] = { LIST(JJREG__SIZE) }; \
			return arr[index]; \
		} \
		/** The total size of the registry, in bytes. */ \
		enum { size = 0 LIST(JJREG__SIZE_SUM) }; \
		enum { capacity = max_size }; \
		static_assert(size <= capacity, "Registry size exceeds capacity"); \
		LIST(JJREG__DECL) \
		struct view_t { \
			uint8_t* ptr; \
			const schema_name& meta; \
			LIST(JJREG__DECL_VIEW) \
			view_t(const schema_name& meta, uint8_t* ptr) : ptr(ptr), meta(meta) LIST(JJREG__INIT_VIEW) {} \
			void reset() { \
				LIST(JJREG__RESET_CALL) \
			} \
		}; \
		struct buffer_t : view_t { \
			uint8_t data[capacity]; \
			buffer_t(const schema_name& meta) : view_t(meta, data) { \
				this->reset(); \
			} \
		}; \
		buffer_t operator()() const { \
			return buffer_t{*this}; \
		} \
		view_t operator()(uint8_t* ptr) const { \
			return view_t{*this, ptr}; \
		} \
	}; \
	template <> \
	struct jjreg_proxy<jjreg_jjreg<schema_name>> : schema_name::view_t { \
		jjreg_proxy(uint8_t* ptr, const jjreg_jjreg<schema_name>& meta) : schema_name::view_t(meta.schema, ptr) {} \
	};

#pragma mark - Utilities

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

template <typename Getter>
constexpr size_t jjreg_offset(Getter getter, size_t index) {
	size_t offset = 0;
	for(size_t i=0; i<index; ++i) {
		offset += getter(i);
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

	void set(field_t v) {
		meta.write(v, ptr);
	}
	field_t get() const {
		return meta.read(ptr);
	}
	operator field_t() const {
		return get();
	}
	void operator=(field_t v) {
		set(v);
	}
	void reset() {
		meta.write(meta.default_value, ptr);
	}
};

#pragma mark Boolean

struct jjreg_bool {
	using field_type = bool;
	static constexpr size_t field_size = 1;

	bool default_value;

	constexpr void write(bool v, uint8_t* out) const {
		*out = v? 1 : 0;
	}
	constexpr bool read(const uint8_t* in) const {
		return (*in != 0);
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

#define JJREG_E8__DECL(prefix, name, str) prefix##_##name,
#define JJREG_E8__STR(prefix, name, str) str,
#define JJREG_E8(name, LIST, defval) \
	enum name##_t : uint8_t { \
		LIST(name, JJREG_E8__DECL) \
		name##_count, \
	}; \
	constexpr const char* name##_str[name##_count] = { \
		LIST(name, JJREG_E8__STR) \
	}; \
	constexpr auto name = jjreg_e8<name##_t>{name##_count, name##_##defval};

#pragma mark Struct

/**
 * A default serializer for structs using memcpy.
 */
template <typename T>
struct jjreg_struct_ {
	static constexpr size_t field_size = sizeof(T);
	static constexpr void write(const T& v, uint8_t* out) {
		std::memcpy(out, &v, sizeof(T));
	}
	static constexpr T read(const uint8_t* in) {
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
	static constexpr size_t field_size = Serializer::field_size;

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
	bool is_empty() const {
		return ptr[0] == 0;
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

#pragma mark Nested schema

template <typename Schema>
struct jjreg_jjreg {
	using field_type = typename Schema::view_t;
	static constexpr size_t field_size = Schema::capacity;

	const Schema& schema;
};

template <typename Schema>
constexpr jjreg_jjreg<Schema> jjreg_nested(const Schema& schema) {
	return jjreg_jjreg<Schema>{schema};
}
