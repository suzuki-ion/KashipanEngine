"""Trace fixed references in Release assets; retain unresolved/dynamic dependencies.

All scenes, scripts and general data are roots (no dead-code/unused-scene pruning).
Materials are followed by path, filename or declared name. Prefabs are always
excluded from Release, including engine-owned prefabs. Apply only to a staging copy.
"""
import argparse
from collections import defaultdict, deque
import json
import os
from pathlib import Path
import posixpath
import re

TEXT_EXTENSIONS = {
    '.as', '.json', '.jsonc', '.mat', '.scene', '.hlsl', '.hlsli', '.fnt',
    '.txt', '.xml', '.ini', '.html', '.css', '.js', '.csv', '.yml', '.yaml',
}
MODEL_EXTENSIONS = {'.fbx', '.obj', '.gltf', '.glb', '.dae', '.3ds', '.blend', '.ply', '.stl', '.x'}
MEDIA_TYPES = {
    **dict.fromkeys(MODEL_EXTENSIONS, 'model'),
    **dict.fromkeys({'.png', '.jpg', '.jpeg', '.bmp', '.dds', '.tga', '.hdr', '.tif', '.tiff', '.webp', '.gif', '.ico'}, 'image'),
    **dict.fromkeys({'.mp3', '.wav', '.ogg', '.flac', '.aac', '.m4a', '.wma'}, 'audio'),
    **dict.fromkeys({'.mp4', '.webm', '.avi', '.wmv', '.mov'}, 'video'),
    **dict.fromkeys({'.ttf', '.otf', '.fnt'}, 'font'),
    '.mat': 'material', '.bin': 'modelDependency', '.mtl': 'modelDependency',
}
# 名前ごとにアセットの種類とパス引数の位置（0 始まり）を対応付ける。変数や定数は評価しない。
SCRIPT_CALLS = {
    **dict.fromkeys(('PlayAudio', 'SetSoundName', 'GetSoundHandleFromAssetPath', 'GetSoundHandleFromFileName'), ('audio', 0)),
    **dict.fromkeys(('GetModelHandleFromAssetPath', 'GetModelHandleFromFileName', 'SetAnimationSourceAssetPath', 'GetSkeletonHandleFromAssetPath', 'GetAnimationHandleFromAssetPath'), ('model', 0)),
    **dict.fromkeys(('SetMaterialName', 'GetMaterialHandleFromName', 'GetMaterialHandleFromAssetPath'), ('material', 0)),
    'SetMaterialNameAt': ('material', 1),
    **dict.fromkeys(('SetTextureAssetPath', 'SetGifAssetPath', 'GetTextureFromAssetPath', 'GetTextureFromFileName', 'SetBaseTexturePath', 'SetMaskTexturePath'), ('image', 0)),
    'SetVideoAssetPath': ('video', 0), 'SetFontName': ('font', 0),
    'LoadJsonFile': ('data', 0), 'SetScriptPath': ('script', 0),
}
KINDS = set(MEDIA_TYPES.values())


def is_prefab(relative: str) -> bool:
    return any(part.lower().endswith('.prefab') for part in relative.split('/'))


def asset_files(root: Path) -> list[Path]:
    result = []
    for directory, dirs, files in os.walk(root, followlinks=False):
        for name in [*dirs, *files]:
            path = Path(directory) / name
            if path.is_symlink() or path.is_junction():
                raise RuntimeError(f'Cannot scan linked asset path: {path}')
        result.extend(Path(directory) / name for name in files)
    return sorted(result)


