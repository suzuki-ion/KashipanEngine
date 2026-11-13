#include "TownLanguage.as"
#include "TownPresentation.as"

void TownApplySettings(Scene@ scene) {
    SetCategoryVolume("SE", GetSettingFloat("town.seVolume", 0.8f));
    SetCategoryVolume("BGM", GetSettingFloat("town.bgmVolume", 0.6f));
    array<string> names = {"TownAmbient", "TownLight", "SceneLight", "Light Object", "Directional Light",
        "PrefabEnemyPart1", "PrefabCheckpointPart2", "PrefabTeleportPart2", "PrefabTeleportPart4",
        "HistoricalStreetSpot", "HistoricalStreetSphere", "HistoricalStreetFill", "HistoricalFactoryShadow", "HistoricalFactoryFill", "TownFacilityLight", "TownFacilityFill", "TownStreetlight", "TownFirefly", "TownFireflyBatch"};
    for (uint n = 0; n < names.length(); ++n) {
        array<Object@>@ objects = scene.GetObjects(names[n]);
        for (uint i = 0; i < objects.length(); ++i) {
            Light@ light;
            if (!objects[i].GetComponent(@light)) continue;
            // 旧Stage1の背面平行光源と街灯の補助光は、影なしのフィルライト。
            bool shadowCapable = names[n] != "SceneLight" && names[n] != "TownAmbient" && names[n] != "HistoricalStreetFill" && names[n] != "HistoricalFactoryFill" && names[n] != "TownFirefly" && names[n] != "TownFireflyBatch" && names[n] != "TownFacilityFill";
            light.SetCastShadows(shadowCapable && GetSettingBool("town.shadows", true));
            light.SetShadowMapResolution(uint(GetSettingInt("town.shadowResolution", 1024)));
        }
    }
    Object@ screen = scene.GetObject("TownScreen");
    if (screen !is null) {
        // GameDefaultObjects/MainScreenの有効状態を基準に一括切替する。
        // SSAOはGTAOとの二重適用を避け、プレハブ既定の無効状態を維持する。
        bool enabled = GetSettingBool("town.postEffects", true);
        GTAOEffect@ gtao; SSAOEffect@ ssao; FXAAEffect@ fxaa;
        MotionBlurEffect@ motion; DepthOfFieldEffect@ dof;
        ChromaticAberrationEffect@ chromatic; VignetteEffect@ vignette;
        if (screen.GetComponent(@gtao)) gtao.SetActive(enabled);
        if (screen.GetComponent(@ssao)) ssao.SetActive(false);
        if (screen.GetComponent(@fxaa)) fxaa.SetActive(enabled);
        if (screen.GetComponent(@motion)) motion.SetActive(enabled);
        if (screen.GetComponent(@dof)) dof.SetActive(enabled);
        if (screen.GetComponent(@chromatic)) chromatic.SetActive(enabled);
        if (screen.GetComponent(@vignette)) vignette.SetActive(enabled);
        BloomEffect@ bloom;
        if (screen.GetComponent(@bloom)) bloom.SetActive(enabled);
        ColorAdjustEffect@ adjust;
        if (screen.GetComponent(@adjust)) {
            // 明るさはポストエフェクト切替と独立した画面設定。
            adjust.SetActive(true); adjust.SetBrightness(GetSettingFloat("town.brightness", 0.1f));
        }
    }
}

