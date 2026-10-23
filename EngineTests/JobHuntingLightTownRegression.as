#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownState.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/ExpeditionGenerator.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownSettings.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownCamera.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/PostEffectController.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/TeleportFloor.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownBuildings.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownFireflies.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownCityHUD.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/ExpeditionHarvest.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/Player/PlayerAttack.as"
#include "../GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownFacilityIndicators.as"

void Check(bool passed, const string &in message) {
    if (!passed) throw(message);
}
void Run() {
    float facilityHit=0;
    Check(TownRayFacility(Vector3(0,3,-5),Vector3(0,-2,10),Vector3(0,1,0),Vector3(.7f,1,.7f),facilityHit)&&facilityHit>0&&facilityHit<1,"facility selection includes the building height");
    Check(!TownRayFacility(Vector3(2,3,-5),Vector3(0,-2,10),Vector3(0,1,0),Vector3(.7f,1,.7f),facilityHit),"parallel ray outside facility does not select it");
    Check(!TownRayFacility(Vector3(0,1,5),Vector3(0,0,10),Vector3(0,1,0),Vector3(.7f,1,.7f),facilityHit),"facilities behind the camera cannot be selected");
    PlayerAttack attack;
    Check(!attack.Update(.04f,1,true)&&attack.Active(),"attack begins with a windup");
    Check(attack.Update(.06f,0,false)&&attack.sequence==1,"attack emits one hit after windup");
    Check(!attack.Update(.1f,-1,true)&&attack.sequence==1&&attack.swingFacing==1,"cooldown blocks repeat attacks and locks swing direction");
    attack.Update(.3f,-1,false);Check(!attack.Active(),"attack animation ends");
    attack.Update(.04f,-1,true);Check(attack.Update(.06f,0,false)&&attack.sequence==2&&attack.swingFacing==-1,"next attack faces the latest movement direction");
    attack.Reset();Check(!attack.Active(),"respawn cancels attack");
    Check(!attack.Update(0,1,true),"paused attack does not emit hits");
    Check(ExpeditionAttackRange(Vector3(),Vector3(1.7f,.4f,0),1)&&!ExpeditionAttackRange(Vector3(),Vector3(1.7f,.4f,0),-1)&&!ExpeditionAttackRange(Vector3(),Vector3(3,0,0),1),"harvest attacks require facing and short range");
    for(int kind=0;kind<3;++kind){ExpeditionResource node;node.kind=kind;node.hp=node.maxHP=ExpeditionResourceHP(kind);
        for(int hit=1;hit<node.maxHP;++hit)Check(!node.Damage()&&node.hp>0,"partial resource damage gives no harvest");
        Check(node.Damage()&&node.hp==0&&!node.Damage(),"resource breaks exactly once after sufficient attacks");
    }
    TownState state;
    Check(state.Initialize(GetScene()), "state initialization");
    state.NewGame();
    int wood = int(state.data.GetInt("wood"));
    Check(!state.Build(5, 0, 0) && state.data.GetInt("wood") == wood, "failed build must not consume materials");
    Check(state.Build(0, 0, 0), "first lamp");
    Check(state.WorkingCount(0)==0,"lamp without adjacent road is inactive");
    Check(state.data.GetFloat("energy")==60&&state.data.GetFloat("safety")==50,"placement keeps existing periodic parameter rules");
    Check(state.Build(6,0,1)&&state.WorkingCount(0)==1,"road-connected lamp bootstraps its own illumination");
    wood = int(state.data.GetInt("wood"));
    Check(!state.Build(1, 0, 0) && state.data.GetInt("wood") == wood, "occupied cell atomicity");
    Check(!state.Build(0, -1, 0) && !state.Build(0, 16, 0), "grid bounds");
    state.BuyTransformation();
    Check(!state.data.GetBool("transformationUnlocked"), "shop required");
    Check(state.Build(3, 1, 0), "shop construction");
    Check(state.WorkingCount(3)==0,"lit shop without road adjacency remains inactive");
    Check(state.Build(6,1,1)&&state.WorkingCount(3)==1,"adjacent road activates illuminated shop");
    state.data.SetInt("crystal", 0); state.BuyTransformation();
    Check(!state.data.GetBool("transformationUnlocked"), "purchase cost required");
    state.AddResources(0, 0, 20); state.BuyTransformation();
    Check(!TownTransformationsEnabled()&&!state.data.GetBool("transformationUnlocked") && state.data.GetInt("crystal") == 20, "transformation unavailable without charging resources");
    Json@ legacyItems=Json();legacyItems.Parse("[{\"action\":\"transformation\",\"crystal\":12}]");state.rules.SetJson("shopItems",legacyItems);
    Check(!state.BuyProduct(0)&&state.data.GetInt("crystal")==20,"legacy transformation item cannot bypass feature policy");
    state.data.SetBool("transformationUnlocked",true);state.Save();
    state.BuyTransformation();
    Check(state.data.GetInt("crystal") == 20, "legacy unlock flag retained without permitting purchase");
    state.AddResources(100, 100, 100);
    Check(state.Build(4, 2, 0), "guild construction");
    Check(state.Build(6,2,1)&&state.WorkingCount(4)==1,"guild road and light requirements");
    state.Dispatch(); state.Tick(20); state.Save();
    TownState resumed;
    Check(resumed.Initialize(GetScene()), "load from disk");
    Check(resumed.data.GetBool("dispatchActive") && resumed.data.GetFloat("dispatchRemaining") == 25, "dispatch countdown persistence");
    resumed.Dispatch();
    Check(resumed.data.GetFloat("dispatchRemaining") == 25, "cannot restart active dispatch");
    resumed.Tick(25);
    Check(!resumed.data.GetBool("dispatchActive"), "dispatch completion");
    int crystal = int(resumed.data.GetInt("crystal")); resumed.Tick(0.1f);
    Check(resumed.data.GetInt("crystal") == crystal, "dispatch reward only once");
    Check(resumed.Buildings().Size() == 6 && resumed.data.GetBool("transformationUnlocked"), "buildings and unlock persist");

    TownState guildState;Check(guildState.Initialize(GetScene()),"per guild state initialization");guildState.NewGame();guildState.AddResources(1000,1000,1000);
    for(int x=4;x<=9;++x)Check(guildState.Build(6,x,5),"guild network roads");
    Check(guildState.Build(0,6,4)&&guildState.Build(4,5,4)&&guildState.Build(4,7,4),"two operational guilds");
    guildState.rules.SetFloat("productionSeconds",3600);guildState.rules.SetFloat("eventSeconds",10000);
    Check(guildState.DispatchAt(5,4)&&!guildState.DispatchAt(5,4),"one independent job per guild");guildState.Tick(10);
    Check(guildState.DispatchAt(7,4)&&guildState.data.GetInt("dispatchCount")==2,"multiple guilds dispatch concurrently");guildState.Save();
    TownState guildReload;Check(guildReload.Initialize(GetScene()),"guild reload");guildReload.rules.SetFloat("productionSeconds",3600);guildReload.rules.SetFloat("eventSeconds",10000);
    Check(guildReload.BuildingAt(5,4).GetFloat("dispatchRemaining")==35&&guildReload.BuildingAt(7,4).GetFloat("dispatchRemaining")==45,"independent guild timers survive save and reload");
    int guildWood=int(guildReload.data.GetInt("wood"));guildReload.Tick(35);
    Check(!guildReload.BuildingAt(5,4).GetBool("dispatchActive")&&guildReload.BuildingAt(7,4).GetFloat("dispatchRemaining")==10&&guildReload.data.GetInt("wood")==guildWood+20,"first guild rewards once without completing the other");
    guildReload.Tick(10);Check(guildReload.data.GetInt("dispatchCount")==0&&guildReload.data.GetInt("wood")==guildWood+40,"second guild awards its own return reward");
    guildReload.Tick(.1f);Check(guildReload.data.GetInt("wood")==guildWood+40,"guild return rewards cannot repeat");
    guildReload.data.Remove("guildDispatchVersion");guildReload.data.SetBool("dispatchActive",true);guildReload.data.SetFloat("dispatchRemaining",17);guildReload.Save();
    TownState legacyGuild;Check(legacyGuild.Initialize(GetScene())&&legacyGuild.BuildingAt(5,4).GetFloat("dispatchRemaining")==17&&legacyGuild.data.GetInt("dispatchCount")==1,"old single dispatch migrates to one guild");
    legacyGuild.Save();TownState legacyAgain;Check(legacyAgain.Initialize(GetScene())&&legacyAgain.data.GetInt("dispatchCount")==1,"legacy dispatch migration is idempotent");
    legacyAgain.data.SetFloat("productionClock",3.5f);Check(Abs(legacyAgain.ProductionProgress()-.35f)<.001f,"production bar uses the actual shared payout clock");
    Check(guildState.DeleteProblem(5,4)!=""&&!guildState.RemoveBuilding(5,4),"away guild cannot be removed before its NPC returns");
    int removeWood=int(legacyAgain.data.GetInt("wood"));Check(legacyAgain.RemoveBuilding(7,4)&&legacyAgain.BuildingAt(7,4) is null&&legacyAgain.data.GetInt("wood")==removeWood,"idle facility deletion has no refund");
    Check(!legacyAgain.RemoveBuilding(7,4),"empty cell deletion is rejected");legacyAgain.Save();TownState removedReload;Check(removedReload.Initialize(GetScene())&&removedReload.BuildingAt(7,4) is null,"facility deletion persists");

    // アクション報酬ゼロでも街側の生産のみで塔まで進める。
    state.NewGame();
    for(int x=0;x<=8;++x)Check(state.Build(6,x,1),"initial roads without action income");
    array<int> plan = {0,0,1,1,2,2,3,4};array<int> positions={0,6,1,2,3,4,5,7};
    for(uint i=0;i<plan.length();++i){
        for(int step=0;step<300&&state.BuildProblem(plan[i],positions[i],0)!="";++step)state.Tick(10);
        Check(state.Build(plan[i],positions[i],0),"city-only operational facility progression");
        Check(state.IsWorking(positions[i],0),"planned facilities have roads and lighting");
    }
    for(int step=0;step<300&&state.BuildProblem(5,8,0)!="";++step)state.Tick(10);
    Check(state.Build(5,8,0)&&state.data.GetBool("cleared"),"city-only victory with operational tower");
    Check(state.ClearPending(), "tower completion queues the clear decision");
    Check(state.Save(), "save before choosing clear action");
    TownState pendingClear;Check(pendingClear.Initialize(GetScene())&&pendingClear.ClearPending(), "unacknowledged clear survives reload");
    Check(pendingClear.AcknowledgeClear()&&!pendingClear.ClearPending(), "continue acknowledges and saves the clear decision");
    TownState continuedClear;Check(continuedClear.Initialize(GetScene())&&continuedClear.data.GetBool("cleared")&&!continuedClear.ClearPending(), "continued game does not reopen clear UI on reload");
    Check(!continuedClear.AcknowledgeClear(), "clear decision cannot be accepted twice");
    continuedClear.NewGame();Check(!continuedClear.ClearPending()&&!continuedClear.data.GetBool("clearAcknowledged"), "new game resets clear decision");
    Check(!state.Build(5,9,0),"no duplicate victory tower");
    TownState darkTower;Check(darkTower.Initialize(GetScene()),"inactive tower scenario");darkTower.NewGame();darkTower.AddResources(1000,1000,1000);
    for(int x=0;x<=8;++x)Check(darkTower.Build(6,x,1),"tower scenario roads");
    for(uint i=0;i<plan.length();++i)Check(darkTower.Build(plan[i],positions[i],0),"tower scenario facilities");
    darkTower.data.SetFloat("energy",100);darkTower.data.SetFloat("prosperity",100);
    Check(darkTower.Build(5,15,15)&&!darkTower.data.GetBool("cleared"),"unlit disconnected tower does not clear the game");
    Check(darkTower.Build(6,15,14)&&!darkTower.data.GetBool("cleared"),"tower road alone does not clear the game");
    Check(darkTower.Build(0,14,14)&&darkTower.data.GetBool("cleared"),"tower becomes functional and clears after road and light are supplied");
    TownState network;Check(network.Initialize(GetScene()),"network test state");network.NewGame();network.AddResources(1000,1000,1000);
    Check(network.Build(1,5,5)&&!network.IsWorking(5,5),"unconnected and unlit house inactive");
    Check(network.Build(6,5,6)&&!network.IsWorking(5,5),"road alone insufficient");
    Check(network.Build(0,6,6)&&network.IsWorking(5,5),"streetlight activates road-adjacent house");
    Check(!network.IsLit(15,15),"distant cells outside lamp radius");
    Check(network.Build(6,10,10)&&network.RoadMask(10,10)==0,"isolated road shape");
    Check(network.Build(6,9,10)&&network.RoadMask(10,10)==1,"left road connection");
    Check(network.Build(6,11,10)&&network.RoadMask(10,10)==3,"straight road connection");
    Check(network.Build(6,10,9)&&network.RoadMask(10,10)==7,"T junction");
    Check(network.Build(6,10,11)&&network.RoadMask(10,10)==15,"four way intersection");
    Check(network.Build(6,9,9)&&network.RoadMask(10,10)==15,"diagonal roads do not add connections");
    Check(network.Build(3,14,14)&&network.Build(6,14,15)&&network.WorkingCount(3)==0,"unlit shop stays inactive");
    int networkCrystal=int(network.data.GetInt("crystal"));Check(!network.BuyProduct(0)&&network.data.GetInt("crystal")==networkCrystal,"inactive shop cannot charge for products");
    Check(network.Build(4,15,12)&&network.Build(6,15,13),"unlit guild fixture");network.Dispatch();Check(!network.data.GetBool("dispatchActive"),"inactive guild cannot dispatch NPCs");
    Check(network.Build(2,12,12)&&network.Build(6,12,13),"unlit workshop fixture");
    int beforeWood=int(network.data.GetInt("wood"));network.Produce();Check(network.data.GetInt("wood")==beforeWood+4,"inactive workshop produces no extra resources");
    Check(network.Build(2,4,6)&&network.WorkingCount(2)==1,"lit workshop on road active");
    beforeWood=int(network.data.GetInt("wood"));network.Produce();Check(network.data.GetInt("wood")==beforeWood+10,"only working workshop contributes production");
    network.QueueEvent("test notice");network.Save();TownState restored;Check(restored.Initialize(GetScene()),"network save restoration");
    Check(restored.RoadMask(10,10)==15&&restored.IsWorking(5,5),"roads and operation recomputed on load");
    Check(restored.TakeEvents().Size()>0&&restored.TakeEvents().Size()==0,"event notifications persist and are consumed once");
    Check(TownDeltaColor(32,1).y>TownDeltaColor(32,1).x&&TownDeltaColor(-16,1).x>TownDeltaColor(-16,1).y,"resource gains green and losses red");
    Check(TownBurstAlpha(0)==1&&TownBurstAlpha(.6f)<1&&TownBurstAlpha(1.3f)==0,"resource delta fades over its animation");
    Check(state.data.GetFloat("energy") >= 0 && state.data.GetFloat("energy") <= 2046, "energy bounds");
    TownState energyState;Check(energyState.Initialize(GetScene()), "energy cap state");energyState.NewGame();
    energyState.data.SetFloat("energy",100);energyState.Produce();Check(energyState.data.GetFloat("energy")==104, "energy grows beyond former cap");
    energyState.data.SetFloat("energy",2045);energyState.Produce();Check(energyState.data.GetFloat("energy")==2046, "production clamps energy at 2046");
    energyState.data.SetFloat("energy",3000);energyState.Save();TownState cappedLoad;Check(cappedLoad.Initialize(GetScene())&&cappedLoad.data.GetFloat("energy")==2046, "loaded energy is capped");
    Check(state.data.GetFloat("happiness") <= 100 && state.data.GetFloat("safety") <= 100 && state.data.GetFloat("prosperity") <= 100, "parameter bounds");

    uint startRows = 0, goalRows = 0; bool leftward = false, rightward = false;
    for (uint seed = 1; seed <= 128; ++seed) {
        ExpeditionGenerator stage;
        Check(stage.Solve(48, seed), "room biome WFC generation");
        Check(stage.routeValidated&&stage.repairCount<2,"normal-form traversal verification without flat fallback seed="+seed+" reason="+stage.routeProblem);
        Check(stage.mainRoute.length()>1&&stage.mainRoute[0]==stage.startRoom&&stage.mainRoute[stage.mainRoute.length()-1]==stage.goalRoom,"main route connects random endpoints");
        Check(stage.columns == 4 && stage.rows == 4 && stage.rooms.length() == 16, "sixteen rooms across a broad two dimensional map");
        Check(stage.links.length() >= stage.rooms.length(), "connected rooms include a loop seed="+seed+" repairs="+stage.repairCount+" reason="+stage.routeProblem);
        Check(stage.links.length() < 24, "some neighbouring rooms stay separated by walls");
        Check(stage.ShortcutEntrance() != stage.startRoom && stage.ShortcutExit() != stage.startRoom && stage.ShortcutEntrance() != stage.ShortcutExit(), "shortcut triggers kept away from initial spawn");
        array<int>@ distance = stage.Distances(stage.startRoom);
        bool woodRoom = false, stoneRoom = false, crystalRoom = false; int vertical = 0, horizontal = 0;
        for (uint i = 0; i < stage.rooms.length(); ++i) {
            Check(distance[i] >= 0, "every room reachable in the connection graph");
            Vector3 anchor = stage.RoomAnchor(i); ExpeditionRoom@ room = stage.rooms[i];
            string supportMask;
            for(int x=room.left;x<room.left+room.width;++x) supportMask += stage.IsSolid(x,room.bottom-1) ? "#" : ".";
            Check(stage.IsSolid(int(anchor.x), room.bottom - 1), "spawn and resource anchors have solid support seed=" + seed + " room=" + i + " x=" + anchor.x + " left=" + room.left + " mask=" + supportMask);
            Check(!stage.IsSolid(int(anchor.x), room.bottom) && !stage.IsSolid(int(anchor.x), room.bottom + 2), "anchors have player headroom");
            int kind = stage.Kind(i % stage.columns, i / stage.columns);
            woodRoom = woodRoom || kind < 2; stoneRoom = stoneRoom || kind == 2; crystalRoom = crystalRoom || kind == 3;
            if (i % stage.columns > 0) Check(Abs(float(stage.levels[kind] - stage.levels[stage.Kind(i % stage.columns - 1, i / stage.columns)])) <= 1, "horizontal biome compatibility");
            if (i >= stage.columns) Check(Abs(float(stage.levels[kind] - stage.levels[stage.Kind(i % stage.columns, i / stage.columns - 1)])) <= 1, "vertical biome compatibility");
        }
        Check(woodRoom && stoneRoom && crystalRoom, "all material rooms exist");
        uint platformCursor=0;
        for (uint i = 0; i < stage.links.length(); ++i) {
            ExpeditionLink@ link = stage.links[i];
            if (!link.vertical) { ++horizontal; continue; }
            ++vertical;
            float rise = float(stage.rooms[link.b].bottom - stage.rooms[link.a].bottom);
            uint steps = uint(Ceil(rise / 2.6f));
            Check(rise / float(steps) < stage.traversal.jump*stage.traversal.jump/(2*stage.traversal.gravity), "shaft platforms below normal jump apex");
            for(uint s=0;s<steps;++s){ExpeditionPlatform@ p=stage.traversal.platforms[platformCursor++];
                Check(Abs(p.position.y+p.size.y*.5f-(float(stage.rooms[link.a].bottom)+rise*float(s+1)/float(steps)))<.001f,"validated shaft surfaces match generated collider top heights");
            }
            for (int y = stage.rooms[link.a].bottom; y < stage.rooms[link.b].bottom + 3; ++y)
                Check(!stage.IsSolid(int(link.shaftX), y), "shaft not blocked by the former room ceiling");
        }
        Check(vertical > 0 && horizontal > 0, "horizontal and vertical passages exist");
        // 実際の掘削マスクも探索する。グラフだけ接続して壁が残る回帰を検出。
        Vector3 spawn = stage.StartPosition(), goal = stage.GoalPosition();
        uint origin = uint(spawn.y) * stage.width + uint(spawn.x);
        array<bool> seen(stage.solid.length(), false); array<uint> queue = {origin}; seen[origin] = true;
        for (uint head = 0; head < queue.length(); ++head) {
            int x = int(queue[head] % stage.width), y = int(queue[head] / stage.width);
            array<int> dx = {-1, 1, 0, 0}, dy = {0, 0, -1, 1};
            for (uint d = 0; d < 4; ++d) {
                int nx = x + dx[d], ny = y + dy[d];
                if (stage.IsSolid(nx, ny)) continue;
                uint next = uint(ny) * stage.width + uint(nx);
                if (!seen[next]) { seen[next] = true; queue.insertLast(next); }
            }
        }
        for (uint i = 0; i < stage.rooms.length(); ++i) {
            Vector3 anchor = stage.RoomAnchor(i);
            Check(seen[uint(anchor.y) * stage.width + uint(anchor.x)], "all room anchors connected in carved geometry");
        }
        Check(distance[stage.goalRoom] >= 4, "goal separated by several connected rooms");
        for (int x = 0; x < int(stage.width); ++x) Check(stage.IsSolid(x, 0) && stage.IsSolid(x, int(stage.height) - 1), "closed cave top and bottom");
        startRows |= uint(1) << (stage.startRoom / stage.columns); goalRows |= uint(1) << (stage.goalRoom / stage.columns);
        leftward = leftward || goal.x < spawn.x; rightward = rightward || goal.x > spawn.x;
        ExpeditionGenerator again; again.Solve(48, seed);
        Check(again.startRoom == stage.startRoom && again.goalRoom == stage.goalRoom && again.links.length() == stage.links.length(), "seed reproducibility");
        for (uint i = 0; i < stage.solid.length(); ++i) Check(again.solid[i] == stage.solid[i], "same seed reproduces carved terrain");
    }
    Check(startRows == 15 && goalRows == 15 && leftward && rightward, "random endpoints cover every room row and both directions");
    Object@ traveller = GetScene().CreateObject("TeleportCCDProbe");
    SphereCollider@ travelSphere; BoxCollider@ travelBox;
    traveller.AddComponent(@travelSphere); traveller.AddComponent(@travelBox);
    travelSphere.SetContinuousDetection(true); travelBox.SetContinuousDetection(false);
    Object@ destination = GetScene().CreateObject("TeleportDestinationProbe");
    destination.GetTransform().SetTranslate(Vector3(40, 20, 0));
    TeleportFloor@ portal = TeleportFloor(); portal.deferTeleport = true; @portal.targetObject = destination;
    HitInfo hit; @hit.otherObject = traveller;
    portal.OnCollisionEnter(hit);
    Check(traveller.GetTransform().GetTranslate().x == 0 && travelSphere.IsContinuousDetection(), "deferred teleport waits until collision dispatch finishes");
    portal.Update();
    Check(traveller.GetTransform().GetTranslate().x == 40 && !travelSphere.IsContinuousDetection(), "teleport suppresses CCD for the relocation frame");
    portal.Update();
    Check(travelSphere.IsContinuousDetection() && !travelBox.IsContinuousDetection(), "teleport restores original per-collider CCD flags");
    // 自転ではカメラ位置を維持する。焦点は選択セル側で管理する。
    Object@ cameraObject = GetScene().CreateObject("CameraMathTest");
    TownCameraRig@ rig = TownCameraRig(); @rig.camera = cameraObject;
    rig.position = Vector3(18, 30, -32);
    rig.Apply();
    Vector3 position = cameraObject.GetTransform().GetTranslate();
    Vector3 forward = cameraObject.GetTransform().GetRotateQuaternion().RotateVector(Vector3(0, 0, 1));
    float oldYaw = rig.yaw; rig.Rotate(Vector2(80, 20)); rig.Apply();
    Check(rig.yaw > oldYaw, "horizontal rotation follows mouse");
    Vector3 error = cameraObject.GetTransform().GetTranslate() - position;
    Check(error.Dot(error) < 0.00001f, "rotation keeps camera position fixed");
    rig.yaw = 1; rig.pitch = 0.7f; rig.position = Vector3(0, 0, 0);
    rig.Move(0, 1, 1);
    Vector3 moveError = rig.position - rig.Orientation().RotateVector(Vector3(0, 0, 12));
    Check(moveError.Dot(moveError) < 0.0001f && rig.position.y < -1, "W moves in full camera forward direction");
    rig.position = Vector3(0, 0, 0); rig.Pan(Vector2(0, 10));
    Check(rig.position.y > 0.1f && rig.position.z > 0.1f, "middle pan follows camera up including depth");
    rig.pitch = -10; rig.Apply(); Check(rig.pitch == 0.3f, "camera pitch lower bound");
    rig.pitch = 10; rig.Apply(); Check(rig.pitch == 1.3f, "camera pitch upper bound");
    // 保存キーだけでなく、音量バスとシーンコンポーネントへの反映も検証。
    Object@ screen = GetScene().CreateObject("TownScreen");
    BloomEffect@ bloom; ColorAdjustEffect@ color;
    screen.AddComponent(@bloom); screen.AddComponent(@color);
    GTAOEffect@ gtao; SSAOEffect@ ssao; FXAAEffect@ fxaa;
    MotionBlurEffect@ motion; DepthOfFieldEffect@ dof;
    ChromaticAberrationEffect@ chromatic; VignetteEffect@ vignette;
    screen.AddComponent(@gtao); screen.AddComponent(@ssao); screen.AddComponent(@fxaa);
    screen.AddComponent(@motion); screen.AddComponent(@dof);
    screen.AddComponent(@chromatic); screen.AddComponent(@vignette);
    Object@ sun = GetScene().CreateObject("TownAmbient");
    Light@ light; sun.AddComponent(@light);
    SetSettingFloat("town.seVolume", 0.25f); SetSettingFloat("town.bgmVolume", 0.4f);
    SetSettingFloat("town.brightness", -0.2f);
    SetSettingBool("town.shadows", false); SetSettingInt("town.shadowResolution", 512);
    SetSettingBool("town.postEffects", false);
    TownApplySettings(GetScene());
    Check(GetCategoryVolume("SE") == 0.25f && GetCategoryVolume("BGM") == 0.4f, "separate audio buses");
    Check(!light.IsCastShadows() && light.GetShadowMapResolution() == 512, "shadow settings applied");
    Object@ spotObject = GetScene().CreateObject("HistoricalStreetSpot");
    Object@ fillObject = GetScene().CreateObject("HistoricalStreetFill");
    Light@ spot; Light@ fill; spotObject.AddComponent(@spot); fillObject.AddComponent(@fill);
    SetSettingBool("town.shadows", true); TownApplySettings(GetScene());
    Check(!light.IsCastShadows() && spot.IsCastShadows() && !fill.IsCastShadows(), "historical directional and sphere fill stay shadowless when shadows are enabled");
    Object@ factoryObject = GetScene().CreateObject("HistoricalFactoryShadow");
    Light@ factoryLight; factoryObject.AddComponent(@factoryLight);
    TownApplySettings(GetScene());
    Check(factoryLight.IsCastShadows(), "historical factory fixture shadows can be enabled");
    SetSettingBool("town.shadows", false); TownApplySettings(GetScene());
    Check(!factoryLight.IsCastShadows() && factoryLight.GetShadowMapResolution() == 512, "factory fixtures follow shadow and resolution settings");
    Check(!bloom.IsActive() && color.IsActive() && color.GetBrightness() < -0.19f, "post effect and independent brightness");
    Check(!gtao.IsActive() && !ssao.IsActive() && !fxaa.IsActive() && !motion.IsActive() && !dof.IsActive() && !chromatic.IsActive() && !vignette.IsActive(), "all optional prefab effects disabled");
    SetSettingBool("town.postEffects", true); TownApplySettings(GetScene());
    Check(gtao.IsActive() && !ssao.IsActive() && fxaa.IsActive() && motion.IsActive() && dof.IsActive() && chromatic.IsActive() && bloom.IsActive() && vignette.IsActive(), "city enables GTAO, DOF and motion blur after settings toggles");
    Check(TownFireflyCount(0) == 0 && TownFireflyCount(25) == 25 && TownFireflyCount(100) == 100 && TownFireflyCount(2046) == 2046 && TownFireflyCount(3000) == 2046, "firefly population follows bounded energy");
    Check(TownStreetlightCovers(3,0,1.5f) && TownStreetlightCovers(-3,0,1.5f) && TownStreetlightCovers(0,3,1.5f) && !TownStreetlightCovers(3,1,1.5f), "streetlight footprint is a centered circle");
    TownFireflies swarm; Vector3 fireflyFirst = swarm.Position(0); swarm.clock = 1;
    Vector3 fireflyMove = swarm.Position(0) - fireflyFirst;
    Check(fireflyMove.Dot(fireflyMove) > 0.001f, "fireflies drift over time");
    Vector3 scrollStart = rig.position;
    rig.Scroll(1); Vector3 scrollError = rig.position - scrollStart - rig.Orientation().RotateVector(Vector3(0,0,2.5f));
    Check(scrollError.Dot(scrollError) < 0.00001f, "wheel dolly follows camera forward including pitch");
    rig.Scroll(-1); scrollError = rig.position - scrollStart;
    Check(scrollError.Dot(scrollError) < 0.00001f, "reverse wheel returns to initial camera position");
    Object@ focus = GetScene().CreateObject("FocusMathTest");
    cameraObject.GetTransform().SetTranslate(Vector3(0, 0, -12));
    PostEffectController controller;
    @controller.playerObject = focus; @controller.cameraObject = cameraObject; @controller.screenObject = screen;
    controller.DoFController();
    Check(Abs(dof.GetFocusDistance() - 12) < 0.001f, "existing controller autofocus follows target distance");
    cameraObject.GetTransform().SetRotate(Vector3(0, 0, 0));
    focus.GetTransform().SetTranslate(Vector3(9, 0, 0));
    controller.DoFController(); Check(Abs(dof.GetFocusDistance() - 15) < 0.001f, "default prefab uses target distance");
    controller.useViewDepth = true; controller.DoFController();
    Check(Abs(dof.GetFocusDistance() - 12) < 0.001f, "city focuses an off-axis selected cell at its view depth");
    controller.focusResponse = 8;
    float blended = controller.BlendFocus(12, 30, 0.1f);
    Check(blended > 12 && blended < 30, "focus moves smoothly without snapping or overshoot");
    float split = controller.BlendFocus(controller.BlendFocus(12, 30, 0.05f), 30, 0.05f);
    Check(Abs(blended - split) < 0.0001f && controller.BlendFocus(12, 30, 0) == 12, "focus interpolation is frame rate independent");
    SetCurrentLanguage("en-US"); Check(TownTr("日本語", "English") == "English", "English UI selection");
    SetCurrentLanguage("ja-JP"); Check(TownTr("日本語", "English") == "日本語", "Japanese UI selection");
}
