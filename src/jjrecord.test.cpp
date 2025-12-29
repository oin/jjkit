#include "../ext/doctest.h"
#include "jjrecord.hpp"
#include <random>

TEST_SUITE_BEGIN("jjrecord");

TEST_CASE("jjrecord_crc16") {
	{
		const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
		const uint16_t crc = jjrecord_crc16(data, sizeof(data));
		CHECK(crc == 0x29B1);
	}
	{
		const uint8_t data[] = { 0x3E, 0xD6, 0xB8, 0x4D, 0x21, 0xF1, 0xC8, 0x7F, 0x34, 0xED, 0x12, 0x39, 0x13, 0x70, 0xED, 0x31 };
		const uint16_t crc = jjrecord_crc16(data, sizeof(data));
		CHECK(crc == 0x3016);
	}
	{
		const uint8_t data[] = { 0x10, 0xD8, 0x03, 0xB0, 0x39, 0x26, 0x0D, 0x5A, 0xD6, 0x48, 0xB7, 0x4D, 0x2F, 0xC8, 0x99, 0x6A };
		const uint16_t crc = jjrecord_crc16(data, sizeof(data));
		CHECK(crc == 0xD4D5);
	}
	{
		const uint8_t data[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
		const uint16_t crc = jjrecord_crc16(data, sizeof(data));
		CHECK(crc == 0xC360);
	}
}

template <typename RecordType>
struct jjrecord_tester_t {
	uint8_t memory[RecordType::redundancy][RecordType::size];

	void fill(uint8_t* payload) {
		for(size_t i=0; i<RecordType::payload_size; ++i) {
			payload[i] = i;
		}
	}

	void setup(uint8_t index, uint8_t sequence_number) {
		RecordType rec = RecordType{{index, sequence_number}};
		uint8_t* payload = rec.payload();
		fill(payload);
		payload[0] = index;
		payload[1] = sequence_number;
		std::copy_n(rec.write_slot(), RecordType::size, memory[index]);
	}
};

TEST_CASE("[jjrecord] read valid record") {
	using jjrecord = jjrecord<0x12, 32, 4>;
	
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 0);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	for(size_t i=2; i<jjrecord::payload_size; ++i) {
		CHECK(payload[i] == static_cast<uint8_t>(i));
	}
}

TEST_CASE("[jjrecord] read record with invalid CRC") {
	using jjrecord = jjrecord<0xEF, 512, 7>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 0);
	// Corrupt the data to cause CRC failure
	tester.memory[0][10] ^= 0xFF;
	
	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == false);
}

TEST_CASE("[jjrecord] picks newest sequential slot") {
	using jjrecord = jjrecord<0xEF, 512, 7>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 5);
	tester.setup(1, 6);
	tester.setup(2, 7);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 2);
	CHECK(payload[1] == 7);
}

TEST_CASE("[jjrecord] tolerates wraparound of sequence numbers") {
	using jjrecord = jjrecord<0xEF, 512, 5>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 252);
	tester.setup(1, 253);
	tester.setup(2, 254);
	tester.setup(3, 255);
	tester.setup(4, 0);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 4);
	CHECK(payload[1] == 0);
}

TEST_CASE("[jjrecord] stops read when storage access fails") {
	using jjrecord = jjrecord<0x42, 64, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 0);
	tester.setup(1, 1);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		if(slot_index == 1) {
			return false; // Simulate EEPROM read failure
		}
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == false);
}

TEST_CASE("[jjrecord] ignores slots too far ahead of window") {
	using jjrecord = jjrecord<0xEF, 128, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 0);
	tester.setup(1, 10); // Jump larger than redundancy window
	tester.setup(2, 1);  // Next expected slot

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 2);
	CHECK(payload[1] == 1);
}

