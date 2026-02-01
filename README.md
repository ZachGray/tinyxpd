# TinyXPD - Header-only C++11 XGen XPD Cache I/O Library

![XPD Writer](img/xpd-writer.png)

TinyXPD is a simple, dependency-free header-only C++11 library for reading and writing XGen XPD cache files (spline format). Perfect for integrating XGen grooming data into your pipeline.

This fork converts xpd data with clump info into .abc files with the clump info written into the uv data per spline primitive.

## Quick Start

### 1. Add the header to your project

```cpp
#define TINY_XPD_IMPLEMENTATION  // Do this in ONE .cc file
#include "include/tiny_xpd.h"
```

### 2. Read an XPD file

```cpp
std::string filename = "myfile.xpd";
tiny_xpd::XPDHeader xpd_header;
std::vector<uint8_t> xpd_data;
std::string err;

bool ret = tiny_xpd::ParseXPDFromFile(filename, &xpd_header, &xpd_data, &err);
if (!ret) {
    std::cerr << "Error: " << err << std::endl;
    return 1;
}

// Access spline data
std::cout << "Number of CVs: " << xpd_header.numCVs << std::endl;
std::cout << "Number of faces: " << xpd_header.numFaces << std::endl;
```

### 3. Build and run the tools

```bash
mkdir build && cd build
cmake ..
cmake --build .

# Convert XPD to Alembic
./xpd_to_abc ../tests/data/box.xpd output.abc

# Generate debug JSON for inspection
./xpd_to_abc ../tests/data/box.xpd output.abc --debug-json
```

See [tests/data/](tests/data/) for sample XPD files.

## Features

- **Header-only** - No linking required, just `#include`
- **Zero dependencies** - Pure C++11, no external libraries
- **Cross-platform** - Windows, macOS, Linux, Android, iOS
- **XPD3 format support** - Compatible with Maya 2018 and earlier
- **Spline primitives** - Full support for XGen spline/hair/fur data
- **Memory efficient** - Stream parsing for large files (mmap support)
- **Tools included** - XPD to Alembic converter, JSON exporter

## Installation

### Option 1: Header-only (Recommended)

1. Copy `include/tiny_xpd.h` to your project
2. In ONE source file, add:
   ```cpp
   #define TINY_XPD_IMPLEMENTATION
   #include "tiny_xpd.h"
   ```
3. In other files, just:
   ```cpp
   #include "tiny_xpd.h"
   ```

### Option 2: CMake Integration

```cmake
add_subdirectory(external/tinyxpd)
target_link_libraries(your_target PRIVATE tinyxpd)
```

## API Reference

### Core Functions

#### `ParseXPDFromFile()`
```cpp
bool ParseXPDFromFile(const std::string& filename,
                      XPDHeader* header,
                      std::vector<uint8_t>* data,
                      std::string* err);
```
Reads an entire XPD file into memory and parses the header.

**Parameters:**
- `filename` - Path to the XPD file
- `header` - Output: Parsed XPD header information
- `data` - Output: Raw XPD primitive data
- `err` - Output: Error message if parsing fails

**Returns:** `true` on success, `false` on error

**Example:**
```cpp
tiny_xpd::XPDHeader header;
std::vector<uint8_t> data;
std::string err;

if (!ParseXPDFromFile("myfile.xpd", &header, &data, &err)) {
    std::cerr << "Failed to parse: " << err << std::endl;
}
```

#### `ParseXPDHeaderFromMemory()`
```cpp
bool ParseXPDHeaderFromMemory(const uint8_t* data,
                              size_t data_size,
                              XPDHeader* header,
                              std::string* err);
```
Parses XPD header from a memory buffer (useful for mmap or streaming).

**Use case:** Large files (>1GB) where you want to mmap the file instead of loading into memory.

### Data Structures

