#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <byte_span/byte_span.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace mfile {
using range3::byte_span;
using range3::byte_view;
using range3::cbyte_view;

// NOLINTNEXTLINE
enum class errc : int {
  success = 0,
  end_of_file = 1,
  insufficient_space = 2,
};

class error_category : public std::error_category {
 public:
  static auto instance() noexcept -> auto const& {
    const static error_category instance;
    return instance;
  }

  [[nodiscard]]
  auto name() const noexcept -> const char* override {
    return "mfile";
  }

  [[nodiscard]]
  auto message(int ev) const -> std::string override {
    switch (static_cast<errc>(ev)) {
      case errc::success:
        return "Success";
      case errc::end_of_file:
        return "End of file reached";
      case errc::insufficient_space:
        return "Insufficient space";
      default:
        return "Unknown mfile error";
    }
  }

  [[nodiscard]]
  auto message(errc ev) const -> std::string {
    return message(static_cast<int>(ev));
  }

  [[nodiscard]]
  auto default_error_condition(int ev) const noexcept
      -> std::error_condition override {
    switch (static_cast<errc>(ev)) {
      case errc::end_of_file:
        return std::make_error_condition(std::errc::no_message);
      case errc::insufficient_space:
        return std::make_error_condition(std::errc::no_space_on_device);
      default:
        return {ev, *this};
    }
  }

 private:
  error_category() = default;
};

inline auto make_error_code(errc e) noexcept -> std::error_code {
  return {static_cast<int>(e), error_category::instance()};
}

inline auto make_error_condition(errc e) noexcept -> std::error_condition {
  return {static_cast<int>(e), error_category::instance()};
}

class mfile_error : public std::system_error {
 public:
  explicit mfile_error(std::error_code ec, std::string_view what_arg)
      : std::system_error{ec, std::string{what_arg}} {}
};

class mfile_system_error : public mfile_error {
 public:
  explicit mfile_system_error(int val, std::string_view what_arg)
      : mfile_error{std::error_code{val, std::system_category()}, what_arg} {}
};

class end_of_file_error : public mfile_error {
 public:
  explicit end_of_file_error(std::size_t bytes_read, std::string_view what_arg)
      : mfile_error{make_error_code(errc::end_of_file), what_arg},
        bytes_read_{bytes_read} {}

  [[nodiscard]]
  auto bytes_read() const noexcept -> std::size_t {
    return bytes_read_;
  }

 private:
  std::size_t bytes_read_;
};

class insufficient_space_error : public mfile_error {
 public:
  explicit insufficient_space_error(std::size_t bytes_written,
                                    std::string_view what_arg)
      : mfile_error{make_error_code(errc::insufficient_space), what_arg},
        bytes_written_{bytes_written} {}

  [[nodiscard]]
  auto bytes_written() const noexcept -> std::size_t {
    return bytes_written_;
  }

 private:
  std::size_t bytes_written_;
};

struct invalid_file_handle {
  static constexpr auto value = -1;
};

class weak_file_handle {
  int fd_{invalid_file_handle::value};

 public:
  using pointer = weak_file_handle;

  constexpr weak_file_handle() noexcept = default;
  // NOLINTNEXTLINE
  constexpr weak_file_handle(std::nullptr_t) noexcept {}
  // NOLINTNEXTLINE
  constexpr weak_file_handle(int fd) noexcept : fd_{fd} {}

  [[nodiscard]]
  constexpr explicit operator bool() const noexcept {
    return fd_ >= 0;
  }

  // smart reference pattern
  [[nodiscard]]
  constexpr auto operator->() noexcept -> pointer* {
    return this;
  }
  [[nodiscard]]
  constexpr auto operator->() const noexcept -> const pointer* {
    return this;
  }

  [[nodiscard]]
  constexpr auto native() const noexcept -> int {
    return fd_;
  }

