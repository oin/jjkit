#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>

/**
 * Calculate the CRC-16-CCITT of the given data.
 */
constexpr uint16_t jjrecord_crc16(const uint8_t* data, size_t size, uint16_t crc = 0xFFFF) {
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
constexpr size_t jjrecord_header_size = 4;

/**
 * A configuration within a record storage area.
 *
 * @tparam Redundancy The number of slots to use for rotating copies of the record.
 */
template <uint8_t Redundancy>
struct jjrecord_slot_t {
	/**
	 * The index of the current slot in the storage area.
	 */
	uint8_t index;
	/**
	 * The sequence number of the current slot.
	 */
	uint8_t sequence_number;

	/**
	 * @return The next slot in the rotation.
	 */
	constexpr jjrecord_slot_t next() const {
		return jjrecord_slot_t{
			static_cast<uint8_t>((index + 1) % Redundancy),
			static_cast<uint8_t>(sequence_number + 1)
		};
	}
};

/**
 * A record with rotating slots.
 *
 * @tparam Type The magic number identifying the record type.
 * @tparam Size The size of the record, in bytes.
 * @tparam Redundancy The number of slots to use for rotating copies of the record.
 */
template <uint8_t Type, size_t Size, uint8_t Redundancy>
class jjrecord {
public:
	/**
	 * A configuration within a record storage area.
	 */
	using slot_t = jjrecord_slot_t<Redundancy>;
	/**
	 * The magic number identifying the record type.
	 */
	static constexpr uint8_t type = Type;
	/**
	 * The size of the record, in bytes.
	 */
	static constexpr size_t size = Size;
	/**
	 * The number of slots to use for rotating copies of the record.
	 */
	static constexpr size_t redundancy = Redundancy;
	/**
	 * The size of the payload attached to the record, in bytes.
	 */
	static constexpr size_t payload_size = size - jjrecord_header_size;
	/**
	 * The total size taken by the record with all its slots, in bytes.
	 */
	static constexpr size_t total_size = size * redundancy;

	static_assert(Size > jjrecord_header_size, "Size must be greater than header size.");
	static_assert(Redundancy > 0, "Redundancy must be greater than zero.");
public:
	jjrecord() : slot{0, 0} {}
	jjrecord(slot_t slot) : slot{slot} {}

	/**
	 * @return A pointer to the payload data within the record, with size `payload_size` bytes.
	 */
	uint8_t* payload() {
		return data + jjrecord_header_size;
	}

	/**
	 * @return A pointer to the payload data within the record, with size `payload_size` bytes.
	 */
	const uint8_t* payload() const {
		return data + jjrecord_header_size;
	}

	/**
	 * The current slot position.
	 */
	slot_t current_slot() const {
		return slot;
	}

	/**
	 * Read the record from storage using the given read function into account.
	 * @param read_fn The function used to read a slot from storage, with signature `bool read_fn(uint8_t slot_index, uint8_t* out, size_t size)`.
	 * @return true if a valid record was found and read, false otherwise.
	 * @note Use `payload()` to access the read payload data and `current_slot()` to get the corresponding slot.
	 */
	template <typename ReadFn>
	bool read(ReadFn&& read_fn) {
		bool found = false;
		uint8_t temp[size];
		for(size_t i=0; i<redundancy; ++i) {
			if(!read_fn(i, temp, size)) {
				return false;
			}
			if(read_slot(i, temp)) {
				found = true;
			}
		}
		return found;
	}

	/**
	 * Write the current payload to storage using the given write function, advancing to the next slot.
	 * @param write_fn The function used to write a slot to storage, with signature `bool write_fn(uint8_t slot_index, const uint8_t* data, size_t size)`.
	 * @return true if the write was successful, false otherwise.
	 * @note Before calling this method, prepare the payload data using `payload()`, and ensure the slot is set correctly.
	 */
	template <typename WriteFn>
	bool write_next(WriteFn&& write_fn) {
		slot = slot.next();
		return write_fn(slot.index, write_slot(), size);
	}

	/**
	 * Read a single slot from storage into the record, taking slot index and sequence number into account.
	 * @param slot_index The index of the slot being read.
	 * @param in The input buffer containing the slot data, of size `size` bytes.
	 * @return true if the slot was valid and read successfully, false otherwise.
	 * @note Use `payload()` to access the read payload data and `current_slot()` to get the corresponding slot.
	 */
	bool read_slot(uint8_t slot_index, const uint8_t* in) {
		const auto crc_read = in[0] | (in[1] << 8);
		const auto crc_calc = jjrecord_crc16(in + 2, size - 2);
		if(crc_read != crc_calc) {
			return false;
		}
		const auto type_read = in[2];
		if(type_read != type) {
			return false;
		}
		const auto seqnum_read = in[3];
		if(slot_index > 0) {
			int distance = seqnum_read - slot.sequence_number;
			while(distance < 0) {
				distance += 0xFF;
			}
			distance = distance % 0xFF;
			if(distance >= (int)redundancy) {
				return false;
			}
		}

		slot.sequence_number = seqnum_read;
		slot.index = slot_index;
		std::copy_n(in + jjrecord_header_size, payload_size, data + jjrecord_header_size);
		return true;
	}

	/**
	 * Write the current slot data into a buffer, preparing the header accordingly.
	 * @return The pointer to the buffer containing the complete slot data, with size `size` bytes.
	 */
	const uint8_t* write_slot() {
		data[2] = type;
		data[3] = slot.sequence_number;
		const auto crc = jjrecord_crc16(data + 2, size - 2);
		data[0] = static_cast<uint8_t>(crc & 0xFF);
		data[1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
		return data;
	}
private:
	uint8_t data[size];
	slot_t slot;
};
