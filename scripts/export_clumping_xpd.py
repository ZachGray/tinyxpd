"""
Export XPD files for each clumping modifier in an XGen scene.

This script iterates through all XGen collections (palettes) and descriptions,
finds clumping modifiers, enables cvAttr for each, generates XPD files via
FileRenderer, and renames them to match the clumping modifier names.

Usage:
    Run in Maya's Script Editor or via mayapy:

    import sys
    sys.path.insert(0, r"z:\vscode_xgen\tinyxpd\scripts")
    import export_clumping_xpd
    export_clumping_xpd.run()
"""

import os
import glob
import subprocess

import xgenm as xg
import maya.mel as mel
import maya.cmds as cmds

# Path to the XPD to Alembic conversion tool
XPD_TO_ABC = "Z:/vscode_xgen/tinyxpd/build/src/Release/xpd_to_abc.exe"


def list_scene_xgen_info():
    """Print all collections and descriptions in the scene.

    Returns:
        list: List of (palette, description) tuples found in the scene.
    """
    results = []
    palettes = xg.palettes()

    if not palettes:
        print("No XGen collections found in the scene.")
        return results

    print("\n=== XGen Scene Info ===")
    for palette in palettes:
        print(f"\nCollection: {palette}")
        descriptions = xg.descriptions(palette)

        if not descriptions:
            print("  (no descriptions)")
            continue

        for description in descriptions:
            print(f"  Description: {description}")
            results.append((palette, description))

            # List FX modules
            modules = xg.fxModules(palette, description)
            if modules:
                print(f"    FX Modules: {', '.join(modules)}")

    print("\n" + "=" * 25)
    return results


def get_clumping_modifiers(palette, description):
    """Get all clumping modifiers for a description.

    Args:
        palette: The XGen palette/collection name.
        description: The XGen description name.

    Returns:
        list: List of clumping modifier names.
    """
    clumping_mods = []
    modules = xg.fxModules(palette, description)

    for module in modules:
        mod_type = xg.fxModuleType(palette, description, module)
        if mod_type == "ClumpingFXModule":
            clumping_mods.append(module)

    return clumping_mods


def is_description_visible(palette, description):
    """Check if a description is visible (not hidden).

    Args:
        palette: The XGen palette/collection name.
        description: The XGen description name.

    Returns:
        bool: True if description is visible, False if hidden.
    """
    try:
        # The description shape node controls visibility
        # Format is typically: xgmDescription node or the description name itself
        desc_shape = description
        if cmds.objExists(desc_shape):
            visibility = cmds.getAttr(f"{desc_shape}.visibility")
            return visibility
        # Also check for lodVisibility attribute used by XGen
        lod_vis = xg.getAttr("lodVisibility", palette, description)
        if lod_vis and lod_vis.lower() == "false":
            return False
    except:
        pass
    return True


def convert_xpd_to_abc(xpd_path):
    """Convert an XPD file to Alembic format using xpd_to_abc tool.

    Args:
        xpd_path: Path to the XPD file.

    Returns:
        str: Path to the generated ABC file, or None on failure.
    """
    if not os.path.exists(XPD_TO_ABC):
        print(f"    Warning: xpd_to_abc tool not found at {XPD_TO_ABC}")
        return None

    if not os.path.exists(xpd_path):
        print(f"    Warning: XPD file not found: {xpd_path}")
        return None

    # Output ABC file has same name but .abc extension
    abc_path = os.path.splitext(xpd_path)[0] + ".abc"

    try:
        print(f"    Converting to ABC: {os.path.basename(abc_path)}")
        result = subprocess.run(
            [XPD_TO_ABC, xpd_path, abc_path],
            capture_output=True,
            text=True
        )

        if result.returncode != 0:
            print(f"    Error converting XPD: {result.stderr}")
            return None

        if os.path.exists(abc_path):
            print(f"    Converted: {abc_path}")
            return abc_path
        else:
            print(f"    Warning: ABC file not created")
            return None

    except Exception as e:
        print(f"    Error running xpd_to_abc: {e}")
        return None


