# Test Data

This directory contains XPD test files for validating the TinyXPD library and tools.

## Test Files

### box.xpd
- **Description**: Well-structured test file with 144 splines across 6 faces
- **Format**: primSize=51 (full grooming format)
- **Features**: Includes clump data, guide information, CV parameters
- **Use case**: Primary test file for Alembic conversion and clump analysis

### box_dense.xpd
- **Description**: Dense variant of box.xpd
- **Format**: primSize=51
- **Use case**: Testing high-density spline data

### box_clump2.xpd
- **Description**: Box with 2 distinct clumps
- **Format**: primSize=51
- **Features**: Clear clump separation for testing clump ID assignment
- **Use case**: Clump detection and UV encoding validation

### mazu_body.xpd
- **Description**: Real-world character body grooming data
- **Format**: primSize=51
- **Use case**: Production data testing, realistic complexity

### sample.xpd
- **Description**: Legacy format test file with 126 splines across 444 faces
- **Format**: primSize=25 (basic format)
- **Use case**: Testing backwards compatibility with older XPD format

### spline_1_taper_1.xpd
- **Description**: Minimal test file with single spline
- **Format**: primSize=25
- **Use case**: Unit testing, format validation

## Running Tests

### Test with xpd_reader

```bash
cd build

# Convert all test files to Alembic
./xpd_reader ../tests/data/box.xpd ../tests/outputs/box.abc
./xpd_reader ../tests/data/box_dense.xpd ../tests/outputs/box_dense.abc
./xpd_reader ../tests/data/box_clump2.xpd ../tests/outputs/box_clump2.abc
./xpd_reader ../tests/data/mazu_body.xpd ../tests/outputs/mazu_body.abc
./xpd_reader ../tests/data/sample.xpd ../tests/outputs/sample.abc
./xpd_reader ../tests/data/spline_1_taper_1.xpd ../tests/outputs/spline_1.abc

# Test debug JSON output
./xpd_reader ../tests/data/box.xpd ../tests/outputs/box.abc --debug-json
```

### Test with xpd_reader_basic

```bash
# Inspect file headers
./xpd_reader_basic ../tests/data/box.xpd > ../tests/outputs/box_basic.txt
./xpd_reader_basic ../tests/data/sample.xpd > ../tests/outputs/sample_basic.txt
```

## Expected Results

All test files should:
- Parse without errors
- Generate valid Alembic or JSON output
- Preserve spline count and CV data
- Correctly identify clump membership (for primSize=51 files)

## Adding New Test Files

To add a new test file:
1. Copy the `.xpd` file to `tests/data/`
2. Add a description in this README
3. Test with both `xpd_reader` and `xpd_reader_basic`
4. Verify output in `tests/outputs/`
