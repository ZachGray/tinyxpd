#define TINY_XPD_IMPLEMENTATION
#include "tiny_xpd.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

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

          // Skip primitive ID
          idx++;

          // Skip surface UV
          if (idx + 1 < offset + floats_per_prim) {
            idx += 2;
          }

          // Extract CV positions
          if (xpd.numCVs > 0 && idx + xpd.numCVs * 3 <= offset + floats_per_prim) {
            for (size_t cv = 0; cv < xpd.numCVs; cv++) {
              float x = prims[idx++];
              float y = prims[idx++];
              float z = prims[idx++];
              positions.push_back(Imath::V3f(x, y, z));
            }
            nVertices.push_back(xpd.numCVs);
          }

          // Extract width data if available (for primSize=51)
          // Skip guide info to get to width data
          size_t remaining = (offset + floats_per_prim) - idx;

          // Skip guide data (7 floats)
          if (remaining >= 7) {
            idx += 7;
          }

          // Skip guide UV (2 floats)
          if (remaining >= 9) {
            idx += 2;
          }

          // Skip surface normal (3 floats)
          if (idx + 2 < offset + floats_per_prim) {
            idx += 3;
          }

          // Skip CV parameters (15 floats for 5 CVs)
          if (idx + xpd.numCVs * 3 <= offset + floats_per_prim) {
            idx += xpd.numCVs * 3;
          }

          // Extract width/scale parameters
          if (idx + 4 <= offset + floats_per_prim) {
            float base_width = prims[idx];
            float tip_width = prims[idx + 1];

            // Interpolate width along the curve (per-vertex)
            for (size_t cv = 0; cv < xpd.numCVs; cv++) {
              float t = float(cv) / float(xpd.numCVs - 1);
              float width = base_width * (1.0f - t) + tip_width * t;
              widths.push_back(width);
            }
          } else {
            // No width data, use default width of 0.01
            for (size_t cv = 0; cv < xpd.numCVs; cv++) {
              widths.push_back(0.01f);
            }
          }
        }
      }
    }
  }

  std::cout << "\nWriting " << nVertices.size() << " curves with "
            << positions.size() << " CVs to Alembic...\n";

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
  sample.setType(kCubic);           // XGen splines are cubic
  sample.setWrap(kNonPeriodic);     // Non-periodic (open curves)
  sample.setBasis(kBsplineBasis);   // B-spline basis (common for hair/fur)

  // Set widths (per-vertex)
  if (!widths.empty()) {
    OFloatGeomParam::Sample widthSample(
      Alembic::Abc::FloatArraySample(widths.data(), widths.size()),
      kVertexScope
    );
    sample.setWidths(widthSample);
  }

  // Write the sample
  schema.set(sample);

  std::cout << "Alembic output written successfully.\n";
  std::cout << "  Curves: " << nVertices.size() << "\n";
  std::cout << "  Total CVs: " << positions.size() << "\n";
  std::cout << "  Basis: B-spline\n";
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
