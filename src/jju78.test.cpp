#include "../ext/doctest.h"
#include "jju78.hpp"

TEST_SUITE_BEGIN("jju78");

// --- Size ---

TEST_CASE("[jju78][size] jju78_size returns worst-case bound") {
	CHECK(jju78_size(0) == 0);
	CHECK(jju78_size(1) == 2);
	CHECK(jju78_size(4) == 8);
	CHECK(jju78_size(256) == 512);
	static_assert(jju78_size(10) == 20, "jju78_size must be constexpr");
}

// --- Constants ---

TEST_CASE("[jju78][constants] control codes have expected values") {
	CHECK(jju78_high == 0x7D);
	CHECK(jju78_esc == 0x7C);
	CHECK(jju78_high < 0x80);
	CHECK(jju78_esc < 0x80);
	static_assert(jju78_high == 0x7D, "");
	static_assert(jju78_esc == 0x7C, "");
}

// --- Encoding (8to7) ---

TEST_CASE("[jju78][8to7] empty input") {
	uint8_t out[1];
	size_t n = jju78_write(nullptr, 0, out);
	CHECK(n == 0);
}

TEST_CASE("[jju78][8to7] normal bytes pass through") {
	constexpr uint8_t input[] = {0x00, 0x01, 0x42, 0x77};
	uint8_t out[jju78_size(sizeof(input))];
	size_t n = jju78_write(input, sizeof(input), out);
	CHECK(n == sizeof(input));
	for(size_t i = 0; i < sizeof(input); ++i) {
		CHECK(out[i] == input[i]);
	}
}

TEST_CASE("[jju78][8to7] special byte 0x7C is escaped") {
	constexpr uint8_t input[] = {jju78_esc};
	uint8_t out[jju78_size(sizeof(input))];
	size_t n = jju78_write(input, sizeof(input), out);
	CHECK(n == 2);
	CHECK(out[0] == jju78_esc);
	CHECK(out[1] == 0x7C);
}

TEST_CASE("[jju78][8to7] special byte 0x7D is escaped") {
	constexpr uint8_t input[] = {jju78_high};
	uint8_t out[jju78_size(sizeof(input))];
	size_t n = jju78_write(input, sizeof(input), out);
	CHECK(n == 2);
	CHECK(out[0] == jju78_esc);
	CHECK(out[1] == 0x7D);
}

TEST_CASE("[jju78][8to7] high bit byte encoding") {
	SUBCASE("0x80 encodes to [0x7D, 0x00]") {
		constexpr uint8_t input[] = {0x80};
		uint8_t out[jju78_size(sizeof(input))];
		size_t n = jju78_write(input, sizeof(input), out);
		CHECK(n == 2);
		CHECK(out[0] == jju78_high);
		CHECK(out[1] == 0x00);
	}
	SUBCASE("0xFF encodes to [0x7D, 0x7F]") {
		constexpr uint8_t input[] = {0xFF};
		uint8_t out[jju78_size(sizeof(input))];
		size_t n = jju78_write(input, sizeof(input), out);
		CHECK(n == 2);
		CHECK(out[0] == jju78_high);
		CHECK(out[1] == 0x7F);
	}
	SUBCASE("0xFC encodes to [0x7D, 0x7C]") {
		constexpr uint8_t input[] = {0xFC};
		uint8_t out[jju78_size(sizeof(input))];
		size_t n = jju78_write(input, sizeof(input), out);
		CHECK(n == 2);
		CHECK(out[0] == jju78_high);
		CHECK(out[1] == 0x7C);
	}
	SUBCASE("0xFD encodes to [0x7D, 0x7D]") {
		constexpr uint8_t input[] = {0xFD};
		uint8_t out[jju78_size(sizeof(input))];
		size_t n = jju78_write(input, sizeof(input), out);
		CHECK(n == 2);
		CHECK(out[0] == jju78_high);
		CHECK(out[1] == 0x7D);
	}
}

