#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>

/**
 * Calculate the CRC-16-CCITT of the given data.
 */
constexpr uint16_t jjcrc16(const uint8_t* data, size_t size, uint16_t crc = 0xFFFF) {
	constexpr const uint16_t polynomial = 0x1021; // CRC-16-CCITT polynomial
	
	for(size_t i=0; i<size; ++i) {
		crc ^= (uint16_t(data[i]) << 8);
		for(size_t j=0; j<8; ++j) {
			if(crc & 0x8000) {
				crc = (crc << 1) ^ polynomial;
			} else {
				crc <<= 1;
			}
		}
	}
	
	return crc;
}

/**
 * The size of the record header, in bytes.
 */
constexpr size_t jjrecord_header_size = 7;

struct jjrecord_t;

/**
 * The configuration for a record.
 */
struct jjrecord_config_t {
	/**
	 * The magic number identifying the record type.
	 */
	uint16_t type;
	/**
	 * The version number of the record format.
	 *
	 * Versions newer than this number will not be read.
	 */
	uint16_t version;
	/**
	 * The size of the record, in bytes.
	 */
	size_t size;
	/**
	 * The number of slots to use for rotating copies of the record.
	 */
	size_t redundancy;

	/**
	 * @return The size of the payload attached to the record, in bytes.
	 */
	constexpr size_t payload_size() const {
		return size - jjrecord_header_size;
	}

	constexpr jjrecord_t record(size_t index, uint8_t sequence_number) const;
	constexpr jjrecord_t record() const;
};

/**
 * A position within a record storage area.
 */
struct jjrecord_t {
	/**
	 * The configuration used for the record.
	 */
	const jjrecord_config_t& config;
	/**
	 * The index of the current slot in the storage area.
	 */
	size_t index;
	/**
	 * The sequence number of the current slot.
	 */
	uint8_t sequence_number;

	/**
	 * Read a payload from slot at the given index from the given data.
	 * @param index The index of the slot to read.
	 * @param in The input data to read from.
	 * @param out The output buffer to write the payload to.
	 * @return true if the slot was read successfully, false otherwise.
	 *
	 * @note Typical usage involves calling this method multiple times with each slot index.
	 */
	bool read(size_t index, const uint8_t* in, uint8_t* out) {
		uint8_t* it = const_cast<uint8_t*>(in);

		const auto crc_read = it[0] | (it[1] << 8);
		it += 2;
		const auto crc_calc = jjcrc16(it, config.size - 2);
		if(crc_read != crc_calc) {
			return false;
		}
		const auto type_read = it[0] | (it[1] << 8);
		it += 2;
		if(type_read != config.type) {
			return false;
		}
		const auto version_read = it[0] | (it[1] << 8);
		it += 2;
		if(version_read > config.version) {
			return false;
		}
		const auto seqnum_read = *it++;
		if(index > 0) {
			int distance = seqnum_read - sequence_number;
			while(distance < 0) {
				distance += 0xFF;
			}
			distance = distance % 0xFF;
			if(distance >= (int)config.redundancy) {
				return false;
			}
		}
		
		sequence_number = seqnum_read;
		this->index = index;
		std::copy_n(it, config.payload_size(), out);
		return true;
	}

	/**
	 * Advance to the next slot in the record.
	 */
	void advance() {
		++sequence_number;
		index = (index + 1) % config.redundancy;
	}

	/**
	 * Write the record corresponding to the given payload into the given output buffer.
	 * @param in The input payload data to write, with size `config.payload_size()`.
	 * @param out The output buffer to write the slot data to, with size `config.slot_size`.
	 *
	 * @note Most uses will want to call `advance()` before calling this method to prepare for writing the next slot.
	 */
	void write(const uint8_t* in, uint8_t* out) {
		uint8_t* it = out + 2; // Reserve space for CRC
		*it++ = static_cast<uint8_t>(config.type & 0xFF);
		*it++ = static_cast<uint8_t>((config.type >> 8) & 0xFF);
		*it++ = static_cast<uint8_t>(config.version & 0xFF);
		*it++ = static_cast<uint8_t>((config.version >> 8) & 0xFF);
		*it++ = static_cast<uint8_t>(sequence_number % 0xFF);
		std::copy_n(in, config.payload_size(), it);

		const uint16_t crc = jjcrc16(out + 2, config.size - 2);
		out[0] = static_cast<uint8_t>(crc & 0xFF);
		out[1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
	}

	/**
	 * Read the record from storage using the given read function.
	 * @param temp A temporary buffer of size `config.slot_size` to use for reading slots.
	 * @param out The output buffer to write the payload to, of size `config.payload_size()`.
	 * @param read_fn The function used to read a slot from storage. It should have the signature `const uint8_t* read_fn(size_t slot_index)` and return nullptr if the slot could not be read.
	 */
	template <typename ReadFn>
	bool read(uint8_t* out, ReadFn&& read_fn) {
		bool found = false;
		for(size_t i=0; i<config.redundancy; ++i) {
			const uint8_t* temp = read_fn(i);
			if(!temp) {
				return false;
			}

			if(read(i, temp, out)) {
				found = true;
			}
		}
		return found;
	}

	template <typename WriteFn>
	bool write_next(const uint8_t* payload, uint8_t* out, WriteFn&& write_fn) {
		advance();
		write(payload, out);
		return write_fn(index, out, config.size);
	}
};

constexpr jjrecord_t jjrecord_config_t::record(size_t index, uint8_t sequence_number) const {
	return {*this, index, sequence_number};
}
constexpr jjrecord_t jjrecord_config_t::record() const {
	return {*this, 0, 0};
}