  [[nodiscard]]
  constexpr auto get() const noexcept -> pointer {
    return *this;
  }

  [[nodiscard]]
  constexpr auto release() noexcept -> int {
    return std::exchange(fd_, invalid_file_handle::value);
  }

  constexpr friend auto operator==(weak_file_handle l,
                                   weak_file_handle r) noexcept -> bool {
    return l.fd_ == r.fd_;
  }
  constexpr friend auto operator!=(weak_file_handle l,
                                   weak_file_handle r) noexcept -> bool {
    return !(l == r);
  }
};

namespace detail {
struct fd_deleter {
  using pointer = weak_file_handle;
  constexpr void operator()(weak_file_handle handle) const noexcept {
    if (handle) {
      ::close(handle.native());
    }
  }
};

class tmpfile_deleter {
 public:
  using pointer = weak_file_handle;

  explicit tmpfile_deleter(std::string&& name) noexcept
      : name_{std::move(name)} {}

  void operator()(weak_file_handle handle) const noexcept {
    if (handle) {
      unlink(name_.c_str());
      ::close(handle.native());
    }
  }

 private:
  std::string name_;
};

}  // namespace detail

using file_handle = std::unique_ptr<weak_file_handle, detail::fd_deleter>;
using tmpfile_handle =
    std::unique_ptr<weak_file_handle, detail::tmpfile_deleter>;

class open_flags {
 public:
  // Python-like open access modes
  enum class basic_mode : std::uint16_t {
    r = O_RDONLY,
    rp = O_RDWR,
    w = O_WRONLY | O_CREAT | O_TRUNC,
    wp = O_RDWR | O_CREAT | O_TRUNC,
    x = O_WRONLY | O_CREAT | O_EXCL,
    xp = O_RDWR | O_CREAT | O_EXCL,
    a = O_WRONLY | O_CREAT | O_APPEND,
    ap = O_RDWR | O_CREAT | O_APPEND,
  };

  static constexpr std::uint32_t default_flags = O_CLOEXEC;

  constexpr explicit open_flags(basic_mode mode) noexcept
      : flags_(static_cast<std::uint32_t>(mode) | default_flags) {}

  static constexpr auto r() { return open_flags{basic_mode::r}; }
  static constexpr auto w() { return open_flags{basic_mode::w}; }
  static constexpr auto x() { return open_flags{basic_mode::x}; }
  static constexpr auto a() { return open_flags{basic_mode::a}; }
  static constexpr auto rp() { return open_flags{basic_mode::rp}; }
  static constexpr auto wp() { return open_flags{basic_mode::wp}; }
  static constexpr auto xp() { return open_flags{basic_mode::xp}; }
  static constexpr auto ap() { return open_flags{basic_mode::ap}; }

  [[nodiscard]]
  constexpr auto direct() noexcept -> open_flags& {
    return set(O_DIRECT);
  }

  [[nodiscard]]
  constexpr auto sync() noexcept -> open_flags& {
    return set(O_SYNC);
  }

  [[nodiscard]]
  constexpr auto noatime() noexcept -> open_flags& {
    return set(O_NOATIME);
  }

  [[nodiscard]]
  constexpr auto tmpfile() noexcept -> open_flags& {
    return set(O_TMPFILE);
  }

  [[nodiscard]]
  constexpr auto set(int flag) noexcept -> open_flags& {
    flags_ |= static_cast<std::uint32_t>(flag);
    return *this;
  }

  [[nodiscard]]
  constexpr auto unset(int flag) noexcept -> open_flags& {
    flags_ &= ~static_cast<std::uint32_t>(flag);
    return *this;
  }

  [[nodiscard]]
  constexpr auto has_flag(std::uint32_t flag) const noexcept -> bool {
    return (flags_ & flag) == flag;
  }

  [[nodiscard]]
  constexpr auto flags() const noexcept -> int {
    return static_cast<int>(flags_);
  }

