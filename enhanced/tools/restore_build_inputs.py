"""Recover the generated attachment tables from this release's compiled plugin."""
import argparse
import json
from pathlib import Path
import struct
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("release_zip", type=Path)
    args = cli.parse_args()
    layout = json.loads((ROOT / "tools/build_inputs.json").read_text(encoding="utf-8"))
    with ZipFile(args.release_zip) as archive:
        info = archive.getinfo("romfs/nativeNX/plugins/mhw_ls_rebuild.nro")
        if info.CRC != layout["plugin_crc"] or info.file_size != layout["plugin_size"]:
            raise ValueError("Use the matching v1.2-Recomp standalone package")
        plugin = archive.read(info)
    target = ROOT / ".cache/mhw_ls_animation_port"
    target.mkdir(parents=True, exist_ok=True)
    for filename, tables in layout["headers"].items():
        lines = ["/* Generated from this release's compiled attachment tables. */"]
        if filename == "attachment_data.h":
            lines.append("typedef struct {unsigned slot,frames,offset;} MLAttachmentClip;")
        for declaration, offset, rows, fields, encoding in tables:
            row = struct.Struct(f"<{fields}{encoding}")
            lines.append(f"static const {declaration}={{")
            for index in range(rows):
                values = row.unpack_from(plugin, offset + index * row.size)
                if encoding == "f":
                    tokens = [f"{value:.9e}f" for value in values]
                else:
                    tokens = [str(value) for value in values]
                if fields == 1:
                    lines.append(tokens[0] + ",")
                elif declaration.startswith("float rise_sheathe"):
                    lines.append("{{" + ",".join(tokens[:7]) + "},{" + ",".join(tokens[7:]) + "}},")
                else:
                    lines.append("{" + ",".join(tokens) + "},")
            lines.append("};")
        (target / filename).write_text("\n".join(lines) + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