def disable_all_cv_attr(palette, description):
    """Disable cvAttr on ALL clumping modifiers for a description.

    Args:
        palette: The XGen palette/collection name.
        description: The XGen description name.

    Returns:
        dict: Original cvAttr states {modifier_name: original_value}
    """
    original_states = {}
    clumping_mods = get_clumping_modifiers(palette, description)

    for modifier in clumping_mods:
        original_states[modifier] = xg.getAttr("cvAttr", palette, description, modifier)
        xg.setAttr("cvAttr", "false", palette, description, modifier)

    return original_states


def restore_cv_attr_states(palette, description, original_states):
    """Restore cvAttr to original states for all clumping modifiers.

    Args:
        palette: The XGen palette/collection name.
        description: The XGen description name.
        original_states: Dict of {modifier_name: original_value}
    """
    for modifier, value in original_states.items():
        xg.setAttr("cvAttr", value, palette, description, modifier)


def get_resolved_output_dir(palette, description):
    """Get the resolved output directory path from ${DESC}/Bake/.

    Args:
        palette: The XGen palette/collection name.
        description: The XGen description name.

    Returns:
        str: Resolved absolute path to the Bake folder.
    """
    # Get the description path and append Bake
    desc_path = xg.getAttr("xgDataPath", palette)
    if desc_path:
        # Resolve ${PROJECT} if present
        project_path = xg.getAttr("xgProjectPath", palette)
        resolved = desc_path.replace("${PROJECT}", project_path) if project_path else desc_path
        bake_path = os.path.join(resolved, description, "Bake")
        return bake_path
    return None


def export_xpd_for_modifier(palette, description, modifier):
    """Export XPD file with cvAttr enabled for a specific clumping modifier.

    Args:
        palette: The XGen palette/collection name.
        description: The XGen description name.
        modifier: The clumping modifier name.

    Returns:
        str: Path to the renamed XPD file, or None on failure.
    """
    # Save original renderer and FileRenderer settings
    orig_renderer = xg.getActive(palette, description, "Renderer")
    orig_percent = xg.getAttr("percent", palette, description, "FileRenderer")
    orig_in_camera = xg.getAttr("inCameraOnly", palette, description, "FileRenderer")
    orig_output_dir = xg.getAttr("outputDir", palette, description, "FileRenderer")

    try:
        # Enable cvAttr ONLY for this modifier (all others should already be disabled)
        print(f"    Enabling cvAttr for {modifier}...")
        xg.setAttr("cvAttr", "true", palette, description, modifier)

        # Switch to FileRenderer
        xg.setActive(palette, description, "FileRenderer")

        # Configure FileRenderer - use ${DESC}/Bake/ which XGen resolves
        xg.setAttr("percent", "100", palette, description, "FileRenderer")
        xg.setAttr("inCameraOnly", "false", palette, description, "FileRenderer")
        xg.setAttr("outputDir", "${DESC}/Bake/", palette, description, "FileRenderer")

        # Get resolved output dir for finding the files
        resolved_dir = get_resolved_output_dir(palette, description)
        print(f"    Output dir: {resolved_dir}")

        # Track existing XPD files before export
        if resolved_dir and os.path.exists(resolved_dir):
            existing_xpds = set(glob.glob(os.path.join(resolved_dir, "*.xpd")))
        else:
            existing_xpds = set()
            print(f"    Note: Output directory does not exist yet, XGen will create it")

        # Generate XPD via MEL command
        print(f"    Generating XPD...")
        cmd = 'xgmFileRender -pb {{"{}"}}'.format(description)
        result = mel.eval(cmd)

        # Find the newly created XPD file
        # Check if directory was created by xgmFileRender
        if resolved_dir and os.path.exists(resolved_dir):
            current_xpds = set(glob.glob(os.path.join(resolved_dir, "*.xpd")))
            new_xpds = current_xpds - existing_xpds

            if new_xpds:
                # Rename the XPD file to match the naming convention
                safe_palette = palette.replace(":", "_").replace("|", "_")
                safe_desc = description.replace(":", "_").replace("|", "_")
                safe_mod = modifier.replace(":", "_").replace("|", "_")

                new_filename = f"{safe_palette}_{safe_desc}_{safe_mod}.xpd"
                new_filepath = os.path.join(resolved_dir, new_filename)

                original_xpd = list(new_xpds)[0]

                if os.path.exists(new_filepath):
                    os.remove(new_filepath)
                os.rename(original_xpd, new_filepath)

                print(f"    Exported: {new_filepath}")
                return new_filepath
            else:
                print(f"    Debug: existing={len(existing_xpds)}, current={len(current_xpds)}")
        else:
            print(f"    Warning: Output directory still does not exist after render")

        print(f"    Warning: No new XPD file found after export")
        return None

    finally:
        # Disable cvAttr for this modifier (leave all disabled)
        xg.setAttr("cvAttr", "false", palette, description, modifier)

        # Restore original renderer and settings
        xg.setActive(palette, description, orig_renderer)
        xg.setAttr("percent", orig_percent, palette, description, "FileRenderer")
        xg.setAttr("inCameraOnly", orig_in_camera, palette, description, "FileRenderer")
        xg.setAttr("outputDir", orig_output_dir, palette, description, "FileRenderer")