TEST_CASE("[jjrecord] rejects newer slot with wrong type") {
	using jjrecord = jjrecord<0xAA, 64, 4>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 3);
	tester.setup(1, 4);
	// Corrupt type but keep CRC valid so the only failing check is the type mismatch
	tester.memory[1][2] ^= 0xFF;
	const auto crc = jjrecord_crc16(tester.memory[1] + 2, jjrecord::size - 2);
	tester.memory[1][0] = static_cast<uint8_t>(crc & 0xFF);
	tester.memory[1][1] = static_cast<uint8_t>((crc >> 8) & 0xFF);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 0);
	CHECK(payload[1] == 3);
}

TEST_CASE("[jjrecord] write_next rotates slots and wraps sequence numbers") {
	using jjrecord = jjrecord<0x77, 48, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	jjrecord record{typename jjrecord::slot_t{2, 254}};

	uint8_t payload[jjrecord::payload_size];
	tester.fill(payload);
	payload[0] = 0xA1;
	payload[1] = 0xFE;
	std::copy_n(payload, jjrecord::payload_size, record.payload());
	auto write_fn = [&](size_t slot_index, const uint8_t* data, size_t size) {
		std::copy_n(data, size, tester.memory[slot_index]);
		return true;
	};
	REQUIRE(record.write_next(write_fn) == true); // Writes seq 255 into slot 0

	tester.fill(payload);
	payload[0] = 0xB2;
	payload[1] = 0x00;
	std::copy_n(payload, jjrecord::payload_size, record.payload());
	REQUIRE(record.write_next(write_fn) == true); // Writes seq 0 into slot 1 after wrap

	jjrecord reader;
	bool result = reader.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto read_payload = reader.payload();
	CHECK(read_payload[0] == 0xB2);
	CHECK(read_payload[1] == 0x00);
}

TEST_CASE("[jjrecord] recovers when earlier slots are corrupted") {
	using jjrecord = jjrecord<0xEF, 256, 4>;
	jjrecord_tester_t<jjrecord> tester{};
	// Slot 0 has bad CRC
	tester.setup(0, 1);
	tester.memory[0][0] ^= 0x01;
	// Slot 1 has wrong type
	tester.setup(1, 2);
	tester.memory[1][2] ^= 0x10;
	// Slot 2 is valid and newest in window
	tester.setup(2, 3);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 2);
	CHECK(payload[1] == 3);
}

TEST_CASE("[jjrecord] write_next surfaces write failures") {
	using jjrecord = jjrecord<0x42, 64, 2>;
	jjrecord record{};
	auto failing_write = [&](size_t, const uint8_t*, size_t) {
		return false;
	};
	CHECK(record.write_next(failing_write) == false);
}

TEST_CASE("[jjrecord] partial slot write leaves prior slot authoritative") {
	using jjrecord = jjrecord<0x10, 64, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	// Existing valid slot 0
	tester.setup(0, 5);

	jjrecord writer{typename jjrecord::slot_t{0, 5}};
	uint8_t payload[jjrecord::payload_size];
	tester.fill(payload);
	payload[0] = 0xEE;
	payload[1] = 0x06;
	std::copy_n(payload, jjrecord::payload_size, writer.payload());

	// Simulate power-cut: header written, payload torn, CRC mismatch
	auto torn_write = [&](size_t slot_index, const uint8_t* data, size_t size) {
		// Only copy header bytes, leave remainder zeroed
		std::fill_n(tester.memory[slot_index], size, 0);
		std::copy_n(data, jjrecord_header_size, tester.memory[slot_index]);
		return true;
	};
	REQUIRE(writer.write_next(torn_write) == true);

	jjrecord reader;
	bool result = reader.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto read_payload = reader.payload();
	CHECK(read_payload[0] == 0); // From slot 0
	CHECK(read_payload[1] == 5);
}

TEST_CASE("[jjrecord] all slots erased returns false") {
	using jjrecord = jjrecord<0x22, 64, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	for(auto& slot : tester.memory) {
		std::fill_n(slot, jjrecord::size, 0xFF);
	}

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == false);
}

