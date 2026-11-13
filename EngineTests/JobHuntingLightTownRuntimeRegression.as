// Run in an isolated initialized CityScene, after the production controller starts.
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownFireflies.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownFacilityIndicators.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownPedestrians.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownCityHUD.as"

class LightTownRuntimeRegression : ScriptComponentBehavior {
    float elapsed=0;bool done=false,completed=false;int verifiedPoolSize=0,framesAfter=0;
    void Check(bool passed,const string &in message){if(!passed){Json@ result=Json();result.SetBool("passed",false);result.SetString("error",message);SaveJsonFile("Results/ScriptRuntimeRegression.json",result);throw(message);}}
    void Verify(TownFireflies@ swarm,int count){
        Check(swarm.batchLights.GetPointInstanceCount()==uint(count),"batched light count");
        Check(swarm.batchMarkers.GetWorldInstanceCount()==uint(count*2),"batched marker count");
        Check(!swarm.batchMarkers.GetCastShadows(),"billboard batch is shadowless");
        for(uint i=0;i<uint(count);++i){
            Vector3 position=swarm.Position(i);
            Check((swarm.batchLights.GetPointInstancePosition(i)-position).Length()<.0001f,"light follows procedural path");
            Check((swarm.batchMarkers.GetWorldInstancePosition(i*2)-position).Length()<.0001f&&(swarm.batchMarkers.GetWorldInstancePosition(i*2+1)-position).Length()<.0001f,"interleaved markers follow the light");
            float pulse=swarm.Pulse(i);
            Check(Abs(swarm.commonLight.GetRadius()-2.2f)<.0001f,"unchanged light radius");
            Check(Abs(swarm.batchLights.GetPointInstanceIntensity(i)-(.024f+pulse*.024f)*swarm.intensityScale)<.00001f&&Abs(swarm.batchMarkers.GetWorldInstanceColor(i*2).w-(.1f+pulse*.12f))<.00001f,"unchanged intensity and marker pulse");
        }
    }
    void VerifyFacing(){
        Check(TownPopulationTarget(5,10,2,0)==6&&TownPopulationTarget(5,10,2,3)==12&&TownPopulationTarget(5,10,0,3)==0,"businesses add pedestrians, houses remain their origin");
        for(int mask=0;mask<16;++mask)for(int detail=0;detail<2;++detail)Check(GetModelHandleFromAssetPath(TownRoadMesh(mask,detail==1))!=0,"all road connection meshes are loaded");

        TownState orientation;Check(orientation.Initialize(GetScene()),"facing state");
        for(int rotation=0;rotation<4;++rotation){
            orientation.NewGame();orientation.AddResources(1000,1000,1000);
            Check(orientation.Build(6,5,4)&&orientation.Build(6,6,5)&&orientation.Build(6,5,6)&&orientation.Build(6,4,5),"four entrance roads");
            Check(orientation.Build(0,4,4,1)&&orientation.Build(1,5,5,rotation),"oriented lamp and house");
            Check(orientation.IsWorking(5,5),"matching front road activates house");
            int expected=(5+TownFrontZ(rotation))*orientation.GridSize()+5+TownFrontX(rotation);
            TownPedestrians paths;@paths.state=orientation;Check(paths.DoorRoad(5*orientation.GridSize()+5)==expected,"citizens use the front entrance road");
            Vector3 front=Math::MakeRotateEuler(Vector3(0,TownBuildingYaw(rotation),0)).RotateVector(Vector3(0,0,1));
            Check((front-Vector3(float(TownFrontX(rotation)),0,float(TownFrontZ(rotation)))).Length()<.001f,"visible model front matches road direction");
            Check(orientation.RemoveBuilding(5+TownFrontX(rotation),5+TownFrontZ(rotation)),"remove front road");
            Check(!orientation.IsWorking(5,5)&&orientation.RoadAdjacent(5,5)&&paths.DoorRoad(5*orientation.GridSize()+5)<0,"side/rear roads cannot activate or serve as entrance");
            Check(orientation.OperationProblem(1,5,5,rotation)==TownTr("入口が道路を向いていません","Entrance does not face a road"),"preview reports wrong facing");
            TownWorld world;TownFacilityIndicators effects;effects.Initialize(GetScene(),orientation,world);effects.Placed(1,5,5);
            Check(effects.forecasts[0].ui.labels[effects.forecasts[0].warningStart].GetText()==orientation.OperationProblem(1,5,5,rotation),"placement billboard warns in warning section");
            orientation.Save();TownState loaded;Check(loaded.Initialize(GetScene())&&loaded.BuildingAt(5,5).GetInt("rotation")==rotation&&!loaded.IsWorking(5,5),"fixed facing persists on reload");
        }
        orientation.NewGame();orientation.AddResources(1000,1000,1000);orientation.Build(6,8,5);
        Check(orientation.AutoFacing(5,5)==1,"nearby non-adjacent road attracts the entrance");orientation.Build(6,5,6);
        Check(orientation.AutoFacing(5,5)==2,"adjacent road wins over distant road");orientation.Build(6,6,5);
        Check(orientation.AutoFacing(5,5,1)==1&&orientation.AutoFacing(5,5,2)==2,"equally close roads keep preferred facing");
        // A rotated-away lamp must not illuminate cells or keep other facilities running.
        orientation.NewGame();orientation.AddResources(1000,1000,1000);orientation.Build(6,5,4);orientation.Build(6,6,5);orientation.Build(0,5,5,2);
        Check(!orientation.IsWorking(5,5)&&!orientation.IsLit(6,5),"misoriented streetlight provides no energy or coverage");
        Json@ legacy=Json();legacy.Parse("[{\"kind\":6,\"x\":5,\"z\":4},{\"kind\":0,\"x\":5,\"z\":5}]");orientation.SetBuildings(legacy);orientation.Publish();TownState migrated;
        Check(migrated.Initialize(GetScene())&&migrated.BuildingAt(5,5).Has("rotation")&&migrated.BuildingAt(5,5).GetInt("rotation")==0&&migrated.IsWorking(5,5),"legacy save aligns and records existing road frontage");
    }
    void Run(){
        VerifyFacing();
        TownFireflies orbit;orbit.extent=13.875f;bool outside=false;
        for(int t=0;t<3;++t){orbit.clock=float(t)*7.3f;for(uint i=0;i<2048;++i){Vector3 p=orbit.Position(i);Check(Abs(p.x)<=orbit.extent*1.6f&&Abs(p.z)<=orbit.extent*1.6f&&p.y>=.77f&&p.y<=2.33f,"geometric orbit stays bounded");if(Abs(p.x)>12||Abs(p.z)>12)outside=true;}}
        Check(outside&&orbit.PathSpeed(0)>0&&orbit.PathSpeed(1)<0,"outer tracks extend beyond town and alternate directions");
        TownFireflies swarm;swarm.Initialize(GetScene(),10);Quaternion original=swarm.camera.GetTransform().GetRotateQuaternion();
        swarm.Update(.125f,3);Verify(swarm,3);swarm.Update(.125f,1);Verify(swarm,1);swarm.Update(.125f,0);Verify(swarm,0);
        swarm.camera.GetTransform().SetRotateQuaternion(Math::MakeRotateEuler(Vector3(.2f,.7f,0)));
        swarm.Update(.125f,0);swarm.Update(.125f,3);Verify(swarm,3);
        swarm.Update(.125f,5);Verify(swarm,5);Check(swarm.phases.length()==5,"motion cache grows after reuse");
        swarm.camera.GetTransform().SetRotateQuaternion(original);swarm.Update(.125f,5);Verify(swarm,5);
        swarm.Update(.125f,0);Verify(swarm,0);

        swarm.Update(.01f,2048);Verify(swarm,2048);swarm.Update(.01f,0);Verify(swarm,0);
        swarm.Update(.01f,5);Verify(swarm,5);
        array<Vector3> invalidPositions(2);array<float> invalidValues(1);
        Check(!swarm.batchLights.SetPointInstances(invalidPositions,invalidValues),"mismatched light arrays rejected");
        Check(!swarm.batchMarkers.SetWorldBillboardPairs(invalidPositions,invalidValues,original,.4f,.1f,Vector4(1,1,1,1),Vector4(1,1,1,1)),"mismatched billboard arrays rejected");Verify(swarm,5);
        Vector3 copied=swarm.batchLights.GetPointInstancePosition(0);swarm.positions[0]=Vector3(999,999,999);
        Check((swarm.batchLights.GetPointInstancePosition(0)-copied).Length()<.0001f,"light submission owns its array data");
        Check((swarm.batchMarkers.GetWorldInstancePosition(0)-copied).Length()<.0001f,"billboard submission owns its array data");
        swarm.Update(.01f,0);
        TownState state;Check(state.Initialize(GetScene()),"indicator state");state.NewGame();state.AddResources(1000,1000,1000);
        Check(state.Build(2,7,7)&&state.Build(4,8,7,2),"indicator buildings");
        TownWorld world;@world.scene=GetScene();@world.state=state;
        for(int i=0;i<state.GridSize()*state.GridSize();++i)world.roots.insertLast(GetScene().CreateObject("RuntimeTestRoot"));
        TownFacilityIndicators indicators;indicators.Initialize(GetScene(),state,world);indicators.Update(0,false,false);
        Check(indicators.bars.length()==2,"workshop and guild bars");
        world.roots[uint(7*state.GridSize()+7)].GetTransform().SetTranslate(Vector3(0,-.8f,0));indicators.Update(.1f,false,false);
        TownFacilityBar@ bar=indicators.bars[0];
        Check((bar.anchor+Vector3(0,bar.rootTransform.GetTranslate().y,0)-indicators.Anchor(7,7)).Length()<.0001f,"cached anchor follows cell rise");
        if(bar.visible){Vector2 p;Check(indicators.Project(indicators.Anchor(7,7),p),"bar projection");Check((bar.backgroundTransform.GetTranslate()-Vector3(p.x,720-p.y,0)).Length()<.001f,"bar screen coordinates");}
        indicators.Update(0,true,false);for(uint i=0;i<indicators.bars.length();++i)Check(!indicators.bars[i].visible,"hide indicators");
        indicators.Update(0,false,false);Check(state.RemoveBuilding(7,7),"delete indexed workshop");indicators.Removed(7,7);indicators.Update(0,false,false);
        Check(indicators.bars.length()==1&&indicators.bars[0].x==8,"indicator handles rebuilt after deletion");
        world.arrivalDone=true;for(int x=6;x<=10;++x)Check(state.Build(6,x,8),"pedestrian roads");
        Check(state.Build(0,9,9)&&state.Build(1,9,7)&&state.Build(1,10,7),"pedestrian homes and illumination");
        TownPedestrians people;people.Initialize(GetScene(),state,world);people.Update(0);Check(people.walkers.length()==8,"house residents plus guild visitors");
        for(uint i=0;i<people.walkers.length();++i)Check(!people.walkers[i].visible&&people.House(people.walkers[i].home),"residents start inside houses");
        Check(state.Build(3,6,9)&&state.Build(2,10,9),"new shop and workshop");people.Update(0);Check(people.walkers.length()==12,"shops and workshops increase existing pedestrian population");
        bool entered=false,visited=false,shopVisited=false,workshopVisited=false,spread=false;for(int frame=0;frame<2400;++frame){people.Update(.1f);for(uint i=0;i<people.walkers.length();++i){TownWalker@ w=people.walkers[i];if(w.phase==3)entered=true;if(w.building==7*state.GridSize()+8&&w.phase==3)visited=true;if(w.building==9*state.GridSize()+6&&w.phase==3)shopVisited=true;if(w.building==9*state.GridSize()+10&&w.phase==3)workshopVisited=true;if(i>0&&w.visible&&people.walkers[0].visible&&(w.position-people.walkers[0].position).Length()>.4f)spread=true;}}
        Check(entered&&visited&&shopVisited&&workshopVisited&&spread,"residents spread out, visit guild/shop/workshop and enter buildings");Check(TownPopulationTarget(200,256,80)==240,"population exceeds former 48-person limit");
        people.Save();TownPedestrians resumed;resumed.Initialize(GetScene(),state,world);resumed.Update(0);
        for(uint i=0;i<people.walkers.length();++i){Check(resumed.walkers[i].phase==people.walkers[i].phase&&Abs(resumed.walkers[i].progress-people.walkers[i].progress)<.001f,"scene return preserves individual travel progress");resumed.Show(resumed.walkers[i],false);}
        Check(state.RemoveBuilding(10,7),"remove a house");for(int frame=0;frame<1200;++frame)people.Update(.1f);
        for(uint i=9;i<people.walkers.length();++i)Check(!people.walkers[i].visible&&people.walkers[i].phase==0,"surplus residents enter a home before disappearing");
        Check(state.RemoveBuilding(8,8),"remove occupied road network cell");people.Update(.1f);
        for(uint i=0;i<people.walkers.length();++i){TownWalker@ w=people.walkers[i];if(w.visible)Check(people.Road(w.from)&&people.Road(w.to),"citizens rehome after roads are removed");}
        for(int x=6;x<=10;++x)if(x!=8)state.RemoveBuilding(x,8);people.Update(.1f);
        for(uint i=0;i<people.walkers.length();++i)Check(!people.walkers[i].visible,"citizens hide with no connected roads");
        TownBuildMenu buildMenu;buildMenu.Initialize(GetScene(),state);for(int kind=0;kind<6;++kind){MeshRenderer@ icon;GetScene().GetObject("TownBuildIcon"+kind+"Model").GetComponent(@icon);Vector4 expected=state.Definition(kind).GetVector4("color",Vector4(1,1,1,1));Check(Abs(icon.GetInstanceColor().x-expected.x)<.001f,"construction icon uses facility color");}
        TownCityHUD hud;hud.Initialize(GetScene(),state);Check(hud.forecastValues.length()==7,"all forecast rows initialized");
        Check(hud.forecastValues[0].GetText()==TownForecastNumber(state.ProductionDeltas()[0]/state.ProductionInterval())+TownTr("/秒","/s"),"top HUD displays per-second production on initialization");
        Check(hud.forecastValues[6].GetText()=="0"+TownTr("/秒","/s"),"fixed construction prosperity has zero periodic rate");
        Check(state.Build(0,6,7),"construction effect fixture");indicators.Placed(0,6,7);Check(indicators.forecasts.length()==1,"construction effects created");
        TownParameterForecast@ effect=indicators.forecasts[0];Check(effect.ui.labels[1].GetText().findFirst(TownTr("建築効果","construction"))>=0,"placement safety text describes one-time effect");
        state.data.SetFloat("energy",0);hud.Refresh(state);Check(hud.forecastValues[4].GetText()=="-0.3"+TownTr("/秒","/s"),"top HUD shows per-second parameter penalties");
        state.data.SetFloat("happiness",99.5f);Check(state.Build(1,7,7),"fractional capped buff fixture");indicators.Placed(1,7,7);
        Check(indicators.forecasts[1].ui.labels[0].GetText().findFirst("+0.5")>=0,"construction billboard shows actual capped fractional gain");
        verifiedPoolSize=int(swarm.phases.length());
        GetScene().DeleteObject(swarm.batchObject);Check(!IsValidObject(swarm.batchObject),"batch owner deleted");
        TownFireflies replacement;replacement.Initialize(GetScene(),10);replacement.Update(.01f,3);Verify(replacement,3);
        // Native buffers must remain valid after this local producer and its
        // script arrays go out of scope, including subsequent render frames.
        completed=true;
    }
    void Update(){
        if(completed){if(++framesAfter<15)return;completed=false;Json@ result=Json();result.SetBool("passed",true);result.SetInt("poolSize",verifiedPoolSize);SaveJsonFile("Results/ScriptRuntimeRegression.json",result);return;}
        elapsed+=GetDeltaTime();if(done||elapsed<5)return;done=true;Run();
    }
}
