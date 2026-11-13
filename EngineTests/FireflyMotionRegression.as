#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownFireflies.as"
void CheckFireflyMotion(bool value,const string &in message){if(!value)throw(message);}
void CheckFireflyGeometry(){
    CheckFireflyMotion(TownFireflyIntensityScale(0)==0&&TownFireflyIntensityScale(-1)==0,"zero lights have no gain");
    CheckFireflyMotion(TownFireflyIntensityScale(1)==16&&TownFireflyIntensityScale(336)==16,"small swarm retains visible illumination");
    CheckFireflyMotion(TownFireflyIntensityScale(2048)>6&&TownFireflyIntensityScale(2048)<7,"maximum swarm stays within measured 4-8x range");
    float last=16;
    for(int count=1;count<=2048;++count){float scale=TownFireflyIntensityScale(count);CheckFireflyMotion(scale>0&&scale<=last,"per-light gain decreases with population");last=scale;}
    TownFireflies exposure;exposure.UpdateIntensity(0,336);exposure.UpdateIntensity(.1f,2048);
    CheckFireflyMotion(exposure.intensityScale<16&&exposure.intensityScale>TownFireflyIntensityScale(2048),"population changes ease without snapping");
    float paused=exposure.intensityScale;exposure.UpdateIntensity(0,2048);CheckFireflyMotion(exposure.intensityScale==paused,"paused intensity remains stable");
    TownFireflies fast,slow;fast.UpdateIntensity(0,336);slow.UpdateIntensity(0,336);
    for(int frame=0;frame<100;++frame)fast.UpdateIntensity(.01f,2048);
    for(int frame=0;frame<10;++frame)slow.UpdateIntensity(.1f,2048);
    CheckFireflyMotion(Abs(fast.intensityScale-slow.intensityScale)<.0001f,"exposure interpolation independent of frame rate");
    exposure.UpdateIntensity(0,0);exposure.UpdateIntensity(0,2048);CheckFireflyMotion(Abs(exposure.intensityScale-TownFireflyIntensityScale(2048))<.0001f,"reappearing swarm has correct initial gain");

    TownFireflies swarm;swarm.extent=13.875f;bool outside=false;
    array<float> times={0,7.3f,37,123,600};
    for(uint t=0;t<times.length();++t){
        swarm.clock=times[t];array<Vector3> before(2048);
        for(uint i=0;i<2048;++i){Vector3 p=swarm.Position(i);before[i]=p;
            CheckFireflyMotion(Abs(p.x)<=swarm.extent*1.08f&&Abs(p.z)<=swarm.extent*1.08f&&p.y>=.77f&&p.y<=2.33f,"motion finite and bounded above the ground");
            if(Abs(p.x)>12||Abs(p.z)>12)outside=true;
        }
        swarm.clock=times[t]+.01f;
        for(uint i=0;i<2048;++i)CheckFireflyMotion((swarm.Position(i)-before[i]).Length()<.0025f,"motion stays continuous across time");
    }
    // A 64-light population should already cover every quarter of the map,
    // rather than requiring thousands of lights to fill the outer area.
    array<bool> covered(16,false);swarm.clock=0;
    for(uint i=0;i<64;++i){Vector3 p=swarm.Home(i);int x=int(Floor((p.x/12+1)*2)),z=int(Floor((p.z/12+1)*2));if(x>=0&&x<4&&z>=0&&z<4)covered[uint(z*4+x)]=true;}
    for(uint i=0;i<16;++i)CheckFireflyMotion(covered[i],"small swarm covers the whole town");
    CheckFireflyMotion(outside,"outer ribbons extend beyond the town");
    swarm.clock=0;Vector3 first=swarm.Position(17);swarm.clock=6.2831853f/Abs(swarm.PathSpeed(17));
    CheckFireflyMotion((swarm.Position(17)-first).Length()>.01f,"ribbon morph prevents a single repeated lap");
    for(uint i=0;i<12;++i)CheckFireflyMotion(swarm.PathSpeed(i)*swarm.PathSpeed(i+1)<0,"alternating ribbon flow");
}