def tokens(content: str):
    """Small lexer: comments never become references; quoted strings stay atomic."""
    result, i, line = [], 0, 1
    while i < len(content):
        char = content[i]
        if char.isspace():
            line += char == '\n'; i += 1
        elif content.startswith('//', i):
            end = content.find('\n', i + 2)
            i = len(content) if end < 0 else end
        elif content.startswith('/*', i):
            end = content.find('*/', i + 2)
            end = len(content) if end < 0 else end + 2
            line += content[i:end].count('\n'); i = end
        elif char in '\"\'':
            quote, start_line, value = char, line, []
            i += 1
            while i < len(content) and content[i] != quote:
                if content[i] == '\\' and i + 1 < len(content):
                    following = content[i + 1]
                    if following in ('\\', '"', "'"):
                        value.append(following); i += 2; continue
                    if following == '/':
                        value.append('/'); i += 2; continue
                    # Windows のパス区切り文字を含め、認識できないエスケープは保持する。
                    value.append('\\'); i += 1; continue
                line += content[i] == '\n'
                value.append(content[i]); i += 1
            i += i < len(content)
            result.append(('string', ''.join(value), start_line))
        elif char.isalnum() or char == '_':
            end = i + 1
            while end < len(content) and (content[end].isalnum() or content[end] == '_'):
                end += 1
            result.append(('identifier', content[i:end], line)); i = end
        else:
            result.append(('symbol', char, line)); i += 1
    return result


def json_without_comments(content: str) -> str:
    # 文字列と行番号を維持し、コメントだけを空白に置き換える。
    return re.sub(r'"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*[\s\S]*?\*/',
                  lambda match: match[0] if match[0].startswith('"') else re.sub(r'[^\r\n]', ' ', match[0]), content)


def json_strings(value, pointer=''):
    if isinstance(value, str):
        yield value, pointer
    elif isinstance(value, dict):
        for key, child in value.items():
            escaped = key.replace('~', '~0').replace('/', '~1')
            yield from json_strings(child, pointer + '/' + escaped)
    elif isinstance(value, list):
        for index, child in enumerate(value):
            yield from json_strings(child, pointer + '/' + str(index))


def call_argument(stream, index, wanted):
    """Extract one complete argument, respecting nested calls/arrays/braces."""
    arguments, current, depth = [], [], 0
    for token in stream[index + 2:]:
        value = token[1]
        if token[0] == 'symbol':
            if value in '([{':
                depth += 1
            elif value in ')]}':
                if value == ')' and depth == 0:
                    arguments.append(current)
                    return arguments[wanted] if wanted < len(arguments) else None
                depth -= 1
            elif value == ',' and depth == 0:
                arguments.append(current); current = []; continue
        current.append(token)
    return None


