#define TINY_XPD_IMPLEMENTATION
#include "tiny_xpd.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>

using namespace tiny_xpd;

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

// Helper function to escape strings for JSON
static std::string jsonEscape(const std::string& str) {
  std::ostringstream ss;
  for (char c : str) {
    switch (c) {
      case '"': ss << "\\\""; break;
      case '\\': ss << "\\\\"; break;
      case '\n': ss << "\\n"; break;
      case '\r': ss << "\\r"; break;
      case '\t': ss << "\\t"; break;
      default: ss << c; break;
    }
  }
  return ss.str();
}

static void WriteDetailedXPDtoJSON(const tiny_xpd::XPDHeader &xpd,
                                   const std::vector<uint8_t> &xpd_data,
                                   const std::string& output_filename) {

  std::ofstream out(output_filename);
  if (!out.is_open()) {
    std::cerr << "Failed to open output file: " << output_filename << "\n";
    return;
  }

  // Start JSON document
  out << "{\n";

  // Header information
  out << "  \"header\": {\n";
  out << "    \"fileVersion\": " << int(xpd.fileVersion) << ",\n";
  out << "    \"primType\": \"" << PrintPrimType(xpd.primType) << "\",\n";
  out << "    \"primVersion\": " << int(xpd.primVersion) << ",\n";
  out << "    \"time\": " << xpd.time << ",\n";
  out << "    \"numCVs\": " << xpd.numCVs << ",\n";
  out << "    \"coordSpace\": \"" << PrintCoordSpace(xpd.coordSpace) << "\",\n";
  out << "    \"numBlocks\": " << xpd.numBlocks << ",\n";
  out << "    \"numFaces\": " << xpd.numFaces << "\n";
  out << "  },\n";

  // Blocks information
  out << "  \"blocks\": [\n";
  for (size_t i = 0; i < xpd.block.size(); i++) {
    out << "    {\n";
    out << "      \"name\": \"" << jsonEscape(xpd.block[i]) << "\",\n";
    out << "      \"primSize\": " << xpd.primSize[i] << "\n";
    out << "    }" << (i < xpd.block.size() - 1 ? "," : "") << "\n";
  }
  out << "  ],\n";

  // Keys information
  out << "  \"keys\": [\n";
  for (size_t i = 0; i < xpd.key.size(); i++) {
    out << "    \"" << jsonEscape(xpd.key[i]) << "\""
        << (i < xpd.key.size() - 1 ? "," : "") << "\n";
  }
  out << "  ],\n";

  // Faces and primitives data
  out << "  \"faces\": [\n";

  // For each face
  for (size_t f = 0; f < xpd.numFaces; f++) {
    std::cout << "Processing face " << f << " (faceId=" << xpd.faceid[f]
              << ", " << xpd.numPrims[f] << " primitives)...\n";

    out << "    {\n";
    out << "      \"faceIndex\": " << f << ",\n";
    out << "      \"faceId\": " << xpd.faceid[f] << ",\n";
    out << "      \"numPrims\": " << xpd.numPrims[f] << ",\n";
    out << "      \"blocks\": [\n";

    // For each block in this face
    for (size_t b = 0; b < xpd.numBlocks; b++) {
      out << "        {\n";
      out << "          \"blockName\": \"" << jsonEscape(xpd.block[b]) << "\",\n";
      out << "          \"primitives\": [\n";

      std::vector<float> prims;
      GetPrimData(xpd, xpd_data, f, b, &prims);

      // Parse primitives based on primSize
      size_t floats_per_prim = xpd.primSize[b];
      size_t num_prims_in_block = xpd.numPrims[f];

      for (size_t p = 0; p < num_prims_in_block; p++) {
        size_t offset = p * floats_per_prim;

        out << "            {\n";

        if (offset + floats_per_prim <= prims.size()) {
          size_t idx = offset;

          // Primitive ID
          int prim_id = int(prims[idx++]);
          out << "              \"primitiveId\": " << prim_id << ",\n";

          // Surface UV
          if (idx + 1 < offset + floats_per_prim) {
            out << "              \"surfaceUV\": [" << std::fixed << std::setprecision(6)
                << prims[idx] << ", " << prims[idx+1] << "],\n";
            idx += 2;
          }

          // CV data
          if (xpd.numCVs > 0 && idx + xpd.numCVs * 3 <= offset + floats_per_prim) {
            out << "              \"cvs\": [\n";
            for (size_t cv = 0; cv < xpd.numCVs; cv++) {
              out << "                {\n";
              out << "                  \"cvId\": " << cv << ",\n";
              out << "                  \"position\": [" << prims[idx] << ", "
                  << prims[idx+1] << ", " << prims[idx+2] << "]\n";
              out << "                }" << (cv < xpd.numCVs - 1 ? "," : "") << "\n";
              idx += 3;
            }
            out << "              ],\n";
          }

          // Guide info
          out << "              \"guideInfo\": {\n";

          size_t remaining = (offset + floats_per_prim) - idx;

          // Parse guide data based on XGen spline format
          if (remaining >= 7) {
            out << "                \"guideId\": " << int(prims[idx++]) << ",\n";
            out << "                \"guideWeight\": " << prims[idx++] << ",\n";
            out << "                \"guideType\": " << int(prims[idx++]) << ",\n";
            out << "                \"guideDirection\": [" << prims[idx] << ", "
                << prims[idx+1] << ", " << prims[idx+2] << "],\n";
            idx += 3;
            out << "                \"guidePrimRef\": " << int(prims[idx++]) << ",\n";
          }

          // Guide UV coordinates
          if (remaining >= 9) {
            out << "                \"guideUV\": [" << prims[idx] << ", " << prims[idx+1] << "],\n";
            idx += 2;
          }

          // Clump data (indices 27-29)
          if (idx + 2 < offset + floats_per_prim) {
            out << "                \"clumpType\": " << prims[idx] << ",\n";
            out << "                \"clumpGuideUV\": [" << prims[idx+1] << ", "
                << prims[idx+2] << "],\n";
            idx += 3;
          }

          // CV parameters (t-values and widths along spline)
          if (idx + xpd.numCVs * 3 <= offset + floats_per_prim) {
            out << "                \"cvParameters\": [\n";
            for (size_t cv = 0; cv < xpd.numCVs; cv++) {
              out << "                  {\n";
              out << "                    \"cvId\": " << cv << ",\n";
              out << "                    \"param1\": " << prims[idx] << ",\n";
              out << "                    \"t\": " << prims[idx+1] << ",\n";
              out << "                    \"param3\": " << prims[idx+2] << "\n";
              out << "                  }" << (cv < xpd.numCVs - 1 ? "," : "") << "\n";
              idx += 3;
            }
            out << "                ],\n";
          }

          // Width/scale parameters
          if (idx + 4 <= offset + floats_per_prim) {
            out << "                \"widthScale\": {\n";
            out << "                  \"base\": " << prims[idx] << ",\n";
            out << "                  \"tip\": " << prims[idx+1] << ",\n";
            out << "                  \"param1\": " << prims[idx+2] << ",\n";
            out << "                  \"param2\": " << prims[idx+3] << "\n";
            out << "                }";
            idx += 4;

            // Check if there's more data
            if (idx < offset + floats_per_prim) {
              out << ",\n";
            } else {
              out << "\n";
            }
          }

          // Remaining data
          if (idx < offset + floats_per_prim) {
            out << "                \"additionalAttributes\": [";
            bool first = true;
            while (idx < offset + floats_per_prim) {
              if (!first) out << ", ";
              out << prims[idx++];
              first = false;
            }
            out << "]\n";
          }

          out << "              }\n";  // End guideInfo
        }

        out << "            }" << (p < num_prims_in_block - 1 ? "," : "") << "\n";
      }

      out << "          ]\n";  // End primitives array
      out << "        }" << (b < xpd.numBlocks - 1 ? "," : "") << "\n";
    }

    out << "      ]\n";  // End blocks array
    out << "    }" << (f < xpd.numFaces - 1 ? "," : "") << "\n";
  }

  out << "  ]\n";  // End faces array
  out << "}\n";     // End JSON document

  out.close();
  std::cout << "\nJSON output written successfully.\n";
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " input.xpd [output.json]\n";
    std::cerr << "  If output.json is not specified, output will be <input_basename>.json\n";
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
      output_filename = xpd_filename.substr(0, last_dot) + ".json";
    } else {
      output_filename = xpd_filename + ".json";
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

  WriteDetailedXPDtoJSON(xpd_header, xpd_data, output_filename);

  std::cout << "\nDone! Output saved to: " << output_filename << "\n";

  return EXIT_SUCCESS;
}
