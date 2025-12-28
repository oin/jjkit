#include "../ext/doctest.h"
#include "jjrecord.hpp"

TEST_SUITE_BEGIN("jjrecord");

TEST_CASE("jjcrc16") {
	{
		const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
		const uint16_t crc = jjcrc16(data, sizeof(data));
		CHECK(crc == 0x29B1);
	}
	{
		const uint8_t data[] = { 0x3E, 0xD6, 0xB8, 0x4D, 0x21, 0xF1, 0xC8, 0x7F, 0x34, 0xED, 0x12, 0x39, 0x13, 0x70, 0xED, 0x31 };
		const uint16_t crc = jjcrc16(data, sizeof(data));
		CHECK(crc == 0x3016);
	}
	{
		const uint8_t data[] = { 0x10, 0xD8, 0x03, 0xB0, 0x39, 0x26, 0x0D, 0x5A, 0xD6, 0x48, 0xB7, 0x4D, 0x2F, 0xC8, 0x99, 0x6A };
		const uint16_t crc = jjcrc16(data, sizeof(data));
		CHECK(crc == 0xD4D5);
	}
	{
		const uint8_t data[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
		const uint16_t crc = jjcrc16(data, sizeof(data));
		CHECK(crc == 0xC360);
	}
}

template <uint16_t Type, uint16_t Version, size_t Size, size_t Redundancy>
struct jjrecord_tester_t {
	static constexpr jjrecord_config_t config = {
		.type = Type,
		.version = Version,
		.size = Size,
		.redundancy = Redundancy,
	};

	constexpr auto record() const {
		return config.record();
	}

	uint8_t memory[Redundancy][Size];

	void fill(uint8_t* payload) {
		for(size_t i=0; i<config.payload_size(); ++i) {
			payload[i] = i;
		}
	}

	void setup(size_t index, uint8_t sequence_number, uint16_t version = 0xFFFF) {
		jjrecord_t record = config.record(index, sequence_number);
		record.index = index;
		record.sequence_number = sequence_number;
		
		uint8_t payload[config.payload_size()];
		fill(payload);
		payload[0] = index;
		payload[1] = sequence_number;
		record.write(payload, memory[index]);
		if(version != 0xFFFF) {
			// Manually overwrite version if needed
			memory[index][4] = static_cast<uint8_t>(version & 0xFF);
			memory[index][5] = static_cast<uint8_t>((version >> 8) & 0xFF);
		}
	}
};
template <uint16_t Type, uint16_t Version, size_t Size, size_t Redundancy>
constexpr jjrecord_config_t jjrecord_tester_t<Type, Version, Size, Redundancy>::config;

TEST_CASE("[jjrecord] read valid record") {
	using tester_t = jjrecord_tester_t<0x1234, 2, 32, 16>;
	
	tester_t tester{};
	tester.setup(0, 0);
	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	for(size_t i=2; i<tester.config.payload_size(); ++i) {
		CHECK(out[i] == static_cast<uint8_t>(i));
	}
}

TEST_CASE("[jjrecord] read record with invalid CRC") {
	using tester_t = jjrecord_tester_t<0x1234, 2, 32, 16>;
	
	tester_t tester{};
	tester.setup(0, 0);
	// Corrupt the data to cause CRC failure
	tester.memory[0][10] ^= 0xFF;
	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == false);
}

TEST_CASE("[jjrecord] picks newest sequential slot") {
	using tester_t = jjrecord_tester_t<0x1234, 2, 32, 8>;

	tester_t tester{};
	tester.setup(0, 0);
	tester.setup(1, 1);
	tester.setup(2, 2);

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 2);
	CHECK(out[1] == 2);
}

TEST_CASE("[jjrecord] tolerates wraparound of sequence numbers") {
	using tester_t = jjrecord_tester_t<0x1234, 2, 32, 4>;

	tester_t tester{};
	tester.setup(0, 252);
	tester.setup(1, 253);
	tester.setup(2, 254);
	tester.setup(3, 0);

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 3);
	CHECK(out[1] == 0);
}

