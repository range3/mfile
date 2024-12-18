#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include <byte_span/byte_span.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mfile/mfile.hpp"

using range3::as_sv;
using range3::byte_span;

TEST_CASE("File basic read/write operations", "[file]") {
  auto file = mfile::make_tmpfile("/tmp/mfile_test_");

  SECTION("read_exact and write_exact") {
    constexpr std::string_view test_data = "Test Data";
    file.write_exact(test_data);
    file.seek(0, SEEK_SET);
    std::array<std::byte, 9> buffer{};  // Exact size of "Test Data"
    file.read_exact(buffer);
    REQUIRE(as_sv(byte_span{buffer}) == test_data);
  }

  SECTION("read with size") {
    constexpr std::string_view test_data = "Read with size test";
    file.write_exact(test_data);
    file.seek(0, SEEK_SET);
    auto data = file.read(test_data.size());
    REQUIRE(data.size() == test_data.size());
    REQUIRE(as_sv(byte_span{data}) == test_data);
  }

  SECTION("read entire file") {
    constexpr std::string_view test_data = "Complete file content";
    file.write_exact(test_data);
    file.seek(0, SEEK_SET);
    auto data = file.read();
    REQUIRE(data.size() == test_data.size());
    REQUIRE(as_sv(byte_span{data}) == test_data);
  }
}

TEST_CASE("File error conditions", "[file]") {
  auto file = mfile::make_tmpfile("/tmp/mfile_test2_");

  SECTION("read_exact throws on EOF") {
    std::array<std::byte, 10> buffer{};
    REQUIRE_THROWS_AS(file.read_exact(buffer), mfile::end_of_file_error);
  }

  SECTION("read from empty file") {
    std::array<std::byte, 10> buffer{};
    auto read = file.read_once(buffer);
    REQUIRE(read == 0);
  }

  SECTION("read after EOF") {
    constexpr std::string_view small_data = "small";
    file.write_exact(small_data);
    file.seek(0, SEEK_SET);
    std::array<std::byte, 10> buffer{};
    auto read = file.read(buffer);
    REQUIRE(read == small_data.size());
    read = file.read_once(buffer);
    REQUIRE(read == 0);
  }
}
