// Install as CurrentQA in an isolated, fully populated CityScene. The external
// runner checks profiler cache-hit calls against phase in this result file.
class ShadowCacheRuntimeRegression : ScriptComponentBehavior {
    float clock=0;Object@ model;Object@ lamp;Vector3 modelPosition,lampPosition;
    void Update(){
        clock+=GetDeltaTime();if(clock<5)return;
        if(model is null){
            array<Object@>@ models=GetScene().GetObjects("TownFacilityModel");if(models.length()==0)throw("Missing shadow caster");@model=models[0];modelPosition=model.GetTransform().GetTranslate();
            array<Object@>@ lamps=GetScene().GetObjects("TownStreetlight");Vector3 eye=GetScene().GetObject("TownCamera").GetTransform().GetWorldPosition();float nearest=1e20f;
            for(uint i=0;i<lamps.length();++i){Light@ light;lamps[i].GetComponent(@light);if(!light.IsCastShadows())continue;float distance=(lamps[i].GetTransform().GetWorldPosition()-eye).LengthSquared();if(distance<nearest){nearest=distance;@lamp=lamps[i];}}
            if(lamp is null)throw("Missing shadow light");lampPosition=lamp.GetTransform().GetTranslate();
        }
        int phase=int((clock-5)/3);
        model.GetTransform().SetTranslate(modelPosition+(phase==1?Vector3(0,Sin(clock*2)*.5f,0):Vector3()));
        lamp.GetTransform().SetTranslate(lampPosition+(phase==3?Vector3(0,Sin(clock*2)*.5f,0):Vector3()));
        Json@ status=Json();status.SetInt("phase",phase);status.SetFloat("phaseTime",clock-5-float(phase)*3);SaveJsonFile("Results/ShadowCacheProbe.json",status);
    }
}
