#include "TownState.as"
#include "TownPresentation.as"
#include "TownBuildings.as"
#include "TownCatalog.as"

Vector4 TownDeltaColor(int delta,float alpha) { return delta>=0?Vector4(.25f,1,.45f,alpha):Vector4(1,.25f,.25f,alpha); }
float TownBurstAlpha(float age) { float t=TownClamp(1-age/1.3f,0,1);return t*t; }
string TownForecastNumber(float value){int hundred=int(Abs(value)*100+.5f),fraction=hundred%100;string decimals=fraction==0?"":("."+(fraction<10?"0":"")+fraction);if(fraction!=0&&fraction%10==0)decimals=decimals.substr(0,decimals.length()-1);return (hundred==0?"":(value>0?"+":"-"))+int(hundred/100)+decimals;}
Vector4 TownForecastColor(float value){return Abs(value)<.005f?Vector4(.65f,.7f,.72f,1):(value>0?Vector4(.25f,1,.45f,1):Vector4(1,.25f,.25f,1));}
string TownWrapNotice(const string &in text) {
    string result;float width=0;
    for(uint i=0;i<text.length();++i){uint8 c=text[i];if((c&0xC0)!=0x80){if(width>=19){result+="\n";width=0;}width+=c<128?.5f:1.0f;}result+=text.substr(i,1);}
    return result;
}
class TownResourceBurst { Object@ object;TextRenderer@ text;float age=0,x=0,y=58;int amount=0; }
TownResourceBurst@ TownCreateBurst(Scene@ scene,Object@ target,int amount,float x,float y) {
    TownResourceBurst@ b=TownResourceBurst();b.amount=amount;b.x=x;b.y=y;
    @b.object=scene.CreateObject("TownResourceDelta");b.object.AddComponent(@b.text);
    b.text.SetTargetObject(target);b.text.SetPipelineName("Text2D.BlendNormal");b.text.SetFontName("NotoSansJP-Regular");b.text.SetFontSize(18);b.text.SetRenderPriority(90);b.text.SetOutlineWidth(0);b.text.SetText((amount>0?"+":"")+amount);return b;
}
bool TownAdvanceBurst(Scene@ scene,TownResourceBurst@ b,float dt) {
    b.age+=dt;if(b.age>=1.3f){scene.DeleteObject(b.object);return true;}
    b.object.GetTransform().SetTranslate(Vector3(b.x,720-b.y+b.age*34,0));b.text.SetFontSize(b.text.GetFontSize());b.text.SetColor(TownDeltaColor(b.amount,TownBurstAlpha(b.age)));return false;
}
class TownEventToast { TownUI@ ui=TownUI();array<Vector3>@ local=array<Vector3>();float age=0,stackY=-100; }
class TownCityHUD {
    Scene@ scene;TownUI@ ui=TownUI();
    array<TextRenderer@>@ values=array<TextRenderer@>();array<double>@ previous=array<double>();
    array<TextRenderer@>@ parameterValues=array<TextRenderer@>();
    array<TextRenderer@>@ forecastValues=array<TextRenderer@>();
    array<TownResourceBurst@>@ bursts=array<TownResourceBurst@>();array<TownEventToast@>@ toasts=array<TownEventToast@>();
    array<Json@>@ waiting=array<Json@>();array<TextRenderer@>@ refinedValues=array<TextRenderer@>();array<TextRenderer@>@ refinedRates=array<TextRenderer@>();array<int>@ previousRefined=array<int>();
    TextRenderer@ stats;TextRenderer@ notice;TextRenderer@ selection;Object@ selectionObject;
    array<float>@ resourceX={240,490,740,990};
    void Initialize(Scene@ owner,TownState@ state) {
        @scene=owner;ui.Initialize(owner);ui.Text("LIGHT TOWN",22,15,23);
        array<string> icons={"Wood","Stone","Crystal","Light"};
        for(uint i=0;i<4;++i){ui.Image("Application/UI/Resources/"+icons[i]+".png",resourceX[i]-21,28,30);values.insertLast(ui.Text("",resourceX[i],15,21));previous.insertLast(Resource(state,i));forecastValues.insertLast(ui.Text("",resourceX[i]+65,37,12));}
        array<string> parameterIcons={"Happiness","Safety","Prosperity"};
        for(uint i=0;i<3;++i){float x=240+float(i)*190;ui.Image("Application/UI/Resources/"+parameterIcons[i]+".png",x-18,64,23);parameterValues.insertLast(ui.Text("",x,53,17));forecastValues.insertLast(ui.Text("",x+45,73,12));}
        @stats=ui.Text("",810,53,16);
        ui.Rect(640,678,1260,76,Vector4(.025f,.06f,.08f,.75f));
        array<string> buttons={TownTr("建築 [B]","Build [B]"),TownTr("遠征 [Tab]","Expedition [Tab]"),TownTr("ショップ","Shop"),TownTr("一括NPC派遣","Dispatch all"),TownTr("保存","Save"),TownTr("設定","Settings"),TownTr("初期化","New Game")};
        array<float> x={22,200,390,550,760,890,1040};for(uint i=0;i<buttons.length();++i)ui.Button(buttons[i],x[i],664);
        @selection=ui.Text(TownTr("Bで建築 / 道路＋街灯で施設が稼働","B: build / Road adjacency + streetlight activates facilities"),22,610,17);
        @selectionObject=ui.objects[ui.objects.length()-1];
        @notice=ui.Text("",22,634,16);
        array<string> refinedIcons={"Wood","Stone","Crystal"};for(uint i=0;i<3;++i){float xx=240+float(i)*300;ui.Image("Application/UI/Resources/"+refinedIcons[i]+".png",xx-18,111,22);refinedValues.insertLast(ui.Text("",xx,98,15));refinedRates.insertLast(ui.Text("",xx+65,115,12));array<string> keys={"planks","masonry","refinedCrystal"};previousRefined.insertLast(int(state.data.GetInt(keys[i])));}
        Refresh(state);
    }
    double Resource(TownState@ state,uint index) {
        if(index==0)return int(state.data.GetInt("wood"));if(index==1)return int(state.data.GetInt("stone"));if(index==2)return int(state.data.GetInt("crystal"));return state.data.GetFloat("energy");
    }
    void LayoutSelection(bool building){selectionObject.GetTransform().SetTranslate(Vector3(building?400:22,110,0));}
    void Burst(uint index,int amount) {
        bursts.insertLast(TownCreateBurst(scene,scene.GetObject("TownCanvas"),amount,resourceX[index]+90+float(bursts.length()%3)*12,32));
    }
    void Toast(const string &in message,Json@ effects=null) {
        if(toasts.length()>=3){Json@ queued=effects is null?Json():effects;queued.SetString("text",message);waiting.insertLast(queued);return;}
        TownEventToast@ toast=TownEventToast();toast.ui.Initialize(scene);toast.ui.layer=85;
        toast.ui.Rect(160,44,320,88,Vector4(.035f,.09f,.1f,.96f));
        toast.ui.ButtonArea("×",20,16,28,28,11,3);toast.ui.Text(TownTr("街のニュース","Town news"),44,8,18);toast.ui.Text(TownWrapNotice(message),16,35,16);
        if(effects !is null){array<string> keys={"happiness","safety","energy"},labels={TownTr("幸福 ","Happiness "),TownTr("治安 ","Safety "),TownTr("光 ","Light ")};float xx=16;for(uint i=0;i<3;++i){float delta=float(effects.GetFloat(keys[i]));if(delta==0)continue;TextRenderer@ t=toast.ui.Text(labels[i]+(delta>0?"+":"")+delta,xx,66,14);t.SetColor(TownForecastColor(delta));xx+=100;}}
        toast.stackY=-140-float(toasts.length())*100;
        for(uint i=0;i<toast.ui.objects.length();++i){toast.local.insertLast(toast.ui.objects[i].GetTransform().GetTranslate());toast.ui.objects[i].GetTransform().SetTranslate(toast.local[i]+Vector3(1285,toast.stackY,0));}
        toasts.insertLast(toast);
    }
    void DismissToast(uint index){
        if(index>=toasts.length())return;
        TownEventToast@ toast=toasts[index];for(uint j=0;j<toast.ui.objects.length();++j)if(IsValidObject(toast.ui.objects[j]))scene.DeleteObject(toast.ui.objects[j]);
        toasts.removeAt(index);
    }
    void Update(float dt,TownState@ state) {
        for(uint i=0;i<4;++i){double now=Resource(state,i);if(now!=previous[i])Burst(i,int(TownClamp(float(now-previous[i]),-2147480000.0f,2147480000.0f)));previous[i]=now;}
        for(int i=int(bursts.length())-1;i>=0;--i)if(TownAdvanceBurst(scene,bursts[uint(i)],dt))bursts.removeAt(uint(i));
        Json@ events=state.TakeEvents();for(uint i=0;i<events.Size();++i)Toast(events.At(i).GetString("text"),events.At(i));
        for(int i=int(toasts.length())-1;i>=0;--i){
            TownEventToast@ t=toasts[uint(i)];t.age+=dt;
            if(t.age>=5||t.ui.Clicked()==0){DismissToast(uint(i));continue;}
            float f=t.age<.4f?TownClamp(t.age/.4f,0,1):(t.age>4.6f?TownClamp((5-t.age)/.4f,0,1):1);
            float ease=1-(1-f)*(1-f)*(1-f);float x=1285-345*ease;
            t.stackY=Easing::Lerp(t.stackY,-140-float(i)*100,TownClamp(dt*12,0,1));
            for(uint j=0;j<t.ui.objects.length();++j)t.ui.objects[j].GetTransform().SetTranslate(t.local[j]+Vector3(x,t.stackY,0));
            for(uint j=0;j<t.ui.labels.length();++j)t.ui.labels[j].SetFontSize(t.ui.labels[j].GetFontSize());
        }
        while(toasts.length()<3&&waiting.length()>0){Json@ next=waiting[0];waiting.removeAt(0);Toast(next.GetString("text"),next);}
        Refresh(state);
    }
    void Refresh(TownState@ state) {
        array<string> names={TownTr("木材 ","Wood "),TownTr("石材 ","Stone "),TownTr("光結晶 ","Crystals "),TownTr("光 ","Light ")};
        for(uint i=0;i<4;++i)values[i].SetText(names[i]+(i==3?""+state.data.GetFloat("energy"):""+Resource(state,i)));
        array<string> refined={"planks","masonry","refinedCrystal"},refinedNames={TownTr("加工木材 ","Planks "),TownTr("加工石材 ","Masonry "),TownTr("加工光結晶 ","Refined ")};array<float>@ production=state.ProductionDeltas();for(uint i=0;i<3;++i){int value=int(state.data.GetInt(refined[i]));refinedValues[i].SetText(refinedNames[i]+value);float rate=production[7+i]/state.ProductionInterval();refinedRates[i].SetText(TownForecastNumber(rate)+TownTr("/秒","/s"));refinedRates[i].SetColor(TownForecastColor(rate));if(value!=previousRefined[i])bursts.insertLast(TownCreateBurst(scene,scene.GetObject("TownCanvas"),value-previousRefined[i],240+float(i)*300,110));previousRefined[i]=value;}
        parameterValues[0].SetText(TownTr("幸福 ","Happiness ")+int(state.data.GetFloat("happiness")));parameterValues[1].SetText(TownTr("治安 ","Safety ")+int(state.data.GetFloat("safety")));parameterValues[2].SetText(TownTr("繁栄 ","Prosperity ")+int(state.data.GetFloat("prosperity")));
        stats.SetText(TownTr("稼働施設 ","Working ")+state.WorkingFacilities()+TownTr(" / 派遣中 "," / Away ")+dataDispatchCount(state));
        notice.SetText(state.saveFailed?TownTr("保存に失敗しました。保存ボタンで再試行できます。","Save failed. Retry with Save."):state.message);
        array<float>@ delta=state.ProductionDeltas();float interval=state.ProductionInterval();
        for(uint i=0;i<7;++i){float rate=delta[i]/interval;forecastValues[i].SetText(TownForecastNumber(rate)+TownTr("/秒","/s"));forecastValues[i].SetColor(TownForecastColor(rate));}
    }
    int dataDispatchCount(TownState@ state){return int(state.data.GetInt("dispatchCount"));}
}
class TownBuildMenu {
 Scene@ scene;TownState@ state;TownUI@ ui=TownUI();TownUI@ details=TownUI();TextRenderer@ description;
 array<TownBuildChoice@>@ catalog=TownBuildCatalog();array<Object@>@ rows=array<Object@>();array<Object@>@ names=array<Object@>();array<Object@>@ costs=array<Object@>();array<Object@>@ iconObjects=array<Object@>();array<Object@>@ iconDisplays=array<Object@>();
 array<int>@ categories={6,0,1,2,3,4,7,5};Object@ cursor;int selected=-1,category=6;float amount=0,clock=0,scroll=0;bool visible=false,rendered=true,detailShown=true;
 bool DetailsDisplayed(){return rendered&&GetSettingBool("town.buildDetails",true);}
 void ToggleDetails(){SetSettingBool("town.buildDetails",!GetSettingBool("town.buildDetails",true));RefreshDetails();}
 void RefreshDetails(){bool show=DetailsDisplayed();if(detailShown!=show){details.Show(show);detailShown=show;}ui.buttonTexts[ui.buttonTexts.length()-1].SetText(GetSettingBool("town.buildDetails",true)?TownTr("詳細：表示","Details: ON"):TownTr("詳細：非表示","Details: OFF"));}
 bool Displayed(){return amount>.0001f;}
 bool CanBuild(){return selected>=0;}
 bool ContainsMouse(){ScreenBufferViewport@ v;scene.GetObject("TownBuildDisplay").GetComponent(@v);return Displayed()&&v.IsMouseOverOffscreen();}
 int Kind(uint i){return i<catalog.length()?catalog[i].kind:6;}
 Json@ ChoiceDefinition(uint i){TownBuildChoice@ c=catalog[i];return state.Definition(c.kind,c.tier,c.recipe);}
 void Initialize(Scene@ owner,TownState@ data){@scene=owner;@state=data;ui.Initialize(scene,"TownBuildScreen","TownBuildCamera","TownBuildPanelTemplate");ui.canvasHeight=550;ui.layer=20;ui.Rect(185,275,370,550,Vector4(.025f,.06f,.08f,1));
  for(int old=0;old<7;++old){array<string> parts={"Screen","Camera","Model","Light","Display"};for(uint j=0;j<parts.length();++j)scene.GetObject("TownBuildIcon"+old+parts[j]).SetActive(false);}
  for(uint i=0;i<=catalog.length();++i){bool remove=i==catalog.length();Json@ d=remove?null:ChoiceDefinition(i);rows.insertLast(ui.ButtonArea(remove?TownTr("削除","Remove"):TownBuildingName(d),185,150,350,60,120,130));names.insertLast(ui.objects[ui.objects.length()-1]);ui.Text(remove?TownTr("施設・道路 / 返金なし","No refunds"):"木 "+d.GetInt("wood")+" / 石 "+d.GetInt("stone")+" / 晶 "+d.GetInt("crystal"),120,156,12);costs.insertLast(ui.objects[ui.objects.length()-1]);if(!remove)CreateIcon(i);}
  ui.layer=100;ui.Rect(185,57,370,114,Vector4(.025f,.06f,.08f,1));ui.Text(TownTr("建築 / カテゴリ","Construction / Category"),12,8,18);
  array<string> labels={TownTr("道路","Road"),TownTr("街灯","Light"),TownTr("住居","Homes"),TownTr("加工工場","Factory"),TownTr("ショップ","Shop"),TownTr("ギルド","Guild"),TownTr("警察署","Police"),TownTr("目標","Goal")};for(uint i=0;i<8;++i){float x=47+float(i%4)*91,y=51+float(i/4)*35;ui.ButtonArea(labels[i],x,y,86,30,x-35,y-10);ui.buttonTexts[ui.buttonTexts.length()-1].SetFontSize(14);}
  TextRenderer@ marker=ui.Text("▶",14,140,18);marker.SetColor(Vector4(1,.85f,.15f,1));@cursor=ui.objects[ui.objects.length()-1];ui.layer=100;ui.Rect(185,524,370,52,Vector4(.025f,.06f,.08f,1));ui.Text(TownTr("道路：左ドラッグ / 他：Shift＋左ドラッグ","Roads: left drag / Others: Shift + left drag"),12,506,12);ui.ButtonArea("▲",295,535,32,25,285,523);ui.ButtonArea("▼",337,535,32,25,327,523);
  ui.ButtonArea(TownTr("詳細：表示","Details: ON"),170,535,155,25,104,523);ui.buttonTexts[ui.buttonTexts.length()-1].SetFontSize(13);
  details.Initialize(scene);details.layer=60;details.Rect(1085,320,350,350,Vector4(.025f,.07f,.09f,.94f));@description=details.Text("",925,165,13);Select(0);Render(false);
 }
 void CreateIcon(uint i){TownBuildChoice@ choice=catalog[i];Json@ d=ChoiceDefinition(i);int source=choice.kind<=6?choice.kind:1;string prefix="TownBuildIcon"+source;array<string> suffix={"Screen","Camera","Model","Light","Display"};array<Object@> group;
  for(uint j=0;j<suffix.length();++j){Object@ o=scene.CloneObject(scene.GetObject(prefix+suffix[j]),"TownCatalogIcon"+i+suffix[j]);o.SetActive(false);group.insertLast(o);iconObjects.insertLast(o);}
  ScreenBufferObject@ screen;group[0].GetComponent(@screen);screen.SetName("TownCatalogIcon"+i);screen.SetSize(256,256);
  CameraRenderer@ camera;group[1].GetComponent(@camera);camera.SetTargetObject(group[0]);group[1].GetTransform().SetTranslate(Vector3(2000+float(i)*20,2.8f,-7));group[1].GetTransform().SetRotate(Vector3(.23f,0,0));
  MeshFilter@ filter;group[2].GetComponent(@filter);filter.SetMeshHandle(GetModelHandleFromAssetPath(choice.kind==6?TownRoadMesh(3):TownVariantMesh(choice.kind,choice.tier,choice.recipe)));group[2].GetTransform().SetTranslate(Vector3(2000+float(i)*20,0,0));group[2].GetTransform().SetScale(Vector3(1,1,1));MeshRenderer@ renderer;group[2].GetComponent(@renderer);renderer.SetTargetObject(group[0]);renderer.SetInstanceColor(d.GetVector4("color",Vector4(1,1,1,1)));renderer.SetMaterialName("TownFacility");
  LightRenderer@ lamp;group[3].GetComponent(@lamp);lamp.SetTargetObject(group[0]);ScreenBufferViewport@ view;group[4].GetComponent(@view);view.SetSourceObject(group[0]);iconDisplays.insertLast(group[4]);
 }
 void Select(int index){if(index<0||index>int(catalog.length()))return;selected=index;category=Kind(uint(index));description.SetText(index<int(catalog.length())?TownDescription(state,ChoiceDefinition(uint(index))):TownTr("クリックで削除。\nShift＋左ドラッグで連続削除。\n返金なし。","Click to remove.\nShift + left drag to erase multiple cells.\nNo refund."));Layout();}
 void Layout(){cursor.SetActive(false);uint row=0;for(uint i=0;i<rows.length();++i){bool matches=Kind(i)==category;float y=148+float(row)*65-scroll;if(matches)++row;bool show=Displayed()&&matches&&y>120&&y<493;if(int(i)==selected&&show){cursor.SetActive(true);cursor.GetTransform().SetTranslate(Vector3(12,550-y+10,0));}rows[i].SetActive(show);names[i].SetActive(show);costs[i].SetActive(show);rows[i].GetTransform().SetTranslate(Vector3(185,550-y,0));names[i].GetTransform().SetTranslate(Vector3(120,550-y+18,0));costs[i].GetTransform().SetTranslate(Vector3(120,550-y-8,0));SpriteRenderer@ r;rows[i].GetComponent(@r);r.SetInstanceColor(int(i)==selected?Vector4(.14f,.38f,.4f,1):Vector4(.04f,.1f,.12f,1));
   if(i<catalog.length()){for(uint j=0;j<5;++j)iconObjects[i*5+j].SetActive(show);iconDisplays[i].GetTransform().SetTranslate(Vector3(65,550-y,0));}}
 }
 void SetVisible(bool show){visible=show;}
 void Render(bool show){if(rendered==show)return;rendered=show;ui.Show(show);RefreshDetails();scene.GetObject("TownBuildScreen").SetActive(show);scene.GetObject("TownBuildCamera").SetActive(show);scene.GetObject("TownBuildDisplay").SetActive(show);if(show)Layout();else for(uint i=0;i<iconObjects.length();++i)iconObjects[i].SetActive(false);}
 void Animate(float dt,bool allowed=true){amount=TownClamp(amount+(visible?1:-1)*dt/.28f,0,1);Render(Displayed()&&allowed);if(!Displayed()||!allowed)return;clock+=dt;for(uint i=0;i<catalog.length();++i)if(iconObjects[i*5+2].IsActive())iconObjects[i*5+2].GetTransform().SetRotate(Vector3(0,clock*.75f,0));float eased=1-(1-amount)*(1-amount)*(1-amount),left=10-390*(1-eased),start=Max(left,0.0f),width=left+370-start;Object@ display=scene.GetObject("TownBuildDisplay");if(width<=0){display.SetActive(false);return;}Transform@ main=scene.GetObject("TownDisplayViewport").GetTransform();float unit=main.GetScale().x/1280;display.GetTransform().SetTranslate(main.GetTranslate()+Vector3((start+width*.5f-640)*unit,10*unit,0));display.GetTransform().SetScale(Vector3(width*unit,550*unit,1));ScreenBufferViewport@ v;display.GetComponent(@v);SpriteRenderer@ r=v.GetSpriteRenderer();r.SetInstanceUvPivot(Vector2(0,0));r.SetInstanceUvScale(Vector2(width/370,1));r.SetInstanceUvTranslate(Vector2((start-left)/370,0));if(detailShown)details.RefreshLabels();}
 void ScrollBy(float delta){int count=0;for(uint i=0;i<rows.length();++i)if(Kind(i)==category)++count;scroll=TownClamp(scroll+delta,0,Max(0.0f,float(count)*65-360));Layout();}
 int Update(float dt){if(!visible||amount<.999f)return -1;ScreenBufferViewport@ v;scene.GetObject("TownBuildDisplay").GetComponent(@v);Vector2 p;if(!v.TryGetOffscreenMousePosition(p))return -1;float y=p.y/1080*550;int click=ui.Clicked(),firstCategory=int(catalog.length())+1;if(click>=firstCategory&&click<firstCategory+8){category=categories[uint(click-firstCategory)];selected=-1;description.SetText(TownTr("カテゴリから建物を選択してください","Choose a building in this category"));scroll=0;Layout();return -1;}if(click==firstCategory+8)ScrollBy(-65);if(click==firstCategory+9)ScrollBy(65);if(click==firstCategory+10)ToggleDetails();if(y>=115&&y<497){float wheel=GetCommandValue("TownCameraScroll");if(wheel!=0)ScrollBy(-wheel*65);if(click>=0&&click<firstCategory){Select(click);return click;}}return -1;}
}
