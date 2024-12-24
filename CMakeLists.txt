cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    mfile
    VERSION 0.2.0
    DESCRIPTION "Modern C++20 file I/O for Linux"
    HOMEPAGE_URL "https://github.com/range3/mfile"
    LANGUAGES NONE
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(mfile_mfile INTERFACE)
add_library(mfile::mfile ALIAS mfile_mfile)

set_property(
    TARGET mfile_mfile PROPERTY
    EXPORT_NAME mfile
)

target_include_directories(
    mfile_mfile ${warning_guard}
    INTERFACE
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(mfile_mfile INTERFACE cxx_std_20)

find_package(ByteSpan REQUIRED)
target_link_libraries(mfile_mfile INTERFACE ByteSpan::ByteSpan)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT mfile_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of mfile"
  )
endif()

include(cmake/dev-mode.cmake)
