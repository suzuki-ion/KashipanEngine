#include "TownTransition.as"
#include "TownState.as"
#include "TownSettings.as"
#include "TownCamera.as"
#include "TownFireflies.as"
#include "TownWorld.as"
#include "TownCityHUD.as"
#include "TownShop.as"
#include "TownClearMenu.as"
#include "TownFacilityIndicators.as"
#include "TownPedestrians.as"
class CityController : ScriptComponentBehavior {
    TownTransition@ transition=TownTransition();
    TownState@ state=TownState();TownWorld@ world=TownWorld();
    TownCityHUD@ hud=TownCityHUD();TownBuildMenu@ buildMenu=TownBuildMenu();TownShopMenu@ shop=TownShopMenu();
    TownSettingsMenu@ settings=TownSettingsMenu();TownCameraRig@ cameraControls=TownCameraRig();TownFireflies@ fireflies=TownFireflies();
    TownClearMenu@ clearMenu=TownClearMenu();
    TownFacilityIndicators@ indicators=TownFacilityIndicators();TownGuildMenu@ guildMenu=TownGuildMenu();
    TownPedestrians@ pedestrians=TownPedestrians();
    Object@ directionArrow;Object@ facilityPreview;int selectedOption=0,tier=0,recipe=0;int rotation=0,previewKind=-1,strokeX=-1,strokeZ=-1;bool strokeBlocked=false,strokeStarted=false,strokePainted=false,manualFacing=false;int facingX=-1,facingZ=-1,facingKind=-1;uint facingRevision=0xffffffff;
    Vector2 strokeMouse;
    Object@ preview;int selected=6,cellX=-1,cellZ=-1,pressedX=-1,pressedZ=-1,gridSize=16;
    float spacing=1.5f,cursorTime=0,resetConfirm=0;bool ready=false,leaving=false;
    void Start(){
        transition.Initialize(GetScene());
        if(!state.Initialize(GetScene())){LogError("LightTownRules.jsonを読み込めません。");return;}
        gridSize=state.GridSize();spacing=float(state.rules.GetFloat("cellSize",1.5f));cameraControls.Initialize(GetScene());
        world.Initialize(GetScene(),state);fireflies.Initialize(GetScene(),float(gridSize)*spacing*.5f+spacing*1.25f);
        hud.Initialize(GetScene(),state);buildMenu.Initialize(GetScene(),state);shop.Initialize(GetScene(),state);settings.Initialize(GetScene());
        clearMenu.Initialize(GetScene());
        indicators.Initialize(GetScene(),state,world);guildMenu.Initialize(GetScene(),state);
        pedestrians.Initialize(GetScene(),state,world);
        @preview=TownMesh(GetScene(),"BuildPreview","PrimitiveMesh-Box",Vector3(),Vector3(spacing*.9f,.12f,spacing*.9f),Vector4(.2f,1,.5f,.3f));
        MeshRenderer@ cursor;preview.GetComponent(@cursor);cursor.SetMaterialName("TownCursor");cursor.SetPipelineName("Town.Cursor");cursor.SetCastShadows(false);HidePreview();
        @directionArrow=GetScene().CreateObject("TownBuildDirection");
        ArrowPart(Vector3(0,.04f,-.43f),Vector3(.075f,.035f,.50f),0);
        ArrowPart(Vector3(-.10f,.04f,-.61f),Vector3(.075f,.035f,.30f),-.78539816f);
        ArrowPart(Vector3(.10f,.04f,-.61f),Vector3(.075f,.035f,.30f),.78539816f);
        directionArrow.SetActive(false);
        @facilityPreview=TownMesh(GetScene(),"TownFacilityPreview",TownBuildingMesh(1),Vector3(),Vector3(1,1,1),Vector4(.3f,.7f,.9f,.45f));
        MeshRenderer@ ghost;facilityPreview.GetComponent(@ghost);ghost.SetMaterialName("TownCursor");ghost.SetPipelineName("Town.Cursor");ghost.SetCastShadows(false);facilityPreview.SetActive(false);
        TownApplySettings(GetScene());ready=true;
        state.message=TownTr("Bで建築メニュー。入口を道路に向け、街灯で照らすと施設が稼働します。","B opens construction. Face entrances toward roads and illuminate facilities with streetlights.");
    }
    void ArrowPart(const Vector3 &in p,const Vector3 &in size,float yaw){Object@ part=TownMesh(GetScene(),"TownBuildArrow", "PrimitiveMesh-Box",p,size,Vector4(1,.85f,.12f,1));part.GetTransform().SetRotate(Vector3(0,yaw,0));part.GetTransform().SetParentObject(directionArrow);MeshRenderer@ r;part.GetComponent(@r);r.SetMaterialName("TownGridUnlit");r.SetCastShadows(false);}
    void HidePreview(){preview.SetActive(false);if(directionArrow !is null)directionArrow.SetActive(false);if(facilityPreview !is null)facilityPreview.SetActive(false);}
    int PlacementFacing(int x,int z){state.EnsureInfrastructure();return manualFacing?rotation:state.AutoFacingFor(x,z,int(state.Definition(selected,tier,recipe).GetInt("size",1)),rotation);}
    void RefreshFacing(int x,int z){state.EnsureInfrastructure();if(x!=facingX||z!=facingZ||selected!=facingKind||state.infrastructureVersion!=facingRevision){manualFacing=false;rotation=state.AutoFacingFor(x,z,int(state.Definition(selected,tier,recipe).GetInt("size",1)),rotation);facingX=x;facingZ=z;facingKind=selected;facingRevision=state.infrastructureVersion;}}
    void PlaceAt(int x,int z){if(state.Build(selected,x,z,PlacementFacing(x,z),true,tier,recipe))ShowBuilding(selected,x,z);}
    void PaintTo(int x,int z){
        // Apply the tool to cells skipped between mouse samples.
        bool removing=selected==8;
        int px=strokeX<0?x:strokeX,pz=strokeZ<0?z:strokeZ;
        bool changed=false;int dx=int(Abs(float(x-px))),dz=-int(Abs(float(z-pz))),sx=px<x?1:-1,sz=pz<z?1:-1,error=dx+dz;
        while(true){
            if(removing){
                if(state.Occupied(px,pz)&&RemoveAt(px,pz,false))changed=true;
            }else if(!state.Occupied(px,pz)&&state.Build(selected,px,pz,PlacementFacing(px,pz),false,tier,recipe)){
                indicators.Placed(selected,px,pz);changed=true;
            }
            if(px==x&&pz==z)break;
            int twice=error*2;
            if(twice>=dz){error+=dz;px+=sx;}
            if(twice<=dx){error+=dx;pz+=sz;}
        }
        if(changed){
            world.Rebuild();TownApplySettings(GetScene());state.Save();
            if(removing){indicators.Rebuild();guildMenu.Show(false);}
        }
        strokeX=x;strokeZ=z;strokeMouse=cameraControls.previousMouse;strokePainted=true;
    }
    Vector3 CellPosition(int x,int z){return world.Cell(x,z);}
    bool OverUI(){
        ScreenBufferViewport@ viewport;GetScene().GetObject("TownDisplayViewport").GetComponent(@viewport);Vector2 p;
        if(!viewport.TryGetOffscreenMousePosition(p)||!viewport.IsMouseOverOffscreen())return true;
        float x=p.x/1.5f,y=p.y/1.5f;
        return clearMenu.visible||settings.visible||shop.visible||y<130||y>600||(guildMenu.visible&&x>=910&&x<=1260&&y>=145&&y<=guildMenu.Bottom())||(buildMenu.DetailsDisplayed()&&x>=910&&x<=1260&&y>=145&&y<=495)||buildMenu.ContainsMouse()||(hud.toasts.length()>0&&x>930&&y>=135&&y<445);
    }
    bool CheckClear() {
        if (!state.ClearPending()) return false;
        if (!clearMenu.visible) {
            buildMenu.SetVisible(false);buildMenu.Render(false);shop.Show(false);guildMenu.Show(false);indicators.Update(0,true,false);HidePreview();
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
    void ToggleBuild(){strokeStarted=false;strokeX=-1;strokeZ=-1;guildMenu.Show(false);buildMenu.SetVisible(!buildMenu.visible);pressedX=-1;pressedZ=-1;HidePreview();}
    void ShowBuilding(int kind,int x,int z){world.Rebuild();TownApplySettings(GetScene());indicators.Placed(kind,x,z);}
    bool RemoveAt(int x,int z,bool persist=true){
        Json@ building=state.BuildingAt(x,z);
        if(!state.RemoveBuilding(x,z,persist))return false;
        indicators.Removed(int(building.GetInt("x")),int(building.GetInt("z")),false);
        if(persist){world.Rebuild();TownApplySettings(GetScene());indicators.Rebuild();guildMenu.Show(false);}
        return true;
    }
    void SelectGuild(int x,int z){Json@ b=state.BuildingAt(x,z);if(b !is null){guildMenu.Open(int(b.GetInt("x")),int(b.GetInt("z")));pressedX=-1;pressedZ=-1;}else guildMenu.Show(false);}
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
        if(!buildMenu.visible||selected==8){
            Json@ buildings=state.Buildings();float nearest=2;int bx=-1,bz=-1;
            for(uint i=0;i<buildings.Size();++i){Json@ b=buildings.At(i);int kind=int(b.GetInt("kind"));if(kind==6)continue;
                int x=int(b.GetInt("x")),z=int(b.GetInt("z"));float height=float(state.BuildingDefinition(b).GetFloat("height",2)),hit=0;float width=float(state.Footprint(b));
                if(TownRayFacility(nearPoint,ray,world.Center(b)+Vector3(0,height*.5f,0),Vector3(spacing*width*.48f,height*.5f,spacing*width*.48f),hit)&&hit<nearest){nearest=hit;bx=x;bz=z;}
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
        if(ready&&!leaving)state.AdvanceRealTime();
        if(transition.Update(GetDeltaTime()))return;if(transition.failed){leaving=false;transition.failed=false;}
        if(!ready||leaving)return;
        if(!buildMenu.visible||settings.visible||shop.visible||clearMenu.visible){HidePreview();strokeStarted=false;}
        world.Update(GetDeltaTime());
        buildMenu.Animate(GetDeltaTime(),!settings.visible&&!shop.visible&&!clearMenu.visible);
        if(CheckClear()){
            hud.Refresh(state);hud.ui.RefreshLabels();
            for(uint i=0;i<hud.toasts.length();++i)hud.toasts[i].ui.RefreshLabels();
            for(uint i=0;i<hud.bursts.length();++i)hud.bursts[i].text.SetFontSize(hud.bursts[i].text.GetFontSize());
            ChooseClear(clearMenu.Clicked());return;
        }
        float dt=GetDeltaTime()*GetGameSpeed();
        if(settings.visible){indicators.Update(0,true,false);cameraControls.Cancel();settings.Update();if(!settings.visible&&settings.needsSceneRefresh){pedestrians.Save();state.Save();GetScene().SetNextSceneName("CityScene");leaving=transition.Begin("CityScene");}return;}
        if(shop.visible){indicators.Update(0,true,false);cameraControls.Cancel();shop.Update();hud.Update(dt,state);fireflies.Update(dt,float(state.data.GetFloat("energy")));return;}
        if(IsCommandTriggered("Cancel")){if(guildMenu.visible){guildMenu.Show(false);return;}if(buildMenu.visible){ToggleBuild();return;}indicators.Update(0,true,false);settings.Open();return;}
        resetConfirm-=dt;cursorTime+=dt;
        int clicked=hud.ui.Clicked();
        if(clicked==0||IsCommandTriggered("TownBuildMenu"))ToggleBuild();
        if(clicked==1||IsCommandTriggered("TownExpedition")){Leave();return;}
        if(clicked==2){buildMenu.SetVisible(false);buildMenu.Render(false);guildMenu.Show(false);HidePreview();shop.Open();}
        if(clicked==3){state.DispatchAll();}
        if(clicked==4){if(state.Save())state.message=TownTr("保存しました。","Game saved.");}
        if(clicked==5){buildMenu.SetVisible(false);buildMenu.Render(false);guildMenu.Show(false);indicators.Update(0,true,false);settings.Open();return;}
        if(clicked==6){if(resetConfirm>0){state.NewGame();pedestrians.Save();state.Save();GetScene().SetNextSceneName("CityScene");leaving=transition.Begin("CityScene");return;}resetConfirm=3;state.message=TownTr("3秒以内にもう一度初期化を押すと街を初期化します。","Select New Game again within 3 seconds to reset.");}
        cameraControls.Update(GetDeltaTime(),OverUI());int choice=buildMenu.Update(dt);if(choice>=0){selectedOption=choice;if(choice==int(buildMenu.catalog.length())){selected=8;tier=0;recipe=0;}else{TownBuildChoice@ c=buildMenu.catalog[uint(choice)];selected=c.kind;tier=c.tier;recipe=c.recipe;}}hud.LayoutSelection(buildMenu.visible);



        guildMenu.Update();bool hasCell=PickCell(),picked=buildMenu.visible&&buildMenu.CanBuild()&&hasCell;preview.SetActive(picked);directionArrow.SetActive(picked&&selected!=6&&selected!=8);facilityPreview.SetActive(picked&&selected!=6&&selected!=8);
        if(picked&&selected!=6&&selected!=8){RefreshFacing(cellX,cellZ);if(IsCommandTriggered("TownBuildRotate")){rotation=TownFacing(rotation+1);manualFacing=true;}}
        UpdateFocus(hasCell);
        if(cameraControls.manipulating){pressedX=-1;pressedZ=-1;}
        if(IsCommandTriggered("TownPlacePress")){pressedX=hasCell&&!cameraControls.manipulating?cellX:-1;pressedZ=hasCell&&!cameraControls.manipulating?cellZ:-1;strokeBlocked=!picked||choice>=0||clicked>=0;strokeStarted=!strokeBlocked&&!cameraControls.manipulating;strokePainted=false;strokeX=-1;strokeZ=-1;}
        bool removing=selected==8;
        bool continuous=selected==6||IsCommandTriggered("TownBuildContinuous");
        bool paintingAllowed=picked&&continuous&&!cameraControls.manipulating&&clicked<0&&choice<0;
        bool held=IsCommandTriggered("TownPlaceHold");
        // Start fresh after a UI/map gap, camera operation or release of Shift.
        if(!paintingAllowed){strokeX=-1;strokeZ=-1;}
        if(!removing&&paintingAllowed&&!strokeBlocked&&held)strokeStarted=true;
        if(!buildMenu.visible&&hasCell&&!cameraControls.manipulating&&clicked<0&&pressedX==cellX&&pressedZ==cellZ&&IsCommandTriggered("TownPlace"))SelectGuild(cellX,cellZ);
        if(picked){
            float width=selected==8?1:float(state.Definition(selected,tier,recipe).GetInt("size",1));Vector3 anchor=CellPosition(cellX,cellZ)+Vector3((width-1)*spacing*.5f,0,(width-1)*spacing*.5f);preview.GetTransform().SetTranslate(anchor+Vector3(0,.15f,0));preview.GetTransform().SetScale(Vector3(spacing*width*.9f,.12f,spacing*width*.9f));MeshRenderer@ r;preview.GetComponent(@r);
            bool possible=(removing?state.DeleteProblem(cellX,cellZ):state.BuildProblem(selected,cellX,cellZ,tier,recipe))=="";state.EnsureInfrastructure();
            string operation=removing?"":state.OperationProblem(selected,cellX,cellZ,rotation,int(state.Definition(selected,tier,recipe).GetInt("size",1)));bool operational=operation=="";
            if(selected!=6&&selected!=8){
                if(previewKind!=selectedOption){MeshFilter@ filter;facilityPreview.GetComponent(@filter);filter.SetMeshHandle(GetModelHandleFromAssetPath(TownVariantMesh(selected,tier,recipe)));previewKind=selectedOption;}
                facilityPreview.SetActive(true);facilityPreview.GetTransform().SetTranslate(anchor);facilityPreview.GetTransform().SetRotate(Vector3(0,TownModelYaw(selected,rotation),0));
                MeshRenderer@ ghost;facilityPreview.GetComponent(@ghost);Vector4 color=state.Definition(selected,tier,recipe).GetVector4("color",Vector4(1,1,1,1));color.w=.42f;ghost.SetInstanceColor(color);
                directionArrow.SetActive(true);directionArrow.GetTransform().SetTranslate(anchor+Vector3(0,float(state.Definition(selected,tier,recipe).GetFloat("height",2))+.3f,0));directionArrow.GetTransform().SetRotate(Vector3(0,-float(rotation)*1.57079633f,0));
            }
            float alpha=.3f+.14f*Sin(cursorTime*6.2831853f/1.2f);
            r.SetInstanceColor(!possible?Vector4(1,.2f,.2f,alpha):(removing?Vector4(1,.35f,.25f,alpha):(operational?Vector4(.2f,1,.5f,alpha):Vector4(1,.65f,.15f,alpha))));
            hud.selection.SetText(removing?TownTr("削除 / Shift＋左ドラッグで連続削除（返金なし）","Remove / Shift + left drag to erase (no refund)"):selected==6?TownTr("道路 / 左ドラッグで連続設置","Road / Left drag to place"):TownBuildingName(state.Definition(selected,tier,recipe))+TownTr(" / "," / ")+(operational?TownTr("稼働可能","Operational"):TownTr("未稼働：","Inactive: ")+operation));
            // Removing a roof can expose another cell under a stationary cursor.
            bool movedSincePaint=(cameraControls.previousMouse-strokeMouse).Length()>0;
            if(strokeStarted&&paintingAllowed&&held&&(strokeX!=cellX||strokeZ!=cellZ)&&(!removing||strokeX<0||movedSincePaint))PaintTo(cellX,cellZ);
            if(!strokePainted&&clicked<0&&choice<0&&pressedX==cellX&&pressedZ==cellZ&&IsCommandTriggered("TownPlace")){if(removing)RemoveAt(cellX,cellZ);else PlaceAt(cellX,cellZ);}
        }else hud.selection.SetText(TownTr("B：建築 / R：回転 / 左ドラッグ：道路 / Shift＋左ドラッグ：その他の連続操作","B: build / R: rotate / Left drag: roads / Shift + left drag: other tools")+(state.data.GetBool("cleared")?" / CLEAR!":""));
        if(IsCommandTriggered("TownPlace")&&!held){pressedX=-1;pressedZ=-1;strokeStarted=false;strokeBlocked=false;strokePainted=false;strokeX=-1;strokeZ=-1;}
        fireflies.Update(dt,float(state.data.GetFloat("energy")));hud.Update(dt,state);
        pedestrians.Update(dt);
        indicators.Update(dt,settings.visible||shop.visible||clearMenu.visible,buildMenu.Displayed());
        CheckClear();
    }
    void Leave(){if(!state.Save())return;pedestrians.Save();GetScene().SetNextSceneName("StageSelectScene");leaving=transition.Begin();if(!leaving)state.message=TownTr("遠征先の選択へ移動できませんでした。","Unable to open expedition selection.");}
}