#### `XPDHeader`
```cpp
struct XPDHeader {
    unsigned char fileVersion;      // XPD file format version
    Xpd::PrimType primType;         // Primitive type (Point, Spline, Card, etc.)
    unsigned char primVersion;      // Primitive data layout version
    float time;                     // Frame/time when data was written
    uint32_t numCVs;               // CVs per primitive
    Xpd::CoordSpace coordSpace;    // Coordinate space (World, Object, Local, Micro)
    uint32_t numFaces;             // Number of mesh faces with primitives
    uint32_t numBlocks;            // Number of data blocks per face

    std::vector<std::string> block;        // Block names (e.g., "BakedGroom")
    std::vector<uint32_t> primSize;        // Floats per primitive (per block)
    std::vector<std::string> key;          // Attribute keys
    std::map<std::string, int> keyToId;    // Key name to ID mapping
    std::vector<int> faceid;               // Face IDs
    std::vector<uint32_t> numPrims;        // Primitives per face
    std::vector<uint64_t> blockPosition;   // Data offsets (face*numBlocks + block)
};
```

See [docs/api/API_REFERENCE.md](docs/api/API_REFERENCE.md) for complete API documentation.

## Tools

TinyXPD includes command-line tools for working with XPD files:

### `xpd_to_abc` - XPD to Alembic Converter

Convert XPD spline data to Alembic format for use in Houdini, Maya, etc.

```bash
# Convert to Alembic
xpd_to_abc input.xpd output.abc

# Convert and generate debug JSON
xpd_to_abc input.xpd output.abc --debug-json

# Print file information
xpd_to_abc input.xpd --info

# Generate JSON only
xpd_to_abc input.xpd --json-only
```

**Features:**
- Converts splines to Alembic OCurves
- Preserves curve widths (base + tip taper)
- Exports clump data as UV attributes (Houdini-compatible)
- Bezier basis for proper endpoint interpolation

See [src/xpd_to_abc.cc](src/xpd_to_abc.cc) for the complete source code.

### `xpd_reader_basic` - Simple XPD Inspector

Basic tool to inspect XPD file contents (no Alembic dependency).

```bash
xpd_reader_basic input.xpd
```

Prints header information and primitive data to stdout.

## Documentation

- **XPD Format Specifications**
  - [XPD Data Format](docs/format/XPD_DATA_FORMAT.md) - Detailed format specification

- **Source Code**
  - [xpd_to_abc.cc](src/xpd_to_abc.cc) - XPD to Alembic converter
  - [xpd_reader.cc](src/xpd_reader.cc) - Basic XPD reader/inspector
  - [tiny_xpd.h](include/tiny_xpd.h) - Header-only library

## Requirements

- **Compiler:** C++11 or later
- **Platforms:** Windows, macOS, Linux, Android, iOS
- **Dependencies:** None (header-only library)

**Tool dependencies (optional):**
- Alembic SDK (for xpd_to_abc converter only)

## Supported Platforms

- macOS
- Linux
- Windows (Visual Studio 2017+)
- Android
- iOS
- Big-endian architectures (untested)

## Supported XPD Versions

- **XPD3** (Maya 2018 and earlier, works on 2026)
- Spline primitives

## Sample Data

See the [tests/data/](tests/data/) directory for sample XPD files including:
- box.xpd - Simple box geometry with splines
- mazu_body.xpd - Complex character body grooming
- sample.xpd - Basic sample file

## Generating XPD Files from Maya

Use the `xgSplineDataToXpd` sample plug-in:
- Location: `/usr/autodesk/maya/plug-ins/xgen/plug-ins/`
- Or write your own XPD writer plugin using TinyXPD

**Note:** Legacy XGen's "From XPD File" feature expects the data layout from `xgSplineDataToXpd`. See examples for compatible layouts.

## Building from Source

### Using CMake

```bash
mkdir build && cd build
cmake ..
cmake --build .

# Run tests
./xpd_to_abc ../tests/data/box.xpd test_output.abc
```



### Build Options

CMake options:
- `TINYXPD_BUILD_TOOLS` - Build command-line tools (default: ON)
- `ALEMBIC_ROOT` - Path to Alembic SDK (required for xpd_to_abc tool)

## License

MIT License - see [LICENSE](LICENSE) for details.
Copyright (c) 2019 Syoyo Fujita



## Related Projects
- [Alembic](https://www.alembic.io/) - Computer graphics interchange framework