TEST_CASE("[jjrecord] ignores slots that jump too far ahead") {
	using tester_t = jjrecord_tester_t<0x1234, 2, 32, 4>;

	tester_t tester{};
	tester.setup(0, 0);
	tester.setup(1, 10); // Too far ahead for redundancy window
	tester.setup(2, 1);

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 2);
	CHECK(out[1] == 1);
}

TEST_CASE("[jjrecord] skips newer but incompatible versions") {
	using tester_t = jjrecord_tester_t<0x1234, 2, 32, 4>;

	tester_t tester{};
	tester.setup(0, 5);
	tester.setup(1, 6, 3); // Version newer than supported
	tester.setup(2, 7);

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 2);
	CHECK(out[1] == 7);
}

TEST_CASE("[jjrecord] ignores corrupted type while keeping older valid slot") {
	using tester_t = jjrecord_tester_t<0x1234, 2, 32, 4>;

	tester_t tester{};
	tester.setup(0, 9);
	tester.setup(1, 10);
	// Corrupt type bytes in slot 1
	tester.memory[1][2] ^= 0xFF;
	tester.memory[1][3] ^= 0x01;

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 0);
	CHECK(out[1] == 9);
}

TEST_CASE("[jjrecord] write_next rotates slots and wraps sequence number") {
	using tester_t = jjrecord_tester_t<0xBEEF, 1, 24, 3>;

	tester_t tester{};
	auto writer = tester.config.record(0, 254);

	uint8_t slot[tester.config.size];
	uint8_t payload1[tester.config.payload_size()];
	tester.fill(payload1);
	payload1[0] = 0xAA;
	payload1[1] = 0xFE;
	auto write_fn = [&](size_t slot_index, const uint8_t* data, size_t len) {
		std::copy_n(data, len, tester.memory[slot_index]);
		return true;
	};
	REQUIRE(writer.write_next(payload1, slot, write_fn) == true);

	uint8_t payload2[tester.config.payload_size()];
	tester.fill(payload2);
	payload2[0] = 0xBB;
	payload2[1] = 0x01;
	REQUIRE(writer.write_next(payload2, slot, write_fn) == true);

	auto reader = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = reader.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 0xBB);
	CHECK(out[1] == 0x01);
	for(size_t i=2; i<tester.config.payload_size(); ++i) {
		CHECK(out[i] == payload2[i]);
	}
}

TEST_CASE("[jjrecord] read aborts when a slot read fails") {
	using tester_t = jjrecord_tester_t<0x1234, 1, 24, 3>;

	tester_t tester{};
	tester.setup(0, 0);

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) -> const uint8_t* {
		if(slot_index == 1) {
			return nullptr;
		}
		return tester.memory[slot_index];
	});
	CHECK(result == false);
}

TEST_CASE("[jjrecord] all corrupted slots return false") {
	using tester_t = jjrecord_tester_t<0x1234, 1, 24, 3>;

	tester_t tester{};
	for(size_t i=0; i<tester.config.redundancy; ++i) {
		tester.setup(i, static_cast<uint8_t>(i));
		tester.memory[i][0] ^= 0xFF; // Break CRC
	}

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == false);
}

TEST_CASE("[jjrecord] sequence window accepts redundancy minus one but rejects redundancy") {
	using tester_t = jjrecord_tester_t<0x1234, 1, 24, 4>;

	// Accept distance == redundancy-1
	{
		tester_t tester{};
		tester.setup(0, 0);
		tester.setup(1, 3);
		auto record = tester.record();
		uint8_t out[tester.config.payload_size()];
		bool result = record.read(out, [&](size_t slot_index) {
			return tester.memory[slot_index];
		});
		CHECK(result == true);
		CHECK(out[1] == 3);
	}

	// Reject distance == redundancy
	{
		tester_t tester{};
		tester.setup(0, 0);
		tester.setup(1, 4);
		auto record = tester.record();
		uint8_t out[tester.config.payload_size()];
		bool result = record.read(out, [&](size_t slot_index) {
			return tester.memory[slot_index];
		});
		CHECK(result == true);
		CHECK(out[1] == 0); // Newer slot rejected, older kept
	}
}

