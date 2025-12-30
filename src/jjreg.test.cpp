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
JJREG(jjreg_settings_t, 30, SETTINGS);
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
JJREG(jjreg_simple_schema_t, 10, SIMPLE_FIELDS);
constexpr jjreg_simple_schema_t jjreg_simple_schema;

#define CLAMP_FIELDS(_) \
	_(u, (jjreg_u8{1, 5, 3})) \
	_(i, (jjreg_i8{-3, 3, 0})) \
	_(e, jjreg_clamp_mode)
JJREG(jjreg_clamp_t, 3, CLAMP_FIELDS);
constexpr jjreg_clamp_t jjreg_clamp;

#define TITLE_FIELDS(_) \
	_(title, (jjreg_string<8>{"abc"}))
JJREG(jjreg_title_t, 10, TITLE_FIELDS);
constexpr jjreg_title_t jjreg_title;

#define LIST_FIELDS(_) \
	_(scores, (jjreg_list<jjreg_u8, 2>{{0, 10, 1}}))
JJREG(jjreg_scores_t, 128, LIST_FIELDS);
constexpr jjreg_scores_t jjreg_scores;

#define ARRAY_FIELDS(_) \
	_(values, (jjreg_array<jjreg_u8, 3>{{0, 9, 5}}))
JJREG(jjreg_array_schema_t, 3, ARRAY_FIELDS);
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
JJREG(jjreg_point_t, 4, POINT_FIELDS);
constexpr jjreg_point_t jjreg_point;

enum single_enum_t : uint8_t {
	single_only,
	single_enum_count
};
constexpr auto jjreg_single_enum = jjreg_e8<single_enum_t>{single_enum_count, single_only};

#define SINGLE_ENUM_FIELDS(_) \
	_(e, jjreg_single_enum)
JJREG(jjreg_single_enum_schema_t, 4, SINGLE_ENUM_FIELDS);
constexpr jjreg_single_enum_schema_t jjreg_single_enum_schema;

struct be_word_meta_t {
	using field_type = uint16_t;
	static constexpr size_t field_size = 2;
	uint16_t default_value = 0x1234;

	void write(uint16_t v, uint8_t* out) const {
		out[0] = static_cast<uint8_t>(v >> 8);
		out[1] = static_cast<uint8_t>(v & 0xFF);
	}
	uint16_t read(const uint8_t* in) const {
		return static_cast<uint16_t>((static_cast<uint16_t>(in[0]) << 8) | in[1]);
	}
};

#define BE_WORD_FIELDS(_) \
	_(word, (be_word_meta_t{}))
JJREG(jjreg_be_word_schema_t, 4, BE_WORD_FIELDS);
constexpr jjreg_be_word_schema_t jjreg_be_word_schema;

#define PADDING_FIELDS(_) \
	_(a, (jjreg_u8{0, 9, 2})) \
	_(b, (jjreg_u8{0, 9, 7}))
JJREG(jjreg_padding_schema_t, 16, PADDING_FIELDS);
constexpr jjreg_padding_schema_t jjreg_padding_schema;

#define MIXED_STR_ARRAY_FIELDS(_) \
	_(labels, (jjreg_array<jjreg_string<5>, 3>{"def"}))
JJREG(jjreg_mixed_str_array_t, 32, MIXED_STR_ARRAY_FIELDS);
constexpr jjreg_mixed_str_array_t jjreg_mixed_str_array;

#define SMALL_FIELD(_) \
	_(v, (jjreg_u8{1, 9, 4}))
JJREG(jjreg_small_field_t, 3, SMALL_FIELD);
constexpr jjreg_small_field_t jjreg_small_field;

#define TWO_NESTED_FIELDS(_) \
	_(left, (jjreg_nested(jjreg_small_field))) \
	_(right, (jjreg_nested(jjreg_small_field))) \
	_(tail, (jjreg_u8{0, 5, 2}))
JJREG(jjreg_two_nested_t, 10, TWO_NESTED_FIELDS);
constexpr jjreg_two_nested_t jjreg_two_nested;

