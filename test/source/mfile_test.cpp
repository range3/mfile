#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>

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