TEST_CASE("[jju78][8to7] output is always 7-bit safe") {
	uint8_t input[256];
	for(int i = 0; i < 256; ++i) input[i] = (uint8_t)i;
	uint8_t out[jju78_size(256)];
	size_t n = jju78_write(input, 256, out);
	for(size_t i = 0; i < n; ++i) {
		CHECK_MESSAGE(out[i] < 0x80, "byte " << i << " = 0x" << std::hex << (int)out[i]);
	}
}

TEST_CASE("[jju78][8to7] encoded size within bound") {
	uint8_t input[256];
	for(int i = 0; i < 256; ++i) input[i] = (uint8_t)i;
	uint8_t out[jju78_size(256)];
	size_t n = jju78_write(input, 256, out);
	CHECK(n <= jju78_size(256));
}

TEST_CASE("[jju78][8to7] consecutive special bytes") {
	constexpr uint8_t input[] = {0x7C, 0x7D, 0x7C, 0x7D};
	uint8_t out[jju78_size(sizeof(input))];
	size_t n = jju78_write(input, sizeof(input), out);
	CHECK(n == 8);
	CHECK(out[0] == jju78_esc); CHECK(out[1] == 0x7C);
	CHECK(out[2] == jju78_esc); CHECK(out[3] == 0x7D);
	CHECK(out[4] == jju78_esc); CHECK(out[5] == 0x7C);
	CHECK(out[6] == jju78_esc); CHECK(out[7] == 0x7D);
}

// --- Decoding (7to8) ---

TEST_CASE("[jju78][7to8] empty input") {
	uint8_t out[1];
	size_t n = jju78_read(nullptr, 0, out);
	CHECK(n == 0);
}

TEST_CASE("[jju78][7to8] normal bytes pass through") {
	constexpr uint8_t input[] = {0x00, 0x01, 0x42, 0x77};
	uint8_t out[sizeof(input)];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == sizeof(input));
	for(size_t i = 0; i < sizeof(input); ++i) {
		CHECK(out[i] == input[i]);
	}
}

TEST_CASE("[jju78][7to8] high prefix decodes to high-bit byte") {
	constexpr uint8_t input[] = {0x7D, 0x00};
	uint8_t out[2];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == 1);
	CHECK(out[0] == 0x80);
}

TEST_CASE("[jju78][7to8] escape prefix decodes to literal") {
	SUBCASE("escape + 0x7C = 0x7C") {
		constexpr uint8_t input[] = {0x7C, 0x7C};
		uint8_t out[2];
		size_t n = jju78_read(input, sizeof(input), out);
		CHECK(n == 1);
		CHECK(out[0] == 0x7C);
	}
	SUBCASE("escape + 0x7D = 0x7D") {
		constexpr uint8_t input[] = {0x7C, 0x7D};
		uint8_t out[2];
		size_t n = jju78_read(input, sizeof(input), out);
		CHECK(n == 1);
		CHECK(out[0] == 0x7D);
	}
}

TEST_CASE("[jju78][7to8] trailing high flag is silently ignored") {
	constexpr uint8_t input[] = {0x42, 0x7D};
	uint8_t out[2];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == 1);
	CHECK(out[0] == 0x42);
}

TEST_CASE("[jju78][7to8] trailing escape flag is silently ignored") {
	constexpr uint8_t input[] = {0x42, 0x7C};
	uint8_t out[2];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == 1);
	CHECK(out[0] == 0x42);
}

// --- Round-trip ---

TEST_CASE("[jju78][roundtrip] single byte round-trip for all 256 values") {
	for(int v = 0; v < 256; ++v) {
		uint8_t input[] = {(uint8_t)v};
		uint8_t encoded[jju78_size(1)];
		size_t enc_size = jju78_write(input, 1, encoded);
		uint8_t decoded[1];
		size_t dec_size = jju78_read(encoded, enc_size, decoded);
		CHECK_MESSAGE(dec_size == 1, "value 0x" << std::hex << v);
		CHECK_MESSAGE(decoded[0] == (uint8_t)v, "value 0x" << std::hex << v);
	}
}

