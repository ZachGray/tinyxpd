#define TINY_XPD_IMPLEMENTATION
#include "tiny_xpd.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <utility>

// Alembic includes
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreOgawa/All.h>

using namespace tiny_xpd;
using namespace Alembic::AbcGeom;

static std::string PrintPrimType(Xpd::PrimType prim) {
  if (prim == Xpd::PrimType::Point) {
    return "Point";
  } else if (prim == Xpd::PrimType::Spline) {
    return "Spline";
  } else if (prim == Xpd::PrimType::Card) {
    return "Card";
  } else if (prim == Xpd::PrimType::Sphere) {
    return "Sphere";
  } else if (prim == Xpd::PrimType::Archive) {
    return "Archive";
  } else if (prim == Xpd::PrimType::CustomPT) {
    return "CustomPT";
  }

  return "UNKNOWN PrimType. value = " + std::to_string(int(prim));
}

static std::string PrintCoordSpace(Xpd::CoordSpace space) {
  if (space == Xpd::CoordSpace::World) {
    return "World";
  } else if (space == Xpd::CoordSpace::Object) {
    return "Object";
  } else if (space == Xpd::CoordSpace::Local) {
    return "Local";
  } else if (space == Xpd::CoordSpace::Micro) {
    return "Micro";
  } else if (space == Xpd::CoordSpace::CustomCS) {
    return "CustomCS";
  }

  return "UNKNOWN CoordSpace. value = " + std::to_string(int(space));
}

static void GetPrimData(const tiny_xpd::XPDHeader &xpd, const std::vector<uint8_t> &xpd_data,
                       size_t face_idx, size_t block_idx, std::vector<float> *prims)
{
  prims->clear();

  uint32_t num_prims = xpd.numPrims[face_idx];
  if (num_prims == 0) {
    return;
  }

  // prim_count = num_prims * sum(xpd.primSize[])
  size_t prim_count = 0;
  for (size_t p = 0; p < xpd.primSize.size(); p++) {
    prim_count += xpd.primSize[p];
  }

  prim_count *= num_prims;

  // Primitive value is always float.
  const size_t num_bytes = sizeof(float) * prim_count;

  const size_t src_offset = xpd.blockPosition[face_idx * xpd.numBlocks + block_idx];
  std::vector<float> buffer;
  buffer.resize(prim_count);
  memcpy(buffer.data(), xpd_data.data() + src_offset, num_bytes);

  prims->insert(prims->end(), buffer.begin(), buffer.end());
}

