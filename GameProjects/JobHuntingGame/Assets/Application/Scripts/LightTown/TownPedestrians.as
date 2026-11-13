#include "TownWorld.as"

int TownPopulationTarget(int facilities,int roads,int homes,int workplaces=0){return facilities>0&&roads>0&&homes>0?homes*3+workplaces*2:0;}
// The expedition map uses one movable marker; city pedestrians remain batched.
Object@ TownCreateCitizen(Scene@ scene,const string &in name,const Vector4 &in color){
    Object@ root=scene.CreateObject(name);
    Object@ body=TownMesh(scene,"TownCitizenBody","PrimitiveMesh-Cone",Vector3(0,.30f,0),Vector3(.32f,.5f,.32f),color);
    Object@ head=TownMesh(scene,"TownCitizenHead","PrimitiveMesh-UVSphere",Vector3(0,.61f,0),Vector3(.25f,.25f,.25f),Vector4(1,.78f,.58f,1));
    body.GetTransform().SetParentObject(root);head.GetTransform().SetParentObject(root);
    MeshRenderer@ renderer;body.GetComponent(@renderer);renderer.SetCastShadows(false);head.GetComponent(@renderer);renderer.SetCastShadows(false);
    return root;
}
// Homes supply residents by tier; operating workshop/shop/guild facilities add visitors.
// Everyone still leaves from and returns to a house; no fixed population cap.
class TownWalker {
    Vector3 position;float scale=1;int home=-1,building=-1,from=-1,to=-1,previous=-1,steps=0,phase=0;
    float progress=0,speed=.5f,wait=0;bool visible=false,returning=false,retiring=false;
    array<int>@ route=array<int>();uint routeIndex=0;
}
class TownPedestrians {
    Scene@ scene;TownState@ state;TownWorld@ world;array<TownWalker@>@ walkers=array<TownWalker@>();
    array<int>@ roads=array<int>();array<int>@ homes=array<int>();array<int>@ destinations=array<int>();
    int residents=0;uint revision=0xffffffff;float clock=0;bool restored=false;
    MeshRenderer@ bodies;MeshRenderer@ heads;
    array<Vector3>@ bodyPositions=array<Vector3>(),headPositions=array<Vector3>(),bodyScales=array<Vector3>(),headScales=array<Vector3>();
    array<Quaternion>@ rotations=array<Quaternion>();array<Vector4>@ bodyColors=array<Vector4>(),headColors=array<Vector4>();
    void Initialize(Scene@ owner,TownState@ data,TownWorld@ town){@scene=owner;@state=data;@world=town;
        Object@ body=TownMesh(scene,"TownCitizenBodyBatch","PrimitiveMesh-Cone",Vector3(),Vector3(1,1,1),Vector4(1,1,1,1));
        Object@ head=TownMesh(scene,"TownCitizenHeadBatch","PrimitiveMesh-UVSphere",Vector3(),Vector3(1,1,1),Vector4(1,1,1,1));
        body.GetComponent(@bodies);head.GetComponent(@heads);bodies.SetCastShadows(false);heads.SetCastShadows(false);Submit();
    }
    void Submit(){
        uint count=0;for(uint i=0;i<walkers.length();++i)if(walkers[i].visible&&walkers[i].scale>0)++count;
        bodyPositions.resize(count);headPositions.resize(count);bodyScales.resize(count);headScales.resize(count);rotations.resize(count);bodyColors.resize(count);headColors.resize(count);
        uint item=0;for(uint i=0;i<walkers.length();++i){TownWalker@ w=walkers[i];if(!w.visible||w.scale<=0)continue;float k=w.scale;
            bodyPositions[item]=w.position+Vector3(0,.30f*k,0);headPositions[item]=w.position+Vector3(0,.61f*k,0);
            bodyScales[item]=Vector3(.32f*k,.5f*k,.32f*k);headScales[item]=Vector3(.25f*k,.25f*k,.25f*k);
            // Both meshes are rotationally symmetric and have solid colors.
            rotations[item]=Quaternion(0,0,0,1);bodyColors[item]=i%3==0?Vector4(.3f,.72f,1,1):(i%3==1?Vector4(1,.45f,.3f,1):Vector4(.7f,.5f,1,1));headColors[item]=Vector4(1,.78f,.58f,1);++item;
        }
        bodies.SetWorldMeshInstances(bodyPositions,rotations,bodyScales,bodyColors);heads.SetWorldMeshInstances(headPositions,rotations,headScales,headColors);
    }
    array<Vector3>@ cellCenters=array<Vector3>();array<int>@ doorRoads=array<int>();
    Vector3 Cell(int index){return index>=0&&index<int(cellCenters.length())?cellCenters[uint(index)]:Vector3();}
    bool Road(int index){return index>=0&&state.IsRoad(index%state.GridSize(),index/state.GridSize());}
    array<int>@ Neighbors(int index){
        array<int>@ result=array<int>();int size=state.GridSize(),x=index%size,z=index/size;
        if(state.IsRoad(x-1,z))result.insertLast(index-1);if(state.IsRoad(x+1,z))result.insertLast(index+1);
        if(state.IsRoad(x,z-1))result.insertLast(index-size);if(state.IsRoad(x,z+1))result.insertLast(index+size);return result;
    }
    float Variation(uint id,uint salt){uint n=(id+1)*747796405+salt*2891336453;n=(n^(n>>16))*2246822519;return float(n&65535)/65535;}
    int Next(int from,int previous,uint id,int steps){array<int>@ options=Neighbors(from);if(options.length()>1)for(int i=int(options.length())-1;i>=0;--i)if(options[uint(i)]==previous)options.removeAt(uint(i));return options.length()==0?from:options[uint(Variation(id,uint(steps))*float(options.length()))%options.length()];}
    int DoorRoad(int building){return building>=0&&building<int(doorRoads.length())?doorRoads[uint(building)]:-1;}
    void Show(TownWalker@ w,bool show){w.visible=show;}
    bool House(int cell){for(uint i=0;i<homes.length();++i)if(homes[i]==cell)return true;return false;}
    bool Destination(int cell){for(uint i=0;i<destinations.length();++i)if(destinations[i]==cell)return true;return false;}
    void Indoor(TownWalker@ w,uint id){w.phase=0;w.building=w.home;w.from=DoorRoad(w.home);w.to=w.from;w.progress=0;w.wait=1+Variation(id,uint(w.steps+11))*12;w.route.resize(0);Show(w,false);}
    void Refresh(){
        state.EnsureInfrastructure();if(revision==state.infrastructureVersion)return;revision=state.infrastructureVersion;roads.resize(0);homes.resize(0);destinations.resize(0);residents=0;
        int size=state.GridSize();cellCenters.resize(uint(size*size));doorRoads.resize(uint(size*size));for(int z=0;z<size;++z)for(int x=0;x<size;++x){uint cell=uint(z*size+x);Json@ b=state.BuildingAt(x,z);cellCenters[cell]=b !is null&&b.GetInt("kind")!=6?world.Center(b):world.Cell(x,z);doorRoads[cell]=b !is null&&b.GetInt("kind")!=6?state.EntranceRoad(x,z):-1;}for(int z=0;z<size;++z)for(int x=0;x<size;++x)if(state.IsRoad(x,z))roads.insertLast(z*size+x);
        Json@ buildings=state.Buildings();for(uint i=0;i<buildings.Size();++i){Json@ b=buildings.At(i);int x=int(b.GetInt("x")),z=int(b.GetInt("z")),kind=int(b.GetInt("kind")),cell=z*size+x;
            if(!state.IsWorking(x,z)||DoorRoad(cell)<0)continue;if(kind==1){homes.insertLast(cell);residents+=state.Residents(b);}else if(kind>=2&&kind<=4)destinations.insertLast(cell);
        }
        for(uint i=0;i<walkers.length();++i){TownWalker@ w=walkers[i];
            if(homes.length()>0&&w.home!=homes[i%homes.length()]){w.home=homes[i%homes.length()];w.returning=true;w.route.resize(0);if(w.phase==2)w.building=-1;}
            if(!House(w.home)){if(homes.length()>0)w.home=homes[i%homes.length()];if(w.phase==2&&Road(w.from)&&Road(w.to)){w.returning=true;w.building=-1;w.route.resize(0);}else Indoor(w,i);}
            else if(!Road(w.from)||!Road(w.to)||(w.phase!=2&&!House(w.building)&&!Destination(w.building)))Indoor(w,i);
            else {w.route.resize(0);w.routeIndex=0;w.returning=true;}
        }
    }
    // Breadth-first search is only run when choosing an outing/return destination,
    // never per pedestrian per frame. Unreachable facilities are not teleported to.
    bool RouteTo(TownWalker@ w,int building){
        int target=DoorRoad(building),size=state.GridSize();if(target<0||!Road(w.from))return false;
        array<int> parent(uint(size*size),-1);array<int> queue={w.from};parent[uint(w.from)]=w.from;
        for(uint q=0;q<queue.length()&&parent[uint(target)]<0;++q){array<int>@ adjacent=Neighbors(queue[q]);for(uint n=0;n<adjacent.length();++n){int cell=adjacent[n];if(parent[uint(cell)]>=0)continue;parent[uint(cell)]=queue[q];queue.insertLast(cell);}}
        if(parent[uint(target)]<0)return false;w.route.resize(0);w.routeIndex=0;int current=target;
        while(current!=w.from){w.route.insertAt(0,current);current=parent[uint(current)];}
        w.building=building;return true;
    }
    void Decide(TownWalker@ w,uint id){
        if(w.routeIndex<w.route.length()){w.to=w.route[w.routeIndex++];return;}
        if(w.building>=0&&DoorRoad(w.building)==w.from){w.phase=3;w.progress=0;w.to=w.from;return;}
        if(w.steps>=8+int(id%11)||w.returning){
            bool found=false;if(!w.returning&&destinations.length()>0){int first=int(Variation(id,uint(w.steps))*float(destinations.length()))%int(destinations.length());for(uint j=0;j<destinations.length();++j)if(RouteTo(w,destinations[(uint(first)+j)%destinations.length()])){found=true;break;}}
            if(!found){w.returning=true;found=RouteTo(w,w.home);}
            if(found){if(w.route.length()==0){w.phase=3;w.progress=0;w.to=w.from;}else w.to=w.route[w.routeIndex++];return;}
        }
        w.building=-1;w.to=Next(w.from,w.previous,id,w.steps);
    }
    void Save(){
        Json@ data=Json();data.SetFloat("clock",clock);Json@ list=Json();list.Parse("[]");
        for(uint i=0;i<walkers.length();++i){TownWalker@ w=walkers[i];Json@ item=Json();item.SetInt("home",w.home);item.SetInt("building",w.building);item.SetInt("from",w.from);item.SetInt("to",w.to);item.SetInt("previous",w.previous);item.SetInt("steps",w.steps);item.SetInt("phase",w.phase);item.SetFloat("progress",w.progress);item.SetFloat("wait",w.wait);item.SetBool("returning",w.returning);item.SetBool("retiring",w.retiring);list.PushJson(item);}
        data.SetJson("walkers",list);scene.SetGlobalVariable("LightTown.Pedestrians",data.ToString());
    }
    void Restore(){
        restored=true;string saved;if(!scene.GetGlobalVariable("LightTown.Pedestrians",saved))return;Json@ data=Json();if(!data.Parse(saved))return;Json@ list=data.GetJson("walkers");if(list is null)return;clock=float(data.GetFloat("clock"));
        for(uint i=0;i<walkers.length()&&i<list.Size();++i){TownWalker@ w=walkers[i];Json@ item=list.At(i);int home=int(item.GetInt("home")),from=int(item.GetInt("from")),to=int(item.GetInt("to")),building=int(item.GetInt("building"));
            if(home!=w.home||!Road(from)||!Road(to)||(building>=0&&!House(building)&&!Destination(building)))continue;
            w.building=building;w.from=from;w.to=to;w.previous=int(item.GetInt("previous"));w.steps=int(item.GetInt("steps"));w.phase=int(item.GetInt("phase"));w.progress=float(item.GetFloat("progress"));w.wait=float(item.GetFloat("wait"));w.returning=item.GetBool("returning");w.retiring=item.GetBool("retiring");
            // A saved route is rebuilt at the next cell, preserving current travel.
            if(w.phase==2&&w.building>=0){int destination=w.building;int origin=w.from;w.from=w.to;RouteTo(w,destination);w.from=origin;}
        }
    }
    void Update(float dt){
        Refresh();if(world.Arriving())return;clock+=dt;bool showPeople=GetSettingBool("town.people",true);uint count=homes.length()==0?0:uint(residents+int(destinations.length())*2);
        while(walkers.length()<count){uint id=walkers.length();TownWalker@ w=TownWalker();
            w.home=homes[id%homes.length()];w.speed=.4f+Variation(id,7)*.23f;Indoor(w,id);walkers.insertLast(w);
        }
        if(!restored)Restore();float spacing=float(state.rules.GetFloat("cellSize",1.5f));
        for(uint i=0;i<walkers.length();++i){TownWalker@ w=walkers[i];if(homes.length()==0){Show(w,false);continue;}
            if(i>=count){if(w.phase==0&&w.building==w.home){Show(w,false);continue;}if(!w.retiring){w.retiring=true;w.returning=true;w.route.resize(0);if(w.phase==2)w.building=-1;}}
            else w.retiring=false;
            if(w.phase==0){Show(w,false);w.wait-=dt;if(w.wait>0)continue;w.phase=1;w.progress=0;w.from=DoorRoad(w.building);w.to=w.from;if(w.building==w.home){w.steps=0;w.returning=w.retiring;}}
            Show(w,showPeople);Vector3 p;float scale=1;
            if(w.phase==1||w.phase==3){w.progress=TownClamp(w.progress+dt*w.speed/spacing,0,1);float t=w.progress;
                p=w.phase==1?Math::Lerp(Cell(w.building),Cell(w.from),t):Math::Lerp(Cell(w.from),Cell(w.building),t);scale=w.phase==1?TownClamp(t*3,0,1):TownClamp((1-t)*3,0,1);
                if(t>=1){if(w.phase==1){bool visited=w.building!=w.home;w.phase=2;w.progress=0;w.building=-1;w.returning=visited;Decide(w,i);}else{bool atHome=w.building==w.home;w.phase=0;w.wait=atHome?4+Variation(i,uint(++w.steps))*14:2+Variation(i,uint(++w.steps))*5;Show(w,false);}}
            }else{
                w.progress+=dt*w.speed/spacing;
                if(w.progress>=1){w.progress=0;int old=w.from;w.from=w.to;w.previous=old;++w.steps;Decide(w,i);}
                p=Math::Lerp(Cell(w.from),Cell(w.to),w.progress);
                // Small per-resident offsets keep people taking the same route apart.
                Vector3 direction=Cell(w.to)-Cell(w.from);if(direction.LengthSquared()>.001f)p+=Vector3(direction.z,0,-direction.x)*(float(int(i%5)-2)*.045f/spacing);
            }
            p.y=.05f+.012f*Sin(clock*6+float(i)*2.39996f);w.position=p;w.scale=scale;
        }
        Submit();
    }
}