TEST_CASE("[jjrecord] alternating good and bad CRC keeps last good") {
	using jjrecord = jjrecord<0x33, 80, 4>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 1);
	tester.setup(1, 2);
	tester.memory[1][0] ^= 0x01; // Corrupt CRC
	tester.setup(2, 3);
	tester.setup(3, 4);
	tester.memory[3][1] ^= 0x01; // Corrupt CRC

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 2);
	CHECK(payload[1] == 3);
}

TEST_CASE("[jjrecord] identical sequence prefers later slot") {
	using jjrecord = jjrecord<0x44, 96, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 9);
	tester.setup(1, 9);
	// Make slot 1 payload unique and fix CRC
	tester.memory[1][jjrecord_header_size + 0] = 0xAB;
	const auto crc = jjrecord_crc16(tester.memory[1] + 2, jjrecord::size - 2);
	tester.memory[1][0] = static_cast<uint8_t>(crc & 0xFF);
	tester.memory[1][1] = static_cast<uint8_t>((crc >> 8) & 0xFF);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 0xAB);
}

TEST_CASE("[jjrecord] wrap window retains newest within window") {
	using jjrecord = jjrecord<0x55, 64, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 254);
	tester.setup(1, 0);
	tester.setup(2, 1);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 2);
	CHECK(payload[1] == 1);
}

TEST_CASE("[jjrecord] type-mismatched newer slot rejected even with valid CRC") {
	using jjrecord = jjrecord<0x66, 80, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 7);
	tester.setup(1, 8);
	// Flip type and recompute CRC to keep slot structurally valid
	tester.memory[1][2] ^= 0x0F;
	const auto crc = jjrecord_crc16(tester.memory[1] + 2, jjrecord::size - 2);
	tester.memory[1][0] = static_cast<uint8_t>(crc & 0xFF);
	tester.memory[1][1] = static_cast<uint8_t>((crc >> 8) & 0xFF);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 0);
	CHECK(payload[1] == 7);
}

TEST_CASE("[jjrecord] write_next respects non-zero starting slot") {
	using jjrecord = jjrecord<0x70, 40, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	jjrecord writer{typename jjrecord::slot_t{1, 9}};

	auto write_fn = [&](size_t slot_index, const uint8_t* data, size_t size) {
		std::copy_n(data, size, tester.memory[slot_index]);
		return true;
	};

	for(uint8_t i=0; i<4; ++i) {
		tester.fill(writer.payload());
		writer.payload()[0] = 0xC0 | i;
		writer.payload()[1] = static_cast<uint8_t>(writer.current_slot().sequence_number + 1);
		REQUIRE(writer.write_next(write_fn) == true);
	}

	jjrecord reader;
	bool result = reader.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = reader.payload();
	CHECK((payload[0] & 0xC0) == 0xC0);
	CHECK(reader.current_slot().index == 2);
	CHECK(reader.current_slot().sequence_number == 13);
	// Verify rotation touched every slot with the expected sequence markers
	CHECK(tester.memory[0][3] == 11);
	CHECK((tester.memory[0][jjrecord_header_size] & 0xC0) == 0xC0);
	CHECK(tester.memory[1][3] == 12);
	CHECK((tester.memory[1][jjrecord_header_size] & 0xC0) == 0xC0);
	CHECK(tester.memory[2][3] == 13);
	CHECK((tester.memory[2][jjrecord_header_size] & 0xC0) == 0xC0);
}

TEST_CASE("[jjrecord] minimal size record works") {
	constexpr size_t record_size = jjrecord_header_size + 1;
	using jjrecord = jjrecord<0x77, record_size, 2>;
	static_assert(record_size == 5, "sanity");
	jjrecord_tester_t<jjrecord> tester{};
	tester.setup(0, 1);

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == true);
	const auto payload = record.payload();
	CHECK(payload[0] == 0);
}

