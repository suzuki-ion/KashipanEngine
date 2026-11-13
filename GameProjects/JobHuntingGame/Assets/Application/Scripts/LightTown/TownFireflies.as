#include "TownPresentation.as"
#include "TownState.as"

int TownFireflyCount(float energy){return int(Min(2048.0f,Max(0.0f,Ceil(energy/8))));}
float TownFireflyIntensityScale(int count){return count<=0?0:1+5*Min(float(count)/2048.0f,1.0f);}
// Shared animation coefficients: evaluate the slow morph/rotation once per
// ribbon, rather than once per light (the pool can contain 2048 lights).
class TownFireflyTrack {
    float cosine=1,sine=0,petalCosine=1,petalSine=0;
    float morph=0,warp=0,centerX=0,centerZ=0,lift=0;
    void Update(uint lane,float time){
        float direction=lane%2==0?1.0f:-1.0f;
        float yaw=float(lane)*.523598776f+time*direction*(.0025f+float(lane%4)*.0004f);
        cosine=Cos(yaw);sine=Sin(yaw);
        float petal=time*.007f+float(lane)*.73f;
        petalCosine=Cos(petal);petalSine=Sin(petal);
        morph=.25f+.2f*Sin(time*.009f+float(lane)*.91f);
        warp=.22f*Sin(time*.013f+float(lane)*1.17f);
        centerX=.018f*Cos(time*.005f+float(lane)*.8f);
        centerZ=.018f*Sin(time*.007f+float(lane)*.8f);
        lift=.06f*Sin(time*.021f+float(lane)*.67f);
    }
}
class TownFireflies {
    Scene@ scene; Object@ camera;
    Object@ batchObject;LightRenderer@ batchLights;MeshRenderer@ batchMarkers;Light@ commonLight;
    array<Vector3>@ positions=array<Vector3>();array<float>@ intensities=array<float>(),opacities=array<float>();
    array<float>@ phases=array<float>(), speeds=array<float>(), radii=array<float>();
    array<Vector3>@ homes=array<Vector3>();
    array<TownFireflyTrack@>@ tracks=array<TownFireflyTrack@>();
    float trackClock=-1;
    int activeCount=0;
    int intensityCount=-1;
    float intensityScale=0,targetIntensityScale=0;
    bool intensityInitialized=false;
    Quaternion lastFacing;bool facingInitialized=false;
    float clock = 0, extent = 10;
    void Initialize(Scene@ owner, float range) {
        @scene = owner; @camera = scene.GetObject("TownCamera"); extent = range;
        @batchObject=TownMesh(scene,"TownFireflyBatch","PrimitiveMesh-Circle-XY",Vector3(),Vector3(1,1,1),Vector4(1,1,1,1));
        batchObject.GetComponent(@batchMarkers);batchMarkers.SetMaterialName("TownFirefly");batchMarkers.SetPipelineName("Town.Firefly");batchMarkers.SetCastShadows(false);
        batchObject.AddComponent(@commonLight);batchObject.AddComponent(@batchLights);
        commonLight.SetType(LightType::Point);commonLight.SetColor(Vector4(1,.8f,.2f,1));commonLight.SetRadius(2.2f);commonLight.SetDecay(2);commonLight.SetIntensity(0);commonLight.SetCastShadows(false);
        batchLights.SetTargetObject(scene.GetObject("TownScreen"));Submit(Quaternion());
    }
    void Submit(const Quaternion &in facing){
        batchLights.SetPointInstances(positions,intensities);
        batchMarkers.SetWorldBillboardPairs(positions,opacities,facing,.4f,.1f,Vector4(1,.75f,.12f,1),Vector4(1,.95f,.5f,.9f));
    }
    // Keep index-based identity stable when energy changes. A little variation
    // in each light's speed/radius avoids rigid strings of equally spaced dots.
    uint Lane(uint i){return (i*7)%12;}
    float Variation(uint i,uint salt){uint v=(i+1)^(salt*2654435769);v=(v^(v>>16))*2146121005;v=(v^(v>>15))*2221713035;v=v^(v>>16);return float(v&16777215)/16777215.0f;}
    float PathSpeed(uint i){uint lane=Lane(i);float speed=(.022f+float(lane%5)*.003f)*(.96f+.08f*Variation(i,3));return lane%2==0?speed:-speed;}
    // R2 low-discrepancy placement spreads even a small population across
    // the square town, without concentrating paths around the center.
    Vector3 Home(uint i){
        float x=.5f+float(i+1)*.754877666f,z=.5f+float(i+1)*.569840296f;
        return Vector3((x-Floor(x)-.5f)*extent*1.68f+(Variation(i,17)-.5f)*extent*.05f,
            1.1f+.9f*Variation(i,11),
            (z-Floor(z)-.5f)*extent*1.68f+(Variation(i,19)-.5f)*extent*.05f);
    }
    float PathRadius(uint i){return extent*(.045f+.065f*Variation(i,7)+.0015f*float(Lane(i)));}
    float Pulse(uint i){return .65f+.35f*Sin(clock*(.55f+float(Lane(i)%4)*.06f)+float(i)*1.7f);}
    void UpdateTracks(){
        if(tracks.length()==12&&trackClock==clock)return;
        while(tracks.length()<12)tracks.insertLast(TownFireflyTrack());
        for(uint lane=0;lane<12;++lane)tracks[lane].Update(lane,clock);
        trackClock=clock;
    }
    Vector3 Position(uint i) {
        UpdateTracks();
        // Position also works without creating any scene/rendering objects.
        float phase,speed,radius;
        if(i<phases.length()){phase=phases[i];speed=speeds[i];radius=radii[i];}
        else{phase=float(i)*2.399963f;speed=PathSpeed(i);radius=PathRadius(i);}
        uint lane=Lane(i);TownFireflyTrack@ t=tracks[lane];
        float angle=phase+clock*speed+t.warp,c=Cos(angle),s=Sin(angle);
        // Harmonic identities provide complex curves using just one sin/cos
        // pair per light. The slowly changing coefficients are shared above.
        float c2=c*c-s*s,s2=2*c*s,c3=c2*c-s2*s,s3=s2*c+c2*s;
        float c5=c3*c2-s3*s2,s5=s3*c2+c3*s2;
        float petal=c5*t.petalCosine-s5*t.petalSine;
        float flowerRadius=.74f+.26f*petal;
        float knotRadius=.76f+.24f*c3;
        float u,v,w;
        if(lane%3==0){ // A torus knot opens into a five-petal ribbon.
            u=knotRadius*c2*(1-t.morph)+flowerRadius*c*t.morph;
            v=knotRadius*s2*(1-t.morph)+flowerRadius*s*t.morph;
            w=s3*(1-t.morph)+s2*t.morph;
        }else if(lane%3==1){ // Rotating rosettes fold through Lissajous loops.
            u=flowerRadius*c*(1-t.morph)+c3*t.morph;
            v=flowerRadius*s*(1-t.morph)+s2*t.morph;
            w=s2*(1-t.morph)+s5*t.morph;
        }else{
            u=c3*(1-t.morph)+knotRadius*c2*t.morph;
            v=s2*(1-t.morph)+knotRadius*s2*t.morph;
            w=s5*(1-t.morph)+s3*t.morph;
        }
        // A firefly drifts around its own distributed home. Geometry gently
        // bends the ellipse instead of pulling the whole swarm into big rings.
        u=.82f*c+.18f*u;v=.82f*s+.18f*v;
        Vector3 home=i<homes.length()?homes[i]:Home(i);
        return home+Vector3(radius*(u*t.cosine-v*t.sine)+extent*t.centerX,
            .18f*w+.08f*s+t.lift,
            radius*(u*t.sine+v*t.cosine)+extent*t.centerZ);
    }
    void UpdateIntensity(float dt,int count){
        if(count!=intensityCount){targetIntensityScale=TownFireflyIntensityScale(count);intensityCount=count;}
        if(count<=0){intensityScale=0;intensityInitialized=false;return;}
        // Initial/reappearing lights start at the appropriate gain. Existing
        // lights ease towards count changes with frame-rate-independent decay.
        if(!intensityInitialized){intensityScale=targetIntensityScale;intensityInitialized=true;return;}
        float blend=1-Exp(-Max(dt,0.0f)*2.5f);
        intensityScale+=(targetIntensityScale-intensityScale)*blend;
    }
    void Update(float dt, float energy) {
        clock += dt; UpdateTracks();int count=GetSettingBool("town.fireflies",true)?TownFireflyCount(energy):0;commonLight.SetRadius(.65f+3.85f*Min(Max(energy,0.0f)/16384,1.0f)); UpdateIntensity(dt,count);
        while(phases.length()<uint(count)){
            uint i=phases.length();homes.insertLast(Home(i));phases.insertLast(float(i)*2.399963f);speeds.insertLast(PathSpeed(i));radii.insertLast(PathRadius(i));
        }
        positions.resize(uint(count));intensities.resize(uint(count));opacities.resize(uint(count));
        for(uint i=0;i<uint(count);++i){
            positions[i]=Position(i);float pulse=Pulse(i);
            intensities[i]=(.024f+pulse*.024f)*intensityScale;opacities[i]=.1f+pulse*.12f;
        }
        Quaternion facing=camera.GetTransform().GetRotateQuaternion();Submit(facing);
        activeCount=count;lastFacing=facing;facingInitialized=true;
    }
}
