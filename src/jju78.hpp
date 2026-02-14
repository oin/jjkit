#pragma once
#include <cstdint>
#include <cstddef>

/**
 * Indicates that the next byte should be read with the highest bit set to 1.
 */
 constexpr uint8_t jju78_high = 0x7D;
/**
 * Indicates that the next byte should be read as a literal value.
 */
constexpr uint8_t jju78_esc = 0x7C;

/**
 * @return The maximum size of the output buffer needed to hold the result of converting `size` bytes of input data using the jju78 encoding scheme.
 */
constexpr size_t jju78_size(size_t size) {
	return size * 2;
}

/**
 * Convert 7-bit data to 8-bit data using the jju78 encoding scheme.
 */
constexpr size_t jju78_read(const uint8_t* in, size_t size, uint8_t* out) {
	auto it = out;
	bool high = false;
	bool escape = false;
	for(size_t i=0; i<size; ++i) {
		const uint8_t v = in[i];
		if(escape) {
			escape = false;
			*it++ = v;
			continue;
		} else if(high) {
			high = false;
			*it++ = v | 0x80;
			continue;
		} else {
			switch(v) {
				case jju78_high:
					high = true;
					break;
				case jju78_esc:
					escape = true;
					break;
				default:
					*it++ = v;
					break;
			}
		}
	}
	return it - out;
}

/**
 * Convert 8-bit data to 7-bit data using the jju78 encoding scheme.
 */
constexpr size_t jju78_write(const uint8_t* in, size_t size, uint8_t* out) {
	auto it = out;
	for(size_t i=0; i<size; ++i) {
		uint8_t v = in[i];
		if(v & 0x80) {
			v &= 0x7F;
			*it++ = jju78_high;
		} else if(v == jju78_high || v == jju78_esc) {
			*it++ = jju78_esc;
		}
		*it++ = v;
	}
	return it - out;
}