TEST_CASE("[jju78][roundtrip] all 256 byte values in sequence") {
	uint8_t input[256];
	for(int i = 0; i < 256; ++i) input[i] = (uint8_t)i;
	uint8_t encoded[jju78_size(256)];
	size_t enc_size = jju78_write(input, 256, encoded);
	uint8_t decoded[256];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == 256);
	for(int i = 0; i < 256; ++i) {
		CHECK_MESSAGE(decoded[i] == (uint8_t)i, "index " << i);
	}
}

TEST_CASE("[jju78][roundtrip] all zeros") {
	constexpr size_t N = 64;
	uint8_t input[N] = {};
	uint8_t encoded[jju78_size(N)];
	size_t enc_size = jju78_write(input, N, encoded);
	CHECK(enc_size == N);
	uint8_t decoded[N];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == N);
	for(size_t i = 0; i < N; ++i) {
		CHECK(decoded[i] == 0);
	}
}

TEST_CASE("[jju78][roundtrip] all 0xFF") {
	constexpr size_t N = 64;
	uint8_t input[N];
	for(size_t i = 0; i < N; ++i) input[i] = 0xFF;
	uint8_t encoded[jju78_size(N)];
	size_t enc_size = jju78_write(input, N, encoded);
	CHECK(enc_size == N * 2);
	uint8_t decoded[N];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == N);
	for(size_t i = 0; i < N; ++i) {
		CHECK(decoded[i] == 0xFF);
	}
}

TEST_CASE("[jju78][roundtrip] alternating low and high bytes") {
	constexpr uint8_t input[] = {0x00, 0xFF, 0x01, 0xFE, 0x42, 0x80, 0x77, 0xAA};
	uint8_t encoded[jju78_size(sizeof(input))];
	size_t enc_size = jju78_write(input, sizeof(input), encoded);
	uint8_t decoded[sizeof(input)];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == sizeof(input));
	for(size_t i = 0; i < sizeof(input); ++i) {
		CHECK(decoded[i] == input[i]);
	}
}

TEST_CASE("[jju78][roundtrip] run of special bytes") {
	constexpr uint8_t input[] = {0x7C, 0x7C, 0x7D, 0x7D, 0x7C, 0x7D, 0xFC, 0xFD};
	uint8_t encoded[jju78_size(sizeof(input))];
	size_t enc_size = jju78_write(input, sizeof(input), encoded);
	uint8_t decoded[sizeof(input)];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == sizeof(input));
	for(size_t i = 0; i < sizeof(input); ++i) {
		CHECK_MESSAGE(decoded[i] == input[i], "index " << i);
	}
}

// --- Encoding (8to7) additional ---

TEST_CASE("[jju78][8to7] boundary bytes around control codes pass through") {
	constexpr uint8_t input[] = {0x7B, 0x7E, 0x7F};
	uint8_t out[jju78_size(sizeof(input))];
	size_t n = jju78_write(input, sizeof(input), out);
	CHECK(n == 3);
	CHECK(out[0] == 0x7B);
	CHECK(out[1] == 0x7E);
	CHECK(out[2] == 0x7F);
}

TEST_CASE("[jju78][8to7] single byte 0x00 passes through") {
	constexpr uint8_t input[] = {0x00};
	uint8_t out[jju78_size(1)];
	size_t n = jju78_write(input, 1, out);
	CHECK(n == 1);
	CHECK(out[0] == 0x00);
}

TEST_CASE("[jju78][8to7] single byte 0x7F passes through") {
	constexpr uint8_t input[] = {0x7F};
	uint8_t out[jju78_size(1)];
	size_t n = jju78_write(input, 1, out);
	CHECK(n == 1);
	CHECK(out[0] == 0x7F);
}

TEST_CASE("[jju78][8to7] all high-bit bytes produce worst-case size") {
	constexpr size_t N = 32;
	uint8_t input[N];
	for(size_t i = 0; i < N; ++i) input[i] = (uint8_t)(0x80 + i);
	uint8_t out[jju78_size(N)];
	size_t n = jju78_write(input, N, out);
	CHECK(n == N * 2);
}

