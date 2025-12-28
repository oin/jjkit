#include "../ext/doctest.h"
#include "jjring.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>

TEST_SUITE_BEGIN("jjring");

TEST_CASE("[jjring][base] construction and capacity") {
    jjring<int, 8> ring;
    
    // Capacity should be N-1 for ring buffer (one slot reserved)
    CHECK(ring.capacity() == 7);
}

TEST_CASE("[jjring][base] initial state is empty") {
    jjring<int, 4> ring;
    
    CHECK(ring.empty() == true);
    CHECK(ring.full() == false);
    CHECK(ring.size_approx() == 0);
}

TEST_CASE("[jjring][single] single element push and pop") {
    jjring<int, 4> ring;
    
    // Test single push
    bool push_result = ring.push(42);
    CHECK(push_result == true);
    CHECK(ring.empty() == false);
    CHECK(ring.size_approx() == 1);
    
    // Test single pop
    int value;
    bool pop_result = ring.pop(value);
    CHECK(pop_result == true);
    CHECK(value == 42);
    CHECK(ring.empty() == true);
    CHECK(ring.size_approx() == 0);
}

TEST_CASE("[jjring][single] push to full buffer returns false") {
    jjring<int, 4> ring; // capacity = 3
    
    // Fill the buffer
    CHECK(ring.push(1) == true);
    CHECK(ring.push(2) == true);
    CHECK(ring.push(3) == true);
    CHECK(ring.full() == true);
    
    // Try to push one more (should fail)
    CHECK(ring.push(4) == false);
    CHECK(ring.size_approx() == 3);
}

TEST_CASE("[jjring][single] pop from empty buffer returns false") {
    jjring<int, 4> ring;
    
    int value;
    CHECK(ring.pop(value) == false);
    CHECK(ring.empty() == true);
}

TEST_CASE("[jjring][bulk] bulk push and pop operations") {
    jjring<int, 8> ring; // capacity = 7
    
    // Prepare test data
    std::vector<int> input_data = {1, 2, 3, 4, 5};
    std::vector<int> output_data(5);
    
    // Test bulk push
    size_t pushed = ring.push(input_data.data(), input_data.size());
    CHECK(pushed == 5);
    CHECK(ring.size_approx() == 5);
    
    // Test bulk pop
    size_t popped = ring.pop(output_data.data(), output_data.size());
    CHECK(popped == 5);
    CHECK(ring.empty() == true);
    
    // Verify data integrity
    for (size_t i = 0; i < 5; ++i) {
        CHECK(output_data[i] == input_data[i]);
    }
}

TEST_CASE("[jjring][bulk] bulk operations with partial success") {
    jjring<int, 4> ring; // capacity = 3
    
    // Try to push more than capacity
    std::vector<int> input_data = {1, 2, 3, 4, 5};
    size_t pushed = ring.push(input_data.data(), input_data.size());
    CHECK(pushed == 3); // Only 3 should fit
    CHECK(ring.full() == true);
    
    // Try to pop more than available
    std::vector<int> output_data(5);
    size_t popped = ring.pop(output_data.data(), output_data.size());
    CHECK(popped == 3); // Only 3 available
    CHECK(ring.empty() == true);
    
    // Verify the 3 elements that were transferred
    for (size_t i = 0; i < 3; ++i) {
        CHECK(output_data[i] == input_data[i]);
    }
}

TEST_CASE("[jjring][ovw] push_overwrite when buffer has space") {
    jjring<int, 4> ring; // capacity = 3
    
    ring.push_overwrite(42);
    CHECK(ring.size_approx() == 1);
    CHECK(ring.empty() == false);
    
    int value;
    CHECK(ring.pop(value) == true);
    CHECK(value == 42);
}

TEST_CASE("[jjring][ovw] push_overwrite when buffer is full") {
    jjring<int, 4> ring; // capacity = 3
    
    // Fill the buffer
    ring.push(1);
    ring.push(2);
    ring.push(3);
    CHECK(ring.full() == true);
    
    // Overwrite (should drop oldest element)
    ring.push_overwrite(4);
    CHECK(ring.full() == true);
    CHECK(ring.size_approx() == 3); // Still at capacity
    
    // Pop elements - should get 2, 3, 4 (1 was overwritten)
    int value;
    CHECK(ring.pop(value) == true);
    CHECK(value == 2); // 1 was overwritten
    CHECK(ring.pop(value) == true);
    CHECK(value == 3);
    CHECK(ring.pop(value) == true);
    CHECK(value == 4);
    CHECK(ring.empty() == true);
}

