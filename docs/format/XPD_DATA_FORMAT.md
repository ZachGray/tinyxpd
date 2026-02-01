# XPD Spline Data Format Documentation

## Overview
This document describes the data format for XGen spline primitives stored in XPD files (version 3).
This is a best guess LLM summary

## File Structure

### Header
- `fileVersion`: Version of the header format (0 in this case)
- `primType`: Type of primitive ("Spline" for hair/fur)
- `primVersion`: Version of the primitive data layout (3)
- `time`: Frame number or time when data was written
- `numCVs`: Number of control vertices per spline (5 in this example)
- `coordSpace`: Coordinate space ("Object", "World", "Local", etc.)
- `numBlocks`: Number of data blocks per face
- `numFaces`: Total number of mesh faces with primitives

### Blocks
Each block represents a named data section. For grooming data:
- **BakedGroom**: Contains all spline and guide information
- `primSize`: Number of floats per primitive (51 in this case)

### Keys
String identifiers for data attributes:
- `vertex point xgClumpingGuide`: Guide curve CV positions
- `vertex float xgClumpingWeight`: Clumping weight values
- Additional custom keys as needed

## Primitive Data Layout

### 1. Primitive Identification (3 floats)
- **primitiveId** (1 float): Unique ID for this spline primitive
- **surfaceUV** (2 floats): [u, v] coordinates on the surface where the spline originates

### 2. Control Vertex Positions (15 floats = 5 CVs × 3)
Array of CV positions in 3D space:
```json
"cvs": [
  {
    "cvId": 0,
    "position": [x, y, z]  // Root CV
  },
  {
    "cvId": 1,
    "position": [x, y, z]
  },
  // ... through CV 4 (tip)
]
```

### 3. Guide Information (7 floats)
Information about the guide curve this spline follows:
- **guideId** (1 float): ID of the guide curve
- **guideWeight** (1 float): Influence weight (0.0 to 1.0, typically 0.1 = 10%)
- **guideType** (1 float): Type of guide (0 = standard guide)
- **guideDirection** (3 floats): [x, y, z] tangent/direction vector at guide root
- **guidePrimRef** (1 float): Reference index to guide primitive

### 4. Guide UV Coordinates (2 floats)
- **guideUV** (2 floats): [u, v] UV coordinates on the guide surface

### 5. Surface Normal (3 floats)
- **surfaceNormal** (3 floats): [x, y, z] normal vector at the surface attachment point

### 6. CV Parameters (15 floats = 5 CVs × 3)
Per-CV parametric data along the spline:
```json
"cvParameters": [
  {
    "cvId": 0,
    "param1": 0.0,      // Often zero, may be width or color
    "t": 0.0,           // Parametric position (0.0 at root)
    "param3": 0.0       // Often zero, may be width or color
  },
  {
    "cvId": 1,
    "param1": value,
    "t": 0.25,          // 1/4 along the spline
    "param3": value
  },
  // ... through t=1.0 at the tip
]
```

**Note**: The `t` value represents the parametric position along the spline from 0.0 (root) to 1.0 (tip). The `param1` and `param3` values are typically near zero but may store additional per-CV attributes.

### 7. Width Scale (4 floats)
Width/scaling parameters for the spline:
```json
"widthScale": {
  "base": 1.0,       // Base width multiplier
  "tip": 0.0,        // Tip width (0.0 = taper to point)
  "param1": value,   // Additional scale parameter
  "param2": value    // Additional scale parameter
}
```

### 8. Additional Attributes (2 floats)
Additional rendering or styling attributes (likely color, roughness, etc.):
```json
"additionalAttributes": [value1, value2]
```

## Face Organization

Data is organized by mesh faces:
```json
"faces": [
  {
    "faceIndex": 0,
    "faceId": 0,           // Mesh face ID
    "numPrims": 24,        // Number of splines on this face
    "blocks": [
      {
        "blockName": "BakedGroom",
        "primitives": [ /* array of primitive data */ ]
      }
    ]
  }
]
```

## Usage Notes

### Reading Primitive Data
Each primitive is exactly `primSize` floats. The data is tightly packed with no headers or delimiters between primitives.

### Coordinate Spaces
- **Object Space**: Default XGen space, relative to the geometry
- **World Space**: Absolute world coordinates
- **Local Space**: Surface coordinate frame at the attachment point
- **Micro Space**: CV-local coordinate frames

### Guide System
- Splines follow guide curves with a weight (typically 0.1 = 10% influence)
- The `guideId` references which guide curve this spline follows
- `guideDirection` provides the initial tangent direction
- `guideUV` locates the attachment point on the guide surface

## Data Format Version

This documentation is based on:
- XPD Format: XPD3
- File Version: 0
- Primitive Version: 3
- Primitive Type: Spline

Different primitive versions may have different layouts. Always check `primVersion` when parsing.
