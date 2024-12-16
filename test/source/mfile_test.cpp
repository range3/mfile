#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>

#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include <sys/stat.h>

#include "mfile/mfile.hpp"

namespace {
template <typename... Args>
constexpr auto cast_bitwise_or(Args... args) -> int {
  return static_cast<int>((static_cast<unsigned int>(args) | ...));
}
}  // namespace

TEST_CASE("open_flags basic functionality", "[open_flags]") {
  using mfile::open_flags;

  SECTION("open_flags basic functionality") {
    REQUIRE(open_flags::a().flags()
            == cast_bitwise_or(O_CLOEXEC, O_WRONLY, O_CREAT, O_APPEND));
  }

  SECTION("open_flags direct") {
    REQUIRE(open_flags::a().direct().flags()
            == cast_bitwise_or(O_CLOEXEC, open_flags::a().flags(), O_DIRECT));
  }

  SECTION("open_flags has_flag") {
    auto flags = open_flags::a();
    REQUIRE(flags.has_flag(O_APPEND));
    REQUIRE_FALSE(flags.has_flag(O_DIRECT));
  }
}

// NOLINTNEXTLINE
TEST_CASE("File basic operations", "[file]") {
  auto file =
      mfile::open(".", mfile::open_flags::rp().tmpfile(), S_IRUSR | S_IWUSR);

  SECTION("write and read simple data") {
    constexpr std::string_view test_data = "Hello, World!";

    auto write_size = file.write_once(test_data);

    REQUIRE(write_size <= test_data.size());

    auto file_size = file.size();
    REQUIRE(file_size <= test_data.size());
    REQUIRE(file_size == write_size);

    file.seek(0, SEEK_SET);

    std::array<std::byte, 64> buffer{};
    auto read_size = file.read_once(buffer);

    REQUIRE(read_size <= test_data.size());
    REQUIRE(read_size <= write_size);
    REQUIRE(std::memcmp(buffer.data(), test_data.data(), read_size) == 0);
  }

  SECTION("read from empty file returns 0") {
    std::array<std::byte, 64> buffer{};
    auto read_size = file.read_once(buffer);
    REQUIRE(read_size == 0);
  }
}

TEST_CASE("File error cases", "[file]") {
  SECTION("opening non-existent file throws") {
    REQUIRE_THROWS_AS(mfile::open("/non/existent/file", mfile::open_flags::r()),
                      mfile::file_error);
  }
}