TEST_CASE("[jju78][8to7] exact encoded output for mixed sequence") {
	// normal, esc, high, high-bit, normal
	constexpr uint8_t input[] = {0x42, 0x7C, 0x7D, 0x80, 0x01};
	uint8_t out[jju78_size(sizeof(input))];
	size_t n = jju78_write(input, sizeof(input), out);
	// 0x42 -> 0x42
	// 0x7C -> 0x7C 0x7C
	// 0x7D -> 0x7C 0x7D
	// 0x80 -> 0x7D 0x00
	// 0x01 -> 0x01
	CHECK(n == 8);
	CHECK(out[0] == 0x42);
	CHECK(out[1] == 0x7C); CHECK(out[2] == 0x7C);
	CHECK(out[3] == 0x7C); CHECK(out[4] == 0x7D);
	CHECK(out[5] == 0x7D); CHECK(out[6] == 0x00);
	CHECK(out[7] == 0x01);
}

TEST_CASE("[jju78][8to7] encoded size equals input plus prefix count") {
	uint8_t input[256];
	for(int i = 0; i < 256; ++i) input[i] = (uint8_t)i;
	uint8_t out[jju78_size(256)];
	size_t n = jju78_write(input, 256, out);
	// Prefixes needed: 0x7C, 0x7D each get +1, 0x80-0xFF (128 bytes) each get +1
	// Total prefixes = 2 + 128 = 130
	CHECK(n == 256 + 130);
}

TEST_CASE("[jju78][8to7] normal bytes between specials stay untouched") {
	constexpr uint8_t input[] = {0x7C, 0x42, 0x7D, 0x13, 0x80};
	uint8_t out[jju78_size(sizeof(input))];
	size_t n = jju78_write(input, sizeof(input), out);
	// 0x7C -> 0x7C 0x7C, 0x42 -> 0x42, 0x7D -> 0x7C 0x7D, 0x13 -> 0x13, 0x80 -> 0x7D 0x00
	CHECK(n == 8);
	CHECK(out[2] == 0x42);
	CHECK(out[5] == 0x13);
}

// --- Decoding (7to8) additional ---

TEST_CASE("[jju78][7to8] high prefix with various values") {
	constexpr uint8_t input[] = {0x7D, 0x01, 0x7D, 0x3F, 0x7D, 0x7F};
	uint8_t out[6];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == 3);
	CHECK(out[0] == 0x81);
	CHECK(out[1] == 0xBF);
	CHECK(out[2] == 0xFF);
}

TEST_CASE("[jju78][7to8] escape passes through any byte") {
	constexpr uint8_t input[] = {0x7C, 0x00, 0x7C, 0x42, 0x7C, 0x7F};
	uint8_t out[6];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == 3);
	CHECK(out[0] == 0x00);
	CHECK(out[1] == 0x42);
	CHECK(out[2] == 0x7F);
}

TEST_CASE("[jju78][7to8] interleaved high and escape prefixes") {
	// high(0x01) esc(0x7C) high(0x7F) esc(0x7D) normal(0x42)
	constexpr uint8_t input[] = {0x7D, 0x01, 0x7C, 0x7C, 0x7D, 0x7F, 0x7C, 0x7D, 0x42};
	uint8_t out[9];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == 5);
	CHECK(out[0] == 0x81);
	CHECK(out[1] == 0x7C);
	CHECK(out[2] == 0xFF);
	CHECK(out[3] == 0x7D);
	CHECK(out[4] == 0x42);
}

TEST_CASE("[jju78][7to8] consecutive high prefixes") {
	constexpr uint8_t input[] = {0x7D, 0x00, 0x7D, 0x7F, 0x7D, 0x3C};
	uint8_t out[6];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == 3);
	CHECK(out[0] == 0x80);
	CHECK(out[1] == 0xFF);
	CHECK(out[2] == 0xBC);
}

TEST_CASE("[jju78][7to8] only a high prefix byte alone") {
	constexpr uint8_t input[] = {0x7D};
	uint8_t out[1];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == 0);
}

TEST_CASE("[jju78][7to8] only an escape byte alone") {
	constexpr uint8_t input[] = {0x7C};
	uint8_t out[1];
	size_t n = jju78_read(input, sizeof(input), out);
	CHECK(n == 0);
}