TEST_CASE("[jjring][clear] clear operation") {
    jjring<int, 4> ring;
    
    // Add some elements
    ring.push(1);
    ring.push(2);
    CHECK(ring.size_approx() == 2);
    CHECK(ring.empty() == false);
    
    // Clear the buffer
    ring.clear();
    CHECK(ring.empty() == true);
    CHECK(ring.full() == false);
    CHECK(ring.size_approx() == 0);
}

TEST_CASE("[jjring][base][size] state methods accuracy") {
    jjring<int, 4> ring; // capacity = 3
    
    // Test progressive filling
    CHECK(ring.size_approx() == 0);
    CHECK(ring.empty() == true);
    CHECK(ring.full() == false);
    
    ring.push(1);
    CHECK(ring.size_approx() == 1);
    CHECK(ring.empty() == false);
    CHECK(ring.full() == false);
    
    ring.push(2);
    CHECK(ring.size_approx() == 2);
    CHECK(ring.empty() == false);
    CHECK(ring.full() == false);
    
    ring.push(3);
    CHECK(ring.size_approx() == 3);
    CHECK(ring.empty() == false);
    CHECK(ring.full() == true);
}

TEST_CASE("[jjring][zero_copy] write_acquire and write_commit") {
    jjring<int, 8> ring; // capacity = 7
    
    int* write_ptr;
    size_t available = ring.write_acquire(&write_ptr);
    CHECK(available > 0);
    CHECK(write_ptr != nullptr);
    
    CHECK(reinterpret_cast<uintptr_t>(write_ptr) % alignof(int) == 0);
    size_t k = std::min(available, size_t(3));
    for (size_t i = 0; i < k; ++i) {
        write_ptr[i] = static_cast<int>(i + 10);
    }
    ring.write_commit(k);
    CHECK(ring.size_approx() == k);

    int value;
    for (int expected = 10; expected < 10 + static_cast<int>(k); ++expected) {
        CHECK(ring.pop(value) == true);
        CHECK(value == expected);
    }
}

TEST_CASE("[jjring][zero_copy] read_acquire and read_commit") {
    jjring<int, 8> ring; // capacity = 7
    
    // First push some data
    for (int i = 20; i < 25; ++i) {
        ring.push(i);
    }
    
    const int* read_ptr;
    size_t available = ring.read_acquire(&read_ptr);
    CHECK(available > 0);
    CHECK(read_ptr != nullptr);
    
    CHECK(reinterpret_cast<uintptr_t>(read_ptr) % alignof(int) == 0);
    size_t k = std::min(available, size_t(3));
    for (size_t i = 0; i < k; ++i) {
        CHECK(read_ptr[i] == static_cast<int>(i + 20));
    }
    ring.read_commit(k);
    CHECK(ring.size_approx() == 5 - k);
}

TEST_CASE("[jjring][zero_copy] zero-copy operations when buffer is full") {
    jjring<int, 4> ring; // capacity = 3
    
    // Fill the buffer
    ring.push(1);
    ring.push(2);
    ring.push(3);
    CHECK(ring.full() == true);
    
    // Try write_acquire on full buffer
    int* write_ptr;
    size_t available = ring.write_acquire(&write_ptr);
    CHECK(available == 0);
    CHECK(write_ptr == nullptr);
}

TEST_CASE("[jjring][zero_copy] zero-copy operations when buffer is empty") {
    jjring<int, 4> ring;
    CHECK(ring.empty() == true);
    
    // Try read_acquire on empty buffer
    const int* read_ptr;
    size_t available = ring.read_acquire(&read_ptr);
    CHECK(available == 0);
    CHECK(read_ptr == nullptr);
}

