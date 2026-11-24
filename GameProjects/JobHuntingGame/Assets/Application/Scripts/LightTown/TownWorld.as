#include "TownBuildings.as"

float TownCellRise(int x,int z,int size,float time){
    float dx=float(x)+.5f-float(size)*.5f,dz=float(z)+.5f-float(size)*.5f;
    float t=TownClamp((time-Sqrt(dx*dx+dz*dz)*.055f)/.8f,0,1);
    return Easing::Eased(-2.5f,0.0f,t,EaseType::EaseOutBack);
}

class TownWorld {
    Scene@ scene; TownState@ state;
    array<Object@>@ objects = array<Object@>();
    array<Object@>@ cells = array<Object@>();
    array<Object@>@ models = array<Object@>();
    array<Object@>@ roots=array<Object@>();
    array<Object@>@ grounds=array<Object@>();
    float arrivalClock=0;bool arrivalDone=false;uint currentCell=0;
    bool Arriving(){return !arrivalDone;}
    Vector3 Cell(int x,int z) { float s=float(state.rules.GetFloat("cellSize",1.5f));float half=float(state.GridSize())*.5f;return Vector3((float(x)+.5f-half)*s,0,(float(z)+.5f-half)*s); }
    Vector3 Center(Json@ b){float offset=float(state.Footprint(b)-1)*float(state.rules.GetFloat("cellSize",1.5f))*.5f;return Cell(int(b.GetInt("x")),int(b.GetInt("z")))+Vector3(offset,0,offset);}
    Object@ Part(const string &in mesh,const Vector3 &in p,const Vector3 &in scale,const Vector4 &in color) {
        Object@ o=TownMesh(scene,"TownInfrastructure",mesh,p,scale,color);
        if(mesh=="PrimitiveMesh-Box"){MeshRenderer@ renderer;o.GetComponent(@renderer);renderer.SetMaterialName("TownGridUnlit");}
        o.GetTransform().SetParentObject(roots[currentCell]);objects.insertLast(o);return o;
    }
    void Initialize(Scene@ owner,TownState@ data) {
        @scene=owner;@state=data;float s=float(state.rules.GetFloat("cellSize",1.5f));
        int size=state.GridSize();
        for(int z=0;z<size;++z)for(int x=0;x<size;++x){
            Object@ root=scene.CreateObject("TownCellRoot");root.GetTransform().SetTranslate(Vector3(0,TownCellRise(x,z,size,0),0));roots.insertLast(root);
            Object@ floor=TownMesh(scene,"TownCellGround","PrimitiveMesh-Box",Cell(x,z)+Vector3(0,-.1f,0),Vector3(s*.98f,.18f,s*.98f),Vector4(.3f,.34f,.36f,1));
            MeshRenderer@ floorRenderer;floor.GetComponent(@floorRenderer);floorRenderer.SetMaterialName("TownGround");floorRenderer.SetCastShadows(false);
            floor.GetTransform().SetParentObject(root);grounds.insertLast(floor);
        }
        for(int z=0;z<state.GridSize();++z)for(int x=0;x<state.GridSize();++x){
            Object@ cell=TownMesh(scene,"TownLitCell","PrimitiveMesh-Plane-XZ",Cell(x,z)+Vector3(0,.012f,0),Vector3(s*.92f,1,s*.92f),Vector4(.85f,.66f,.14f,.38f));
            MeshRenderer@ r;cell.GetComponent(@r);r.SetPipelineName("Town.Cells");r.SetMaterialName("TownLitCell");r.SetInstanceColor(Vector4(.85f,.66f,.14f,.13f));r.SetCastShadows(false);cells.insertLast(cell);
            cell.GetTransform().SetParentObject(roots[uint(z*size+x)]);
        }
        for(int z=0;z<=size;++z)for(int x=0;x<=size;++x){int cx=x<size?x:size-1,cz=z<size?z:size-1;Object@ root=roots[uint(cz*size+cx)];
            BindGrid("TownGridCorner_"+x+"_"+z,root);
            if(x<size)BindGrid("TownGridRailX_"+x+"_"+z,root);
            if(z<size)BindGrid("TownGridRailZ_"+x+"_"+z,root);
        }
        Rebuild();
    }
    void BindGrid(const string &in name,Object@ root){Object@ o=scene.GetObject(name);if(o !is null)o.GetTransform().SetParentObject(root);}
    void Update(float dt){
        if(arrivalDone)return;arrivalClock+=dt;int size=state.GridSize();
        for(int z=0;z<size;++z)for(int x=0;x<size;++x)roots[uint(z*size+x)].GetTransform().SetTranslate(Vector3(0,TownCellRise(x,z,size,arrivalClock),0));
        if(arrivalClock>=.8f+Sqrt(2.0f)*float(size)*.5f*.055f)arrivalDone=true;
    }
    void Road(int x,int z) {
        Vector3 p=Cell(x,z)+Vector3(0,.012f,0);float spacing=float(state.rules.GetFloat("cellSize",1.5f));int mask=state.RoadMask(x,z);
        // マスクごとに共有メッシュを持つ。隣り合う道路の端点はセル境界でつながる。
        Object@ surface=Part(TownRoadMesh(mask),p,Vector3(spacing,1,spacing),Vector4(.23f,.28f,.30f,1));
        Object@ details=Part(TownRoadMesh(mask,true),p,Vector3(spacing,1,spacing),Vector4(.64f,.66f,.58f,1));
        MeshRenderer@ renderer;surface.GetComponent(@renderer);renderer.SetMaterialName("TownGridUnlit");renderer.SetCastShadows(false);
        details.GetComponent(@renderer);renderer.SetMaterialName("TownGridUnlit");renderer.SetCastShadows(false);
    }
    void Rebuild() {
        for(uint i=0;i<objects.length();++i)if(IsValidObject(objects[i]))scene.DeleteObject(objects[i]);
        objects.resize(0);models.resize(0);state.EnsureInfrastructure();
        Json@ list=state.Buildings();
        for(uint i=0;i<list.Size();++i){
            Json@ b=list.At(i);int kind=int(b.GetInt("kind")),x=int(b.GetInt("x")),z=int(b.GetInt("z"));
            currentCell=uint(z*state.GridSize()+x);
            if(kind==6){Road(x,z);continue;}
            Json@ definition=state.BuildingDefinition(b);if(definition is null)continue;
            bool working=state.IsWorking(x,z);Vector3 p=Cell(x,z)+Vector3(float(state.Footprint(b)-1)*float(state.rules.GetFloat("cellSize",1.5f))*.5f,0,float(state.Footprint(b)-1)*float(state.rules.GetFloat("cellSize",1.5f))*.5f);
            Vector4 color=working?definition.GetVector4("color",Vector4(1,1,1,1)):Vector4(.28f,.28f,.28f,1);
            Object@ model=TownBuildingModel(scene,kind,p,color);MeshFilter@ variant;model.GetComponent(@variant);variant.SetMeshHandle(GetModelHandleFromAssetPath(GetSettingBool("town.simpleModels",false)?"PrimitiveMesh-Box":TownVariantMesh(kind,int(b.GetInt("tier")),int(b.GetInt("recipe")))));
            if(GetSettingBool("town.simpleModels",false)){float height=float(definition.GetFloat("height",2));model.GetTransform().SetScale(Vector3(float(state.Footprint(b))*1.2f,height,float(state.Footprint(b))*1.2f));model.GetTransform().SetTranslate(p+Vector3(0,height*.5f,0));}model.GetTransform().SetRotate(Vector3(0,TownModelYaw(kind,int(b.GetInt("rotation"))),0));objects.insertLast(model);models.insertLast(model);model.GetTransform().SetParentObject(roots[currentCell]);
            // 稼働中の施設だけ補助光を点灯する。稼働判定の照明範囲は街灯のみ。
            if(working){
            Object@ fill=scene.CreateObject("TownFacilityFill");fill.GetTransform().SetTranslate(p+Vector3(0,.85f,0));fill.GetTransform().SetParentObject(roots[currentCell]);
            Light@ soft;LightRenderer@ softRenderer;fill.AddComponent(@soft);fill.AddComponent(@softRenderer);
            soft.SetType(LightType::Point);soft.SetColor(Vector4(1,.84f,.62f,1));soft.SetIntensity(.06f);soft.SetRadius(2.7f);soft.SetDecay(2);soft.SetCastShadows(false);
            softRenderer.SetTargetObject(scene.GetObject("TownScreen"));objects.insertLast(fill);
            }
            if(kind==0 && working){
                Vector3 pos=p+Vector3(0,1.72f,0);
                Object@ lightObject=scene.CreateObject("TownStreetlight");lightObject.GetTransform().SetTranslate(pos);
                Light@ lamp;LightRenderer@ renderer;lightObject.AddComponent(@lamp);lightObject.AddComponent(@renderer);
                lamp.SetType(LightType::Point);lamp.SetColor(Vector4(1,.83f,.46f,1));lamp.SetIntensity(1.3f);lamp.SetRadius(Sqrt(TownStreetlightRadius()*TownStreetlightRadius()+1.72f*1.72f));lamp.SetDecay(2);
                lightObject.GetTransform().SetParentObject(roots[currentCell]);
                renderer.SetTargetObject(scene.GetObject("TownScreen"));objects.insertLast(lightObject);
                Object@ glow=Part("PrimitiveMesh-UVSphere",p+Math::MakeRotateEuler(Vector3(0,-float(b.GetInt("rotation"))*1.57079633f-1.57079633f,0)).RotateVector(Vector3(-.3f,1.72f,0)),Vector3(.12f,.12f,.12f),Vector4(1,.85f,.4f,1));MeshRenderer@ r;glow.GetComponent(@r);r.SetMaterialName("TownGridUnlit");r.SetCastShadows(false);
                Object@ disk=Part("PrimitiveMesh-Circle-XZ",p+Vector3(0,.018f,0),Vector3(9,1,9),Vector4(.9f,.7f,.2f,.065f));
                MeshRenderer@ diskRenderer;disk.GetComponent(@diskRenderer);diskRenderer.SetMaterialName("TownLitCell");diskRenderer.SetPipelineName("Town.Cells");diskRenderer.SetCastShadows(false);
            }
        }
        int size=state.GridSize();for(int z=0;z<size;++z)for(int x=0;x<size;++x)cells[uint(z*size+x)].SetActive(state.IsLit(x,z));
    }
}
