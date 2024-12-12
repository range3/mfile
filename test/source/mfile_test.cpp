#include "mfile/mfile.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Name is mfile", "[library]")
{
  REQUIRE(name() == "mfile");
}
