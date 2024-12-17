#include <cerrno>
#include <cstddef>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

#include "mfile/mfile.hpp"

TEST_CASE("mfile error category", "[error]") {
  SECTION("error category basics") {
    const auto& cat = mfile::error_category::instance();
    REQUIRE(std::string_view{cat.name()} == "mfile");
    REQUIRE(&cat == &mfile::error_category::instance());
  }

  SECTION("error messages") {
    const auto& cat = mfile::error_category::instance();
    REQUIRE(cat.message(mfile::errc::success) == "Success");
    REQUIRE(cat.message(mfile::errc::end_of_file) == "End of file reached");
    REQUIRE(cat.message(mfile::errc::insufficient_space)
            == "Insufficient space");
    REQUIRE(cat.message(999) == "Unknown mfile error");
  }
}

TEST_CASE("mfile error code and condition", "[error]") {
  SECTION("make_error_code") {
    auto ec = make_error_code(mfile::errc::end_of_file);
    REQUIRE(ec.category() == mfile::error_category::instance());
    REQUIRE(ec.value() == static_cast<int>(mfile::errc::end_of_file));

    const std::error_code std_ec = mfile::errc::insufficient_space;
    REQUIRE(std_ec.category() == mfile::error_category::instance());
    REQUIRE(std_ec.value()
            == static_cast<int>(mfile::errc::insufficient_space));
  }

  SECTION("make_error_condition") {
    auto cond = make_error_condition(mfile::errc::insufficient_space);
    REQUIRE(cond.category() == mfile::error_category::instance());
    REQUIRE(cond.value() == static_cast<int>(mfile::errc::insufficient_space));
  }

  SECTION("error condition mapping") {
    auto ec_eof = make_error_code(mfile::errc::end_of_file);
    REQUIRE(ec_eof.default_error_condition()
            == std::make_error_condition(std::errc::no_message));

    auto ec_space = make_error_code(mfile::errc::insufficient_space);
    REQUIRE(ec_space.default_error_condition()
            == std::make_error_condition(std::errc::no_space_on_device));
  }

  SECTION("comparison operators") {
    REQUIRE(mfile::errc::end_of_file
            == make_error_code(mfile::errc::end_of_file));
    REQUIRE(mfile::errc::insufficient_space
            == make_error_condition(mfile::errc::insufficient_space));
    REQUIRE(mfile::errc::end_of_file
            == std::make_error_condition(std::errc::no_message));
    REQUIRE(mfile::errc::insufficient_space
            == std::make_error_condition(std::errc::no_space_on_device));
  }
}

// NOLINTNEXTLINE
TEST_CASE("mfile exceptions", "[error]") {
  SECTION("end_of_file_error") {
    std::size_t const bytes_read = 42;
    try {
      throw mfile::end_of_file_error(bytes_read, "EOF test message");
    } catch (const mfile::end_of_file_error& e) {
      REQUIRE(e.bytes_read() == bytes_read);
      REQUIRE(e.code() == make_error_code(mfile::errc::end_of_file));
      REQUIRE(std::string_view{e.what()}.find("EOF test message")
              != std::string_view::npos);
    }
  }

  SECTION("insufficient_space_error") {
    std::size_t const bytes_written = 128;
    try {
      throw mfile::insufficient_space_error(bytes_written,
                                            "No space test message");
    } catch (const mfile::insufficient_space_error& e) {
      REQUIRE(e.bytes_written() == bytes_written);
      REQUIRE(e.code() == make_error_code(mfile::errc::insufficient_space));
      REQUIRE(std::string_view{e.what()}.find("No space test message")
              != std::string_view::npos);
    }
  }

  SECTION("mfile_system_error") {
    try {
      throw mfile::mfile_system_error(ENOSPC, "System error test");
    } catch (const mfile::mfile_system_error& e) {
      REQUIRE(e.code() == std::error_code(ENOSPC, std::system_category()));
      REQUIRE(std::string_view{e.what()}.find("System error test")
              != std::string_view::npos);
    }
  }

  SECTION("error hierarchy") {
    auto throw_and_catch = []() -> bool {
      try {
        throw mfile::end_of_file_error(0, "test");
        return false;
      } catch (const mfile::mfile_error&) {
        return true;
      }
    };
    REQUIRE(throw_and_catch());
  }
}
