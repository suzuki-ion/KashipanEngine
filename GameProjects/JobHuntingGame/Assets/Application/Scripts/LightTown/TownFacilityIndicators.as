#include "TownWorld.as"

bool TownRayFacility(const Vector3 &in origin,const Vector3 &in direction,const Vector3 &in center,const Vector3 &in half,float &out hit){
    array<float> o={origin.x,origin.y,origin.z},d={direction.x,direction.y,direction.z},c={center.x,center.y,center.z},h={half.x,half.y,half.z};float nearT=0,farT=1;
    for(uint i=0;i<3;++i){if(Abs(d[i])<.000001f){if(o[i]<c[i]-h[i]||o[i]>c[i]+h[i])return false;continue;}float a=(c[i]-h[i]-o[i])/d[i],b=(c[i]+h[i]-o[i])/d[i];if(a>b){float swap=a;a=b;b=swap;}if(a>nearT)nearT=a;if(b<farT)farT=b;if(nearT>farT)return false;}hit=nearT;return true;
}

// ワールド座標に追従し、常にカメラへ正対するスクリーン空間ビルボード。
// TownCanvas上の論理座標を使うため、Letterboxやウィンドウ変更にも追従する。
class TownFacilityBar {
    TownUI@ ui=TownUI();Object@ fill;TextRenderer@ label;int x,z,kind;
}
class TownParameterForecast {
    TownUI@ ui=TownUI();array<Vector3>@ offsets=array<Vector3>();
    int x,z;float age=0;
    uint warningStart=9999;
}
class TownFacilityIndicators {
    Scene@ scene;TownState@ state;TownWorld@ world;Object@ camera;
    array<TownFacilityBar@>@ bars=array<TownFacilityBar@>();
    array<TownParameterForecast@>@ forecasts=array<TownParameterForecast@>();
    uint buildingCount=0;
    void Initialize(Scene@ owner,TownState@ data,TownWorld@ town){@scene=owner;@state=data;@world=town;@camera=scene.GetObject("TownCamera");Rebuild();}
    void DeleteUI(TownUI@ ui){for(uint i=0;i<ui.objects.length();++i)if(IsValidObject(ui.objects[i]))scene.DeleteObject(ui.objects[i]);}
    void Rebuild(){
        for(uint i=0;i<bars.length();++i)DeleteUI(bars[i].ui);bars.resize(0);
        Json@ list=state.Buildings();buildingCount=list.Size();
        for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);int kind=int(b.GetInt("kind"));if(kind!=2&&kind!=4)continue;
            TownFacilityBar@ bar=TownFacilityBar();bar.kind=kind;bar.x=int(b.GetInt("x"));bar.z=int(b.GetInt("z"));bar.ui.Initialize(scene);bar.ui.layer=44;
            bar.ui.Rect(0,0,76,9,Vector4(.02f,.035f,.045f,.9f));
            @bar.fill=bar.ui.Rect(0,0,70,5,Vector4(.25f,.9f,.55f,1));@bar.label=bar.ui.Text("",-36,8,13);bar.ui.Show(false);bars.insertLast(bar);
        }
    }
    Vector3 Anchor(int x,int z){Json@ b=state.BuildingAt(x,z);float h=b is null?1.0f:float(state.Definition(int(b.GetInt("kind"))).GetFloat("height",2));return world.Cell(x,z)+Vector3(0,h+.4f+world.roots[uint(z*state.GridSize()+x)].GetTransform().GetTranslate().y,0);}
    bool Project(const Vector3 &in position,Vector2 &out pixel){
        Vector3 view=position.Transform(camera.GetTransform().GetWorldMatrix().Inverse());Camera3D@ lens;camera.GetComponent(@lens);
        if(view.z<=lens.GetNearClip())return false;
        Matrix4x4 projection;projection.MakePerspectiveFovMatrix(lens.GetFovY(),lens.GetAspectRatio(),lens.GetNearClip(),lens.GetFarClip());Vector3 ndc=view.Transform(projection);
        pixel=Vector2((ndc.x+1)*640,(1-ndc.y)*360);return ndc.z<=1&&pixel.x>=0&&pixel.x<=1280&&pixel.y>=90&&pixel.y<=590;
    }
    void EffectLine(TownParameterForecast@ f,const string &in icon,const string &in text,int row){float y=float(row)*23;f.ui.Image("Application/UI/Resources/"+icon+".png",-44,y+8,20);f.ui.Text(text,-30,y,15);}
    void Placed(int kind,int x,int z){
        if(kind==6)return;
        TownParameterForecast@ f=TownParameterForecast();f.x=x;f.z=z;f.ui.Initialize(scene);f.ui.layer=46;
        string cycle=" / "+int(state.ProductionInterval())+TownTr("秒","s"),inactive=state.IsWorking(x,z)?"":TownTr("（稼働時）"," (when active)");int row=0;
        if(kind==0){EffectLine(f,"Light",TownTr("光 +12","Light +12")+cycle+inactive,row++);EffectLine(f,"Safety",TownTr("治安 +2","Safety +2")+cycle+inactive,row++);}
        if(kind==1)EffectLine(f,"Happiness",TownTr("幸福 +1","Happiness +1")+cycle+inactive,row++);
        EffectLine(f,"Prosperity",TownTr("繁栄 +5 / 施設効果","Prosperity +5 / facility")+inactive,row++);
        f.warningStart=f.ui.labels.length();
        if(!state.RoadAdjacent(x,z))EffectLine(f,"Warning",TownTr("道路に隣接していません","No adjacent road"),row++);
        if(!state.IsLit(x,z))EffectLine(f,"Warning",TownTr("光が届いていません","Not illuminated"),row++);
        for(uint i=0;i<f.ui.objects.length();++i)f.offsets.insertLast(f.ui.objects[i].GetTransform().GetTranslate()-Vector3(0,720,0));f.ui.Show(false);forecasts.insertLast(f);
    }
    void Removed(int x,int z){for(int i=int(forecasts.length())-1;i>=0;--i)if(forecasts[uint(i)].x==x&&forecasts[uint(i)].z==z){DeleteUI(forecasts[uint(i)].ui);forecasts.removeAt(uint(i));}Rebuild();}
    void Update(float dt,bool hidden,bool buildMenu){
        if(buildingCount!=state.Buildings().Size())Rebuild();
        for(uint i=0;i<bars.length();++i){TownFacilityBar@ bar=bars[i];Json@ b=state.BuildingAt(bar.x,bar.z);Vector2 p;
            bool away=b !is null&&b.GetBool("dispatchActive"),show=!hidden&&(bar.kind==2||away)&&Project(Anchor(bar.x,bar.z),p);if(show&&buildMenu&&p.x<390)show=false;bar.ui.Show(show);if(!show)continue;
            float remaining=bar.kind==2?state.ProductionInterval()-float(state.data.GetFloat("productionClock")):float(b.GetFloat("dispatchRemaining"));
            float duration=bar.kind==2?state.ProductionInterval():TownClamp(float(b.GetFloat("dispatchDuration",45)),1,86400),progress=TownClamp(1-remaining/duration,0,1);
            bool working=bar.kind==4||state.IsWorking(bar.x,bar.z);if(!working)progress=0;
            bar.ui.objects[0].GetTransform().SetTranslate(Vector3(p.x,720-p.y,0));
            bar.fill.GetTransform().SetTranslate(Vector3(p.x-35+35*progress,720-p.y,0));bar.fill.GetTransform().SetScale(Vector3(70*progress,5,1));bar.fill.SetActive(progress>.0001f);
            SpriteRenderer@ sprite;bar.fill.GetComponent(@sprite);sprite.SetInstanceColor(bar.kind==4?Vector4(.3f,.7f,1,1):Vector4(.25f,.9f,.55f,1));
            bar.ui.objects[2].GetTransform().SetTranslate(Vector3(p.x-36,720-p.y-8,0));
            bar.label.SetText(!working?TownTr("停止中","Inactive"):(bar.kind==4?TownTr("帰還 ","Return "):TownTr("生産 ","Produce "))+int(remaining+.999f)+"s"+(bar.kind==2&&state.data.GetFloat("energy")<10?TownTr(" / 光不足"," / low light"):""));
            bar.ui.RefreshLabels();
        }
        for(int i=int(forecasts.length())-1;i>=0;--i){TownParameterForecast@ f=forecasts[uint(i)];f.age+=dt;if(f.age>=2.6f){DeleteUI(f.ui);forecasts.removeAt(uint(i));continue;}
            Vector2 p;bool show=!hidden&&Project(Anchor(f.x,f.z),p);if(show&&buildMenu&&p.x<390)show=false;f.ui.Show(show);if(!show)continue;
            float alpha=TownClamp(1-f.age/2.6f,0,1);float y=720-p.y+float(f.offsets.length()/2)*23+15+f.age*25;
            uint textIndex=0;for(uint j=0;j<f.ui.objects.length();++j){Object@ o=f.ui.objects[j];o.GetTransform().SetTranslate(f.offsets[j]+Vector3(p.x,y,0));SpriteRenderer@ sprite;TextRenderer@ text;if(o.GetComponent(@sprite))sprite.SetInstanceColor(Vector4(1,1,1,alpha));if(o.GetComponent(@text)){text.SetColor(textIndex>=f.warningStart?Vector4(1,.25f,.25f,alpha):Vector4(.6f,1,.7f,alpha));++textIndex;}}f.ui.RefreshLabels();
        }
    }
}