#define ALIGN_FIELDS(_) \
	_(lead, (jjreg_u8{0, 255, 1})) \
	_(word, (be_word_meta_t{})) \
	_(trail, (jjreg_u8{0, 255, 2}))
JJREG(jjreg_align_schema_t, 8, ALIGN_FIELDS);
constexpr jjreg_align_schema_t jjreg_align_schema;

#define CLAMP_FUZZ_FIELDS(_) \
	_(u, (jjreg_u8{10, 20, 15})) \
	_(i, (jjreg_i8{-5, 5, 0})) \
	_(e, jjreg_clamp_mode)
JJREG(jjreg_clamp_fuzz_t, 6, CLAMP_FUZZ_FIELDS);
constexpr jjreg_clamp_fuzz_t jjreg_clamp_fuzz;

struct point_default_meta_t {
	using field_type = point_t;
	static constexpr size_t field_size = sizeof(point_t);
	point_t default_value{7, -3};

	void write(const point_t& v, uint8_t* out) const {
		std::memcpy(out, &v, sizeof(point_t));
	}
	point_t read(const uint8_t* in) const {
		point_t v;
		std::memcpy(&v, in, sizeof(point_t));
		return v;
	}
};

#define POINT_ARRAY_FIELDS(_) \
	_(pts, (jjreg_array<point_default_meta_t, 2>{}))
JJREG(jjreg_point_array_t, 12, POINT_ARRAY_FIELDS);
constexpr jjreg_point_array_t jjreg_point_array;

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

	CHECK(settings.meta.size == 30);
}

