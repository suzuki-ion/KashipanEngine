"""Validate checked-in build paths and the separation between source and runtime data."""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}

class RepositoryLayoutTests(unittest.TestCase):
    def test_solution_project_paths(self):
        solution = ROOT / 'Build/KashipanEngine.sln'
        for relative in re.findall(r'^Project\([^\n]+? = "[^"]+", "([^"]+\.vcxproj)"', solution.read_text(encoding='utf-8-sig'), re.M):
            with self.subTest(project=relative):
                self.assertTrue((solution.parent / relative.replace('\\', '/')).is_file())

    def test_project_items_exist(self):
        for project in (ROOT / 'Build').glob('*.vcxproj'):
            tree = ET.parse(project)
            for kind in ['ClCompile', 'ClInclude', 'None', 'Text', 'ProjectReference', 'Manifest', 'ResourceCompile', 'CustomBuild', 'Image', 'FxCompile']:
                for node in tree.findall('.//m:' + kind, NS):
                    relative = node.get('Include')
                    if not relative or '$(' in relative: continue
                    with self.subTest(project=project.name, item=relative):
                        self.assertTrue((project.parent / relative.replace('\\', '/')).is_file())

    def test_runtime_roots(self):
        for relative in ['EngineData/AssetsTemplate/Default', 'EngineData/AssetsTemplate/Empty',
                         'EngineData/EditorTools', 'EngineData/Locales', 'EngineData/UI/Splash',
                         'EngineData/UI/Launcher', 'GameProjects', 'Docs/Reference', 'EngineTools/Profiler']:
            with self.subTest(directory=relative):
                self.assertTrue((ROOT / relative).is_dir())
        for definition in (ROOT / 'GameProjects').glob('*/Project.json'):
            with self.subTest(game=definition.parent.name):
                self.assertTrue((definition.parent / 'Assets').is_dir())
        self.assertFalse((ROOT / 'Project').exists())
        self.assertFalse((ROOT / 'SourceFiles/KashipanEngine/Splash/UI').exists())
        self.assertFalse((ROOT / 'SourceFiles/Launcher/UI').exists())

    def test_build_commands_use_repository_root(self):
        for project in (ROOT / 'Build').glob('*.vcxproj'):
            text = project.read_text(encoding='utf-8-sig')
            self.assertIn('RepositoryPaths.props', text)
            self.assertNotIn('$(ProjectDir)', text)
            for node in ET.fromstring(text).findall('.//m:Command', NS):
                for script in re.findall(r'-File "\$\(RepositoryRoot\)([^"]+)"', node.text or ''):
                    with self.subTest(script=script):
                        self.assertTrue((ROOT / script.replace('\\', '/')).is_file())

if __name__ == '__main__':
    unittest.main()