def scan(root: Path) -> dict:
    paths = asset_files(root)
    files = {path.relative_to(root).as_posix(): path for path in paths}
    prefabs = {name for name in files if is_prefab(name)}
    candidates = {name for name, path in files.items() if path.suffix.lower() in MEDIA_TYPES and not name.lower().startswith('kashipanengine/') and name not in prefabs}
    aliases, relative_names = defaultdict(set), {}
    contents, json_values, diagnostics = {}, {}, []
    for name, path in files.items():
        if name in prefabs:
            continue
        relative_names[name.lower()] = name
        aliases[path.name.lower()].add(name)
        aliases[path.stem.lower()].add(name)
        if path.suffix.lower() in TEXT_EXTENSIONS:
            try:
                contents[name] = path.read_text(encoding='utf-8-sig', errors='strict')
                if path.suffix.lower() in {'.json', '.jsonc', '.mat', '.scene'}:
                    json_values[name] = json.loads(json_without_comments(contents[name]))
                    if path.suffix.lower() == '.mat':
                        material_name = json_values[name].get('name', '')
                        if isinstance(material_name, str) and material_name:
                            aliases[material_name.lower()].add(name)
            except (UnicodeError, json.JSONDecodeError, AttributeError) as error:
                diagnostics.append({'source': name, 'reason': 'unreadable-or-unparsed-text', 'detail': str(error)})

    kept, reasons, queue = set(), defaultdict(list), deque()
    edges, unknown_reasons, roots = [], defaultdict(list), []
    edge_set = set()

    def retain(name, reason):
        if name in prefabs:
            return
        if reason not in reasons[name]:
            reasons[name].append(reason)
        if name not in kept:
            kept.add(name); queue.append(name)

    def uncertain(kind, source, reason, line=None):
        entry = {'source': source, 'reason': reason}
        if line is not None:
            entry['line'] = line
        if entry in unknown_reasons[kind]:
            return
        unknown_reasons[kind].append(entry)
        for name, path in files.items():
            matched = MEDIA_TYPES.get(path.suffix.lower()) == kind
            if kind == 'data':
                matched = name in contents
            if kind == 'script':
                matched = path.suffix.lower() == '.as'
            if matched:
                retain(name, {'reason': 'uncertain-dependency', 'source': source, 'kind': kind})

    def resolve(value, source):
        text = value.replace('\\', '/').strip()
        if not text or is_prefab(text):
            return set()
        lower = text.lower()
        if lower.startswith('./'):
            lower = lower[2:]
        if lower.startswith('assets/'):
            lower = lower[7:]
        # インクルードやモデル・マテリアルのメタデータでは、参照元ファイルからの相対パスを使う場合がある。
        options = [posixpath.normpath(lower), posixpath.normpath(posixpath.join(posixpath.dirname(source.lower()), lower))]
        matches = {relative_names[option] for option in options if option in relative_names}
        # .scene ディレクトリと旧形式の単一ファイルのシーンの両方を対象とする。
        if lower.endswith('.scene'):
            for option in options:
                matches.update(name for name in files if name.lower().startswith(option + '/'))
        # マネージャーは名前とパスの両方を受け付ける。複数の候補がある場合はすべて保持する。
        matches.update(aliases.get(lower, ()))
        matches.update(aliases.get(posixpath.basename(lower), ()))
        return matches - prefabs

    def reference(value, source, location):
        matches = resolve(value, source)
        for target in sorted(matches):
            identity = (source, target, location)
            if identity not in edge_set:
                edge_set.add(identity)
                edges.append({'source': source, 'target': target, 'location': location, 'value': value})
            retain(target, {'reason': 'fixed-reference', 'source': source, 'location': location})
        return matches

    # シーン、スクリプト、一般データはすべて依存関係の起点として残す。
    # この段階ではネイティブ登録や未使用コードは扱わず、マテリアルと対応メディアのみ削減する。
    for name, path in files.items():
        if name in prefabs:
            continue
        if name.lower().startswith('kashipanengine/') or (name in contents and path.suffix.lower() != '.mat') or name not in candidates or path.suffix.lower() == '.bin':
            roots.append(name)
            retain(name, {'reason': 'engine-root' if name.lower().startswith('kashipanengine/') else 'conservative-root'})
    for diagnostic in diagnostics:
        for kind in KINDS:
            uncertain(kind, diagnostic['source'], diagnostic['reason'])
    while queue:
        source = queue.popleft()
        path = files[source]
        if path.suffix.lower() in MODEL_EXTENSIONS:
            # この段階では、モデル内部のパスやマテリアル名を意図的に解析しない。
            # 外部依存先の可能性があるファイルは、依存関係をたどって再帰的に保持する。
            for kind in ('image', 'material', 'modelDependency'):
                uncertain(kind, source, 'model-internal-dependencies-not-analysed')
        if source not in contents:
            continue
        if source in json_values:
            for value, pointer in json_strings(json_values[source]):
                matches = reference(value, source, 'json:' + pointer)
                field = pointer.rsplit('/', 1)[-1].lower()
                field_kinds = {'meshassetpath': 'model', 'animationassetpath': 'model', 'animationsourceassetpath': 'model',
                               'skeletonassetpath': 'model', 'texturefile': 'image', 'textureassetpath': 'image',
                               'gifassetpath': 'image', 'videoassetpath': 'video', 'materialname': 'material',
                               'soundname': 'audio', 'fontname': 'font', 'scriptpath': 'script'}
                asset_kind = field_kinds.get(field, MEDIA_TYPES.get(Path(value).suffix.lower()))
                builtin = value.startswith('PrimitiveMesh-') or (asset_kind == 'material' and value == 'Default')
                if value and not matches and asset_kind and not builtin and not is_prefab(value.replace('\\', '/')):
                    uncertain(asset_kind, source, 'unresolved-json-reference:' + pointer)
        else:
            stream = tokens(contents[source])
            for index, token in enumerate(stream):
                kind, value, line = token
                if kind == 'string':
                    matches = reference(value, source, 'line:' + str(line))
                    media_kind = MEDIA_TYPES.get(Path(value).suffix.lower())
                    if value and not matches and media_kind and not is_prefab(value.replace('\\', '/')):
                        uncertain(media_kind, source, 'unresolved-file-literal', line)
                    # 断片の末尾が対応拡張子（例: "Surface.obj"）であっても、
                    # 連結した文字列全体の値は解決しない。
                    adjacent = [stream[index - 1][1] if index else '', stream[index + 1][1] if index + 1 < len(stream) else '']
                    media_kind = MEDIA_TYPES.get(Path(value).suffix.lower())
                    if path.suffix.lower() == '.as' and '+' in adjacent and media_kind:
                        uncertain(media_kind, source, 'constructed-asset-path', line)
                if path.suffix.lower() != '.as' or kind != 'identifier' or index + 1 >= len(stream) or stream[index + 1][1] != '(':
                    continue
                call = SCRIPT_CALLS.get(value)
                if call is None and re.fullmatch(r'(?:Get|Set)\w*(?:Texture|Gif)\w*', value):
                    call = ('image', 0)
                if call is None:
                    continue
                asset_kind, argument_index = call
                argument = call_argument(stream, index, argument_index)
                if argument is None:
                    uncertain(asset_kind, source, 'unparsed-call:' + value, line)
                elif len(argument) != 1 or argument[0][0] != 'string':
                    uncertain(asset_kind, source, 'non-literal-call:' + value, line)
                else:
                    literal = argument[0][1]
                    matches = reference(literal, source, 'line:' + str(line))
                    # 固定の別名が見つからない場合は、ネイティブ側で登録したものや手続き的に生成したものの可能性がある。
                    # 組み込みのプリミティブメッシュと既定のマテリアルには対応するファイルがない。
                    builtin = literal.startswith('PrimitiveMesh-') or (asset_kind == 'material' and literal == 'Default')
                    external_data = asset_kind == 'data' and not literal.replace('\\', '/').lower().startswith('assets/')
                    if literal and not matches and not builtin and not external_data:
                        uncertain(asset_kind, source, 'unresolved-literal-call:' + value, line)

    excluded = sorted(prefabs | (candidates - kept))
    retained_unknown = sorted(name for name in candidates & kept if any(reason['reason'] == 'uncertain-dependency' for reason in reasons[name]))
    details = [{'path': name, 'bytes': files[name].stat().st_size, 'kind': 'prefab' if name in prefabs else MEDIA_TYPES[files[name].suffix.lower()],
                'reason': 'prefabs-not-supported-in-release' if name in prefabs else 'no-fixed-reference-and-no-uncertain-dependency'} for name in excluded]
    return {
        'version': 2, 'scannedFiles': len(files), 'scannedTextFiles': len(contents),
        'policy': 'all-scenes-and-scripts-are-roots; fixed-references-only; uncertain-types-retained; release-prefabs-excluded',
        'roots': sorted(roots), 'dynamicMediaTypes': sorted(unknown_reasons),
        'uncertainDependencies': dict(sorted(unknown_reasons.items())), 'diagnostics': diagnostics,
        'referenceEdges': edges, 'retentionReasons': {name: reasons[name] for name in sorted(kept)},
        'excluded': excluded, 'excludedDetails': details, 'excludedBytes': sum(entry['bytes'] for entry in details),
        'retainedUnknown': retained_unknown,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets-root', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    if args.assets_root.is_symlink() or args.assets_root.is_junction():
        parser.error('--assets-root must not be a linked directory')
    root = args.assets_root.resolve(strict=True)
    if not root.is_dir():
        parser.error('--assets-root must be a directory')
    result = scan(root)
    if args.apply:
        for relative in result['excluded']:
            target = root / relative
            if not target.resolve().is_relative_to(root) or target.is_symlink():
                raise RuntimeError(f'Unsafe asset path: {relative}')
            target.unlink()
        # プレハブがディレクトリ形式の場合もある。中身が空になったディレクトリ構造は削除する。
        for directory, _, _ in os.walk(root, topdown=False, followlinks=False):
            path = Path(directory)
            if is_prefab(path.relative_to(root).as_posix()) and not any(path.iterdir()):
                if not path.resolve().is_relative_to(root):
                    raise RuntimeError(f'Unsafe prefab directory: {path}')
                path.rmdir()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"Asset scan: {result['scannedFiles']} files, {len(result['excluded'])} excluded "
          f"({result['excludedBytes'] / 1048576:.2f} MiB), {len(result['retainedUnknown'])} retained as unknown")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
