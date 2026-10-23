#include "TownState.as"
#include "TownPresentation.as"

Vector4 TownDeltaColor(int delta,float alpha) { return delta>=0?Vector4(.25f,1,.45f,alpha):Vector4(1,.25f,.25f,alpha); }
float TownBurstAlpha(float age) { float t=TownClamp(1-age/1.3f,0,1);return t*t; }
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
    array<TextRenderer@>@ values=array<TextRenderer@>();array<int>@ previous=array<int>();
    array<TextRenderer@>@ parameterValues=array<TextRenderer@>();
    array<TownResourceBurst@>@ bursts=array<TownResourceBurst@>();array<TownEventToast@>@ toasts=array<TownEventToast@>();
    array<string>@ waiting=array<string>();
    TextRenderer@ stats;TextRenderer@ notice;TextRenderer@ selection;Object@ selectionObject;
    array<float>@ resourceX={240,490,740,990};
    void Initialize(Scene@ owner,TownState@ state) {
        @scene=owner;ui.Initialize(owner);ui.Text("LIGHT TOWN",22,15,23);
        array<string> icons={"Wood","Stone","Crystal","Light"};
        for(uint i=0;i<4;++i){ui.Image("Application/UI/Resources/"+icons[i]+".png",resourceX[i]-21,28,30);values.insertLast(ui.Text("",resourceX[i],15,21));previous.insertLast(Resource(state,i));}
        array<string> parameterIcons={"Happiness","Safety","Prosperity"};
        for(uint i=0;i<3;++i){float x=240+float(i)*190;ui.Image("Application/UI/Resources/"+parameterIcons[i]+".png",x-18,64,23);parameterValues.insertLast(ui.Text("",x,53,17));}
        @stats=ui.Text("",810,53,16);
        ui.Rect(640,678,1260,76,Vector4(.025f,.06f,.08f,.75f));
        array<string> buttons={TownTr("建築 [B]","Build [B]"),TownTr("遠征 [Tab]","Expedition [Tab]"),TownTr("ショップ","Shop"),TownTr("ギルド選択","Select guild"),TownTr("保存","Save"),TownTr("設定","Settings"),TownTr("初期化","New Game")};
        array<float> x={22,200,390,550,760,890,1040};for(uint i=0;i<buttons.length();++i)ui.Button(buttons[i],x[i],664);
        @selection=ui.Text(TownTr("Bで建築 / 道路＋街灯で施設が稼働","B: build / Road adjacency + streetlight activates facilities"),22,610,17);
        @selectionObject=ui.objects[ui.objects.length()-1];
        @notice=ui.Text("",22,634,16);Refresh(state);
    }
    int Resource(TownState@ state,uint index) {
        if(index==0)return int(state.data.GetInt("wood"));if(index==1)return int(state.data.GetInt("stone"));if(index==2)return int(state.data.GetInt("crystal"));return int(state.data.GetFloat("energy"));
    }
    void LayoutSelection(bool building){selectionObject.GetTransform().SetTranslate(Vector3(building?400:22,110,0));}
    void Burst(uint index,int amount) {
        bursts.insertLast(TownCreateBurst(scene,scene.GetObject("TownCanvas"),amount,resourceX[index]+90+float(bursts.length()%3)*12,32));
    }
    void Toast(const string &in message) {
        if(toasts.length()>=3){waiting.insertLast(message);return;}
        TownEventToast@ toast=TownEventToast();toast.ui.Initialize(scene);toast.ui.layer=85;
        toast.ui.Rect(160,44,320,88,Vector4(.035f,.09f,.1f,.96f));
        toast.ui.ButtonArea("×",20,16,28,28,11,3);toast.ui.Text(TownTr("街のニュース","Town news"),44,8,18);toast.ui.Text(TownWrapNotice(message),16,35,16);
        toast.stackY=-100-float(toasts.length())*100;
        for(uint i=0;i<toast.ui.objects.length();++i){toast.local.insertLast(toast.ui.objects[i].GetTransform().GetTranslate());toast.ui.objects[i].GetTransform().SetTranslate(toast.local[i]+Vector3(1285,toast.stackY,0));}
        toasts.insertLast(toast);
    }
    void DismissToast(uint index){
        if(index>=toasts.length())return;
        TownEventToast@ toast=toasts[index];for(uint j=0;j<toast.ui.objects.length();++j)if(IsValidObject(toast.ui.objects[j]))scene.DeleteObject(toast.ui.objects[j]);
        toasts.removeAt(index);
    }
    void Update(float dt,TownState@ state) {
        for(uint i=0;i<4;++i){int now=Resource(state,i);if(now!=previous[i])Burst(i,now-previous[i]);previous[i]=now;}
        for(int i=int(bursts.length())-1;i>=0;--i)if(TownAdvanceBurst(scene,bursts[uint(i)],dt))bursts.removeAt(uint(i));
        Json@ events=state.TakeEvents();for(uint i=0;i<events.Size();++i)Toast(events.At(i).GetString("text"));
        for(int i=int(toasts.length())-1;i>=0;--i){
            TownEventToast@ t=toasts[uint(i)];t.age+=dt;
            if(t.age>=5||t.ui.Clicked()==0){DismissToast(uint(i));continue;}
            float f=t.age<.4f?TownClamp(t.age/.4f,0,1):(t.age>4.6f?TownClamp((5-t.age)/.4f,0,1):1);
            float ease=1-(1-f)*(1-f)*(1-f);float x=1285-345*ease;
            t.stackY=Easing::Lerp(t.stackY,-100-float(i)*100,TownClamp(dt*12,0,1));
            for(uint j=0;j<t.ui.objects.length();++j)t.ui.objects[j].GetTransform().SetTranslate(t.local[j]+Vector3(x,t.stackY,0));
            for(uint j=0;j<t.ui.labels.length();++j)t.ui.labels[j].SetFontSize(t.ui.labels[j].GetFontSize());
        }
        while(toasts.length()<3&&waiting.length()>0){string next=waiting[0];waiting.removeAt(0);Toast(next);}
        Refresh(state);
    }
    void Refresh(TownState@ state) {
        array<string> names={TownTr("木材 ","Wood "),TownTr("石材 ","Stone "),TownTr("光結晶 ","Crystals "),TownTr("光 ","Light ")};
        for(uint i=0;i<4;++i)values[i].SetText(names[i]+Resource(state,i));
        parameterValues[0].SetText(TownTr("幸福 ","Happiness ")+int(state.data.GetFloat("happiness")));parameterValues[1].SetText(TownTr("治安 ","Safety ")+int(state.data.GetFloat("safety")));parameterValues[2].SetText(TownTr("繁栄 ","Prosperity ")+int(state.data.GetFloat("prosperity")));
        stats.SetText(TownTr("稼働施設 ","Working ")+state.WorkingFacilities()+TownTr(" / 派遣中 "," / Away ")+dataDispatchCount(state));
        notice.SetText(state.saveFailed?TownTr("保存に失敗しました。保存ボタンで再試行できます。","Save failed. Retry with Save."):state.message);
    }
    int dataDispatchCount(TownState@ state){return int(state.data.GetInt("dispatchCount"));}
}
class TownBuildMenu {
    Scene@ scene;TownUI@ ui=TownUI();bool visible=false;float clock=0,amount=0,scroll=0;
    array<Object@>@ rows=array<Object@>();array<Object@>@ names=array<Object@>();array<Object@>@ costs=array<Object@>();Object@ cursor;Object@ removeIcon;int selected=-1;
    bool Displayed(){return amount>.0001f;}
    bool ContainsMouse(){ScreenBufferViewport@ viewport;scene.GetObject("TownBuildDisplay").GetComponent(@viewport);return Displayed()&&viewport.IsMouseOverOffscreen();}
    void Initialize(Scene@ owner,TownState@ state){
        @scene=owner;ui.Initialize(scene,"TownBuildScreen","TownBuildCamera","TownBuildPanelTemplate");ui.canvasHeight=550;ui.layer=20;ui.Rect(185,275,370,550,Vector4(.025f,.06f,.08f,1));
        for(int k=0;k<8;++k){Json@ d=k<7?state.Definition(k):null;string title=k<7?TownBuildingName(d):TownTr("削除","Remove");rows.insertLast(ui.ButtonArea(title,185,float(100+k*60),350,56,122,float(78+k*60)));names.insertLast(ui.objects[ui.objects.length()-1]);ui.Text(k<7?TownTr("木","W ")+d.GetInt("wood")+" / "+TownTr("石","S ")+d.GetInt("stone")+" / "+TownTr("結晶","C ")+d.GetInt("crystal"):TownTr("施設・道路 / 返金なし","Facility / road, no refund"),122,float(104+k*60),14);costs.insertLast(ui.objects[ui.objects.length()-1]);}
        @removeIcon=ui.Image("Application/UI/Resources/Remove.png",65,520,42);
        TextRenderer@ arrow=ui.Text("▶",20,86,18);arrow.SetColor(Vector4(1,.85f,.25f,1));@cursor=ui.objects[ui.objects.length()-1];
        ui.layer=100;ui.Rect(185,35,370,70,Vector4(.025f,.06f,.08f,1));ui.Text(TownTr("建築 [Bで閉じる]","Construction [B]"),15,18,20);
        ui.ButtonArea("▲",295,42,32,28,285,28);ui.ButtonArea("▼",337,42,32,28,327,28);
        ui.Rect(185,527.5f,370,45,Vector4(.025f,.06f,.08f,1));ui.Text(TownTr("ホイール / ▲▼：スクロール","Wheel / ▲▼: scroll"),15,518,14);
        Select(6);Render(false);
    }
    void Select(int kind){
        if(kind<0||kind>=int(rows.length()))return;
        if(kind!=selected){float center=float(100+kind*60)-scroll;if(center+28>505)scroll+=center+28-505;if(center-28<70)scroll-=70-(center-28);scroll=TownClamp(scroll,0,45);}selected=kind;
        for(uint i=0;i<rows.length();++i){SpriteRenderer@ sprite;rows[i].GetComponent(@sprite);sprite.SetInstanceColor(int(i)==selected?(selected==7?Vector4(.45f,.12f,.12f,1):Vector4(.12f,.35f,.38f,1)):Vector4(.04f,.1f,.12f,1));}Layout();
    }
    void Layout(){
        for(uint i=0;i<rows.length();++i){float y=float(100+i*60)-scroll;rows[i].GetTransform().SetTranslate(Vector3(185,550-y,0));names[i].GetTransform().SetTranslate(Vector3(122,550-y+22,0));costs[i].GetTransform().SetTranslate(Vector3(122,550-y-4,0));if(i<7)scene.GetObject("TownBuildIcon"+i+"Display").GetTransform().SetTranslate(Vector3(65,550-y,0));}
        cursor.GetTransform().SetTranslate(Vector3(20,550-float(86+selected*60)+scroll,0));removeIcon.GetTransform().SetTranslate(Vector3(65,550-520+scroll,0));
    }
    void ScrollBy(float delta){scroll=TownClamp(scroll+delta,0,45);Layout();}
    void SetVisible(bool show){visible=show;}
    void Render(bool show){
        ui.Show(show);array<string> targets={"TownBuildScreen","TownBuildCamera","TownBuildDisplay"};for(uint i=0;i<targets.length();++i)scene.GetObject(targets[i]).SetActive(show);
        for(uint i=0;i<7;++i){string prefix="TownBuildIcon"+i;array<string> names={"Screen","Camera","Model","Light","Display"};for(uint j=0;j<names.length();++j)scene.GetObject(prefix+names[j]).SetActive(show);}
    }
    void Animate(float dt,bool allowed=true){
        amount=TownClamp(amount+(visible?1:-1)*dt/.28f,0,1);Render(Displayed()&&allowed);if(!Displayed()||!allowed)return;
        clock+=dt;for(uint i=0;i<7;++i)scene.GetObject("TownBuildIcon"+i+"Model").GetTransform().SetRotate(Vector3(0,clock*.75f,0));
        // 左のLetterbox余白にはみ出さないよう、移動中の見えている部分だけを表示する。
        float eased=1-(1-amount)*(1-amount)*(1-amount),left=10-390*(1-eased),start=left<0?0:left,right=left+370,width=right-start;
        Object@ display=scene.GetObject("TownBuildDisplay");if(width<=0){display.SetActive(false);return;}
        Transform@ main=scene.GetObject("TownDisplayViewport").GetTransform();float unit=main.GetScale().x/1280;Vector3 center=main.GetTranslate();display.GetTransform().SetTranslate(center+Vector3((start+width*.5f-640)*unit,10*unit,0));display.GetTransform().SetScale(Vector3(width*unit,550*unit,1));
        ScreenBufferViewport@ viewport;display.GetComponent(@viewport);SpriteRenderer@ sprite=viewport.GetSpriteRenderer();sprite.SetInstanceUvPivot(Vector2(0,0));sprite.SetInstanceUvScale(Vector2(width/370,1));sprite.SetInstanceUvTranslate(Vector2((start-left)/370,0));
        ui.RefreshLabels();
    }
    int Update(float dt){
        if(!visible||amount<.999f)return -1;ScreenBufferViewport@ viewport;scene.GetObject("TownBuildDisplay").GetComponent(@viewport);Vector2 p;if(!viewport.TryGetOffscreenMousePosition(p))return -1;
        float y=p.y/1080*550;if(y>=70&&y<=505){float wheel=GetCommandValue("TownCameraScroll");if(wheel!=0)ScrollBy(-wheel*30);}
        int clicked=ui.Clicked();if(clicked==8)ScrollBy(-30);if(clicked==9)ScrollBy(30);if(clicked>=0&&clicked<8&&y>=70&&y<=505){Select(clicked);return clicked;}return -1;
    }
}
