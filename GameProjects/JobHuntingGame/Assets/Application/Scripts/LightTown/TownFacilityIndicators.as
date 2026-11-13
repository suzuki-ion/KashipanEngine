#include "TownWorld.as"
#include "TownCatalog.as"

bool TownRayFacility(const Vector3 &in origin,const Vector3 &in direction,const Vector3 &in center,const Vector3 &in half,float &out hit){
    array<float> o={origin.x,origin.y,origin.z},d={direction.x,direction.y,direction.z},c={center.x,center.y,center.z},h={half.x,half.y,half.z};float nearT=0,farT=1;
    for(uint i=0;i<3;++i){if(Abs(d[i])<.000001f){if(o[i]<c[i]-h[i]||o[i]>c[i]+h[i])return false;continue;}float a=(c[i]-h[i]-o[i])/d[i],b=(c[i]+h[i]-o[i])/d[i];if(a>b){float swap=a;a=b;b=swap;}if(a>nearT)nearT=a;if(b<farT)farT=b;if(nearT>farT)return false;}hit=nearT;return true;
}

// ワールド座標に追従し、常にカメラへ正対するスクリーン空間ビルボード。
// TownCanvas上の論理座標を使うため、Letterboxやウィンドウ変更にも追従する。
class TownFacilityBar {
    TownUI@ ui=TownUI();Object@ fill;TextRenderer@ label;int x,z,kind;
    Transform@ backgroundTransform;Transform@ fillTransform;Transform@ labelTransform;SpriteRenderer@ sprite;
    Transform@ rootTransform;Vector3 anchor;
    bool visible=false;string lastText;
}
class TownParameterForecast {
    TownUI@ ui=TownUI();array<Vector3>@ offsets=array<Vector3>();
    array<Vector4>@ colors=array<Vector4>();int x,z;float age=0;
    uint warningStart=9999;
}
class TownFacilityIndicators {
    Scene@ scene;TownState@ state;TownWorld@ world;Object@ camera;
    array<TownFacilityBar@>@ bars=array<TownFacilityBar@>();
    array<TownParameterForecast@>@ forecasts=array<TownParameterForecast@>();
    uint buildingCount=0;
    Matrix4x4 viewMatrix,projectionMatrix;float nearClip=0;
    void Initialize(Scene@ owner,TownState@ data,TownWorld@ town){@scene=owner;@state=data;@world=town;@camera=scene.GetObject("TownCamera");Rebuild();}
    void DeleteUI(TownUI@ ui){for(uint i=0;i<ui.objects.length();++i)if(IsValidObject(ui.objects[i]))scene.DeleteObject(ui.objects[i]);}
    void Rebuild(){
        for(uint i=0;i<bars.length();++i)DeleteUI(bars[i].ui);bars.resize(0);
        Json@ list=state.Buildings();buildingCount=list.Size();
        for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);int kind=int(b.GetInt("kind"));if(kind!=2&&kind!=4)continue;
            TownFacilityBar@ bar=TownFacilityBar();bar.kind=kind;bar.x=int(b.GetInt("x"));bar.z=int(b.GetInt("z"));bar.ui.Initialize(scene);bar.ui.layer=44;
            bar.ui.Rect(0,0,76,9,Vector4(.02f,.035f,.045f,.9f));
            @bar.fill=bar.ui.Rect(0,0,70,5,Vector4(.25f,.9f,.55f,1));@bar.label=bar.ui.Text("",-36,8,13);bar.ui.Show(false);bars.insertLast(bar);
            @bar.backgroundTransform=bar.ui.objects[0].GetTransform();@bar.fillTransform=bar.fill.GetTransform();@bar.labelTransform=bar.ui.objects[2].GetTransform();bar.fill.GetComponent(@bar.sprite);
            bar.sprite.SetInstanceColor(kind==4?Vector4(.3f,.7f,1,1):Vector4(.25f,.9f,.55f,1));
            bar.anchor=world.Center(b)+Vector3(0,float(state.BuildingDefinition(b).GetFloat("height",2))+.4f,0);
            @bar.rootTransform=world.roots[uint(bar.z*state.GridSize()+bar.x)].GetTransform();
        }
    }
    Vector3 Anchor(int x,int z){Json@ b=state.BuildingAt(x,z);float h=b is null?1.0f:float(state.BuildingDefinition(b).GetFloat("height",2));return (b is null?world.Cell(x,z):world.Center(b))+Vector3(0,h+.4f+world.roots[uint(z*state.GridSize()+x)].GetTransform().GetTranslate().y,0);}
    bool Project(const Vector3 &in position,Vector2 &out pixel){
        Vector3 view=position.Transform(viewMatrix);
        if(view.z<=nearClip)return false;
        Vector3 ndc=view.Transform(projectionMatrix);
        pixel=Vector2((ndc.x+1)*640,(1-ndc.y)*360);return ndc.z<=1&&pixel.x>=0&&pixel.x<=1280&&pixel.y>=130&&pixel.y<=590;
    }
    void EffectLine(TownParameterForecast@ f,const string &in icon,const string &in text,int row,float delta=1){float y=float(row)*23;f.ui.Image("Application/UI/Resources/"+icon+".png",-44,y+8,20);f.ui.Text(text,-30,y,15);f.colors.insertLast(delta<0?Vector4(1,.25f,.25f,1):Vector4(.6f,1,.7f,1));}
    void Placed(int kind,int x,int z){
        if(kind==6)return;
        TownParameterForecast@ f=TownParameterForecast();f.x=x;f.z=z;f.ui.Initialize(scene);f.ui.layer=46;
        string cycle=" / "+int(state.ProductionInterval())+TownTr("秒","s"),inactive=state.IsWorking(x,z)?"":TownTr("（稼働時）"," (when active)");int row=0;
        if(kind==0)EffectLine(f,"Light",TownTr("光 +12","Light +12")+cycle+inactive,row++);
        array<string> icons={"Happiness","Safety","Prosperity"},names={TownTr("幸福","Happiness"),TownTr("治安","Safety"),TownTr("繁栄","Prosperity")};
        for(uint i=0;i<3;++i)if(state.placedParameterDeltas[i]!=0)EffectLine(f,icons[i],names[i]+(state.placedParameterDeltas[i]>0?" +":" ")+state.placedParameterDeltas[i]+TownTr(" / 建築効果"," / construction"),row++,state.placedParameterDeltas[i]);
        f.warningStart=f.ui.labels.length();
        if(!state.RoadAdjacent(x,z))EffectLine(f,"Warning",TownTr("道路に隣接していません","No adjacent road"),row++);
        if(state.RoadAdjacent(x,z)&&state.EntranceRoad(x,z)<0)EffectLine(f,"Warning",TownTr("入口が道路を向いていません","Entrance does not face a road"),row++);
        if(!state.IsLit(x,z))EffectLine(f,"Warning",TownTr("光が届いていません","Not illuminated"),row++);
        for(uint i=0;i<f.ui.objects.length();++i)f.offsets.insertLast(f.ui.objects[i].GetTransform().GetTranslate()-Vector3(0,720,0));f.ui.Show(false);forecasts.insertLast(f);
    }
    void Removed(int x,int z,bool rebuild=true){for(int i=int(forecasts.length())-1;i>=0;--i)if(forecasts[uint(i)].x==x&&forecasts[uint(i)].z==z){DeleteUI(forecasts[uint(i)].ui);forecasts.removeAt(uint(i));}if(rebuild)Rebuild();}
    void Update(float dt,bool hidden,bool buildMenu){
        if(buildingCount!=state.BuildingCount())Rebuild();
        viewMatrix=camera.GetTransform().GetWorldMatrix().Inverse();Camera3D@ lens;camera.GetComponent(@lens);nearClip=lens.GetNearClip();
        projectionMatrix.MakePerspectiveFovMatrix(lens.GetFovY(),lens.GetAspectRatio(),nearClip,lens.GetFarClip());
        for(uint i=0;i<bars.length();++i){TownFacilityBar@ bar=bars[i];Json@ b=state.BuildingAt(bar.x,bar.z);Vector2 p;
            bool away=b !is null&&b.GetBool("dispatchActive"),show=!hidden&&GetSettingInt("town.bars",0)!=2&&(bar.kind==2||away)&&Project(bar.anchor+Vector3(0,bar.rootTransform.GetTranslate().y,0),p);if(show&&buildMenu&&p.x<390)show=false;if(show!=bar.visible){bar.ui.Show(show);bar.visible=show;}if(!show)continue;
            float remaining=bar.kind==2?state.ProductionInterval()-float(state.data.GetFloat("productionClock")):float(b.GetFloat("dispatchRemaining"));
            float duration=bar.kind==2?state.ProductionInterval():TownClamp(float(b.GetFloat("dispatchDuration",45)),1,86400),progress=TownClamp(1-remaining/duration,0,1);
            bool working=bar.kind==4||state.IsWorking(bar.x,bar.z);if(bar.kind==2){int scale=state.Capacity(b);array<string> raw={"wood","stone","crystal"};if(state.data.GetInt(raw[uint(b.GetInt("recipe"))])<scale*state.Balance("processingInput",0,2))working=false;}if(!working)progress=0;
            bar.ui.objects[0].SetActive(GetSettingInt("town.bars",0)==0);
            bar.backgroundTransform.SetTranslate(Vector3(p.x,720-p.y,0));
            bar.fillTransform.SetTranslate(Vector3(p.x-35+35*progress,720-p.y,0));bar.fillTransform.SetScale(Vector3(70*progress,5,1));bar.fill.SetActive(progress>.0001f&&GetSettingInt("town.bars",0)==0);
            bar.labelTransform.SetTranslate(Vector3(p.x-36,720-p.y-8,0));
            string text=!working?TownTr("停止中","Inactive"):(bar.kind==4?TownTr("帰還 ","Return "):TownTr("生産 ","Produce "))+int(remaining+.999f)+"s"+(bar.kind==2&&state.data.GetFloat("energy")<10?TownTr(" / 光不足"," / low light"):"");
            if(text!=bar.lastText){bar.label.SetText(text);bar.lastText=text;}
            bar.ui.RefreshLabels();
        }
        for(int i=int(forecasts.length())-1;i>=0;--i){TownParameterForecast@ f=forecasts[uint(i)];f.age+=dt;if(f.age>=2.6f){DeleteUI(f.ui);forecasts.removeAt(uint(i));continue;}
            Vector2 p;bool show=!hidden&&GetSettingInt("town.bars",0)!=2&&Project(Anchor(f.x,f.z),p);if(show&&buildMenu&&p.x<390)show=false;f.ui.Show(show);if(!show)continue;
            float alpha=TownClamp(1-f.age/2.6f,0,1);float y=720-p.y+float(f.offsets.length()/2)*23+15+f.age*25;
            uint textIndex=0;for(uint j=0;j<f.ui.objects.length();++j){Object@ o=f.ui.objects[j];o.GetTransform().SetTranslate(f.offsets[j]+Vector3(p.x,y,0));SpriteRenderer@ sprite;TextRenderer@ text;if(o.GetComponent(@sprite))sprite.SetInstanceColor(Vector4(1,1,1,alpha));if(o.GetComponent(@text)){text.SetColor(textIndex>=f.warningStart?Vector4(1,.25f,.25f,alpha):Vector4(f.colors[textIndex].x,f.colors[textIndex].y,f.colors[textIndex].z,alpha));++textIndex;}}f.ui.RefreshLabels();
        }
    }
}

