#include "TownState.as"
#include "TownPresentation.as"
#include "ExpeditionGenerator.as"
#include "TownSettings.as"
#include "ExpeditionObjects.as"
#include "ExpeditionHarvest.as"

class ExpeditionController : ScriptComponentBehavior {
    TownState@ state = TownState();
    TownUI@ ui = TownUI();
    ExpeditionGenerator@ generator = ExpeditionGenerator();
    Object@ player;
    array<Object@>@ pickups = array<Object@>();
    array<int>@ pickupKinds = array<int>();
    array<ExpeditionResource@>@ harvestNodes=array<ExpeditionResource@>();
    Object@ toolHandle;Object@ toolHead;Object@ weaponSocket;
    ScriptComponent@ playerScript;
    int lastAttack=0;
    TextRenderer@ resources; TextRenderer@ notice;
    bool ready = false, leaving = false;
    int collected = 0;
    Vector3 goalPosition;

    void Start() {
        if (!state.Initialize(GetScene())) { LogError("LightTownRules.jsonを読み込めません。"); return; }
        TownApplySettings(GetScene());
        @player = GetScene().GetObject("Player");
        if (player is null) return;
        int run = int(state.data.GetInt("expedition")) + 1;
        state.data.SetInt("expedition", run); state.Save();
        player.GetComponent(@playerScript);
        float jump=20,gravity=.8f;Vector3 maximum(8,32,8);
        playerScript.GetVariable("jumpPower",jump);playerScript.GetVariable("gravity",gravity);playerScript.GetVariable("maxVelocity",maximum);
        generator.MovementProfile(jump,gravity,maximum.x);
        if(!generator.Solve(uint(state.rules.GetInt("stageColumns",48)),uint(7301+run*7919))){LogError("Expedition: unable to construct a traversable stage.");Return();return;}
        goalPosition = generator.GoalPosition();
        Generate();
        @toolHandle=TownMesh(GetScene(),"PlayerPickaxeHandle","PrimitiveMesh-Box",Vector3(),Vector3(.13f,.9f,.13f),Vector4(.6f,.3f,.12f,1));
        @toolHead=TownMesh(GetScene(),"PlayerPickaxeHead","PrimitiveMesh-Box",Vector3(),Vector3(.7f,.16f,.23f),Vector4(.8f,.86f,.9f,1));
        toolHandle.SetActive(false);toolHead.SetActive(false);
        @weaponSocket=GetScene().GetObject("WeaponSocket_R");
        GetScene().GetVariable("Player.AttackSequence",lastAttack);
        TownApplySettings(GetScene());
        Vector3 spawn = generator.StartPosition();
        player.GetTransform().SetTranslate(spawn);
        // ランダム開始位置でも既定の追従カメラと先読みターゲットを自機の近くから開始する。
        Object@ camera = GetScene().GetObject("TownCamera");
        CameraController@ follow;
        if (camera.GetComponent(@follow)) camera.GetTransform().SetTranslate(spawn + follow.GetPositionOffset());
        Object@ cameraTarget = GetScene().GetObject("CameraTarget");
        if (cameraTarget !is null) cameraTarget.GetTransform().SetTranslate(spawn);
        GetScene().SetVariable("CheckpointPosition", spawn);
        GetScene().SetVariable("CheckpointPriority", -1);
        ui.Initialize(GetScene());
        ui.Text(TownTr("素材収集の遠征  #", "Gathering expedition #") + run, 22, 12, 24);
        @resources = ui.Text("", 355, 18, 20);
        ui.Text(TownTr("A/D・矢印：移動\nSpace：ジャンプ\n\nJ / 左クリック：攻撃・採集\n光の門に到着すると帰還", "A/D or arrows: Move\nSpace: Jump\n\nJ / Left click: Attack\nBreak resources to gather.\nReach the gate to return."), 22, 112, 19);
        ui.Button(TownTr("街に帰還 [Esc]", "Return to town [Esc]"), 22, 340);
        ui.Text(TownTr("木：丸太 / 石：岩塊 / 光結晶：結晶群\n\n攻撃で耐久値を削って採集。\n獲得時に保存します。\n途中帰還でも持ち帰れます。", "Wood: Logs / Stone: Rocks\nCrystals: Crystal clusters\n\nMaterials save on pickup.\nKeep them when returning early."), 22, 410, 17);
        @notice = ui.Text("", 350, 650, 18);
        ui.Text(TownTr("帰還ゲートの位置\nX: ", "Return gate position\nX: ") + int(goalPosition.x) + "  Y: " + int(goalPosition.y) +
            TownTr("\n通路・縦穴を探索（転送は近道）", "\nExplore tunnels and shafts; portals are shortcuts."), 22, 550, 17);
        if (generator.usedFallback) state.message = TownTr("生成失敗のため安全な代替ステージを使用しています。", "Using a safe fallback stage after generation failed.");
        else state.message = TownTr("素材を集め、部屋・通路・縦穴を探索して光の門から帰還しましょう。", "Explore chambers, tunnels and shafts, collect materials, and find the return gate.");
        ready = true;
        GetScene().SetVariable("Player.HarvestTarget",HarvestTarget(spawn,1)>=0);
    }
    void Generate() {
        // 岩盤／工場の躯体から部屋と通路を掘り抜く。連続した固体は矩形にまとめる。
        array<bool> built(generator.width * generator.height, false);
        Vector4 white(1, 1, 1, 1);
        for (int y = 0; y < int(generator.height); ++y) for (int x = 0; x < int(generator.width); ++x) {
            uint index = uint(y) * generator.width + uint(x);
            if (!generator.IsSolid(x, y) || built[index]) continue;
            int w = 1;
            while (x + w < int(generator.width) && generator.IsSolid(x + w, y) && !built[index + uint(w)]) ++w;
            int h = 1; bool extend = true;
            while (y + h < int(generator.height) && extend) {
                for (int dx = 0; dx < w; ++dx) if (!generator.IsSolid(x + dx, y + h) || built[uint(y + h) * generator.width + uint(x + dx)]) { extend = false; break; }
                if (extend) ++h;
            }
            for (int dy = 0; dy < h; ++dy) for (int dx = 0; dx < w; ++dx) built[uint(y + dy) * generator.width + uint(x + dx)] = true;
            ExpeditionFloor(GetScene(), "GeneratedGround", Vector3(float(x) + float(w) * 0.5f, float(y) + float(h) * 0.5f, 0), Vector3(float(w), float(h), 3), white);
        }
        for(uint i=0;i<generator.traversal.platforms.length();++i){ExpeditionPlatform@ p=generator.traversal.platforms[i];
            ExpeditionFloor(GetScene(),p.ramp?"GeneratedSlope":"GeneratedStep",p.position,p.size,white,p.angle);
        }
        for (uint i = 0; i < generator.rooms.length(); ++i) GenerateRoom(i);
        GenerateBranches();
        TownMesh(GetScene(), "ReturnGateLeft", "PrimitiveMesh-Box", goalPosition + Vector3(-0.9f, 0, 0), Vector3(0.2f, 2.6f, 0.4f), Vector4(0.3f, 0.9f, 1, 1));
        TownMesh(GetScene(), "ReturnGateRight", "PrimitiveMesh-Box", goalPosition + Vector3(0.9f, 0, 0), Vector3(0.2f, 2.6f, 0.4f), Vector4(0.3f, 0.9f, 1, 1));
        TownMesh(GetScene(), "ReturnGateTop", "PrimitiveMesh-Box", goalPosition + Vector3(0, 1.3f, 0), Vector3(2, 0.2f, 0.4f), Vector4(0.3f, 0.9f, 1, 1));
        TownLight(GetScene(), goalPosition + Vector3(0, 0.7f, -1), Vector4(0.3f, 0.9f, 1, 1), 8);
    }
    void GenerateRoom(uint index) {
        ExpeditionRoom@ room = generator.rooms[index];
        int kind = generator.Kind(index % generator.columns, index / generator.columns);
        bool factory = kind >= 2;
        Vector4 color = factory ? Vector4(0.32f, 0.36f, 0.4f, 1) : Vector4(0.3f, 0.28f, 0.25f, 1);
        TownMesh(GetScene(), "GeneratedBackground", "PrimitiveMesh-Box", Vector3(room.CenterX(), float(room.bottom) + float(room.height) * 0.5f, 4), Vector3(float(room.width), float(room.height), 0.6f), color);
        // 旧GameSceneの器具を天井と奥の壁へ取り付ける。プレイヤーの通行面は空ける。
        ExpeditionFactoryLight(GetScene(), Vector3(room.CenterX(), float(room.bottom + room.height) - 0.5f, 0), true);
        ExpeditionFactoryLight(GetScene(), Vector3(float(room.left) + 1.5f, float(room.bottom) + 4.6f, 0), false);
        if (factory) {
            TownMesh(GetScene(), "GeneratedPipe", "PrimitiveMesh-Box", Vector3(room.CenterX(), float(room.bottom + room.height) - 1.2f, 2.5f), Vector3(float(room.width), 0.18f, 0.3f), Vector4(0.6f, 0.65f, 0.7f, 1));
            TownMesh(GetScene(), "GeneratedMachine", "PrimitiveMesh-Box", Vector3(float(room.left + room.width) - 1.5f, float(room.bottom) + 1.2f, 2.5f), Vector3(2, 2.4f, 0.8f), color);
        } else {
            for (uint rock = 0; rock < 3; ++rock) {
                float x = float(room.left) + 1 + float(rock) * float(room.width - 2) * 0.5f;
                TownMesh(GetScene(), "GeneratedCaveRock", "PrimitiveMesh-Octahedron", Vector3(x, float(room.bottom + room.height) - 0.4f, 2), Vector3(1.5f, 2, 1), color);
            }
        }
        Vector3 anchor = generator.RoomAnchor(index);
        // 素材は支持床上に置く。全素材の部屋はWFCで最低一室ずつ確保する。
        int resource = kind == 3 ? 2 : (kind == 2 ? 1 : 0);
        ExpeditionResource@ node=ExpeditionResource();
        node.Initialize(GetScene(),resource,anchor+Vector3(1.7f,-1.06f,0));
        harvestNodes.insertLast(node);pickups.insertLast(node.model);pickupKinds.insertLast(resource);
        if(index!=generator.startRoom&&index!=generator.goalRoom&&index!=generator.ShortcutEntrance()&&index!=generator.ShortcutExit()&&index%3==0&&index%4!=0)
            ExpeditionEnemy(GetScene(),Vector3(float(room.left+room.width)-2,float(room.bottom)+.6f,0));
        if (index == generator.startRoom || index % 4 == 0) ExpeditionCheckpoint(GetScene(), anchor, 0);
        // 可動ギミックは上段の脇道へ配置し、検証済みの幹線と縦穴へ重ねない。
        // 可動コライダーを含む掃引範囲が幹線の外に収まる部屋だけを使う。
        if((room.ports&12)==0&&room.templateKind==3){
            if(index%2==0)ExpeditionMovingFloor(GetScene(),Vector3(room.CenterX()+2,float(room.bottom)+5.5f,0),1,.65f);
            else ExpeditionRotatingFloor(GetScene(),Vector3(room.CenterX()+2,float(room.bottom)+6,0),.65f);
        }
        if (index % 6 == 0) ExpeditionStreetlight(GetScene(), Vector3(float(room.left + room.width) - 1, float(room.bottom) + 0.5f, 2.5f), false);
    }
    void GenerateBranches() {
        for (uint i = 0; i < generator.links.length(); ++i) {
            ExpeditionLink@ link = generator.links[i]; ExpeditionRoom@ low = generator.rooms[link.a]; ExpeditionRoom@ high = generator.rooms[link.b];
            if (!link.vertical) {
                ExpeditionFactoryLight(GetScene(), Vector3((low.CenterX() + high.CenterX()) * 0.5f, float(low.bottom + high.bottom) * 0.5f + 4, 0), false);
                continue;
            }
            ExpeditionFactoryLight(GetScene(), Vector3(link.shaftX + 0.5f, float(low.bottom + high.bottom) * 0.5f + 3, 0), false);
        }
        // 任意のショートカット。通常ルートはテレポートなしでも全室につながる。
        uint entrance = generator.ShortcutEntrance(), other = generator.ShortcutExit();
        Vector3 a = generator.RoomAnchor(entrance), b = generator.RoomAnchor(other);
        ExpeditionTeleport(GetScene(), a + Vector3(1.5f, 0, 0), b);
        ExpeditionTeleport(GetScene(), b + Vector3(1.5f, 0, 0), a);
    }
    void Update() {
        if (!ready || leaving) return;
        state.Tick(GetDeltaTime() * GetGameSpeed());
        if (ui.Clicked() == 0 || IsCommandTriggered("Cancel")) { Return(); return; }
        Vector3 p = player.GetTransform().GetTranslate();
        UpdateAttack(GetDeltaTime()*GetGameSpeed());
        for(uint i=0;i<harvestNodes.length();++i)harvestNodes[i].Update(GetDeltaTime()*GetGameSpeed());
        // マップ外に出たら初期位置または最後に触れたチェックポイントへ復帰。
        if (p.y < -6 || p.y > float(generator.height) + 6 || p.x < -8 || p.x > float(generator.width) + 6) {
            ScriptComponent@ script; player.GetComponent(@script); script.CallMethod("Respawn");
            state.message = TownTr("チェックポイントへ戻りました。収集済み素材は保持しています。", "Returned to the checkpoint. Collected materials are safe.");
        }
        Vector3 goalDelta = p - goalPosition;
        if (goalDelta.x * goalDelta.x + goalDelta.y * goalDelta.y < 2.25f) { Return(); return; }
        resources.SetText(TownTr("木材 ", "Wood ") + state.data.GetInt("wood") + TownTr("    石材 ", "    Stone ") + state.data.GetInt("stone") + TownTr("    光結晶 ", "    Crystal ") + state.data.GetInt("crystal") +
            TownTr("    収集 ", "    Collected ") + collected);
        notice.SetText(state.saveFailed ? TownTr("保存に失敗しました。街へ戻って保存を再試行してください。", "Save failed. Return to town and retry.") : state.message);
    }
    bool ClearHarvestRay(const Vector3 &in a,const Vector3 &in b){
        for(uint i=1;i<=24;++i){Vector3 p=a+(b-a)*(float(i)/24);if(generator.IsSolid(int(Floor(p.x)),int(Floor(p.y))))return false;
            for(uint j=0;j<generator.traversal.platforms.length();++j){ExpeditionPlatform@ s=generator.traversal.platforms[j];Vector3 d=p-s.position;
                float x=d.x*Cos(s.angle)+d.y*Sin(s.angle),y=-d.x*Sin(s.angle)+d.y*Cos(s.angle);
                if(Abs(x)<s.size.x*.5f&&Abs(y)<s.size.y*.5f)return false;
            }
        }
        return true;
    }
    int HarvestTarget(const Vector3 &in origin,float facing){
        int best=-1;float distance=100;
        for(uint i=0;i<harvestNodes.length();++i){ExpeditionResource@ n=harvestNodes[i];if(n.hp<=0)continue;
            Vector3 target=n.base+Vector3(0,.7f,0);Vector3 d=target-origin;
            if(ExpeditionAttackRange(origin,target,facing)&&d.Dot(d)<distance&&ClearHarvestRay(origin,target)){best=int(i);distance=d.Dot(d);}
        }
        return best;
    }
    bool Strike(const Vector3 &in origin,float facing){
        int best=HarvestTarget(origin,facing);if(best<0)return false;ExpeditionResource@ n=harvestNodes[uint(best)];bool broken=n.Damage();
        if(broken){int k=n.kind;state.AddResources(k==0?4:0,k==1?4:0,k==2?3:0);++collected;@pickups[uint(best)]=null;state.Save();
            state.message=ExpeditionResourceName(k)+" +"+(k==2?3:4);
        }else state.message=ExpeditionResourceName(n.kind)+TownTr(" 耐久 "," HP ")+n.hp+" / "+n.maxHP;
        return true;
    }
    void UpdateAttack(float dt){
        if(dt<=0)return;
        int sequence=lastAttack;float facing=1,age=1;bool active=false;
        GetScene().GetVariable("Player.AttackSequence",sequence);GetScene().GetVariable("Player.AttackFacing",facing);
        GetScene().GetVariable("Player.AttackAge",age);GetScene().GetVariable("Player.AttackActive",active);
        Vector3 p=player.GetTransform().GetTranslate();
        if(sequence!=lastAttack){lastAttack=sequence;Strike(p,facing);}
        float targetFacing=facing;GetScene().GetVariable("Player.Facing",targetFacing);
        GetScene().SetVariable("Player.HarvestTarget",HarvestTarget(p,targetFacing)>=0);
        bool humanoid=false,weapon=false;GetScene().GetVariable("Player.Humanoid",humanoid);GetScene().GetVariable("Player.WeaponSwing",weapon);
        if(humanoid&&weaponSocket !is null){
            toolHandle.SetActive(active&&weapon);toolHead.SetActive(active&&weapon);
            toolHandle.GetTransform().SetParentObject(weaponSocket);toolHead.GetTransform().SetParentObject(weaponSocket);
            toolHandle.GetTransform().SetScale(Vector3(.04f,.32f,.04f));toolHandle.GetTransform().SetTranslate(Vector3(-.1f,0,0));toolHandle.GetTransform().SetRotate(Vector3(0,0,1.5707963f));
            toolHead.GetTransform().SetScale(Vector3(.18f,.045f,.07f));toolHead.GetTransform().SetTranslate(Vector3(-.26f,0,0));toolHead.GetTransform().SetRotate(Vector3(0,0,1.5707963f));
            return;
        }
        toolHandle.GetTransform().SetParentObject(null);toolHead.GetTransform().SetParentObject(null);
        toolHandle.GetTransform().SetScale(Vector3(.13f,.9f,.13f));toolHead.GetTransform().SetScale(Vector3(.7f,.16f,.23f));
        toolHandle.SetActive(active);toolHead.SetActive(active);
        if(active){float angle=facing*(-1.1f+age/.3f*3);Vector3 pivot=p+Vector3(facing*.7f,.15f,-.7f);
            toolHandle.GetTransform().SetTranslate(pivot);toolHandle.GetTransform().SetRotate(Vector3(0,0,angle));
            toolHead.GetTransform().SetTranslate(pivot+Vector3(-Sin(angle)*.43f,Cos(angle)*.43f,0));toolHead.GetTransform().SetRotate(Vector3(0,0,angle));
        }
    }
    void Return() {
        state.Save(); GetScene().SetNextSceneName("CityScene"); leaving = GetScene().ChangeToNextScene();
        if (!leaving) state.message = TownTr("街への帰還に失敗しました。", "Unable to return to town.");
    }
}