TEST_CASE("[jjrecord] write_next surfaces write failures") {
	using tester_t = jjrecord_tester_t<0x1234, 1, 24, 3>;

	tester_t tester{};
	auto writer = tester.config.record(0, 0);

	uint8_t slot[tester.config.size];
	uint8_t payload[tester.config.payload_size()];
	tester.fill(payload);
	payload[0] = 0x11;
	payload[1] = 0x22;
	auto failing_write = [&](size_t, const uint8_t*, size_t) {
		return false;
	};
	CHECK(writer.write_next(payload, slot, failing_write) == false);
}

TEST_CASE("[jjrecord] mixed corruption still finds last good slot") {
	using tester_t = jjrecord_tester_t<0x1234, 2, 32, 4>;

	tester_t tester{};
	tester.setup(0, 1);
	tester.setup(1, 2);
	tester.memory[1][2] ^= 0xFF; // Corrupt type
	tester.setup(2, 3, 3); // Too-new version
	tester.setup(3, 4);

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 3);
	CHECK(out[1] == 4);
}

TEST_CASE("[jjrecord] last duplicate sequence wins") {
	using tester_t = jjrecord_tester_t<0x1234, 1, 24, 3>;

	tester_t tester{};
	uint8_t payload_a[tester.config.payload_size()];
	tester.fill(payload_a);
	payload_a[0] = 0x10;
	auto rec_a = tester.config.record(0, 5);
	rec_a.write(payload_a, tester.memory[0]);

	uint8_t payload_b[tester.config.payload_size()];
	tester.fill(payload_b);
	payload_b[0] = 0xAB;
	auto rec_b = tester.config.record(1, 5);
	rec_b.write(payload_b, tester.memory[1]);

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 0xAB);
}

TEST_CASE("[jjrecord] later older slot is ignored") {
	using tester_t = jjrecord_tester_t<0x1234, 1, 24, 4>;

	tester_t tester{};
	// Newer slot first
	tester.setup(0, 5);
	// Then an older sequence appears afterward
	tester.setup(1, 3);

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 0); // slot 0 chosen
	CHECK(out[1] == 5);
}

TEST_CASE("[jjrecord] older slot arriving after newest stays ignored") {
	using tester_t = jjrecord_tester_t<0x1234, 1, 24, 4>;

	tester_t tester{};
	tester.setup(0, 10);
	tester.setup(1, 11);
	tester.setup(2, 9); // Presented last but older

	auto record = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = record.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 1);
	CHECK(out[1] == 11);
}

TEST_CASE("[jjrecord] write_next remains consistent across multiple wraps") {
	using tester_t = jjrecord_tester_t<0xCAFE, 1, 24, 3>;

	tester_t tester{};
	auto writer = tester.config.record(0, 250);

	uint8_t slot[tester.config.size];
	auto write_fn = [&](size_t slot_index, const uint8_t* data, size_t len) {
		std::copy_n(data, len, tester.memory[slot_index]);
		return true;
	};

	for(int i=0; i<10; ++i) {
		uint8_t payload[tester.config.payload_size()];
		tester.fill(payload);
		payload[0] = static_cast<uint8_t>(i);
		payload[1] = static_cast<uint8_t>((writer.sequence_number + 1) % 0xFF);
		REQUIRE(writer.write_next(payload, slot, write_fn) == true);
	}

	auto reader = tester.record();
	uint8_t out[tester.config.payload_size()];
	bool result = reader.read(out, [&](size_t slot_index) {
		return tester.memory[slot_index];
	});
	CHECK(result == true);
	CHECK(out[0] == 9);
}

TEST_SUITE_END();