def process_all_clumping_modifiers():
    """Main function to process all clumping modifiers in the scene.

    Returns:
        dict: Dictionary with 'xpd' and 'abc' lists of exported file paths.
    """
    exported_xpd = []
    exported_abc = []

    # List scene info
    scene_info = list_scene_xgen_info()

    if not scene_info:
        return {"xpd": exported_xpd, "abc": exported_abc}

    print("\n=== Processing Clumping Modifiers ===")

    for palette, description in scene_info:
        print(f"\nProcessing: {palette}/{description}")

        # Check if description is visible
        if not is_description_visible(palette, description):
            print("  Warning: Description is hidden, skipping...")
            continue

        # Find clumping modifiers
        clumping_mods = get_clumping_modifiers(palette, description)

        if not clumping_mods:
            print("  No clumping modifiers found.")
            continue

        print(f"  Found {len(clumping_mods)} clumping modifier(s): {', '.join(clumping_mods)}")

        # IMPORTANT: Disable ALL cvAttr first to avoid conflicts
        print("  Disabling all cvAttr attributes...")
        original_cv_states = disable_all_cv_attr(palette, description)

        try:
            # Process each clumping modifier one at a time
            for modifier in clumping_mods:
                print(f"\n  Processing modifier: {modifier}")

                xpd_path = export_xpd_for_modifier(palette, description, modifier)
                if xpd_path:
                    exported_xpd.append(xpd_path)

                    # Convert XPD to ABC
                    abc_path = convert_xpd_to_abc(xpd_path)
                    if abc_path:
                        exported_abc.append(abc_path)

        finally:
            # Restore original cvAttr states
            print("\n  Restoring original cvAttr states...")
            restore_cv_attr_states(palette, description, original_cv_states)

    # Summary
    print("\n=== Export Complete ===")
    print(f"Exported {len(exported_xpd)} XPD file(s):")
    for f in exported_xpd:
        print(f"  {f}")

    print(f"\nConverted {len(exported_abc)} ABC file(s):")
    for f in exported_abc:
        print(f"  {f}")

    return {"xpd": exported_xpd, "abc": exported_abc}


def run():
    """Entry point for the script."""
    return process_all_clumping_modifiers()


# Allow running directly in Maya Script Editor
if __name__ == "__main__":
    run()
