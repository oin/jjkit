#include "../ext/doctest.h"
#include "jjreg.hpp"

TEST_SUITE_BEGIN("jjreg");

enum settings_mode_t : uint8_t {
	settings_mode_a,
	settings_mode_b,
	settings_mode_auto,
	settings_mode_count
};
constexpr auto jjreg_settings_mode = jjreg_e8<settings_mode_t>{settings_mode_count, settings_mode_auto};

#define SETTINGS(_) \
	_(brightness, (jjreg_u8{0, 100, 80})) \
	_(octave, (jjreg_i8{-2, 2, 0})) \
	_(mode, jjreg_settings_mode) \
	_(scores, (jjreg_list<jjreg_u8, 10>{{0, 50, 25}})) \
	_(title, (jjreg_string<16>{"untitled"}))
JJREG(jjreg_settings_t, SETTINGS);
constexpr jjreg_settings_t jjreg_settings;

enum clamp_mode_t : uint8_t {
	clamp_mode_a,
	clamp_mode_b,
	clamp_mode_c,
	clamp_mode_count
};
constexpr auto jjreg_clamp_mode = jjreg_e8<clamp_mode_t>{clamp_mode_count, clamp_mode_b};

#define SIMPLE_FIELDS(_) \
	_(a, (jjreg_u8{0, 10, 5})) \
	_(title, (jjreg_string<4>{"xy"})) \
	_(scores, (jjreg_list<jjreg_u8, 2>{{0, 3, 1}}))
JJREG(jjreg_simple_schema_t, SIMPLE_FIELDS);
constexpr jjreg_simple_schema_t jjreg_simple_schema;

#define CLAMP_FIELDS(_) \
	_(u, (jjreg_u8{1, 5, 3})) \
	_(i, (jjreg_i8{-3, 3, 0})) \
	_(e, jjreg_clamp_mode)
JJREG(jjreg_clamp_t, CLAMP_FIELDS);
constexpr jjreg_clamp_t jjreg_clamp;

#define TITLE_FIELDS(_) \
	_(title, (jjreg_string<8>{"abc"}))
JJREG(jjreg_title_t, TITLE_FIELDS);
constexpr jjreg_title_t jjreg_title;

#define LIST_FIELDS(_) \
	_(scores, (jjreg_list<jjreg_u8, 2>{{0, 10, 1}}))
JJREG(jjreg_scores_t, LIST_FIELDS);
constexpr jjreg_scores_t jjreg_scores;

#define ARRAY_FIELDS(_) \
	_(values, (jjreg_array<jjreg_u8, 3>{{0, 9, 5}}))
JJREG(jjreg_array_schema_t, ARRAY_FIELDS);
constexpr jjreg_array_schema_t jjreg_array_schema;

struct point_t {
	uint16_t x;
	int16_t y;
};

struct point_meta_t {
	using field_type = point_t;
	static constexpr size_t field_size = sizeof(point_t);
	point_t default_value{1, -1};

	void write(const point_t& v, uint8_t* out) const {
		std::memcpy(out, &v, sizeof(point_t));
	}
	point_t read(const uint8_t* in) const {
		point_t v;
		std::memcpy(&v, in, sizeof(point_t));
		return v;
	}
};

#define POINT_FIELDS(_) \
	_(p, (point_meta_t{}))
JJREG(jjreg_point_t, POINT_FIELDS);
constexpr jjreg_point_t jjreg_point;

TEST_CASE("[jjreg] simple test") {
	uint8_t data[512] = {0};
	auto settings = jjreg_settings(data);
	settings.reset();

	settings.brightness = 120;
	CHECK(settings.brightness == 100);

	settings.title = "This is a simple test, and it is quite long";
	CHECK(std::string(settings.title) == "This is a simpl");
	settings.scores.push_back(75);
	CHECK(settings.scores.size() == 1);
	CHECK(settings.scores[0] == 50);
	auto m = settings.mode;
	CHECK(m == settings_mode_auto);

	CHECK(settings.meta.size() == 30);
}

