#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <byte_span/byte_span.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mfile/mfile.hpp"

using namespace std::string_view_literals;

// NOLINTNEXTLINE
TEST_CASE("File positional operations", "[file]") {
  auto file = mfile::make_tmpfile("/tmp/mfile_test_");

  SECTION("pwrite_once and pread_once") {
    constexpr std::string_view test_data = "Hello, World!";
    constexpr std::uint64_t offset = 100;

    auto write_size = file.pwrite_once(test_data, offset);
    REQUIRE(write_size <= test_data.size());

    auto buffer = std::array<std::byte, 64>{};
    auto read_size = file.pread_once(buffer, offset);
    REQUIRE(read_size <= test_data.size());
    REQUIRE(read_size == write_size);
    REQUIRE(std::memcmp(buffer.data(), test_data.data(), read_size) == 0);
  }

  SECTION("pread_once from empty region returns 0") {
    auto buffer = std::array<std::byte, 64>{};
    auto read_size = file.pread_once(buffer, 0);
    REQUIRE(read_size == 0);
  }

  SECTION("pread_once beyond EOF returns 0") {
    auto buffer = std::array<std::byte, 64>{};
    auto read_size = file.pread_once(buffer, 999999);
    REQUIRE(read_size == 0);
  }

  SECTION("invalid offset throws") {
    std::array<std::byte, 64> buffer{};
    REQUIRE_THROWS_AS(
        file.pread_once(buffer, std::numeric_limits<std::uint64_t>::max()),
        mfile::mfile_system_error);
  }
  SECTION("write and read at different positions") {
    auto const data1 = "Hello"sv;
    auto const data2 = "World"sv;

    REQUIRE(file.pwrite(data1, 0) == data1.size());
    REQUIRE(file.pwrite(data2, 100) == data2.size());

    auto buf1 = std::array<std::byte, 32>{};
    auto buf2 = std::array<std::byte, 32>{};

    REQUIRE(file.pread(buf1, 0) == buf1.size());
    REQUIRE(file.pread(buf2, 100) == data2.size());

    REQUIRE(std::memcmp(buf1.data(), data1.data(), data1.size()) == 0);
    auto buf1_hole = range3::byte_view{buf1}.subspan(data1.size());
    REQUIRE(std::all_of(buf1_hole.begin(), buf1_hole.end(),
                        [](std::byte b) { return b == std::byte{0}; }));
    REQUIRE(std::memcmp(buf2.data(), data2.data(), data2.size()) == 0);
  }

  SECTION("partial read due to EOF") {
    auto const data = "Test Data"sv;
    REQUIRE(file.pwrite(data, 50) == data.size());

    auto buffer = std::array<std::byte, 1024>{};
    auto read_size = file.pread(buffer, 50);
    REQUIRE(read_size == data.size());  // EOF reached
  }

  SECTION("read from empty regions") {
    auto buffer = std::array<std::byte, 64>{};
    REQUIRE(file.pread(buffer, 0) == 0);     // 先頭
    REQUIRE(file.pread(buffer, 1000) == 0);  // 途中
  }

  SECTION("large data operation") {
    auto write_data = std::vector<std::byte>(64U << 10U);  // 64KB
    std::generate(write_data.begin(), write_data.end(),
                  [n = 0]() mutable { return std::byte(n++ % 256); });

    auto written = file.pwrite(write_data, 1024);  // 1KB offset
    REQUIRE(written == write_data.size());

    std::vector<std::byte> read_data(write_data.size());
    auto read = file.pread(read_data, 1024);
    REQUIRE(read == written);
    REQUIRE(
        std::equal(write_data.begin(), write_data.end(), read_data.begin()));
  }

  SECTION("sparse write and read") {
    auto const data = "Sparse Test"sv;
    auto offset = std::uint64_t{1U << 20U};  // 1MB offset

    REQUIRE(file.pwrite(data, offset) == data.size());

    auto buffer = std::array<std::byte, 32>{};
    REQUIRE(file.pread(buffer, offset) == data.size());
    REQUIRE(std::memcmp(buffer.data(), data.data(), data.size()) == 0);

    auto hole_buf = std::array<std::byte, 64>{};
    auto hole_read = file.pread(hole_buf, 1024);  // sparse region
    REQUIRE(hole_read == 64);
    REQUIRE(std::all_of(hole_buf.begin(), hole_buf.end(),
                        [](std::byte b) { return b == std::byte{0}; }));
  }

  SECTION("multiple partial operations") {
    auto const data = std::string(1024, 'A');  // 1KB data
    auto const offset = std::uint64_t{512};    // 512B offset

    auto write1 = file.pwrite(std::string_view(data).substr(0, 400), offset);
    auto write2 = file.pwrite(std::string_view(data).substr(400), offset + 400);
    REQUIRE(write1 == 400);
    REQUIRE(write2 == 624);

    // Read back the data at once
    auto buffer = std::vector<std::byte>(1024);
    auto read = file.pread(buffer, offset);
    REQUIRE(read == 1024);
    REQUIRE(std::memcmp(buffer.data(), data.data(), data.size()) == 0);
  }
}