TEST_CASE("[jjring][wrap] wraparound behavior") {
    jjring<int, 4> ring; // capacity = 3
    
    // Fill, empty, and fill again to test wraparound
    ring.push(1);
    ring.push(2);
    ring.push(3);
    
    int value;
    ring.pop(value); // Remove 1
    ring.pop(value); // Remove 2
    
    // Now add more elements (should wrap around)
    ring.push(4);
    ring.push(5);
    
    // Buffer should contain: 3, 4, 5
    CHECK(ring.size_approx() == 3);
    CHECK(ring.full() == true);
    
    // Verify order
    CHECK(ring.pop(value) == true);
    CHECK(value == 3);
    CHECK(ring.pop(value) == true);
    CHECK(value == 4);
    CHECK(ring.pop(value) == true);
    CHECK(value == 5);
}

TEST_CASE("[jjring][single] alternating push and pop operations") {
    jjring<int, 4> ring; // capacity = 3
    
    int value;
    
    // Alternating operations
    for (int i = 0; i < 10; ++i) {
        CHECK(ring.push(i) == true);
        CHECK(ring.pop(value) == true);
        CHECK(value == i);
        CHECK(ring.empty() == true);
    }
}

TEST_CASE("[jjring][types] with different data types") {
    SUBCASE("with double") {
        jjring<double, 4> ring;
        
        CHECK(ring.push(3.14) == true);
        CHECK(ring.push(2.71) == true);
        
        double value;
        CHECK(ring.pop(value) == true);
        CHECK(value == doctest::Approx(3.14));
        CHECK(ring.pop(value) == true);
        CHECK(value == doctest::Approx(2.71));
    }
    
    SUBCASE("with struct") {
		struct TestStruct {
			int x;
			char y;
			float z;
			
			bool operator==(const TestStruct& other) const {
				return x == other.x && y == other.y && z == other.z;
			}
		};

        jjring<TestStruct, 4> ring;

        TestStruct input = {42, 100, 0.5f};
        CHECK(ring.push(input) == true);
        
        TestStruct output;
        CHECK(ring.pop(output) == true);
        CHECK(output == input);
    }
    
    SUBCASE("with array") {
        jjring<std::array<int, 3>, 4> ring;
        
        std::array<int, 3> input = {{1, 2, 3}};
        CHECK(ring.push(input) == true);
        
        std::array<int, 3> output;
        CHECK(ring.pop(output) == true);
        CHECK(output == input);
    }
}

TEST_CASE("[jjring][limits][N2] capacity_one edge behaviors") {
    jjring<int, 2> ring; // capacity = 1
    CHECK(ring.capacity() == 1);
    CHECK(ring.push(1) == true);
    CHECK(ring.full() == true);
    CHECK(ring.push(2) == false);
    int v;
    CHECK(ring.pop(v) == true);
    CHECK(v == 1);
    CHECK(ring.empty() == true);
    CHECK(ring.push(3) == true);
    CHECK(ring.full() == true);
    ring.push_overwrite(4);
    CHECK(ring.full() == true);
    CHECK(ring.size_approx() == 1);
    CHECK(ring.pop(v) == true);
    CHECK(v == 4);
}

TEST_CASE("[jjring][bulk][wrap] push bulk splits across wrap") {
    jjring<int, 8> ring; // capacity = 7
    std::vector<int> a = {1,2,3,4,5,6};
    CHECK(ring.push(a.data(), a.size()) == 6); // h=6
    int tmp;
    CHECK(ring.pop(tmp) == true);
    CHECK(ring.pop(tmp) == true);
    CHECK(ring.pop(tmp) == true); // t=3, head near end
    std::vector<int> b = {7,8,9};
    CHECK(ring.push(b.data(), b.size()) == 3); // wraps
    std::vector<int> out(6);
    CHECK(ring.pop(out.data(), out.size()) == 6);
    std::vector<int> exp = {4,5,6,7,8,9};
    for (size_t i = 0; i < exp.size(); ++i) CHECK(out[i] == exp[i]);
}

TEST_CASE("[jjring][bulk][wrap] pop bulk splits across wrap") {
    jjring<int, 8> ring; // capacity = 7
    std::vector<int> a = {1,2,3,4,5,6};
    CHECK(ring.push(a.data(), a.size()) == 6); // h=6
    std::vector<int> scratch(5);
    CHECK(ring.pop(scratch.data(), 5) == 5);   // t=5, left {6}
    std::vector<int> b = {7,8,9,10};
    CHECK(ring.push(b.data(), b.size()) == 4); // h=2
    std::vector<int> out(5);
    CHECK(ring.pop(out.data(), out.size()) == 5); // wraps 3+2
    std::vector<int> exp = {6,7,8,9,10};
    for (size_t i = 0; i < exp.size(); ++i) CHECK(out[i] == exp[i]);
}

