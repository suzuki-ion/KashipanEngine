import os
import uuid
import xml.etree.ElementTree as ET

MSBUILD_NS = "http://schemas.microsoft.com/developer/msbuild/2003"
NS = {"msb": MSBUILD_NS}


def _norm_folder(folder: str) -> str:
    folder = folder.replace("/", "\\")
    folder = folder.strip("\\")
    return folder


def _iter_items(root: ET.Element, tag: str):
    for ig in root.findall("msb:ItemGroup", NS):
        for item in ig.findall(f"msb:{tag}", NS):
            yield item


def main() -> int:
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    filters_path = os.path.join(repo_root, "KashipanEngine.vcxproj.filters")

    ET.register_namespace("", MSBUILD_NS)

    tree = ET.parse(filters_path)
    root = tree.getroot()

    # Collect folders referenced by source/header items
    folders = set()
    for tag in ("ClCompile", "ClInclude"):
        for item in _iter_items(root, tag):
            inc = item.get("Include")
            if not inc:
                continue
            d = os.path.dirname(inc)
            d = _norm_folder(d)
            if not d:
                continue
            parts = d.split("\\")
            for i in range(1, len(parts) + 1):
                folders.add("\\".join(parts[:i]))

    # Existing filter nodes
    existing_filters = set()
    for ig in root.findall("msb:ItemGroup", NS):
        for f in ig.findall("msb:Filter", NS):
            inc = f.get("Include")
            if inc:
                existing_filters.add(inc)

    # Ensure there is an ItemGroup containing Filter nodes (use first if present)
    filter_ig = None
    for ig in root.findall("msb:ItemGroup", NS):
        if ig.find("msb:Filter", NS) is not None:
            filter_ig = ig
            break
    if filter_ig is None:
        filter_ig = ET.SubElement(root, f"{{{MSBUILD_NS}}}ItemGroup")

    # Add missing folder filters
    added = 0
    for folder in sorted(folders):
        if folder in existing_filters:
            continue
        fe = ET.SubElement(filter_ig, f"{{{MSBUILD_NS}}}Filter", Include=folder)
        uid = ET.SubElement(fe, f"{{{MSBUILD_NS}}}UniqueIdentifier")
        uid.text = "{" + str(uuid.uuid4()).upper() + "}"
        added += 1

    # Assign each item to folder filter (matching physical path)
    for tag in ("ClCompile", "ClInclude"):
        for item in _iter_items(root, tag):
            inc = item.get("Include")
            if not inc:
                continue
            d = _norm_folder(os.path.dirname(inc))
            # remove existing Filter child
            for child in list(item):
                if child.tag.endswith("Filter"):
                    item.remove(child)
            fc = ET.SubElement(item, f"{{{MSBUILD_NS}}}Filter")
            fc.text = d if d else "."

    tree.write(filters_path, encoding="utf-8", xml_declaration=True)
    print(f"Updated `{os.path.relpath(filters_path, repo_root)}`: added {added} folder filters")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
