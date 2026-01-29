# XPD Clump Analysis - box.xpd

## Discovery

The clump membership for XGen splines is encoded in the **clump guide UV coordinates** at **indices 28-29** of the primitive data array.

## Clump Distribution

box.xpd contains **5 clumps** with **144 total primitives**:

| Clump ID | Guide UV (u, v) | Primitive Count |
|----------|-----------------|-----------------|
| 1 | (0.470866, 0.606274) | 44 primitives |
| 2 | (0.539555, 0.440540) | 31 primitives |
| 3 | (0.711953, 0.840411) | 25 primitives |
| 4 | (0.482326, 0.429606) | 23 primitives |
| 5 | (0.906047, 0.784779) | 21 primitives |

**Total:** 144 primitives

## Data Format Correction

### Previous (Incorrect) Labeling:
```
Index 25-26: guideUV        -> [0.0, 2.0] (all identical)
Index 27-29: surfaceNormal  -> [?, ?, ?]
```

### Corrected Labeling:
```
Index 25-26: guideUV               -> [0.0, 2.0] (guide curve UV reference)
Index 27:    clumpType/unknown     -> 0.0 or 3.0 (purpose TBD)
Index 28-29: clumpGuideUV          -> Unique UV per clump (CLUMP IDENTIFIER)
```

## Example Data

Primitive from Clump 1:
```json
{
  "index_27": 3.000000,
  "clumpGuideUV": [0.470866, 0.606274]  // Clump 1
}
```

Primitive from Clump 3:
```json
{
  "index_27": 0.000000,
  "clumpGuideUV": [0.711953, 0.840411]  // Clump 3
}
```

## Usage in Alembic Export

For proper clump-based hair rendering, we should:

1. **Add clump ID as primitive attribute**: Use the clump guide UV as a unique identifier
2. **Group primitives by clump**: Create separate curve groups or add clump ID metadata
3. **Preserve clump UV data**: Store as a V2f user property on curves

## Next Steps

- [ ] Update xpd_reader_detailed.cc to correctly label clumpGuideUV
- [ ] Update xpd_reader_abc.cc to export clump IDs as Alembic arbitrary geometry parameters
- [ ] Update DATA_FORMAT_COMPARISON.md with corrected field names
- [ ] Add clump visualization/grouping in exported Alembic files