TEST_CASE("[jju78][7to8] decoded size is always <= encoded size") {
	uint8_t input[256];
	for(int i = 0; i < 256; ++i) input[i] = (uint8_t)i;
	uint8_t encoded[jju78_size(256)];
	size_t enc_size = jju78_write(input, 256, encoded);
	uint8_t decoded[256];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size <= enc_size);
}

// --- Round-trip additional ---

TEST_CASE("[jju78][roundtrip] boundary region 0x7A to 0x81") {
	constexpr uint8_t input[] = {0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81};
	uint8_t encoded[jju78_size(sizeof(input))];
	size_t enc_size = jju78_write(input, sizeof(input), encoded);
	uint8_t decoded[sizeof(input)];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == sizeof(input));
	for(size_t i = 0; i < sizeof(input); ++i) {
		CHECK_MESSAGE(decoded[i] == input[i], "index " << i);
	}
}

TEST_CASE("[jju78][roundtrip] buffer of only esc bytes") {
	constexpr size_t N = 32;
	uint8_t input[N];
	for(size_t i = 0; i < N; ++i) input[i] = 0x7C;
	uint8_t encoded[jju78_size(N)];
	size_t enc_size = jju78_write(input, N, encoded);
	CHECK(enc_size == N * 2);
	uint8_t decoded[N];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == N);
	for(size_t i = 0; i < N; ++i) {
		CHECK(decoded[i] == 0x7C);
	}
}

TEST_CASE("[jju78][roundtrip] buffer of only high bytes") {
	constexpr size_t N = 32;
	uint8_t input[N];
	for(size_t i = 0; i < N; ++i) input[i] = 0x7D;
	uint8_t encoded[jju78_size(N)];
	size_t enc_size = jju78_write(input, N, encoded);
	CHECK(enc_size == N * 2);
	uint8_t decoded[N];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == N);
	for(size_t i = 0; i < N; ++i) {
		CHECK(decoded[i] == 0x7D);
	}
}

TEST_CASE("[jju78][roundtrip] large buffer with repeating pattern") {
	constexpr size_t N = 1024;
	uint8_t input[N];
	for(size_t i = 0; i < N; ++i) input[i] = (uint8_t)(i * 7 + 13);
	uint8_t encoded[jju78_size(N)];
	size_t enc_size = jju78_write(input, N, encoded);
	uint8_t decoded[N];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == N);
	for(size_t i = 0; i < N; ++i) {
		CHECK_MESSAGE(decoded[i] == input[i], "index " << i);
	}
}

TEST_CASE("[jju78][roundtrip] realistic SysEx config payload") {
	// Simulates a device config: header, param ID, value bytes, checksum
	constexpr uint8_t input[] = {
		0x01, 0x02, 0x03,       // header
		0x7C, 0x7D,             // param ID (happens to be control codes)
		0x00, 0xFF, 0x80, 0xA5, // value: min, max, mid, arbitrary
		0x42,                    // checksum
	};
	uint8_t encoded[jju78_size(sizeof(input))];
	size_t enc_size = jju78_write(input, sizeof(input), encoded);
	for(size_t i = 0; i < enc_size; ++i) {
		CHECK_MESSAGE(encoded[i] < 0x80, "encoded byte " << i);
	}
	uint8_t decoded[sizeof(input)];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == sizeof(input));
	for(size_t i = 0; i < sizeof(input); ++i) {
		CHECK_MESSAGE(decoded[i] == input[i], "index " << i);
	}
}

TEST_CASE("[jju78][roundtrip] reversed 256 byte values") {
	uint8_t input[256];
	for(int i = 0; i < 256; ++i) input[i] = (uint8_t)(255 - i);
	uint8_t encoded[jju78_size(256)];
	size_t enc_size = jju78_write(input, 256, encoded);
	uint8_t decoded[256];
	size_t dec_size = jju78_read(encoded, enc_size, decoded);
	CHECK(dec_size == 256);
	for(int i = 0; i < 256; ++i) {
		CHECK_MESSAGE(decoded[i] == (uint8_t)(255 - i), "index " << i);
	}
}

TEST_SUITE_END();
