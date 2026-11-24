#include "TownTransition.as"
#include "TownStages.as"
#include "TownPedestrians.as"
#include "ExpeditionHarvest.as"
#include "TownSettings.as"

class StageSelectController : ScriptComponentBehavior {
    TownTransition@ transition=TownTransition();
    TownState@ state=TownState();TownStages@ stages=TownStages();TownUI@ ui=TownUI();
    array<Object@>@ nodes=array<Object@>();array<MeshButton@>@ buttons=array<MeshButton@>();array<TextRenderer@>@ labels=array<TextRenderer@>();array<Object@>@ labelObjects=array<Object@>();
    Object@ camera;Object@ marker;Object@ ring;TextRenderer@ title;TextRenderer@ description;TextRenderer@ weights;TextRenderer@ notice;
    int selected=0;float clock=0,routePosition=0;bool ready=false,leaving=false;Vector3 markerPosition;
    Object@ Mesh(const string &in name,const string &in mesh,const Vector3 &in p,const Vector3 &in scale,const Vector4 &in color){Object@ o=TownMesh(GetScene(),name,mesh,p,scale,color);MeshRenderer@ r;o.GetComponent(@r);r.SetCastShadows(false);return o;}
    void Start(){
        transition.Initialize(GetScene());
        if(!state.Initialize(GetScene())||!stages.Load()){LogError("Stage selection data could not be loaded.");return;}
        selected=stages.Index(state.data.GetString("selectedStage","forest"));@camera=GetScene().GetObject("TownCamera");
        Quaternion rotation=Math::MakeRotateEuler(Vector3(.65f,-.35f,0));camera.GetTransform().SetRotateQuaternion(rotation);camera.GetTransform().SetTranslate(-rotation.RotateVector(Vector3(0,0,1))*22+Vector3(0,-2,0));
        ui.Initialize(GetScene());ui.Text(TownTr("遠征先を選ぶ","CHOOSE AN EXPEDITION"),28,20,30);ui.Text(TownTr("道でつながった地点を選択してください","Choose a destination along the route"),30,65,18);
        ui.Rect(640,622,1260,170,Vector4(.025f,.06f,.08f,.94f));@title=ui.Text("",30,552,25);@description=ui.Text("",30,590,18);@weights=ui.Text("",30,624,17);
        ui.ButtonArea(TownTr("このステージへ出発","Start expedition"),1120,590,270,54,1000,574);ui.ButtonArea(TownTr("街に戻る [Esc]","Back to town [Esc]"),1120,665,270,42,1000,651);
        @notice=ui.Text(TownTr("地点をクリック / ←→：選択 / Enter：出発","Click a node / Left-Right: choose / Enter: start"),30,676,16);
        Mesh("StageMapIsland","PrimitiveMesh-Box",Vector3(0,-.45f,0),Vector3(21,.7f,12),Vector4(.16f,.3f,.24f,1));
        for(uint i=0;i<stages.Count();++i){Json@ stage=stages.At(int(i));Vector3 p=stages.Position(stage);Vector4 color=stage.GetVector4("color",Vector4(.4f,.8f,.6f,1));
            if(i>0){Vector3 start=stages.Position(stages.At(int(i-1)));for(int j=0;j<=20;++j){Vector3 point=Math::Lerp(start,p,float(j)/20);point.z+=Sin(float(j)/20*3.14159265f)*.8f;Mesh("StageMapRoute","PrimitiveMesh-Box",point+Vector3(0,.03f,0),Vector3(.42f,.10f,.42f),Vector4(.7f,.57f,.3f,1));}}
            Object@ node=Mesh("StageNode","PrimitiveMesh-Cylinder-Closed",p+Vector3(0,.25f,0),Vector3(2.5f,.45f,2.5f),color);MeshButton@ button;node.AddComponent(@button);button.SetDisplayCameraObject(camera);nodes.insertLast(node);buttons.insertLast(button);
            Vector4 resourceColor=i%3==0?Vector4(.68f,.38f,.16f,1):(i%3==1?Vector4(.62f,.66f,.7f,1):Vector4(.18f,.78f,1,1));
            Object@ resource=Mesh("StageResource",ExpeditionResourceModel(int(i%3)),p+Vector3(0,.5f,0),Vector3(.65f,.65f,.65f),resourceColor);MeshButton@ resourceButton;resource.AddComponent(@resourceButton);resourceButton.SetDisplayCameraObject(camera);buttons.insertLast(resourceButton);
            for(int j=0;j<4;++j){float x=p.x+(j%2==0?-1.7f:1.7f),z=p.z+float(j/2)*1.8f-1;
                if(i==0){Mesh("StageTreeTrunk","PrimitiveMesh-Cylinder-Closed",Vector3(x,.6f,z),Vector3(.18f,1.1f,.18f),Vector4(.42f,.25f,.12f,1));Mesh("StageTree","PrimitiveMesh-Cone",Vector3(x,1.4f,z),Vector3(1.1f,1.6f,1.1f),color);}
                else Mesh("StageRock","PrimitiveMesh-Octahedron",Vector3(x,.4f,z),Vector3(.8f,i==2?1.4f:.8f,.8f),color);
            }
            TextRenderer@ label=ui.Text(stages.Name(stage),0,0,17);labels.insertLast(label);labelObjects.insertLast(ui.objects[ui.objects.length()-1]);
        }
        @marker=TownCreateCitizen(GetScene(),"StageTraveler",Vector4(1,.5f,.2f,1));markerPosition=stages.Position(stages.At(selected));routePosition=float(selected);
        @ring=Mesh("StageSelectionRing","PrimitiveMesh-Cylinder-Closed",markerPosition+Vector3(0,.08f,0),Vector3(3.1f,.12f,3.1f),Vector4(1,.84f,.2f,1));MeshRenderer@ r;ring.GetComponent(@r);r.SetMaterialName("TownGridUnlit");
        TownLight(GetScene(),Vector3(0,9,-3),Vector4(1,.9f,.75f,1),1.0f,"StageMapLight");Light@ fill;GetScene().GetObject("StageMapLight").GetComponent(@fill);fill.SetRadius(28);fill.SetCastShadows(false);
        TownApplySettings(GetScene());
        // マップのどこでも、行き先のモデルと経路が見分けられるようにする。
        Object@ screen=GetScene().GetObject("TownScreen");DepthOfFieldEffect@ dof;MotionBlurEffect@ motion;
        if(screen.GetComponent(@dof))dof.SetActive(false);if(screen.GetComponent(@motion))motion.SetActive(false);
        Object@ sun=GetScene().GetObject("SceneLight");Light@ sunlight;
        if(sun !is null&&sun.GetComponent(@sunlight)){sun.GetTransform().SetRotate(Vector3(.9f,-.5f,0));sunlight.SetColor(Vector4(1,.96f,.88f,1));sunlight.SetIntensity(.65f);}
        Select(selected);ready=true;
    }
    void Select(int index){if(index<0||index>=int(stages.Count()))return;selected=index;Json@ stage=stages.At(index);state.data.SetString("selectedStage",stage.GetString("id"));title.SetText(stages.Name(stage));description.SetText(TownTr(stage.GetString("description"),stage.GetString("descriptionEn")));weights.SetText(TownTr("素材傾向：木材 ","Resource bias: wood ")+stage.GetInt("woodWeight")+" / "+TownTr("石材 ","stone ")+stage.GetInt("stoneWeight")+" / "+TownTr("光結晶 ","crystals ")+stage.GetInt("crystalWeight"));ring.GetTransform().SetTranslate(stages.Position(stage)+Vector3(0,.08f,0));}
    void Go(bool expedition){if(!state.Save()){notice.SetText(TownTr("保存に失敗しました。再試行してください。","Save failed. Please retry."));return;}GetScene().SetNextSceneName(expedition?"ExpeditionScene":"CityScene");leaving=transition.Begin(expedition?"ExpeditionScene":"CityScene");if(!leaving)notice.SetText(TownTr("シーンの切り替えに失敗しました。","Unable to change scene."));}
    void Update(){
        if(ready&&!leaving)state.AdvanceRealTime();
        if(transition.Update(GetDeltaTime()))return;if(transition.failed){leaving=false;transition.failed=false;}if(!ready||leaving)return;float dt=GetDeltaTime()*GetGameSpeed();clock+=dt;
        if(IsCommandTriggered("SelectLeft")||IsCommandTriggered("SelectUp"))Select((selected+int(stages.Count())-1)%int(stages.Count()));if(IsCommandTriggered("SelectRight")||IsCommandTriggered("SelectDown"))Select((selected+1)%int(stages.Count()));
        // 薄い台座上の素材モデルをクリックしても、ノードを選択できる。
        for(uint i=0;i<buttons.length();++i)if(buttons[i].IsClicked())Select(int(i/2));
        int clicked=ui.Clicked();if(clicked==0||IsCommandTriggered("Submit")){Go(true);return;}if(clicked==1||IsCommandTriggered("Cancel")){Go(false);return;}
        routePosition=Easing::Lerp(routePosition,float(selected),TownClamp(GetDeltaTime()*4,0,1));int segment=int(Floor(routePosition));if(segment>=int(stages.Count())-1)segment=int(stages.Count())-2;
        if(segment>=0){float t=routePosition-float(segment);markerPosition=Math::Lerp(stages.Position(stages.At(segment)),stages.Position(stages.At(segment+1)),t);markerPosition.z+=Sin(t*3.14159265f)*.8f;}
        marker.GetTransform().SetTranslate(markerPosition+Vector3(-.4f,.5f+.025f*Sin(clock*6),-1.1f));
        ring.GetTransform().SetScale(Vector3(3.1f+.12f*Sin(clock*3),.12f,3.1f+.12f*Sin(clock*3)));
        Quaternion look=Math::MakeRotateEuler(Vector3(.65f+.008f*Sin(clock*.23f),-.35f+.016f*Sin(clock*.17f),.003f*Sin(clock*.19f)));
        camera.GetTransform().SetRotateQuaternion(look);
        Quaternion baseLook=Math::MakeRotateEuler(Vector3(.65f,-.35f,0));camera.GetTransform().SetTranslate(-baseLook.RotateVector(Vector3(0,0,1))*22+Vector3(.06f*Sin(clock*.13f),-2,0));
        Camera3D@ lens;camera.GetComponent(@lens);Matrix4x4 projection;projection.MakePerspectiveFovMatrix(lens.GetFovY(),lens.GetAspectRatio(),lens.GetNearClip(),lens.GetFarClip());Matrix4x4 view=camera.GetTransform().GetWorldMatrix().Inverse();
        for(uint i=0;i<labels.length();++i){Vector3 p=(stages.Position(stages.At(int(i)))+Vector3(0,2.5f,0)).Transform(view).Transform(projection);labelObjects[i].GetTransform().SetTranslate(Vector3((p.x+1)*640-65,(p.y+1)*360,0));labels[i].SetColor(int(i)==selected?Vector4(1,.9f,.3f,1):Vector4(1,1,1,1));}
        ui.RefreshLabels();
    }
}
