// Install as CurrentQA in an isolated CityScene. Two separated light volumes
// make local invalidation testable even when the city's normal lights exceed
// the shadow-slice budget. The external runner checks both reused/dirty jobs.
class PartialShadowCacheRuntimeRegression : ScriptComponentBehavior {
    float clock=0;Object@ model;Object@ lamp;
    void DisableShadow(const string &in name){array<Object@>@ objects=GetScene().GetObjects(name);for(uint i=0;i<objects.length();++i){Light@ light;if(objects[i].GetComponent(@light))light.SetCastShadows(false);}}
    Object@ ProbeLight(const string &in name,const Vector3 &in p){
        Object@ o=GetScene().CreateObject(name);o.GetTransform().SetTranslate(p);
        Light@ light;LightRenderer@ renderer;o.AddComponent(@light);o.AddComponent(@renderer);
        light.SetType(LightType::Point);light.SetRadius(4);light.SetIntensity(1);light.SetCastShadows(true);light.SetShadowMapResolution(256);
        renderer.SetTargetObject(GetScene().GetObject("TownScreen"));return o;
    }
    void Update(){
        clock+=GetDeltaTime();if(clock<5)return;
        if(model is null){
            DisableShadow("TownStreetlight");DisableShadow("SceneLight");DisableShadow("Light Object");
            for(uint i=0;i<7;++i)DisableShadow("TownBuildIcon"+i+"Light");
            @model=GetScene().CreateObject("ShadowCacheProbeCaster");
            MeshFilter@ mesh;MeshRenderer@ renderer;model.AddComponent(@mesh);model.AddComponent(@renderer);
            mesh.SetMeshHandle(GetModelHandleFromAssetPath("PrimitiveMesh-Box"));
            renderer.SetPipelineName("Object3D.Solid.BlendNormal");renderer.SetMaterialName("TownFacility");renderer.SetTargetObject(GetScene().GetObject("TownScreen"));renderer.SetCastShadows(true);
            @lamp=ProbeLight("ShadowCacheProbeLightA",Vector3(-8,3,0));ProbeLight("ShadowCacheProbeLightB",Vector3(8,3,0));
        }
        int phase=int((clock-5)/3);
        model.GetTransform().SetTranslate(Vector3(-8,1+(phase==1?Sin(clock*2)*.5f:0),0));
        lamp.GetTransform().SetTranslate(Vector3(-8,3+(phase==3?Sin(clock*2)*.5f:0),0));
        Json@ status=Json();status.SetInt("phase",phase);status.SetFloat("phaseTime",clock-5-float(phase)*3);SaveJsonFile("Results/ShadowCacheProbe.json",status);
    }
}