TEST_CASE("[jjreg] schema size and offsets") {
	uint8_t data[32] = {0};
	auto view = jjreg_simple_schema(data);

	CHECK(view.meta.field_size(jjreg_simple_schema_t::_index_a) == 1);
	CHECK(view.meta.field_size(jjreg_simple_schema_t::_index_title) == 4);
	CHECK(view.meta.field_size(jjreg_simple_schema_t::_index_scores) == 3);
	CHECK(view.meta.size() == 8);

	CHECK(view.title.ptr == view.ptr + 1);
	CHECK(view.scores.ptr == view.ptr + 5);

	view.reset();
	CHECK(view.a == 5);
	CHECK(std::string(view.title) == "xy");
	CHECK(view.scores.size() == 0);
}

TEST_CASE("[jjreg] clamping and defaults") {
	uint8_t data[32] = {0};
	auto view = jjreg_clamp(data);
	view.reset();

	view.u = 0;
	CHECK(view.u == 1);
	view.u = 99;
	CHECK(view.u == 5);

	view.i = -10;
	CHECK(view.i == -3);
	view.i = 9;
	CHECK(view.i == 3);

	view.e = static_cast<clamp_mode_t>(clamp_mode_count);
	CHECK(view.e == clamp_mode_c);
}

TEST_CASE("[jjreg] string truncation and null terminator") {
	uint8_t data[32];
	std::memset(data, 0xFF, sizeof(data));
	auto view = jjreg_title(data);
	view.reset();

	CHECK(std::string(view.title) == "abc");
	view.title = "1234567890";
	CHECK(std::string(view.title) == "1234567");
	CHECK(data[7] == 0);
}

TEST_CASE("[jjreg] list capacity and clamping") {
	uint8_t data[32] = {0};
	auto view = jjreg_scores(data);
	view.reset();

	CHECK(view.scores.size() == 0);
	CHECK(view.scores.push_back(5));
	CHECK(view.scores.push_back(12));
	CHECK_FALSE(view.scores.push_back(1));
	CHECK(view.scores.size() == 2);
	CHECK(view.scores[0] == 5);
	CHECK(view.scores[1] == 10);
}

TEST_CASE("[jjreg] array reset and set") {
	uint8_t data[32] = {0};
	auto view = jjreg_array_schema(data);
	view.reset();

	CHECK(view.values[0] == 5);
	CHECK(view.values[1] == 5);
	CHECK(view.values[2] == 5);

	uint8_t payload[3] = {0, 4, 9};
	view.values.set(payload, 3);
	CHECK(view.values[0] == 0);
	CHECK(view.values[1] == 4);
	CHECK(view.values[2] == 9);

	view.values[2] = 42;
	CHECK(view.values[2] == 9);
}

TEST_CASE("[jjreg] struct round trip") {
	uint8_t data[32] = {0};
	auto view = jjreg_point(data);
	view.reset();

	point_t p = view.p;
	CHECK(p.x == 1);
	CHECK(p.y == -1);

	view.p = point_t{9, -7};
	point_t read_back = view.p;
	CHECK(read_back.x == 9);
	CHECK(read_back.y == -7);
}

TEST_CASE("[jjreg] shared view coherence") {
	uint8_t data[64] = {0};
	auto a = jjreg_scores(data);
	auto b = jjreg_scores(data);

	a.reset();
	b.scores.push_back(3);
	CHECK(a.scores.size() == 1);
	CHECK(a.scores[0] == 3);

	uint8_t other[64] = {0};
	auto isolated = jjreg_scores(other);
	isolated.reset();
	isolated.scores.push_back(8);
	CHECK(a.scores[0] == 3);
	CHECK(isolated.scores[0] == 8);
}

TEST_SUITE_END();