class TownGuildMenu {
 TownUI@ ui=TownUI();TownState@ state;bool visible=false;int x=-1,z=-1;TextRenderer@ title;TextRenderer@ status;Object@ dispatchLabel;Object@ statusObject;Object@ panel;
 float Bottom(){return GetSettingBool("town.buildDetails",true)?495:245;}
 void Initialize(Scene@ scene,TownState@ data){@state=data;ui.Initialize(scene);ui.layer=65;@panel=ui.Rect(1085,320,350,350,Vector4(.02f,.07f,.09f,.97f));@title=ui.Text("",925,160,16);@status=ui.Text("",925,200,13);@statusObject=ui.objects[ui.objects.length()-1];ui.Button(TownTr("NPC派遣","Dispatch"),930,462);@dispatchLabel=ui.objects[ui.objects.length()-1];ui.Button(TownTr("閉じる","Close"),1190,462);ui.Button(TownTr("詳細切替","Details"),1045,462);for(uint i=0;i<ui.buttonTexts.length();++i)ui.buttonTexts[i].SetFontSize(13);Show(false);}
 void Show(bool show){visible=show;ui.Show(show);}
 void Open(int cx,int cz){x=cx;z=cz;Show(true);Refresh();}
 void Refresh(){Json@ b=state.BuildingAt(x,z);if(b is null){Show(false);return;}Json@ d=state.BuildingDefinition(b);title.SetText(TownBuildingName(d)+" ("+(x+1)+", "+(z+1)+")");bool expanded=GetSettingBool("town.buildDetails",true);statusObject.SetActive(expanded);float bottom=Bottom();panel.GetTransform().SetTranslate(Vector3(1085,720-(145+bottom)*.5f,0));panel.GetTransform().SetScale(Vector3(350,bottom-145,1));for(uint i=0;i<ui.buttonObjects.length();++i){Transform@ t=ui.buttonObjects[i].GetTransform();Vector3 p=t.GetTranslate();t.SetTranslate(Vector3(p.x,720-(bottom-33),0));}if(expanded){string text=TownDescription(state,d)+"\n"+(state.IsWorking(x,z)?TownTr("稼働中","Operating"):TownTr("停止中：","Inactive: ")+state.OperationProblem(int(b.GetInt("kind")),x,z,int(b.GetInt("rotation")),state.Footprint(b)));if(b.GetBool("dispatchActive"))text+="\n"+TownTr("帰還まで ","Return in ")+int(b.GetFloat("dispatchRemaining")+.999f)+TownTr("秒","s");status.SetText(text);}bool guild=b.GetInt("kind")==4;ui.buttons[0].SetActive(guild);dispatchLabel.SetActive(guild);}
 void Update(){if(!visible)return;int choice=ui.Clicked();if(choice==0)state.DispatchAt(x,z);if(choice==1)Show(false);if(choice==2)SetSettingBool("town.buildDetails",!GetSettingBool("town.buildDetails",true));if(visible)Refresh();}
}