TEST_CASE("[jjrecord] large payload record works") {
	using jjrecord = jjrecord<0x88, 1024, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	jjrecord writer{};
	// Fill payload with pattern and write into slot 0
	for(size_t i=0; i<jjrecord::payload_size; ++i) {
		writer.payload()[i] = static_cast<uint8_t>(i & 0xFF);
	}
	std::copy_n(writer.write_slot(), jjrecord::size, tester.memory[0]);

	jjrecord reader;
	bool result = reader.read([&](size_t slot_index, uint8_t* out, size_t size) {
		if(slot_index == 0) {
			std::copy_n(tester.memory[0], size, out);
			return true;
		}
		// other slots empty
		std::fill_n(out, size, 0xFF);
		return true;
	});
	CHECK(result == true);
	const auto payload = reader.payload();
	CHECK(payload[100] == static_cast<uint8_t>(100));
	CHECK(payload[500] == static_cast<uint8_t>(500 & 0xFF));
}

TEST_CASE("[jjrecord] random noise yields no valid slot") {
	using jjrecord = jjrecord<0x99, 64, 3>;
	jjrecord_tester_t<jjrecord> tester{};
	const uint8_t noise[jjrecord::size] = {
		0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
		0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
		0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x10,
		0x20,0x30,0x40,0x50,0x60,0x70,0x80,0x90,
		0xA0,0xB0,0xC0,0xD0,0xE0,0xF1,0x01,0x02,
		0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,
		0x0B,0x0C,0x0D,0x0E,0x0F,0xAA,0xBB,0xCC,
		0xDD,0xEE,0xFF,0x12,0x13,0x14,0x15,0x16
	};
	for(auto& slot : tester.memory) {
		std::copy_n(noise, jjrecord::size, slot);
	}

	jjrecord record;
	bool result = record.read([&](size_t slot_index, uint8_t* out, size_t size) {
		std::copy_n(tester.memory[slot_index], size, out);
		return true;
	});
	CHECK(result == false);
}

TEST_CASE("[jjrecord] fuzz random slots with occasional valid record") {
	using jjrecord = jjrecord<0xA5, 64, 4>;
	jjrecord_tester_t<jjrecord> tester{};
	std::mt19937 rng(0xBEEF);
	std::uniform_int_distribution<int> byte_dist(0, 0xFF);
	std::uniform_int_distribution<int> bool_dist(0, 1);
	std::uniform_int_distribution<int> slot_dist(0, jjrecord::redundancy - 1);
	std::uniform_int_distribution<int> seq_dist(0, jjrecord::redundancy - 1);

	for(int iter = 0; iter < 200; ++iter) {
		// Fill all slots with random noise (CRC will almost never match)
		for(auto& slot : tester.memory) {
			for(size_t i=0; i<jjrecord::size; ++i) {
				slot[i] = static_cast<uint8_t>(byte_dist(rng));
			}
		}

		const bool inject_valid = bool_dist(rng) != 0;
		uint8_t expected_seq = 0;
		uint8_t expected_index = 0;
		if(inject_valid) {
			expected_index = static_cast<uint8_t>(slot_dist(rng));
			expected_seq = static_cast<uint8_t>(seq_dist(rng));
			jjrecord rec{typename jjrecord::slot_t{expected_index, expected_seq}};
			auto* payload = rec.payload();
			for(size_t i=0; i<jjrecord::payload_size; ++i) {
				payload[i] = static_cast<uint8_t>((i + iter) & 0xFF);
			}
			std::copy_n(rec.write_slot(), jjrecord::size, tester.memory[expected_index]);
		}

		jjrecord reader;
		bool result = reader.read([&](size_t slot_index, uint8_t* out, size_t size) {
			std::copy_n(tester.memory[slot_index], size, out);
			return true;
		});

		if(inject_valid) {
			CHECK(result == true);
			const auto payload = reader.payload();
			CHECK(reader.current_slot().index == expected_index);
			CHECK(reader.current_slot().sequence_number == expected_seq);
			for(size_t i=0; i<jjrecord::payload_size; ++i) {
				CHECK(payload[i] == static_cast<uint8_t>((i + iter) & 0xFF));
			}
		} else {
			CHECK(result == false);
		}
	}
}

TEST_SUITE_END();