TEST_CASE("[jjring][zero_copy][wrap] write_acquire wraps at end then restarts") {
    jjring<int, 8> ring;
    int init[7]; for (int i=0;i<7;++i) init[i]=i; // 0..6
    CHECK(ring.push(init, 7) == 7);
    int drop; CHECK(ring.pop(drop) == true); // remove 0 => t=1, h=7
    int* wp; size_t n1 = ring.write_acquire(&wp);
    CHECK(n1 == 1);
    CHECK(reinterpret_cast<uintptr_t>(wp) % alignof(int) == 0);
    *wp = 999;
    ring.write_commit(1);
    std::vector<int> out(7);
    CHECK(ring.pop(out.data(), out.size()) == 7);
    std::vector<int> exp = {1,2,3,4,5,6,999};
    for (size_t i = 0; i < exp.size(); ++i) CHECK(out[i] == exp[i]);
}

TEST_CASE("[jjring][zero_copy][wrap] read_acquire wraps at end then restarts") {
    jjring<int, 8> ring;
    for (int i=0;i<7;++i) CHECK(ring.push(i) == true); // 0..6
    int tmp;
    for (int i=0;i<6;++i) CHECK(ring.pop(tmp) == true); // leave {6}, t=6,h=7
    int more[3] = {7,8,9};
    CHECK(ring.push(more, 3) == 3); // queue {6,7,8,9}, t=6,h=2
    CHECK(ring.pop(tmp) == true);   // drop 6 => t=7
    const int* rp; size_t n1 = ring.read_acquire(&rp);
    CHECK(n1 == 1);
    CHECK(reinterpret_cast<uintptr_t>(rp) % alignof(int) == 0);
    CHECK(*rp == 7);
    ring.read_commit(1);
    const int* rp2; size_t n2 = ring.read_acquire(&rp2);
    CHECK(n2 == 2);
    CHECK(rp2[0] == 8);
    CHECK(rp2[1] == 9);
    ring.read_commit(2);
    CHECK(ring.empty() == true);
}

TEST_CASE("[jjring][zero_copy] commit zero has no effect") {
    jjring<int, 8> ring;
    int* wp; size_t wn = ring.write_acquire(&wp);
    size_t s0 = ring.size_approx();
    ring.write_commit(0);
    CHECK(ring.size_approx() == s0);
    CHECK(wn > 0);
    wp[0] = 123;
    ring.write_commit(1);
    const int* rp; size_t rn = ring.read_acquire(&rp);
    CHECK(rn >= 1);
    ring.read_commit(0);
    CHECK(ring.size_approx() == s0 + 1);
    int v; CHECK(ring.pop(v) == true);
    CHECK(v == 123);
}

TEST_CASE("[jjring][types][align] strong alignment 32B") {
    struct alignas(32) S { unsigned char b[32]; };
    jjring<S, 4> ring;
    S s{}; CHECK(ring.push(s) == true);
    const S* rp; size_t n = ring.read_acquire(&rp);
    CHECK(n >= 1);
    CHECK(reinterpret_cast<uintptr_t>(rp) % alignof(S) == 0);
    ring.read_commit(1);
}

TEST_CASE("[jjring][bulk] zero sizes are no-ops") {
    jjring<int, 8> ring;
    int buf[3] = {1,2,3};
    CHECK(ring.push(buf, 0) == 0);
    CHECK(ring.size_approx() == 0);
    CHECK(ring.pop(buf, 0) == 0);
    CHECK(ring.size_approx() == 0);
}

TEST_CASE("[jjring][base][size] size_approx bounds and flags") {
    jjring<int, 4> ring; // cap=3
    CHECK(ring.size_approx() == 0);
    CHECK(ring.empty() == true);
    CHECK(ring.full() == false);
    CHECK(ring.push(1) == true);
    CHECK(ring.size_approx() <= ring.capacity());
    CHECK(ring.empty() == false);
    CHECK(ring.push(2) == true);
    int v; CHECK(ring.pop(v) == true);
    CHECK(ring.size_approx() <= ring.capacity());
    CHECK((ring.empty() == (ring.size_approx()==0)));
}

TEST_SUITE_END();
