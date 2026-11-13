"""Windows Release regression runner; requires an initialized D3D12 adapter.

python EngineTests/RunRendererOptimizationRegression.py --msbuild <MSBuild.exe>
Use --skip-engine-build only when production Release objects are already current.
"""
from pathlib import Path
import argparse,subprocess,xml.etree.ElementTree as ET,os,shutil

root=Path(__file__).resolve().parents[1]
args=argparse.ArgumentParser();args.add_argument('--msbuild',default=shutil.which('msbuild'));args.add_argument('--skip-engine-build',action='store_true');args=args.parse_args()
if not args.msbuild:raise SystemExit('Supply --msbuild with the 64-bit Visual Studio MSBuild.exe path.')
def run(command):subprocess.run(list(map(str,command)),cwd=root,check=True)
options=['/p:Configuration=Release','/p:Platform=x64','/p:PreferredToolArchitecture=x64','/m','/v:minimal']
if not args.skip_engine_build:run([args.msbuild,root/'Build/KashipanEngine.vcxproj',*options])
sdk=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Windows Kits/10/bin'
dxc=next(iter(sorted(sdk.glob('*/x64/dxc.exe'),reverse=True)),None)
if dxc is None:raise SystemExit('Windows SDK DXC not found')
shader=root/'EngineData/AssetsTemplate/Default/KashipanEngine/Pipeline/Preset/Shader/LightCulling/LightCullingCS.hlsl'
for source,target in [(shader,'LightCullingOptimized.dxil'),(root/'EngineTests/Fixtures/LightCullingReference.hlsl','LightCullingReference.dxil')]:
 run([dxc,'-T','cs_6_0','-E','main','-I',shader.parent,'-Fo',root/'Generated'/target,source])
out=root/'Generated/RendererRegression';out.mkdir(exist_ok=True)
(out/'Main.cpp').write_text('''#define main RendererOptimizationMain
#include "../../EngineTests/RendererOptimizationRegression.cpp"
#undef main
#define main LightTownGameplayMain
#include "../../EngineTests/JobHuntingLightTownRegression.cpp"
#undef main
#include "../../EngineTests/LightCullingGpuRegression.cpp"
int main(int argc,char **argv){try{int r=RendererOptimizationMain();if(r)return r;RunLightCullingGpuRegression(argv[1]);return LightTownGameplayMain(argc,argv);}catch(const std::exception &e){std::fprintf(stderr,"FAILED: %s\\n",e.what());return 1;}}
''',encoding='utf-8')
u='{http://schemas.microsoft.com/developer/msbuild/2003}';ET.register_namespace('',u[1:-1]);tree=ET.parse(root/'Build/KashipanEngine.vcxproj');project=tree.getroot();objects=[]
for group in project.findall(u+'ItemGroup'):
 for node in list(group):
  if node.tag==u+'ProjectReference':group.remove(node)
  if node.tag!=u+'ClCompile':continue
  excluded=any(e.text=='true' and 'Release|x64' in e.attrib.get('Condition','') for e in node.findall(u+'ExcludedFromBuild'))
  if not excluded:
   name=Path(node.attrib['Include'].replace('\\','/')).stem
   obj=root/'Generated/Obj/KashipanEngine/x64/Release'/node.find(u+'ObjectFileName').text.replace('$(IntDir)','').replace('\\','/')/(name+'.obj');assert obj.exists(),obj;objects.append(obj)
  group.remove(node)
for group in project.findall(u+'ItemDefinitionGroup'):
 for node in list(group):
  if node.tag in [u+'PreBuildEvent',u+'PostBuildEvent']:group.remove(node)
group=ET.SubElement(project,u+'ItemGroup');ET.SubElement(group,u+'ClCompile',Include=str(out/'Main.cpp'))
for obj in objects:ET.SubElement(group,u+'Link',Include=str(obj))
index=next(i for i,n in enumerate(project) if n.tag==u+'Import' and 'Microsoft.Cpp.targets' in n.attrib.get('Project',''))
group=ET.Element(u+'PropertyGroup')
for key,value in {'OutDir':str(out)+'\\','IntDir':str(out/'Obj')+'\\','TargetName':'Regression'}.items():ET.SubElement(group,u+key).text=value
project.insert(index,group);group=ET.Element(u+'ItemDefinitionGroup');link=ET.SubElement(group,u+'Link');ET.SubElement(link,u+'SubSystem').text='Console';ET.SubElement(link,u+'AdditionalDependencies').text=str(root/'Generated/Outputs/x64/Release/DirectXTex.lib')+';'+str(root/'Externals/angelscript/lib/angelscript64.lib')+';%(AdditionalDependencies)';project.insert(index+1,group)
path=root/'Build/RendererOptimizationRegression.generated.vcxproj';tree.write(path,encoding='utf-8',xml_declaration=True)
run([args.msbuild,path,*options]);run([out/'Regression.exe',root,out/'Scratch'])