static void WriteXPDtoAlembic(const tiny_xpd::XPDHeader &xpd,
                              const std::vector<uint8_t> &xpd_data,
                              const std::string& output_filename) {

  // Create Alembic archive
  Alembic::AbcCoreOgawa::WriteArchive writer;
  Alembic::Abc::OArchive archive(writer, output_filename.c_str());

  if (!archive.valid()) {
    std::cerr << "Failed to create Alembic archive: " << output_filename << "\n";
    return;
  }

  std::cout << "Created Alembic archive: " << output_filename << "\n";

  // Create OCurves object for all splines
  OCurves curves(Alembic::Abc::OObject(archive, Alembic::Abc::kTop), "xgen_splines");
  OCurvesSchema &schema = curves.getSchema();

  // Collect all curve data
  std::vector<Imath::V3f> positions;
  std::vector<int32_t> nVertices;
  std::vector<float> widths;
  std::vector<Imath::V2f> clumpGuideUVs;  // Per-curve clump guide UV
  std::vector<int32_t> clumpIds;          // Per-curve clump ID (simplified 0-4)

  // Map to assign clump IDs
  std::map<std::pair<float, float>, int32_t> clumpUVtoID;
  int32_t next_clump_id = 0;

  // Track correlation between clumpType and invalid width data
  std::map<int, int> invalid_width_by_face_id;
  std::map<int, int> total_by_face_id;

  size_t total_curves = 0;

  // Count total curves first
  for (size_t f = 0; f < xpd.numFaces; f++) {
    total_curves += xpd.numPrims[f];
  }

  std::cout << "Processing " << total_curves << " curves across " << xpd.numFaces << " faces...\n";

  // Reserve space
  positions.reserve(total_curves * xpd.numCVs);
  nVertices.reserve(total_curves);
  widths.reserve(total_curves * xpd.numCVs);
  clumpGuideUVs.reserve(total_curves);
  clumpIds.reserve(total_curves);

  // For each face
  for (size_t f = 0; f < xpd.numFaces; f++) {
    if (xpd.numPrims[f] == 0) continue;

    std::cout << "  Face " << f << " (faceId=" << xpd.faceid[f]
              << ", " << xpd.numPrims[f] << " curves)\n";

    // For each block in this face (we assume there's one "BakedGroom" block)
    for (size_t b = 0; b < xpd.numBlocks; b++) {
      std::vector<float> prims;
      GetPrimData(xpd, xpd_data, f, b, &prims);

      // Parse primitives based on primSize
      size_t floats_per_prim = xpd.primSize[b];
      size_t num_prims_in_block = xpd.numPrims[f];

      for (size_t p = 0; p < num_prims_in_block; p++) {
        size_t offset = p * floats_per_prim;

        if (offset + floats_per_prim <= prims.size()) {
          size_t idx = offset;

          // Get primitive ID for debugging
          float prim_id = prims[offset];
          idx++;

          // Skip surface UV
          if (idx + 1 < offset + floats_per_prim) {
            idx += 2;
          }

          // Extract CV positions with duplicated endpoints for Catmull-Rom interpolation
          if (xpd.numCVs > 0 && idx + xpd.numCVs * 3 <= offset + floats_per_prim) {
            // Duplicate first CV (for Catmull-Rom endpoint interpolation)
            positions.push_back(Imath::V3f(prims[idx], prims[idx + 1], prims[idx + 2]));

            // Extract all CVs
            for (size_t cv = 0; cv < xpd.numCVs; cv++) {
              float x = prims[idx++];
              float y = prims[idx++];
              float z = prims[idx++];
              positions.push_back(Imath::V3f(x, y, z));
            }

            // Duplicate last CV (for Catmull-Rom endpoint interpolation)
            positions.push_back(Imath::V3f(prims[idx - 3], prims[idx - 2], prims[idx - 1]));

            // Total vertices = original CVs + 2 duplicates
            nVertices.push_back(xpd.numCVs + 2);
          }

          // Extract clump guide UV and assign clump ID
          // Data layout: [prim_id(1)] [surface_uv(2)] [cv_positions(numCVs*3)]
          //              [guide_info(7)] [guide_uv(2)] [face_id(1)] [clump_guide_uv(2)]
          //              [cv_parameters(numCVs*3)] [width_scale(base, tip, ...)] ...
          float clump_uv_u = 0.0f;
          float clump_uv_v = 0.0f;
          float face_id = 0.0f;

          // Calculate face_id offset: 1 (prim_id) + 2 (surface_uv) + numCVs*3 (positions) +
          //                              7 (guide_info) + 2 (guide_uv)
          size_t face_id_offset = 1 + 2 + (xpd.numCVs * 3) + 7 + 2;

          // Extract face_id
          if (face_id_offset < floats_per_prim) {
            face_id = prims[offset + face_id_offset];
          }

          // Calculate clump_guide_uv offset: face_id_offset + 1
          size_t clump_uv_offset = face_id_offset + 1;

          // Calculate cv_parameters offset and check if guide data is valid
          size_t cv_params_offset = 1 + 2 + (xpd.numCVs * 3) + 7 + 2 + 1 + 2;
          size_t cv_params_count = xpd.numCVs * 3;

          // Check if CV parameters section is all zeros (indicates no guide associated)
          bool has_valid_guide = false;
          if (cv_params_offset + cv_params_count <= floats_per_prim) {
            for (size_t i = 0; i < cv_params_count; i++) {
              if (prims[offset + cv_params_offset + i] != 0.0f) {
                has_valid_guide = true;
                break;
              }
            }
          }

          if (!has_valid_guide) {
            // No guide associated - assign to clump_id -1 for later culling
            clumpGuideUVs.push_back(Imath::V2f(0.0f, 0.0f));
            clumpIds.push_back(-1);
          } else if (clump_uv_offset + 1 < floats_per_prim) {
            // Valid guide - extract clump UV and assign clump ID
            clump_uv_u = prims[offset + clump_uv_offset];
            clump_uv_v = prims[offset + clump_uv_offset + 1];

            // Round to avoid floating point comparison issues
            float rounded_u = std::round(clump_uv_u * 1000000.0f) / 1000000.0f;
            float rounded_v = std::round(clump_uv_v * 1000000.0f) / 1000000.0f;

            clumpGuideUVs.push_back(Imath::V2f(clump_uv_u, clump_uv_v));

            // Assign clump ID based on unique clumpGuideUV
            auto uv_pair = std::make_pair(rounded_u, rounded_v);
            if (clumpUVtoID.find(uv_pair) == clumpUVtoID.end()) {
              clumpUVtoID[uv_pair] = next_clump_id++;
            }
            clumpIds.push_back(clumpUVtoID[uv_pair]);
          } else {
            // No clump data available
            clumpGuideUVs.push_back(Imath::V2f(0.0f, 0.0f));
            clumpIds.push_back(-1);
          }

          // Extract width data
          // Data layout: [prim_id(1)] [surface_uv(2)] [cv_positions(numCVs*3)]
          //              [guide_info(7)] [guide_uv(2)] [face_id(1)] [clump_guide_uv(2)]
          //              [cv_parameters(numCVs*3)] [width_scale(base, tip, param1, param2)...] ...

          // Calculate width offset: cv_params_offset + cv_params_count
          size_t width_offset = cv_params_offset + cv_params_count;

          // Track total curves by clumpType
          int face_id_int = static_cast<int>(face_id);
          total_by_face_id[face_id_int]++;

          // Track curves without guide data
          if (!has_valid_guide) {
            invalid_width_by_face_id[face_id_int]++;
          }

          // Try to read width data regardless of guide status
          if (width_offset + 1 < floats_per_prim) {
            float base_width = prims[offset + width_offset];
            float tip_width = prims[offset + width_offset + 1];

            // Duplicate first width (for Catmull-Rom endpoint interpolation)
            widths.push_back(base_width);

            // Interpolate width along the curve (per-vertex)
            for (size_t cv = 0; cv < xpd.numCVs; cv++) {
              float t = float(cv) / float(xpd.numCVs - 1);
              float width = base_width * (1.0f - t) + tip_width * t;
              widths.push_back(width);
            }

            // Duplicate last width (for Catmull-Rom endpoint interpolation)
            widths.push_back(tip_width);
          } else {
            // No width data available, use default width of 0.01
            // Duplicate first width
            widths.push_back(0.01f);

            for (size_t cv = 0; cv < xpd.numCVs; cv++) {
              widths.push_back(0.01f);
            }

            // Duplicate last width
            widths.push_back(0.01f);
          }
        }
      }
    }
  }

  std::cout << "\nWriting " << nVertices.size() << " curves with "
            << positions.size() << " CVs to Alembic...\n";
  std::cout << "  Unique clumps: " << clumpUVtoID.size() << "\n";

  // Report correlation between face ID and curves without guide data
  std::cout << "\nFace ID analysis:\n";
  for (const auto& entry : total_by_face_id) {
    int face = entry.first;
    int total = entry.second;
    int no_guide = invalid_width_by_face_id[face];
    std::cout << "  Face " << face << ": " << total << " curves, "
              << no_guide << " without guide data";
    if (no_guide > 0) {
      std::cout << " (" << (100.0f * no_guide / total) << "%)";
    }
    std::cout << "\n";
  }

  // WORKAROUND: Store clump_id in UV channel
  //
  // Ideally we would use Alembic user properties (via schema.getUserProperties()) to store
  // custom attributes like clump_id. However, Houdini's Alembic importer does not read
  // user properties on OCurves geometry - it only reads the built-in schema properties.
  //
  // As a workaround, we use the UV attribute (which is part of the OCurves schema) to
  // store the clump_id. In Houdini, this will appear as:
  //   - uv[0] or @uv.x = clump_id (integer 0-N representing which clump this curve belongs to)
  //   - uv[1] or @uv.y = curve_index (curve number, stored for reference/debugging)
  //   - uv[2] or @uv.z = 0.0 (Houdini expands 2D UVs to 3D)
  //
  // If you need actual UV coordinates, you'll need to use a different approach or
  // generate UVs in Houdini after import.

  std::vector<Imath::V2f> uvs;
  uvs.reserve(positions.size());

  for (size_t i = 0; i < nVertices.size(); i++) {
    int numVerts = nVertices[i];
    int32_t curveClumpId = clumpIds[i];

    // Repeat the same clump_id for all vertices in this curve
    for (int v = 0; v < numVerts; v++) {
      uvs.push_back(Imath::V2f(
        static_cast<float>(curveClumpId),  // U component = clump_id
        static_cast<float>(i)              // V component = curve_index (for debugging)
      ));
    }
  }

  // Create the curve sample
  OCurvesSchema::Sample sample;

  // Set positions
  sample.setPositions(Alembic::Abc::P3fArraySample(
    reinterpret_cast<const Imath::V3f*>(positions.data()),
    positions.size()
  ));

  // Set number of vertices per curve
  sample.setCurvesNumVertices(Alembic::Abc::Int32ArraySample(
    nVertices.data(),
    nVertices.size()
  ));

  // Set curve type and wrap
  sample.setType(kCubic);               // XGen splines are cubic
  sample.setWrap(kNonPeriodic);         // Non-periodic (open curves)
  sample.setBasis(kBezierBasis);        // Bezier passes through endpoints

  // Set widths (per-vertex)
  if (!widths.empty()) {
    OFloatGeomParam::Sample widthSample(
      Alembic::Abc::FloatArraySample(widths.data(), widths.size()),
      kVertexScope
    );
    sample.setWidths(widthSample);
  }

  // Set UVs (per-vertex) - WORKAROUND: stores clump_id in U component
  // See comment above for explanation of why we use UV instead of user properties
  if (!uvs.empty()) {
    OV2fGeomParam::Sample uvSample(
      Alembic::Abc::V2fArraySample(
        reinterpret_cast<const Imath::V2f*>(uvs.data()),
        uvs.size()
      ),
      kVertexScope
    );
    sample.setUVs(uvSample);
    std::cout << "  UVs written with clump data (U=clump_id, V=curve_index): " << uvs.size() << " values\n";
  }

  // Write the main sample
  schema.set(sample);

  std::cout << "Alembic output written successfully.\n";
  std::cout << "  Curves: " << nVertices.size() << "\n";
  std::cout << "  Total CVs: " << positions.size() << "\n";
  std::cout << "  Clumps: " << clumpUVtoID.size() << "\n";
  std::cout << "  Basis: Bezier\n";
  std::cout << "  Type: Cubic\n";
  std::cout << "  Periodicity: Non-periodic\n";
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " input.xpd [output.abc]\n";
    std::cerr << "  If output.abc is not specified, output will be <input_basename>.abc\n";
    return EXIT_FAILURE;
  }

  std::string xpd_filename = argv[1];

  // Determine output filename
  std::string output_filename;
  if (argc >= 3) {
    output_filename = argv[2];
  } else {
    // Generate output filename from input filename
    size_t last_dot = xpd_filename.find_last_of('.');
    if (last_dot != std::string::npos) {
      output_filename = xpd_filename.substr(0, last_dot) + ".abc";
    } else {
      output_filename = xpd_filename + ".abc";
    }
  }

  std::cout << "Reading XPD file: " << xpd_filename << "\n";
  std::cout << "Output will be written to: " << output_filename << "\n\n";

  std::string err;
  tiny_xpd::XPDHeader xpd_header;
  std::vector<uint8_t> xpd_data;

  if (!tiny_xpd::ParseXPDFromFile(xpd_filename, &xpd_header, &xpd_data, &err)) {
    if (!err.empty()) {
      std::cerr << "Parse error message: " << err << "\n";
    }

    std::cerr << "Failed to parse XPD file : " << xpd_filename << "\n";
    return EXIT_FAILURE;
  }

  if (xpd_header.primType != tiny_xpd::Xpd::PrimType::Spline) {
    std::cerr << "Currently we only support Spline primitive.\n";
    return EXIT_FAILURE;
  }

  std::cout << "File info:\n";
  std::cout << "  primType: " << PrintPrimType(xpd_header.primType) << "\n";
  std::cout << "  primVersion: " << int(xpd_header.primVersion) << "\n";
  std::cout << "  numCVs: " << xpd_header.numCVs << "\n";
  std::cout << "  numFaces: " << xpd_header.numFaces << "\n";
  std::cout << "  numBlocks: " << xpd_header.numBlocks << "\n\n";

  WriteXPDtoAlembic(xpd_header, xpd_data, output_filename);

  std::cout << "\nDone! Output saved to: " << output_filename << "\n";

  return EXIT_SUCCESS;
}