class TownGuildMenu {
    TownUI@ ui=TownUI();TownState@ state;bool visible=false;int x=-1,z=-1;TextRenderer@ title;TextRenderer@ status;
    void Initialize(Scene@ scene,TownState@ data){@state=data;ui.Initialize(scene);ui.layer=65;ui.Rect(640,490,430,170,Vector4(.02f,.07f,.09f,.97f));@title=ui.Text("",440,420,21);@status=ui.Text("",440,452,17);ui.Text(TownTr("報酬：木材","Reward: Wood")+state.rules.GetInt("dispatchWood",20)+TownTr(" / 石材"," / Stone")+state.rules.GetInt("dispatchStone",15)+TownTr(" / 光結晶"," / Crystal")+state.rules.GetInt("dispatchCrystal",6),440,481,15);ui.Button(TownTr("NPC派遣","Dispatch NPC"),450,526);ui.Button(TownTr("閉じる","Close"),710,526);Show(false);}
    void Show(bool show){visible=show;ui.Show(show);}
    void Open(int cx,int cz){x=cx;z=cz;Show(true);Refresh();}
    void Refresh(){Json@ b=state.BuildingAt(x,z);title.SetText(TownTr("ギルド","Guild")+" ("+(x+1)+", "+(z+1)+")");status.SetText(b !is null&&b.GetBool("dispatchActive")?TownTr("帰還まで ","Returns in ")+int(b.GetFloat("dispatchRemaining")+.999f)+"s":(state.IsWorking(x,z)?TownTr("NPCは待機中です。","NPC ready to depart."):TownTr("道路と街灯が必要です。","Requires a road and streetlight.")));}
    void Update(){if(!visible)return;int choice=ui.Clicked();if(choice==0)state.DispatchAt(x,z);if(choice==1)Show(false);Refresh();}
}
