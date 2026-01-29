#!/usr/bin/env python3
"""Extract clump IDs from box.xpd"""

import struct
import json
from collections import Counter

def parse_xpd_clumps(filepath: str):
    """Parse XPD and extract clump guide UVs"""

    clump_uvs = []

    with open(filepath, 'rb') as fp:
        # === HEADER ===
        magic = fp.read(4).decode('utf-8')
        if magic != "XPD3":
            raise ValueError(f"Invalid XPD magic: {magic}")

        file_version = struct.unpack('B', fp.read(1))[0]
        prim_type_int = struct.unpack('<I', fp.read(4))[0]
        prim_version = struct.unpack('B', fp.read(1))[0]
        time = struct.unpack('<f', fp.read(4))[0]
        num_cvs = struct.unpack('<I', fp.read(4))[0]
        coord_space_int = struct.unpack('<I', fp.read(4))[0]

        num_blocks = struct.unpack('<I', fp.read(4))[0]
        block_name_size = struct.unpack('<I', fp.read(4))[0]

        # Read block names
        block_names = []
        block_data = fp.read(block_name_size)
        pos = 0
        for _ in range(num_blocks):
            end = block_data.find(b'\x00', pos)
            block_names.append(block_data[pos:end].decode('utf-8'))
            pos = end + 1

        # Primitive sizes per block
        prim_sizes = list(struct.unpack(f'<{num_blocks}I', fp.read(4 * num_blocks)))

        print(f"Prim size per block: {prim_sizes}")
        print(f"Num CVs: {num_cvs}")

        # Keys
        num_keys = struct.unpack('<I', fp.read(4))[0]
        if num_keys > 0:
            key_name_size = struct.unpack('<I', fp.read(4))[0]
            fp.read(key_name_size)  # skip key names

        # Faces
        num_faces = struct.unpack('<I', fp.read(4))[0]
        face_ids = list(struct.unpack(f'<{num_faces}i', fp.read(4 * num_faces)))
        num_prims_per_face = list(struct.unpack(f'<{num_faces}I', fp.read(4 * num_faces)))

        # Block positions
        num_positions = num_faces * num_blocks
        positions = list(struct.unpack(f'<{num_positions}Q', fp.read(8 * num_positions)))

        block_positions = []
        for face_idx in range(num_faces):
            face_positions = []
            for block_idx in range(num_blocks):
                pos_idx = face_idx * num_blocks + block_idx
                face_positions.append(positions[pos_idx])
            block_positions.append(face_positions)

        # === PRIMITIVE DATA ===
        prim_index = 0

        for face_idx, face_id in enumerate(face_ids):
            num_prims = num_prims_per_face[face_idx]

            for block_idx, block_name in enumerate(block_names):
                if block_name != "BakedGroom":
                    continue

                block_pos = block_positions[face_idx][block_idx]
                prim_size_floats = prim_sizes[block_idx]
                prim_size_bytes = prim_size_floats * 4

                fp.seek(block_pos)

                for _ in range(num_prims):
                    data = fp.read(prim_size_bytes)
                    floats = struct.unpack(f'<{prim_size_floats}f', data)

                    # Extract clump guide UV at indices 28-29
                    if prim_size_floats >= 30:
                        clump_uv = (
                            round(floats[28], 6),
                            round(floats[29], 6)
                        )
                        clump_uvs.append(clump_uv)

                    prim_index += 1

        return clump_uvs

# Parse the file
clump_uvs = parse_xpd_clumps('box.xpd')

# Count unique clump UVs
uv_counts = Counter(clump_uvs)

print(f"\nTotal primitives: {len(clump_uvs)}")
print(f"Unique clump guide UVs: {len(uv_counts)}")
print("\nClump distribution:")
for uv, count in sorted(uv_counts.items(), key=lambda x: x[1], reverse=True):
    print(f"  UV {uv}: {count} primitives")