 private:
  std::uint32_t flags_;
};

template <typename T>
concept weak_file_handle_like = requires(T& h) {
  { h.native() } -> std::same_as<int>;
  { static_cast<bool>(h) } -> std::same_as<bool>;
};

template <typename T>
concept managed_file_handle_like = requires(T& h) {
  { h.get() } -> std::same_as<weak_file_handle>;
  { static_cast<bool>(h) } -> std::same_as<bool>;
};

template <typename T>
concept file_handle_like =
    weak_file_handle_like<T> || managed_file_handle_like<T>;

template <typename T>
concept copyable_handle = std::is_copy_constructible_v<T>;

template <file_handle_like Handle>
class file {
 public:
  using handle_type = Handle;

  constexpr file() noexcept = default;
  constexpr file(const file&) = delete;
  constexpr auto operator=(const file&) -> file& = delete;
  constexpr file(file&&) noexcept = default;
  constexpr auto operator=(file&&) noexcept -> file& = default;
  constexpr ~file() noexcept = default;

  constexpr file(const file&)
    requires copyable_handle<Handle>
  = default;
  constexpr auto operator=(const file&) -> file&
    requires copyable_handle<Handle>
  = default;

  constexpr explicit file(handle_type handle) noexcept
      : handle_{std::move(handle)} {}

  [[nodiscard]]
  auto read(byte_view data) const -> std::size_t {
    std::size_t bytes_read{};

    while (bytes_read < data.size()) {
      auto result =
          read_once(data.subspan(bytes_read, data.size() - bytes_read));

      // EOF
      if (result == 0) {
        break;
      }

      bytes_read += result;
    }

    return bytes_read;
  }

  [[nodiscard]]
  auto write(cbyte_view data) const -> std::size_t {
    std::size_t bytes_written{};

    while (bytes_written < data.size()) {
      auto result =
          write_once(data.subspan(bytes_written, data.size() - bytes_written));

      // No space left on device
      if (result == 0) {
        break;
      }

      bytes_written += result;
    }

    return bytes_written;
  }

  void read_exact(byte_view data) const {
    auto bytes_read = read(data);
    if (bytes_read != data.size()) {
      throw end_of_file_error{bytes_read,
                              "Failed to read exact amount of bytes"};
    }
  }

  void write_exact(cbyte_view data) const {
    auto bytes_written = write(data);
    if (bytes_written != data.size()) {
      throw insufficient_space_error{bytes_written,
                                     "Failed to write exact amount of bytes"};
    }
  }

  [[nodiscard]]
  auto read(std::size_t size) const -> std::vector<std::byte> {
    std::vector<std::byte> buffer(size);
    buffer.resize(read(buffer));
    buffer.shrink_to_fit();
    return buffer;
  }

  [[nodiscard]]
  auto read() const -> std::vector<std::byte> {
    constexpr std::size_t init_buffer_size = 4096;
    constexpr auto resize_factor_limit =
        (std::numeric_limits<std::size_t>::max)() / 3 * 2;
    auto buffer = std::vector<std::byte>();

    auto file_size = size();
    if (!file_size) {
      buffer.resize(init_buffer_size);
    } else {
      auto current_pos = tell();
      if (current_pos == file_size) {
        return buffer;
      }
      auto remaining_size = file_size - current_pos;
      if (remaining_size > (std::numeric_limits<std::size_t>::max)()) {
        throw std::length_error{"File size too large"};
      }
      buffer.resize(static_cast<std::size_t>(remaining_size));
    }

    std::size_t bytes_read = 0;
    while (true) {
      auto read_span = byte_view{buffer}.subspan(bytes_read);
      auto res = read(read_span);
      bytes_read += res;

      // EOF
      if (res < read_span.size()) {
        buffer.resize(bytes_read);
        break;
      }

      std::size_t new_size{};
      if (buffer.size() < resize_factor_limit) {
        new_size = buffer.size() / 2 * 3;
      } else {
        new_size = std::numeric_limits<std::size_t>::max();
      }
      buffer.resize(new_size);
    }

    buffer.shrink_to_fit();
    return buffer;
  }

