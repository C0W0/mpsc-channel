# MpscChannel

A C++ implementation of a thread-safe Multi-Producer Single-Consumer (MPSC) channel inspired by Rust's standard library. This project aims to provide similar functionality to Rust's `std::sync::mpsc` channels but in a C++ context.

## Current Status

The project is in early development with the following components:

- `ConcurrentQueue`: A thread-safe queue implementation that supports multiple producers and a single consumer.

## Features

- **Thread-safe**: Built using C++20 atomic operations and semaphores
- **Lock-free operations**: Uses atomic operations for high performance
- **Fixed-capacity**: Pre-allocated with a fixed power-of-two size
- **Template-based**: Works with any movable C++ type

## Implementation Details

The current `ConcurrentQueue` implementation:

- Uses a circular buffer with atomic head and tail pointers
- Guarantees thread safety through atomic operations
- Provides blocking behavior managed by C++20 semaphores
- Has a fixed-size buffer with power-of-two capacity for efficient wrapping
- Supports a maximum of 2^32 elements per queue (configurable via template parameter)

## Build Requirements

- C++20 compliant compiler (for std::counting_semaphore and other C++20 features)
- CMake 3.29 or newer
- Any platform with proper C++20 support

### Building with CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

The project builds both a static library and a test executable.

## Roadmap

- Complete the full MPSC channel implementation
- Add more comprehensive test coverage
- Provide usage examples and documentation
- Implement performance benchmarking

## License

This project is open source and will be released under an MIT License once complete.