TEST_CASE("High-level positional I/O operations", "[file]") {
  auto file = mfile::make_tmpfile("/tmp/mfile_test_");

  SECTION("pread_exact and pwrite_exact basic operation") {
    constexpr std::string_view test_data = "Hello, World!";
    constexpr std::uint64_t offset = 100;

    // Write test data
    file.pwrite_exact(test_data, offset);

    // Read back the exact amount
    auto buffer = std::array<std::byte, 13>{};  // test_data.size() == 13
    file.pread_exact(buffer, offset);

    REQUIRE(std::memcmp(buffer.data(), test_data.data(), test_data.size())
            == 0);
  }

  SECTION("pread_exact throws on EOF") {
    auto buffer = std::array<std::byte, 64>{};
    REQUIRE_THROWS_AS(file.pread_exact(buffer, 0), mfile::end_of_file_error);
  }

  SECTION("pread helper returns exact data") {
    constexpr std::string_view test_data = "Test Data";
    file.pwrite_exact(test_data, 50);

    auto read_data = file.pread(test_data.size(), 50);
    REQUIRE(read_data.size() == test_data.size());
    REQUIRE(std::memcmp(read_data.data(), test_data.data(), test_data.size())
            == 0);
  }
}

// NOLINTNEXTLINE
TEST_CASE("Positional read to EOF", "[file]") {
  auto file = mfile::make_tmpfile("/tmp/mfile_test_");

  SECTION("Read from empty file") {
    auto data = file.pread(0);
    REQUIRE(data.empty());
  }

  SECTION("Read entire file from offset") {
    file.pwrite_exact("First"sv, 0);
    file.pwrite_exact("Second"sv, 100);
    file.pwrite_exact("Third"sv, 200);

    auto data = file.pread(100);

    REQUIRE(data.size() == 105);

    REQUIRE(std::memcmp(data.data(), "Second", 6) == 0);

    auto middle = range3::byte_view{data}.subspan(6, 94);
    REQUIRE(std::all_of(middle.begin(), middle.end(),
                        [](std::byte b) { return b == std::byte{0}; }));

    REQUIRE(std::memcmp(range3::byte_view{data}.subspan(100).data(), "Third", 5)
            == 0);
  }

  SECTION("Read from offset beyond EOF") {
    file.write_exact("Test Data"sv);
    auto data = file.pread(100);
    REQUIRE(data.empty());
  }

  SECTION("Read large sparse file") {
    constexpr auto offset = 1ULL << 20U;  // 1MB
    constexpr auto write_data = "Large Sparse"sv;
    file.pwrite_exact(write_data, offset);

    auto half_mb = 1ULL << 19U;  // 512KB
    auto data = file.pread(half_mb);

    REQUIRE(data.size() == offset + write_data.size() - half_mb);

    auto first_half = range3::byte_view{data}.first(half_mb);
    REQUIRE(std::all_of(first_half.begin(), first_half.end(),
                        [](std::byte b) { return b == std::byte{0}; }));

    auto content = range3::byte_view{data}.subspan(half_mb);
    REQUIRE(std::memcmp(content.data(), write_data.data(), write_data.size())
            == 0);
  }
}
