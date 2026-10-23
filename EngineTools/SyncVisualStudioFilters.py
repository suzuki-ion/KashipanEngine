"""Mirror repository-relative directories for the engine's registered project items."""
import argparse
from collections import defaultdict
from pathlib import Path
import uuid
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'Build/KashipanEngine.vcxproj'
NAMESPACE = 'http://schemas.microsoft.com/developer/msbuild/2003'
NS = {'m': NAMESPACE}
FILE_TYPES = {'ClCompile', 'ClInclude', 'None', 'Text', 'Manifest',
              'ResourceCompile', 'CustomBuild', 'Image', 'FxCompile', 'Natvis'}


def generate():
    items = defaultdict(list)
    directories = set()
    for group in ET.parse(PROJECT).getroot().findall('m:ItemGroup', NS):
        for node in group:
            kind = node.tag.rsplit('}', 1)[-1]
            include = node.get('Include')
            if kind not in FILE_TYPES or include is None:
                continue
            # Resolve against the project, then display relative to the repository.
            relative = (PROJECT.parent / include.replace('\\', '/')).resolve().relative_to(ROOT)
            directory = relative.parent
            while directory != Path('.'):
                directories.add(str(directory).replace('/', '\\'))
                directory = directory.parent
            items[kind].append((include, str(relative.parent).replace('/', '\\')))

    ET.register_namespace('', NAMESPACE)
    result = ET.Element('Project', {'ToolsVersion': '4.0', 'xmlns': NAMESPACE})
    filters = ET.SubElement(result, 'ItemGroup')
    for directory in sorted(directories, key=str.casefold):
        node = ET.SubElement(filters, 'Filter', Include=directory)
        identifier = uuid.uuid5(uuid.NAMESPACE_URL, 'KashipanEngine/filters/' + directory)
        ET.SubElement(node, 'UniqueIdentifier').text = '{' + str(identifier).upper() + '}'
    for kind in sorted(items):
        group = ET.SubElement(result, 'ItemGroup')
        for include, directory in sorted(items[kind], key=lambda item: item[0].casefold()):
            node = ET.SubElement(group, kind, Include=include)
            if directory != '.':
                ET.SubElement(node, 'Filter').text = directory
    ET.indent(result, space='  ')
    return '<?xml version="1.0" encoding="utf-8"?>\n' + ET.tostring(result, encoding='unicode') + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Check without writing files.')
    args = parser.parse_args()
    destination = PROJECT.with_suffix('.vcxproj.filters')
    expected = generate()
    if args.check:
        if not destination.exists() or destination.read_text(encoding='utf-8-sig') != expected:
            parser.exit(1, 'Filters are out of date. Run EngineTools/SyncVisualStudioFilters.py.\n')
        print('Visual Studio filters match the registered files and repository directories.')
    else:
        destination.write_text(expected, encoding='utf-8', newline='\r\n')
        print(f'Updated {destination.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
