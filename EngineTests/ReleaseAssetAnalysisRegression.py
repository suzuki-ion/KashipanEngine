"""Fixed-reference and conservative-retention regression, without GPU/build tools."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('release_assets', REPO / 'EngineTools/AnalyzeReleaseAssets.py')
ANALYZER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ANALYZER)


class AssetAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.fixture = tempfile.TemporaryDirectory(prefix='AssetReference-', dir=REPO / 'Generated')
        self.root = Path(self.fixture.name) / 'Assets'
        self.root.mkdir()

    def tearDown(self):
        self.fixture.cleanup()

    def write(self, name, content='asset'):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding='utf-8')
        return path

    def data(self, name, value):
        return self.write(name, json.dumps(value, ensure_ascii=False))

    def scan(self):
        return ANALYZER.scan(self.root)

    def test_scene_material_name_relative_texture_and_script_include(self):
        self.data('Scenes/Main.scene/scene.json', {'materialName': '壁材', 'scriptPath': 'Assets/Scripts/Main.as'})
        self.data('Materials/renamed.mat', {'name': '壁材', 'textureFile': '../Images/日本語 壁.png'})
        self.write('Images/日本語 壁.png')
        self.write('Scripts/Main.as', '#include "nested/Helper.as"\nvoid Start(){PlayAudio("Audio/theme.mp3");}')
        self.write('Scripts/nested/Helper.as', 'void Helper() {}')
        self.write('Audio/theme.mp3')
        self.write('Images/unused.png')
        self.write('Models/unused.obj')
        result = self.scan()
        self.assertEqual(set(result['excluded']), {'Images/unused.png', 'Models/unused.obj'})
        edges = result['referenceEdges']
        self.assertTrue(any(e['source'].endswith('scene.json') and e['target'] == 'Materials/renamed.mat' for e in edges))
        self.assertTrue(any(e['source'] == 'Materials/renamed.mat' and e['target'] == 'Images/日本語 壁.png' for e in edges))
        self.assertTrue(any(e['source'] == 'Scripts/Main.as' and e['target'] == 'Scripts/nested/Helper.as' for e in edges))

    def test_unreferenced_material_does_not_keep_texture(self):
        self.data('Materials/unused.mat', {'name': 'Unused', 'textureFile': 'Images/only-unused.png'})
        self.write('Images/only-unused.png')
        self.assertEqual(set(self.scan()['excluded']), {'Materials/unused.mat', 'Images/only-unused.png'})

    def test_prefabs_override_engine_protection_and_do_not_keep_dependencies(self):
        self.data('Objects/Only.prefab', {'meshAssetPath': 'Models/only.obj'})
        self.data('KashipanEngine/Internal.prefab', {'textureFile': 'Images/only.png'})
        self.data('Objects/Directory.prefab/Objects/a/object.json', {'soundName': 'only'})
        self.write('Models/only.obj')
        self.write('Images/only.png')
        self.write('Audio/only.mp3')
        self.data('Scenes/Scene.scene/scene.json', {'prefabPath': 'Assets/Objects/Only.prefab'})
        result = self.scan()
        self.assertEqual(len(result['excluded']), 6)
        self.assertTrue(all(detail['reason'] == 'prefabs-not-supported-in-release' for detail in result['excludedDetails'] if detail['kind'] == 'prefab'))
        self.assertFalse(any('prefab' in edge['source'].lower() for edge in result['referenceEdges']))

    def test_dynamic_model_call_keeps_models_and_unknown_model_dependencies(self):
        self.write('Scripts/Main.as', 'void Start(){GetModelHandleFromAssetPath(ChooseModel(1, 2));}')
        for name in ['Models/a.obj', 'Models/b.fbx', 'Models/file.mtl', 'Models/file.bin', 'Images/external.png']:
            self.write(name)
        self.data('Materials/from-model.mat', {'name': 'Imported'})
        result = self.scan()
        self.assertEqual(result['excluded'], [])
        self.assertIn('model', result['dynamicMediaTypes'])
        self.assertIn('image', result['dynamicMediaTypes'])
        self.assertIn('Models/a.obj', result['retainedUnknown'])

    def test_literal_model_keeps_opaque_dependencies_but_not_other_models(self):
        self.data('Scenes/Main.scene', {'meshAssetPath': 'Models/used.obj'})
        self.write('Models/used.obj')
        self.write('Models/unused.obj')
        self.write('Images/external.png')
        self.assertEqual(self.scan()['excluded'], ['Models/unused.obj'])

    def test_second_argument_dynamic_material_and_custom_texture_setter(self):
        self.write('Scripts/Main.as', 'void Start(){r.SetMaterialNameAt(0, Lookup(1, 2));r.SetCustomTexture(texture);}')
        self.data('Materials/a.mat', {'name': 'A'})
        self.write('Images/a.png')
        result = self.scan()
        self.assertEqual(result['excluded'], [])
        self.assertTrue(any(item.get('line') == 1 for item in result['uncertainDependencies']['material']))

    def test_comments_do_not_keep_assets_and_json_comments_are_supported(self):
        self.write('Scripts/Main.as', '// PlayAudio(name); "unused.mp3"\n/* GetModelHandleFromAssetPath(variable); */\nstring message="https://example.test";')
        self.write('Scenes/Main.scene', '{/*comment*/"soundName":"used.mp3"//tail\n}')
        self.write('Audio/used.mp3')
        self.write('Audio/unused.mp3')
        self.write('Models/unused.obj')
        result = self.scan()
        self.assertEqual(set(result['excluded']), {'Audio/unused.mp3', 'Models/unused.obj'})
        self.assertNotIn('audio', result['dynamicMediaTypes'])

    def test_suffix_construction_unresolved_json_and_duplicate_names(self):
        self.write('Scripts/Main.as', 'string path="Road" + mask + "Surface.obj";')
        self.write('Models/a.obj')
        self.data('Scenes/Main.scene', {'textureAssetPath': '${name}.png', 'materialName': 'Duplicate'})
        for name in ['A/one.mat', 'B/two.mat']:
            self.data(name, {'name': 'Duplicate'})
        self.write('Images/a.png')
        self.assertEqual(self.scan()['excluded'], [])

    def test_invalid_json_keeps_candidates(self):
        self.write('Scenes/Main.scene', '{broken')
        self.write('Images/a.png')
        self.write('Models/a.obj')
        result = self.scan()
        self.assertEqual(result['excluded'], [])
        self.assertEqual(result['diagnostics'][0]['source'], 'Scenes/Main.scene')

    def test_unknown_binary_engine_assets_and_builtin_names(self):
        self.write('Unknown/data.bin')
        self.write('Unknown/data.xyz')
        self.write('KashipanEngine/white1x1.png')
        self.write('Scripts/Main.as', 'void Start(){GetModelHandleFromAssetPath("PrimitiveMesh-Box");r.SetMaterialName("Default");}')
        self.write('Models/unused.obj')
        self.assertEqual(self.scan()['excluded'], ['Models/unused.obj'])

    def test_apply_only_removes_reported_files_and_prefab_directories(self):
        self.write('Application/Unused.prefab/Objects/a/object.json', '{}')
        self.write('Application/Unused.prefab/texture.png')
        self.write('Application/unused.obj')
        self.write('Scripts/Keep.as', 'void Keep() {}')
        report = self.root.parent / 'Report.json'
        result = subprocess.run([sys.executable, str(REPO / 'EngineTools/AnalyzeReleaseAssets.py'), '--assets-root', str(self.root), '--report', str(report), '--apply'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.root / 'Application/Unused.prefab').exists())
        self.assertFalse((self.root / 'Application/unused.obj').exists())
        self.assertTrue((self.root / 'Scripts/Keep.as').is_file())
        self.assertEqual(len(json.loads(report.read_text(encoding='utf-8'))['excluded']), 3)


if __name__ == '__main__':
    unittest.main(verbosity=2)