class TownSettingSlider {
 UIButton@ input;Object@ thumb;TextRenderer@ label;float value=0,y=0,scroll=0;bool changed=false;
 void Create(TownUI@ ui,float row,float initial){y=row;value=initial;Object@ track=ui.Rect(740,row,300,24,Vector4(.16f,.25f,.3f,1));track.AddComponent(@input);input.SetDisplayCameraObject(ui.camera);@thumb=ui.Rect(590+value*300,row,12,32,Vector4(.4f,.9f,1,1));@label=ui.Text("",908,row-10,18);Refresh();}
 void Refresh(){thumb.GetTransform().SetTranslate(Vector3(590+value*300,620-y+scroll,0));label.SetText(""+int(value*100+.5f)+"%");}
 void Update(){changed=false;if(!input.IsPressed()&&!input.IsClicked())return;Vector2 p;if(!input.TryGetLocalHoverPosition(p))return;float next=TownClamp(p.x,0,1);if(next!=value){value=next;changed=true;Refresh();}}
}
class TownSettingsMenu {
 TownUI@ ui=TownUI();TownSettingSlider@ se=TownSettingSlider(),bgm=TownSettingSlider(),brightness=TownSettingSlider();Scene@ scene;
 array<Object@>@ moving=array<Object@>();array<Vector3>@ positions=array<Vector3>();array<TextRenderer@>@ rowLabels=array<TextRenderer@>();
 TextRenderer@ heading;bool visible=false,languageChanged=false,needsSceneRefresh=false,dirty=false;float scroll=0,flushClock=0;
 void Initialize(Scene@ owner){@scene=owner;ui.Initialize(scene,"TownSettingsScreen","TownSettingsCamera","TownSettingsPanelTemplate");ui.canvasHeight=620;ui.layer=120;ui.Rect(500,310,1000,620,Vector4(.025f,.05f,.07f,1));
  array<string> names={"表示言語","効果音の音量","BGMの音量","ライトの影","影の解像度","ポストエフェクト","画面の明るさ","道路を歩く人","建物の簡易表示","建物の生産・派遣バー","街の浮遊光"};
  array<string> english={"Language","Sound effects","Music volume","Light shadows","Shadow resolution","Post effects","Brightness","Pedestrians","Simple building models","Production / dispatch bars","Floating lights"};
  for(uint i=0;i<names.length();++i){uint start=ui.objects.length();float y=110+float(i)*66;rowLabels.insertLast(ui.Text(TownTr(names[i],english[i]),40,y-12,21));
   if(i==1)se.Create(ui,y,GetSettingFloat("town.seVolume",.8f));else if(i==2)bgm.Create(ui,y,GetSettingFloat("town.bgmVolume",.6f));else if(i==6)brightness.Create(ui,y,(GetSettingFloat("town.brightness",.1f)+.4f)/.8f);else ui.ButtonArea("",760,y,340,40,605,y-12);
   for(uint j=start;j<ui.objects.length();++j){moving.insertLast(ui.objects[j]);positions.insertLast(ui.objects[j].GetTransform().GetTranslate());}}
  ui.layer=150;ui.Rect(500,35,1000,70,Vector4(.025f,.05f,.07f,1));@heading=ui.Text(TownTr("設定","Settings"),40,15,30);ui.Rect(500,580,1000,80,Vector4(.025f,.05f,.07f,1));ui.ButtonArea(TownTr("戻る [Esc]","Back [Esc]"),160,580,240,42,65,566);ui.ButtonArea("▲",870,580,60,40,858,565);ui.ButtonArea("▼",945,580,60,40,933,565);ui.Text(TownTr("ホイール / ▲▼：スクロール・自動保存","Wheel / arrows: scroll. Saved automatically."),400,570,15);Refresh();Show(false);
 }
 string On(bool value){return value?"ON":"OFF";}
 void Refresh(){array<string> jp={"表示言語","効果音の音量","BGMの音量","ライトの影","影の解像度","ポストエフェクト","画面の明るさ","道路を歩く人","建物の簡易表示","建物の生産・派遣バー","街の浮遊光"},en={"Language","Sound effects","Music volume","Light shadows","Shadow resolution","Post effects","Brightness","Pedestrians","Simple building models","Production / dispatch bars","Floating lights"};for(uint i=0;i<rowLabels.length();++i)rowLabels[i].SetText(TownTr(jp[i],en[i]));heading.SetText(TownTr("設定","Settings"));ui.buttonTexts[8].SetText(TownTr("戻る [Esc]","Back [Esc]"));ui.buttonTexts[0].SetText(GetCurrentLanguage()=="ja-JP"?"日本語":"English");ui.buttonTexts[1].SetText(On(GetSettingBool("town.shadows",true)));ui.buttonTexts[2].SetText(""+GetSettingInt("town.shadowResolution",1024));ui.buttonTexts[3].SetText(On(GetSettingBool("town.postEffects",true)));ui.buttonTexts[4].SetText(On(GetSettingBool("town.people",true)));ui.buttonTexts[5].SetText(On(GetSettingBool("town.simpleModels",false)));int mode=GetSettingInt("town.bars",0);ui.buttonTexts[6].SetText(mode==0?TownTr("バー＋秒数","Bar and seconds"):(mode==1?TownTr("秒数のみ","Seconds only"):TownTr("非表示","Hidden")));ui.buttonTexts[7].SetText(On(GetSettingBool("town.fireflies",true)));}
 void Show(bool show){visible=show;ui.Show(show);array<string> parts={"TownSettingsScreen","TownSettingsCamera","TownSettingsDisplay"};for(uint i=0;i<parts.length();++i)scene.GetObject(parts[i]).SetActive(show);}
 void Open(){languageChanged=false;needsSceneRefresh=false;Show(true);Refresh();Layout();}
 void Layout(){for(uint i=0;i<moving.length();++i)moving[i].GetTransform().SetTranslate(positions[i]+Vector3(0,scroll,0));se.scroll=scroll;bgm.scroll=scroll;brightness.scroll=scroll;se.Refresh();bgm.Refresh();brightness.Refresh();Transform@ view=scene.GetObject("TownDisplayViewport").GetTransform();float unit=view.GetScale().x/1280;Transform@ display=scene.GetObject("TownSettingsDisplay").GetTransform();display.SetTranslate(view.GetTranslate());display.SetScale(Vector3(1000*unit,620*unit,1));ScreenBufferViewport@ viewport;scene.GetObject("TownSettingsDisplay").GetComponent(@viewport);viewport.GetSpriteRenderer().SetInstanceColor(Vector4(1,1,1,.92f));}
 void Flush(){if(!dirty)return;SetSettingFloat("town.seVolume",se.value);SetSettingFloat("town.bgmVolume",bgm.value);SetSettingFloat("town.brightness",brightness.value*.8f-.4f);dirty=false;}
 void Update(){if(!visible)return;se.changed=false;bgm.changed=false;brightness.changed=false;languageChanged=false;Layout();ScreenBufferViewport@ viewport;scene.GetObject("TownSettingsDisplay").GetComponent(@viewport);Vector2 mouse;bool over=viewport.TryGetOffscreenMousePosition(mouse);float y=mouse.y/1080*620;int click=ui.Clicked();bool content=over&&y>=70&&y<540;
  if(content){float wheel=GetCommandValue("TownCameraScroll");if(wheel!=0)scroll=TownClamp(scroll-wheel*66,0,280);if(click==0){SetCurrentLanguage(GetCurrentLanguage()=="ja-JP"?"en-US":"ja-JP");SetSettingString("language",GetCurrentLanguage());languageChanged=true;needsSceneRefresh=true;}if(click==1)SetSettingBool("town.shadows",!GetSettingBool("town.shadows",true));if(click==2){int size=GetSettingInt("town.shadowResolution",1024);SetSettingInt("town.shadowResolution",size>=2048?512:size*2);}if(click==3)SetSettingBool("town.postEffects",!GetSettingBool("town.postEffects",true));if(click==4)SetSettingBool("town.people",!GetSettingBool("town.people",true));if(click==5){SetSettingBool("town.simpleModels",!GetSettingBool("town.simpleModels",false));needsSceneRefresh=true;}if(click==6)SetSettingInt("town.bars",(GetSettingInt("town.bars",0)+1)%3);if(click==7)SetSettingBool("town.fireflies",!GetSettingBool("town.fireflies",true));se.Update();bgm.Update();brightness.Update();}
  if(se.changed||bgm.changed||brightness.changed){dirty=true;SetCategoryVolume("SE",se.value);SetCategoryVolume("BGM",bgm.value);ColorAdjustEffect@ adjust;scene.GetObject("TownScreen").GetComponent(@adjust);if(adjust !is null)adjust.SetBrightness(brightness.value*.8f-.4f);}
  flushClock+=GetDeltaTime();if(flushClock>=.3f){Flush();flushClock=0;}if(content&&click>=0&&click<8){TownApplySettings(scene);Refresh();}if(over&&y>=540&&click==9)scroll=TownClamp(scroll-66,0,280);if(over&&y>=540&&click==10)scroll=TownClamp(scroll+66,0,280);if((over&&y>=540&&click==8)||IsCommandTriggered("Cancel")){Flush();Show(false);}ui.RefreshLabels();
 }
}
