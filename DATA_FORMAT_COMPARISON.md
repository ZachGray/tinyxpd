# XPD Data Format Comparison

## Summary

The XPD reader successfully handles **two different spline data layouts** based on `primSize`:

### Format 1: Full Grooming Data (primSize = 51)
**Files:** box.xpd
**Use case:** Complete grooming/hair data with detailed guide and styling information

**Data Layout (51 floats):**
1. primitiveId (1)
2. surfaceUV (2)
3. CVs positions (15 = 5 CVs × 3)
4. guideId (1)
5. guideWeight (1)
6. guideType (1)
7. guideDirection (3)
8. guidePrimRef (1)
9. guideUV (2)
10. surfaceNormal (3)
11. cvParameters (15 = 5 CVs × 3)
12. widthScale (4)
13. additionalAttributes (2)

**Total:** 51 floats

### Format 2: Basic Spline Data (primSize = 25)
**Files:** sample.xpd, spline_1_taper_1.xpd
**Use case:** Simple spline data without detailed styling

**Data Layout (25 floats):**
1. primitiveId (1)
2. surfaceUV (2)
3. CVs positions (15 = 5 CVs × 3)
4. guideId (1)
5. guideWeight (1)
6. guideType (1)
7. guideDirection (3)
8. guidePrimRef (1)

**Total:** 25 floats

## Validation Results

### ✅ box.xpd
- **primSize:** 51
- **numFaces:** 6
- **Total primitives:** 144 (24+25+25+25+24+21)
- **Parser status:** ✅ All fields parsed correctly
- **Output:** [box.json](box.json) (351KB)

### ✅ sample.xpd
- **primSize:** 25
- **numFaces:** 444 (sparse, ~100 primitives total)
- **Total primitives:** ~100 (scattered across faces)
- **Parser status:** ✅ Correctly handles smaller format
- **Output:** [samples/sample.json](samples/sample.json)

### ✅ spline_1_taper_1.xpd
- **primSize:** 25
- **numFaces:** 13
- **Total primitives:** 1 (on face 12)
- **Parser status:** ✅ Correctly handles smaller format
- **Output:** [samples/spline_1_taper_1.json](samples/spline_1_taper_1.json)

## Parser Behavior

The parser uses **conditional logic** to handle different data sizes:

```cpp
// Parse guide data based on XGen spline format
if (remaining >= 7) {
  // Parse guide id, weight, type, direction, primRef
  ...
}

// Only parse additional fields if they exist
if (remaining >= 9) {
  // Parse guide UV
  ...
}

if (idx + 2 < offset + floats_per_prim) {
  // Parse surface normal
  ...
}

// etc.
```

This means the parser gracefully handles:
- **Full format (51 floats):** Parses all available fields
- **Basic format (25 floats):** Stops after guide data, doesn't try to read non-existent fields
- **Future formats:** Will work with any primSize as long as the basic fields are present

## Common Fields

All formats share these core fields:

### Primitive Identification
- `primitiveId`: Unique spline ID
- `surfaceUV`: [u, v] attachment point on surface

### Geometry
- `cvs`: Array of 5 control vertices with positions [x, y, z]

### Guide Reference
- `guideId`: Which guide curve this follows
- `guideWeight`: Influence (typically 0.1 = 10%)
- `guideType`: Type of guide (0 = standard)
- `guideDirection`: Tangent vector at attachment
- `guidePrimRef`: Guide primitive reference index

## Extended Fields (primSize = 51 only)

- `guideUV`: UV coordinates on guide surface
- `surfaceNormal`: Normal vector at attachment point
- `cvParameters`: Per-CV parametric data (t-values 0.0-1.0)
- `widthScale`: Width/taper control (base, tip, params)
- `additionalAttributes`: Extra styling data

## Conclusion

✅ **Parser is robust and handles all tested formats correctly**

The conditional parsing logic ensures compatibility with:
- Different primSize values (25, 51, potentially others)
- Different primVersion values (all tested files use version 3)
- Sparse data (many faces with 0 primitives)
- Various mesh topologies (6 faces to 444 faces)
