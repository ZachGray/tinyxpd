# Test Outputs

This directory contains generated outputs from running XPD conversion tools on test data.

Files in this directory are **not tracked by git** (see `.gitignore`).

## Generating Outputs

From the repository root:

```bash
# Build tools
mkdir build && cd build
cmake ..
cmake --build .

# Generate outputs
./xpd_reader ../tests/data/box.xpd ../tests/outputs/box.abc
./xpd_reader ../tests/data/box.xpd ../tests/outputs/box.abc --debug-json
./xpd_reader ../tests/data/mazu_body.xpd ../tests/outputs/mazu_body.abc
```

## Output Files

After running the tools, you should see:
- `*.abc` - Alembic curve files
- `*_debug.json` - Detailed JSON debug files (if --debug-json used)
