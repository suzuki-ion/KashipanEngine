#include "TownState.as"
#include "TownSettings.as"
#include "TownCamera.as"
#include "TownFireflies.as"
#include "TownWorld.as"
#include "TownCityHUD.as"
#include "TownShop.as"
#include "TownClearMenu.as"
#include "TownFacilityIndicators.as"
class CityController : ScriptComponentBehavior {
    TownState@ state=TownState();TownWorld@ world=TownWorld();
    TownCityHUD@ hud=TownCityHUD();TownBuildMenu@ buildMenu=TownBuildMenu();TownShopMenu@ shop=TownShopMenu();
    TownSettingsMenu@ settings=TownSettingsMenu();TownCameraRig@ cameraControls=TownCameraRig();TownFireflies@ fireflies=TownFireflies();
    TownClearMenu@ clearMenu=TownClearMenu();
    TownFacilityIndicators@ indicators=TownFacilityIndicators();TownGuildMenu@ guildMenu=TownGuildMenu();
    Object@ preview;int selected=6,cellX=-1,cellZ=-1,pressedX=-1,pressedZ=-1,gridSize=16;
    float spacing=1.5f,cursorTime=0,resetConfirm=0;bool ready=false,leaving=false;
    void Start(){
        if(!state.Initialize(GetScene())){LogError("LightTownRules.jsonを読み込めません。");return;}
        gridSize=state.GridSize();spacing=float(state.rules.GetFloat("cellSize",1.5f));cameraControls.Initialize(GetScene());
        world.Initialize(GetScene(),state);fireflies.Initialize(GetScene(),float(gridSize)*spacing*.45f);
        hud.Initialize(GetScene(),state);buildMenu.Initialize(GetScene(),state);shop.Initialize(GetScene(),state);settings.Initialize(GetScene());
        clearMenu.Initialize(GetScene());
        indicators.Initialize(GetScene(),state,world);guildMenu.Initialize(GetScene(),state);
        @preview=TownMesh(GetScene(),"BuildPreview","PrimitiveMesh-Box",Vector3(),Vector3(spacing*.9f,.12f,spacing*.9f),Vector4(.2f,1,.5f,.3f));
        MeshRenderer@ cursor;preview.GetComponent(@cursor);cursor.SetMaterialName("TownCursor");cursor.SetPipelineName("Town.Cursor");cursor.SetCastShadows(false);preview.SetActive(false);
        TownApplySettings(GetScene());ready=true;
        state.message=TownTr("Bで建築メニュー。道路を置き、隣に街灯を建てると施設を稼働できます。","B opens construction. Place a road, then a streetlight next to it to activate nearby facilities.");
    }
    Vector3 CellPosition(int x,int z){return world.Cell(x,z);}
    bool OverUI(){
        ScreenBufferViewport@ viewport;GetScene().GetObject("TownDisplayViewport").GetComponent(@viewport);Vector2 p;
        if(!viewport.TryGetOffscreenMousePosition(p)||!viewport.IsMouseOverOffscreen())return true;
        float x=p.x/1.5f,y=p.y/1.5f;
        return clearMenu.visible||settings.visible||shop.visible||y<88||y>600||(guildMenu.visible&&x>=425&&x<=855&&y>=405&&y<=575)||buildMenu.ContainsMouse()||(hud.toasts.length()>0&&x>930&&y>=95&&y<400);
    }
    bool CheckClear() {
        if (!state.ClearPending()) return false;
        if (!clearMenu.visible) {
            buildMenu.SetVisible(false);buildMenu.Render(false);shop.Show(false);guildMenu.Show(false);indicators.Update(0,true,false);preview.SetActive(false);
            pressedX=-1; pressedZ=-1; cameraControls.Cancel(); clearMenu.Show(true);
        }
        return true;
    }
    void ChooseClear(int choice) {
        if (!clearMenu.visible || (choice!=0 && choice!=1)) return;
        if (!state.AcknowledgeClear()) { clearMenu.SaveFailed(); return; }
        clearMenu.Show(false);
        if (choice==1) { leaving=true; RequestExitGameLoop(); }
    }
    void ToggleBuild(){guildMenu.Show(false);buildMenu.SetVisible(!buildMenu.visible);pressedX=-1;pressedZ=-1;preview.SetActive(false);}
    void ShowBuilding(int kind,int x,int z){world.Rebuild();TownApplySettings(GetScene());indicators.Placed(kind,x,z);}
    void RemoveAt(int x,int z){if(!state.RemoveBuilding(x,z))return;world.Rebuild();TownApplySettings(GetScene());indicators.Removed(x,z);guildMenu.Show(false);}
    void SelectGuild(int x,int z){Json@ b=state.BuildingAt(x,z);if(b !is null&&b.GetInt("kind")==4){guildMenu.Open(x,z);pressedX=-1;pressedZ=-1;}else guildMenu.Show(false);}
    bool PickCell() {
        cellX = -1; cellZ = -1;
        if(world.Arriving())return false;
        if (OverUI()) return false;
        ScreenBufferViewport@ viewport; GetScene().GetObject("TownDisplayViewport").GetComponent(@viewport);
        Vector2 pixel;
        if (!viewport.TryGetOffscreenMousePosition(pixel) || !viewport.IsMouseOverOffscreen()) return false;
        // 回転・移動したフレームも950優先度のCameraRenderer更新を待たずに選択位置を算出する。
        Camera3D@ camera; cameraControls.camera.GetComponent(@camera);
        Matrix4x4 projection;
        projection.MakePerspectiveFovMatrix(camera.GetFovY(), camera.GetAspectRatio(), camera.GetNearClip(), camera.GetFarClip());
        Matrix4x4 inverse = (cameraControls.camera.GetTransform().GetWorldMatrix().Inverse() * projection).Inverse();
        float nx = pixel.x / 1920 * 2 - 1, ny = 1 - pixel.y / 1080 * 2;
        Vector3 nearPoint = Vector3(nx, ny, 0).Transform(inverse);
        Vector3 farPoint = Vector3(nx, ny, 1).Transform(inverse);
        Vector3 ray = farPoint - nearPoint;
        // 通常時は建物の高さを含む選択範囲を先に調べ、屋根を押した場合もその施設を選ぶ。
        if(!buildMenu.visible||selected==7){
            Json@ buildings=state.Buildings();float nearest=2;int bx=-1,bz=-1;
            for(uint i=0;i<buildings.Size();++i){Json@ b=buildings.At(i);int kind=int(b.GetInt("kind"));if(kind==6)continue;
                int x=int(b.GetInt("x")),z=int(b.GetInt("z"));float height=float(state.Definition(kind).GetFloat("height",2)),hit=0;
                if(TownRayFacility(nearPoint,ray,world.Cell(x,z)+Vector3(0,height*.5f,0),Vector3(spacing*.48f,height*.5f,spacing*.48f),hit)&&hit<nearest){nearest=hit;bx=x;bz=z;}
            }
            if(bx>=0){cellX=bx;cellZ=bz;return true;}
        }
        if (ray.y > -0.00001f && ray.y < 0.00001f) return false;
        float t = -nearPoint.y / ray.y;
        if (t < 0 || t > 1) return false;
        Vector3 p = nearPoint + ray * t;
        float gx = p.x / spacing + gridSize * 0.5f, gz = p.z / spacing + gridSize * 0.5f;
        if (gx < 0 || gz < 0 || gx >= gridSize || gz >= gridSize) return false;
        cellX = int(gx); cellZ = int(gz); return true;
    }
    void UpdateFocus(bool hasCell){
        // UI上や街の外では直前の焦点を保つ。補間はPostEffectControllerで行う。
        if(!hasCell)return;
        Object@ focus=GetScene().GetObject("TownCameraFocus");
        if(focus !is null)focus.GetTransform().SetTranslate(CellPosition(cellX,cellZ));
    }
    void Update(){
        if(!ready||leaving)return;
        world.Update(GetDeltaTime());
        buildMenu.Animate(GetDeltaTime(),!settings.visible&&!shop.visible&&!clearMenu.visible);
        if(CheckClear()){
            hud.Refresh(state);hud.ui.RefreshLabels();
            for(uint i=0;i<hud.toasts.length();++i)hud.toasts[i].ui.RefreshLabels();
            for(uint i=0;i<hud.bursts.length();++i)hud.bursts[i].text.SetFontSize(hud.bursts[i].text.GetFontSize());
            ChooseClear(clearMenu.Clicked());return;
        }
        float dt=GetDeltaTime()*GetGameSpeed();
        if(settings.visible){indicators.Update(0,true,false);cameraControls.Cancel();settings.Update();if(!settings.visible&&settings.needsSceneRefresh){state.Save();GetScene().SetNextSceneName("CityScene");leaving=GetScene().ChangeToNextScene();}return;}
        if(shop.visible){indicators.Update(0,true,false);cameraControls.Cancel();shop.Update();hud.Update(dt,state);fireflies.Update(dt,float(state.data.GetFloat("energy")));return;}
        if(IsCommandTriggered("Cancel")){if(guildMenu.visible){guildMenu.Show(false);return;}if(buildMenu.visible){ToggleBuild();return;}indicators.Update(0,true,false);settings.Open();return;}
        state.Tick(dt);resetConfirm-=dt;cursorTime+=dt;
        if(CheckClear())return;
        int clicked=hud.ui.Clicked();
        if(clicked==0||IsCommandTriggered("TownBuildMenu"))ToggleBuild();
        if(clicked==1||IsCommandTriggered("TownExpedition")){Leave();return;}
        if(clicked==2){buildMenu.SetVisible(false);buildMenu.Render(false);guildMenu.Show(false);preview.SetActive(false);shop.Open();}
        if(clicked==3){buildMenu.SetVisible(false);preview.SetActive(false);state.message=TownTr("マップ上のギルドをクリックして、ギルドごとにNPCを派遣できます。","Click a guild on the map to dispatch its NPC.");}
        if(clicked==4){if(state.Save())state.message=TownTr("保存しました。","Game saved.");}
        if(clicked==5){buildMenu.SetVisible(false);buildMenu.Render(false);guildMenu.Show(false);indicators.Update(0,true,false);settings.Open();return;}
        if(clicked==6){if(resetConfirm>0){state.NewGame();state.Save();GetScene().SetNextSceneName("CityScene");leaving=GetScene().ChangeToNextScene();return;}resetConfirm=3;state.message=TownTr("3秒以内にもう一度初期化を押すと街を初期化します。","Select New Game again within 3 seconds to reset.");}
        cameraControls.Update(GetDeltaTime(),OverUI());int choice=buildMenu.Update(dt);if(choice>=0)selected=choice;hud.LayoutSelection(buildMenu.visible);
        if(buildMenu.visible)for(int i=0;i<7;++i)if(IsCommandTriggered("TownBuild"+(i+1)))selected=i;
        buildMenu.Select(selected);
        guildMenu.Update();bool hasCell=PickCell(),picked=buildMenu.visible&&hasCell;preview.SetActive(picked);
        UpdateFocus(hasCell);
        if(cameraControls.manipulating){pressedX=-1;pressedZ=-1;}
        if(IsCommandTriggered("TownPlacePress")){pressedX=hasCell&&!cameraControls.manipulating?cellX:-1;pressedZ=hasCell&&!cameraControls.manipulating?cellZ:-1;}
        if(!buildMenu.visible&&hasCell&&!cameraControls.manipulating&&clicked<0&&pressedX==cellX&&pressedZ==cellZ&&IsCommandTriggered("TownPlace"))SelectGuild(cellX,cellZ);
        if(picked){
            preview.GetTransform().SetTranslate(CellPosition(cellX,cellZ)+Vector3(0,.15f,0));MeshRenderer@ r;preview.GetComponent(@r);
            bool removing=selected==7;bool possible=(removing?state.DeleteProblem(cellX,cellZ):state.BuildProblem(selected,cellX,cellZ))=="";state.EnsureInfrastructure();
            bool operational=removing||selected==6||(state.RoadAdjacent(cellX,cellZ)&&(selected==0||state.IsLit(cellX,cellZ)));
            float alpha=.3f+.14f*Sin(cursorTime*6.2831853f/1.2f);
            r.SetInstanceColor(!possible?Vector4(1,.2f,.2f,alpha):(removing?Vector4(1,.35f,.25f,alpha):(operational?Vector4(.2f,1,.5f,alpha):Vector4(1,.65f,.15f,alpha))));
            hud.selection.SetText(removing?TownTr("削除 / 施設・道路をクリック（返金なし）","Remove / Click a facility or road (no refund)"):TownBuildingName(state.Definition(selected))+TownTr(" / "," / ")+(operational?TownTr("稼働可能","Operational"):TownTr("未稼働：道路と街灯が必要","Inactive: road and streetlight required")));
            if(clicked<0&&choice<0&&pressedX==cellX&&pressedZ==cellZ&&IsCommandTriggered("TownPlace")){if(removing)RemoveAt(cellX,cellZ);else if(state.Build(selected,cellX,cellZ))ShowBuilding(selected,cellX,cellZ);}
        }else hud.selection.SetText(TownTr("B：建築 / 道路に隣接し街灯に照らされた施設が稼働","B: construction / Facilities require adjacent roads and streetlights")+(state.data.GetBool("cleared")?" / CLEAR!":""));
        if(IsCommandTriggered("TownPlace")){pressedX=-1;pressedZ=-1;}
        fireflies.Update(dt,float(state.data.GetFloat("energy")));hud.Update(dt,state);
        indicators.Update(dt,settings.visible||shop.visible||clearMenu.visible,buildMenu.Displayed());
        CheckClear();
    }
    void Leave(){state.Save();GetScene().SetNextSceneName("ExpeditionScene");leaving=GetScene().ChangeToNextScene();if(!leaving)state.message=TownTr("遠征への移動に失敗しました。","Unable to start expedition.");}
}