TEST_CASE("[jjreg] schema size and offsets") {
	uint8_t data[32] = {0};
	auto view = jjreg_simple_schema(data);

	CHECK(view.meta.field_size(jjreg_simple_schema_t::_index_a) == 1);
	CHECK(view.meta.field_size(jjreg_simple_schema_t::_index_title) == 4);
	CHECK(view.meta.field_size(jjreg_simple_schema_t::_index_scores) == 3);
	CHECK(view.meta.size == 8);

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

#define SUPERSCHEMA(_) \
	_(version, (jjreg_u8{1, 0xFF, 0})) \
	_(point, (jjreg_nested(jjreg_scores))) \
	_(label, (jjreg_string<6>{"point"}))
JJREG(jjreg_superschema_t, 140, SUPERSCHEMA);
constexpr jjreg_superschema_t jjreg_superschema;

TEST_CASE("[jjreg] subschema in schema") {
	auto root = jjreg_superschema();
	root.point.scores.push_back(4);
	root.point.scores.push_back(8);
	CHECK(root.point.scores.size() == 2);
}

#define SUPERSUPERSCHEMA(_) \
	_(header, (jjreg_string<8>{"jjkitv1"})) \
	_(data1, (jjreg_nested(jjreg_superschema))) \
	_(data2, (jjreg_array<jjreg_string<4>, 2>{"ab"})) \
	_(footer, (jjreg_u8{0, 255, 42}))
JJREG(jjreg_supersuperschema_t, 200, SUPERSUPERSCHEMA);
constexpr jjreg_supersuperschema_t jjreg_supersuperschema;

TEST_CASE("[jjreg] nested subschema and array of strings") {
	auto root = jjreg_supersuperschema();
	CHECK(std::string(root.header) == "jjkitv1");
	CHECK(root.footer == 42);

	root.data1.point.scores.push_back(7);
	CHECK(root.data1.point.scores.size() == 1);

	constexpr const char* strings[] = { "hi", "ok" };
	root.data2.set(strings, 2);
	CHECK(std::string(root.data2[0]) == "hi");
	CHECK(std::string(root.data2[1]) == "ok");
}

TEST_CASE("[jjreg] string robustness and termination") {
	uint8_t data[16];
	std::memset(data, 0xAA, sizeof(data));
	auto view = jjreg_title(data);
	view.reset();

	char raw_no_null[16];
	std::memset(raw_no_null, 'z', sizeof(raw_no_null));
	raw_no_null[15] = 0;
	view.title = raw_no_null;
	CHECK(std::string(view.title) == "zzzzzzz");
	CHECK(data[7] == 0);

	std::memset(data, 0x55, sizeof(data));
	view.title = "abcdefghijk";
	CHECK(std::string(view.title) == "abcdefg");
	CHECK(data[7] == 0);

	view.title = "";
	CHECK(std::string(view.title) == "");
	CHECK(data[0] == 0);
}

TEST_CASE("[jjreg] list capacity reuse and overflow guard") {
	uint8_t data[16] = {0};
	auto view = jjreg_scores(data);
	view.reset();

	CHECK(view.scores.push_back(2));
	CHECK(view.scores.push_back(9));
	CHECK_FALSE(view.scores.push_back(7));
	CHECK(view.scores.size() == 2);
	CHECK(view.scores[0] == 2);
	CHECK(view.scores[1] == 9);

	view.reset();
	CHECK(view.scores.size() == 0);
	CHECK(view.scores.push_back(4));
	CHECK(view.scores.size() == 1);

	view.scores[0] = 3;
	CHECK(view.scores[0] == 3);

	for(int i=0; i<5; ++i) {
		view.reset();
		CHECK(view.scores.size() == 0);
		CHECK(view.scores.push_back(static_cast<uint8_t>(i)));
		CHECK(view.scores.size() == 1);
	}
}

TEST_CASE("[jjreg] enum and integer clamping extremes") {
	uint8_t data[16] = {0};
	auto tiny = jjreg_single_enum_schema(data);
	tiny.reset();

	tiny.e = static_cast<single_enum_t>(9);
	CHECK(tiny.e == single_only);
	tiny.e = static_cast<single_enum_t>(0);
	CHECK(tiny.e == single_only);

	auto clamp = jjreg_clamp(data);
	clamp.reset();

	clamp.u = static_cast<uint8_t>(-1);
	CHECK(clamp.u == 5);
	clamp.i = static_cast<int8_t>(-120);
	CHECK(clamp.i == -3);
	clamp.i = static_cast<int8_t>(120);
	CHECK(clamp.i == 3);
}

TEST_CASE("[jjreg] clamping table coverage") {
	uint8_t data[8] = {0};
	auto view = jjreg_clamp_fuzz(data);
	view.reset();

	for(int v = 0; v <= 30; ++v) {
		view.u = static_cast<uint8_t>(v);
		CHECK(view.u >= 10);
		CHECK(view.u <= 20);
	}

	for(int v = -20; v <= 20; ++v) {
		view.i = static_cast<int8_t>(v);
		CHECK(view.i >= -5);
		CHECK(view.i <= 5);
	}

	for(int v = -3; v < static_cast<int>(clamp_mode_count) + 3; ++v) {
		view.e = static_cast<clamp_mode_t>(v);
		CHECK(static_cast<size_t>(view.e) < clamp_mode_count);
	}
}

TEST_CASE("[jjreg] reset preserves reserved capacity bytes") {
	uint8_t data[jjreg_padding_schema_t::capacity];
	std::memset(data, 0xCC, sizeof(data));
	auto view = jjreg_padding_schema(data);
	view.reset();

	CHECK(view.a == 2);
	CHECK(view.b == 7);

	for(size_t i = view.meta.size; i < view.meta.capacity; ++i) {
		CHECK(data[i] == 0xCC);
	}

	view.a = 9;
	view.b = 0;
	for(size_t i = view.meta.size; i < view.meta.capacity; ++i) {
		CHECK(data[i] == 0xCC);
	}
}

TEST_CASE("[jjreg] nested view alias coherence") {
	auto root = jjreg_superschema();
	root.reset();
	root.point.scores.push_back(6);

	auto direct = jjreg_scores(root.point.ptr);
	CHECK(direct.scores.size() == 1);
	direct.scores.push_back(9);

	CHECK(root.point.scores.size() == 2);
	CHECK(root.point.scores[0] == 6);
	CHECK(root.point.scores[1] == 9);
}

TEST_CASE("[jjreg] nested capacity boundary") {
	uint8_t data[jjreg_two_nested_t::capacity];
	std::memset(data, 0xAB, sizeof(data));
	auto view = jjreg_two_nested(data);
	view.reset();

	CHECK(view.left.v == 4);
	CHECK(view.right.v == 4);
	CHECK(view.tail == 2);
	CHECK(view.left.ptr == view.ptr);
	CHECK(view.right.ptr == view.ptr + jjreg_small_field_t::capacity);
	CHECK(view.tail.ptr == view.ptr + jjreg_small_field_t::capacity * 2);

	view.left.v = 9;
	view.right.v = 1;
	view.tail = 5;
	for(size_t i = view.meta.size; i < view.meta.capacity; ++i) {
		CHECK(data[i] == 0xAB);
	}
}

TEST_CASE("[jjreg] custom serializer honored") {
	uint8_t data[4] = {0};
	auto view = jjreg_be_word_schema(data);
	view.reset();
	CHECK(view.word == 0x1234);

	view.word = 0x00FF;
	CHECK(data[0] == 0x00);
	CHECK(data[1] == 0xFF);
	CHECK(view.word == 0x00FF);
}

TEST_CASE("[jjreg] alignment and struct array behavior") {
	uint8_t data[16] = {0};
	auto align_view = jjreg_align_schema(data);
	align_view.reset();

	CHECK(align_view.meta.field_size(jjreg_align_schema_t::_index_lead) == 1);
	CHECK(align_view.meta.field_size(jjreg_align_schema_t::_index_word) == 2);
	CHECK(align_view.meta.field_size(jjreg_align_schema_t::_index_trail) == 1);
	CHECK(align_view.meta.size == 4);
	CHECK(align_view.word.ptr == align_view.ptr + 1);
	CHECK(align_view.trail.ptr == align_view.ptr + 3);

	align_view.word = 0x0A0B;
	CHECK(data[1] == 0x0A);
	CHECK(data[2] == 0x0B);

	auto arr_view = jjreg_point_array(data);
	arr_view.reset();
	CHECK(arr_view.pts[0].get().x == 7);
	CHECK(arr_view.pts[0].get().y == -3);
	CHECK(arr_view.pts[1].get().x == 7);
	CHECK(arr_view.pts[1].get().y == -3);

	point_t tmp{9, -9};
	arr_view.pts[0] = tmp;
	CHECK(arr_view.pts[0].get().x == 9);
	CHECK(arr_view.pts[1].get().x == 7);

	point_t one[] = { {5, -1} };
	arr_view.pts.set(one, 1);
	CHECK(arr_view.pts[0].get().x == 5);
	CHECK(arr_view.pts[1].get().x == 7);

}

TEST_CASE("[jjreg] reset cascades to nested and arrays") {
	auto root = jjreg_supersuperschema();

	root.header = "custom";
	root.data1.point.scores.push_back(9);
	const char* strings[] = { "hi" };
	root.data2.set(strings, 1);
	root.footer = 7;

	root.reset();
	CHECK(std::string(root.header) == "jjkitv1");
	CHECK(root.data1.point.scores.size() == 0);
	CHECK(std::string(root.data2[0]) == "ab");
	CHECK(std::string(root.data2[1]) == "ab");
	CHECK(root.footer == 42);
}

TEST_CASE("[jjreg] partial set on mixed string array") {
	uint8_t data[32] = {0};
	auto view = jjreg_mixed_str_array(data);
	view.reset();

	CHECK(std::string(view.labels[0]) == "def");
	CHECK(std::string(view.labels[1]) == "def");
	CHECK(std::string(view.labels[2]) == "def");

	const char* partial[] = { "hi" };
	view.labels.set(partial, 1);
	CHECK(std::string(view.labels[0]) == "hi");
	CHECK(std::string(view.labels[1]) == "def");
	CHECK(std::string(view.labels[2]) == "def");

	const char* longer[] = { "alpha", "beta", "gamma" };
	view.labels.set(longer, 3);
	CHECK(std::string(view.labels[0]) == "alph");
	CHECK(std::string(view.labels[1]) == "beta");
	CHECK(std::string(view.labels[2]) == "gamm");
}

TEST_SUITE_END();
