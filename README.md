# mfile

A modern C++20 file I/O library with STL span-like interface and Python-like ease of use.

## Acknowledgments
This project is inspired by zpp_file (https://github.com/eyalz800/zpp_file)
Copyright (c) 2019 Eyal Z
The original implementation has been heavily refactored and modernized with C++20 features.

## Example Usage

File I/O in just 4 lines:

```cpp
auto file = mfile::open("data.txt", mfile::open_flags::wp());  // Python's "w+" mode
file.write_exact("Hello, World!"sv);  // string_view implicitly converts to cbyte_view
file.seek(0, SEEK_SET);
auto content = file.read();  // reads entire content as std::vector<std::byte>
```

## Features

- Python-like open modes
  ```cpp
  mfile::open_flags::r()   // read only
  mfile::open_flags::w()   // write (create & truncate)
  mfile::open_flags::a()   // append
  mfile::open_flags::x()   // exclusive creation
  // Add 'p' suffix for read+write: rp, wp, ap, xp
  ```

- Flexible byte operations
  - Uses byte_view (writable byte sequence view) and cbyte_view (read-only byte sequence view)
  - Implicit conversion from std::string, std::vector, raw arrays, and any contiguous memory container
  
- RAII-based resource management
  - File descriptors are automatically closed
  - Temporary files are automatically deleted

## Core APIs

Read/Write interfaces:

```cpp
// Low-level API - Single system call read/write
auto read_once(byte_view data) -> std::size_t;    // Reads up to data.size() bytes
auto write_once(cbyte_view data) -> std::size_t;  // Writes up to data.size() bytes

// Mid-level API - Multiple read/write attempts until size
auto read(byte_view data) -> std::size_t;    // Reads data.size() bytes if not EOF
auto write(cbyte_view data) -> std::size_t;  // Writes data.size() bytes if not disk full

// High-level API - Throws if exact size cannot be processed
void read_exact(byte_view data);    // Throws if can't read data.size() bytes
void write_exact(cbyte_view data);  // Throws if can't write data.size() bytes

// Helper APIs
auto read(std::size_t size) -> std::vector<std::byte>;  // Read specified size
auto read() -> std::vector<std::byte>;                  // Read until EOF
```

## Temporary Files

```cpp
auto tmp = mfile::make_tmpfile("/tmp/prefix_");  // Creates temporary file with prefix
// File is automatically deleted when tmp goes out of scope
```

## Error Handling

mfile uses C++'s exception handling system, with all exceptions inheriting from `mfile::mfile_error` (derived from `std::system_error`).

### Exception Hierarchy

```cpp
std::system_error
    └── mfile::mfile_error
            ├── mfile::mfile_system_error    // System call errors (errno based)
            ├── mfile::end_of_file_error     // EOF with partial read
            └── mfile::insufficient_space_error // Device full with partial write
```

### Common Exceptions

1. System-related (`mfile::mfile_system_error`):
```cpp
try {
    auto file = mfile::open("/nonexistent/path", mfile::open_flags::r());
} catch (const mfile::mfile_system_error& e) {
    // e.code() returns std::error_code with system_category
    if (e.code().value() == ENOENT) {  // No such file
        // Handle missing file
    }
}
```

2. EOF-related (`mfile::end_of_file_error`):
```cpp
std::array<std::byte, 1024> buffer;
try {
    file.read_exact(buffer);  // Requires exact size read
} catch (const mfile::end_of_file_error& e) {
    std::cout << "Read " << e.bytes_read() << " bytes before EOF\n";
}
```

3. Space-related (`mfile::insufficient_space_error`):
```cpp
try {
    file.write_exact(large_buffer);
} catch (const mfile::insufficient_space_error& e) {
    std::cout << "Wrote " << e.bytes_written() << " bytes before device full\n";
}
```

### Exception Safety

The RAII design ensures that file resources are properly cleaned up even when exceptions occur:

```cpp
try {
    auto file = mfile::open("test.txt", mfile::open_flags::w());
    file.write_exact("test"sv);
    throw std::runtime_error("Some error");
} catch (...) {
    // File is automatically closed
}
```

## Requirements

- range3::byte_span (https://github.com/range3/byte-span)
- C++20 compliant compiler
- CMake 3.14+
- Linux environment

# Building and installing

See the [BUILDING](BUILDING.md) document.

## License

MIT License

## Acknowledgments

This project is inspired by [zpp_file](https://github.com/eyalz800/zpp_file) and reimplemented with modern C++20 features.