  [[nodiscard]]
  auto read_once(byte_view data) const -> std::size_t {
    ssize_t result = -1;
    do {  // NOLINT
      result = ::read(native(), data.data(), data.size());
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
      throw mfile_system_error{errno, "read failed"};
    }
    return static_cast<std::size_t>(result);
  }

  [[nodiscard]]
  auto write_once(cbyte_view data) const -> std::size_t {
    ssize_t result = -1;
    do {  // NOLINT
      result = ::write(native(), data.data(), data.size());
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
      throw mfile_system_error{errno, "write failed"};
    }
    return static_cast<std::size_t>(result);
  }

  auto seek(std::int64_t offset, int whence) const -> std::uint64_t {
    auto result = ::lseek(native(), offset, whence);
    if (result == -1) {
      throw mfile_system_error{errno, "seek failed"};
    }
    return static_cast<std::uint64_t>(result);
  }

  [[nodiscard]]
  auto tell() const -> std::uint64_t {
    auto result = ::lseek(native(), 0, SEEK_CUR);
    if (result == -1) {
      throw mfile_system_error{errno, "tell failed"};
    }
    return static_cast<std::uint64_t>(result);
  }

  [[nodiscard]]
  auto stat() const -> struct stat {
    struct stat st {};
    if (::fstat(native(), &st) == -1) {
      throw mfile_system_error{errno, "stat failed"};
    }
    return st;
  }

  [[nodiscard]]
  auto size() const -> std::uint64_t {
    return static_cast<std::uint64_t>(stat().st_size);
  }

  [[nodiscard]]
  auto empty() const -> bool {
    return size() == 0;
  }

  void truncate(std::uint64_t size) const {
    int result = -1;
    do {  // NOLINT
      result = ::ftruncate(native(), static_cast<off_t>(size));
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
      throw mfile_system_error{errno, "truncate failed"};
    }
  }

  void sync() const {
    if (::fsync(native()) == -1) {
      throw mfile_system_error{errno, "sync failed"};
    }
  }

  constexpr void swap(file& other) noexcept {
    using std::swap;
    swap(handle_, other.handle_);
  }

  [[nodiscard]]
  constexpr auto handle() const noexcept -> const handle_type& {
    return handle_;
  }

 private:
  handle_type handle_{};

  [[nodiscard]]
  constexpr auto native() const noexcept -> int {
    return handle_->native();
  }
};

// deduction guides
template <file_handle_like H>
file(H) -> file<H>;

// non-member functions
template <file_handle_like Handle>
constexpr void swap(file<Handle>& lhs, file<Handle>& rhs) noexcept {
  lhs.swap(rhs);
}

[[nodiscard]]
inline auto open(const char* path,
                 open_flags flags,
                 mode_t mode = 0666) -> file<file_handle> {
  auto fd = ::open(path, flags.flags(), mode);  // NOLINT
  if (fd == -1) {
    throw mfile_system_error{errno,
                             std::format("Failed to open file: {}", path)};
  }
  return file{file_handle{weak_file_handle{fd}}};
}

[[nodiscard]]
inline auto make_tmpfile(std::string_view prefix) -> file<tmpfile_handle> {
  auto template_name = std::string{prefix} + "XXXXXX";
  auto fd = mkstemp(template_name.data());
  if (fd == -1) {
    throw mfile_system_error{errno, "Failed to create tmpfile"};
  }
  return file{tmpfile_handle{
      weak_file_handle{fd}, detail::tmpfile_deleter{std::move(template_name)}}};
}

}  // namespace mfile

namespace std {
template <>
struct is_error_code_enum<mfile::errc> : true_type {};
}  // namespace std
