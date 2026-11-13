#include "TownWorld.as"
#include "TownPedestrians.as"
#include "TownFireflies.as"
class TownTitleBackdrop {
 TownState@ town=TownState();TownWorld@ world=TownWorld();TownPedestrians@ people=TownPedestrians();TownFireflies@ fireflies=TownFireflies();Scene@ scene;float clock=0;
 void Initialize(Scene@ owner){@scene=owner;@town.scene=scene;@town.rules=LoadJsonFile("Assets/Application/Data/LightTownRules.json");town.NewGame();Json@ list=Json();list.Parse("[]");int size=town.GridSize();int rowA=3+int(Random::Float(0,1.99f)),rowB=7+int(Random::Float(0,1.99f)),columnA=4+int(Random::Float(0,1.99f)),columnB=9+int(Random::Float(0,1.99f));
  for(int z=2;z<size-2;++z)for(int x=2;x<size-2;++x)if(z==rowA||z==rowB||z==11||x==columnA||x==columnB){Json@ road=Json();road.SetInt("kind",6);road.SetInt("x",x);road.SetInt("z",z);road.SetInt("rotation",0);list.PushJson(road);}
  town.SetBuildings(list);town.infrastructureDirty=true;town.EnsureInfrastructure();
  for(int z=2;z<size-2;++z)for(int x=2;x<size-2;++x){if(town.IsRoad(x,z)||!town.RoadAdjacent(x,z))continue;float roll=Random::Float(0,1);if(roll>.77f)continue;int kind=(x+z)%4==0?0:(roll<.43f?1:(roll<.57f?3:(roll<.68f?4:2)));Json@ building=Json();building.SetInt("kind",kind);building.SetInt("x",x);building.SetInt("z",z);building.SetInt("rotation",town.AutoFacing(x,z));building.SetInt("tier",kind==1&&roll<.15f?1:0);building.SetInt("recipe",(x+z)%3);list.PushJson(building);}
  town.SetBuildings(list);town.infrastructureDirty=true;ConfigureView();world.Initialize(scene,town);people.Initialize(scene,town,world);people.restored=true;fireflies.Initialize(scene,14);
  Object@ sun=scene.GetObject("SceneLight");Light@ light;if(sun !is null&&sun.GetComponent(@light))light.SetIntensity(.25f);
  UpdateCamera();
 }
 void ConfigureView(){Object@ screen=scene.GetObject("TownScreen");DepthOfFieldEffect@ dof;MotionBlurEffect@ motion;if(screen.GetComponent(@dof))dof.SetActive(false);if(screen.GetComponent(@motion))motion.SetActive(false);}
 void UpdateCamera(){
  Quaternion orientation=Math::MakeRotateEuler(Vector3(.72f,-.45f+.05f*Sin(clock*.07f),0));
  Transform@ camera=scene.GetObject("TownCamera").GetTransform();
  camera.SetRotateQuaternion(orientation);
  // カメラを視点基準で左へ寄せ、街をメニューの右側に収める。
  camera.SetTranslate(-orientation.RotateVector(Vector3(15,0,40)));
 }
 void Update(float dt){clock+=dt;UpdateCamera();world.Update(dt);people.Update(dt);fireflies.Update(dt,12000);}
}
