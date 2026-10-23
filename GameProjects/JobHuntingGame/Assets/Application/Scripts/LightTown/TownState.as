#include "TownLanguage.as"
#include "TownFeatures.as"
// 街と遠征で共有するセーブデータとゲームルール。
float TownClamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}
float TownEnergyLimit() { return 2046; }
float TownStreetlightRadius(){return 4.5f;}
bool TownStreetlightCovers(int dx,int dz,float spacing){float x=float(dx)*spacing,z=float(dz)*spacing;return x*x+z*z<=TownStreetlightRadius()*TownStreetlightRadius()+.0001f;}

class TownState {
    Json@ data;
    Json@ rules;
    Scene@ scene;
    string message = TownTr("街灯や工房を建てて、光の街を育てましょう。", "Build streetlights and workshops to grow your town.");
    bool saveFailed = false;
    bool infrastructureDirty = true;
    array<bool>@ roadCells = array<bool>();
    array<bool>@ litCells = array<bool>();
    array<bool>@ workingCells = array<bool>();
    array<int>@ workingCounts = array<int>();

    bool Initialize(Scene@ owner) {
        @scene = owner;
        @rules = LoadJsonFile("Assets/Application/Data/LightTownRules.json");
        if (rules is null || !rules.IsObject()) return false;
        string cached;
        if (scene.GetGlobalVariable("LightTown.State", cached)) {
            @data = Json();
            if (!data.Parse(cached)) @data = null;
        } else @data = LoadJsonFile("SaveData/LightTown.json");
        if (data is null || data.GetInt("version") != 1 || data.GetJson("buildings") is null) NewGame();
        if(data.GetJson("notifications") is null){Json@ empty=Json();empty.Parse("[]");data.SetJson("notifications",empty);}
        data.SetFloat("energy", TownClamp(float(data.GetFloat("energy")), 0, TownEnergyLimit()));
        MigrateGuildDispatch();
        infrastructureDirty = true;
        Publish();
        return true;
    }
    void NewGame() {
        infrastructureDirty = true;
        @data = Json();
        data.SetInt("version", 1);
        data.SetInt("wood", rules.GetInt("initialWood", 70));
        data.SetInt("stone", rules.GetInt("initialStone", 55));
        data.SetInt("crystal", rules.GetInt("initialCrystal", 15));
        data.SetFloat("energy", 60); data.SetFloat("happiness", 50);
        data.SetFloat("safety", 50); data.SetFloat("prosperity", 10);
        data.SetFloat("productionClock", 0); data.SetFloat("eventClock", 0);
        data.SetFloat("dispatchRemaining", 0); data.SetBool("dispatchActive", false);
        data.SetInt("guildDispatchVersion",1);
        data.SetBool("transformationUnlocked", false); data.SetBool("cleared", false);
        data.SetBool("clearAcknowledged", false);
        data.SetInt("expedition", 0);
        Json@ list = Json(); list.Parse("[]"); data.SetJson("buildings", list);
        Json@ notifications = Json(); notifications.Parse("[]"); data.SetJson("notifications", notifications);
    }
    void Publish() {
        scene.SetGlobalVariable("LightTown.State", data.ToString());
        scene.SetGlobalVariable("LightTown.TransformationUnlocked", data.GetBool("transformationUnlocked"));
    }
    bool Save() {
        Publish();
        saveFailed = !SaveJsonFile("SaveData/LightTown.json", data);
        if (saveFailed) LogError("LightTown: セーブ失敗。進行状況はメモリ上に保持しています。");
        return !saveFailed;
    }
    Json@ Definition(int kind) { return rules.GetJson("buildings").At(uint(kind)); }
    Json@ Buildings() { return data.GetJson("buildings"); }
    Json@ BuildingAt(int x,int z){Json@ list=Buildings();for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);if(b.GetInt("x")==x&&b.GetInt("z")==z)return b;}return null;}
    // Json.GetJson/Atはコピーを返すため、変更した施設を配列へ明示的に書き戻す。
    void StoreBuilding(Json@ updated){Json@ list=Buildings();Json@ next=Json();next.Parse("[]");for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);next.PushJson(b.GetInt("x")==updated.GetInt("x")&&b.GetInt("z")==updated.GetInt("z")?updated:b);}data.SetJson("buildings",next);}
    float ProductionInterval(){return TownClamp(float(rules.GetFloat("productionSeconds",10)),1,3600);}
    float ProductionProgress(){return TownClamp(float(data.GetFloat("productionClock"))/ProductionInterval(),0,1);}
    void SyncDispatchSummary(){
        int active=0;float nearest=0;Json@ list=Buildings();
        for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);if(b.GetInt("kind")!=4||!b.GetBool("dispatchActive"))continue;float remaining=float(b.GetFloat("dispatchRemaining"));if(active==0||remaining<nearest)nearest=remaining;++active;}
        data.SetBool("dispatchActive",active>0);data.SetFloat("dispatchRemaining",nearest);data.SetInt("dispatchCount",active);
    }
    void MigrateGuildDispatch(){
        if(data.GetInt("guildDispatchVersion")<1){
            if(data.GetBool("dispatchActive")){Json@ list=Buildings();for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);if(b.GetInt("kind")!=4)continue;b.SetBool("dispatchActive",true);b.SetFloat("dispatchRemaining",data.GetFloat("dispatchRemaining"));b.SetFloat("dispatchDuration",rules.GetFloat("dispatchSeconds",45));StoreBuilding(b);break;}}
            data.SetInt("guildDispatchVersion",1);
        }
        SyncDispatchSummary();
    }
    int Count(int kind) {
        int count = 0; Json@ list = Buildings();
        for (uint i = 0; i < list.Size(); ++i) if (list.At(i).GetInt("kind") == kind) ++count;
        return count;
    }
    int GridSize() { return int(rules.GetInt("gridSize",16)); }
    bool InBounds(int x,int z) { return x>=0 && z>=0 && x<GridSize() && z<GridSize(); }
    bool RawRoad(int x,int z) { return InBounds(x,z) && roadCells[uint(z*GridSize()+x)]; }
    bool RoadAdjacent(int x,int z) { return RawRoad(x-1,z)||RawRoad(x+1,z)||RawRoad(x,z-1)||RawRoad(x,z+1); }
    void EnsureInfrastructure() {
        if(!infrastructureDirty)return;
        int size=GridSize();roadCells.resize(uint(size*size));litCells.resize(uint(size*size));workingCells.resize(uint(size*size));workingCounts.resize(7);
        for(uint i=0;i<roadCells.length();++i){roadCells[i]=false;litCells[i]=false;workingCells[i]=false;}
        for(uint i=0;i<workingCounts.length();++i)workingCounts[i]=0;
        Json@ list=Buildings();
        for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);int x=int(b.GetInt("x")),z=int(b.GetInt("z"));if(InBounds(x,z)&&b.GetInt("kind")==6)roadCells[uint(z*size+x)]=true;}
        float spacing=float(rules.GetFloat("cellSize",1.5f));
        // 道路に隣接した街灯が点灯し、自身を含むセルを照らす。最初の一灯から開始できる。
        for(uint i=0;i<list.Size();++i){
            Json@ lamp=list.At(i);int lx=int(lamp.GetInt("x")),lz=int(lamp.GetInt("z"));
            if(lamp.GetInt("kind")!=0||!InBounds(lx,lz)||!RoadAdjacent(lx,lz))continue;
            for(int z=0;z<size;++z)for(int x=0;x<size;++x){
                // セル中心を基準としたXZ平面の円。光源と範囲表示も同じ中心を使う。
                if(TownStreetlightCovers(x-lx,z-lz,spacing))litCells[uint(z*size+x)]=true;
            }
        }
        for(uint i=0;i<list.Size();++i){
            Json@ b=list.At(i);int x=int(b.GetInt("x")),z=int(b.GetInt("z")),kind=int(b.GetInt("kind"));
            if(!InBounds(x,z)||kind<0||kind>6)continue;
            bool working=kind==6||(RoadAdjacent(x,z)&&litCells[uint(z*size+x)]);
            workingCells[uint(z*size+x)]=working;if(working)++workingCounts[uint(kind)];
        }
        infrastructureDirty=false;
    }
    bool IsRoad(int x,int z){EnsureInfrastructure();return RawRoad(x,z);}
    bool IsLit(int x,int z){EnsureInfrastructure();return InBounds(x,z)&&litCells[uint(z*GridSize()+x)];}
    bool IsWorking(int x,int z){EnsureInfrastructure();return InBounds(x,z)&&workingCells[uint(z*GridSize()+x)];}
    int RoadMask(int x,int z){EnsureInfrastructure();return (RawRoad(x-1,z)?1:0)|(RawRoad(x+1,z)?2:0)|(RawRoad(x,z-1)?4:0)|(RawRoad(x,z+1)?8:0);}
    int WorkingCount(int kind){EnsureInfrastructure();return kind>=0&&kind<7?workingCounts[uint(kind)]:0;}
    int WorkingFacilities(){int total=0;for(int k=0;k<6;++k)total+=WorkingCount(k);return total;}
    bool ClearPending() { return data.GetBool("cleared") && !data.GetBool("clearAcknowledged"); }
    bool AcknowledgeClear() {
        if (!ClearPending()) return false;
        data.SetBool("clearAcknowledged", true);
        if (Save()) return true;
        data.SetBool("clearAcknowledged", false); Publish(); return false;
    }
    void TryClear(){
        if(WorkingCount(5)>0&&WorkingFacilities()-WorkingCount(5)>=rules.GetInt("goalBuildingCount",8)&&data.GetFloat("energy")>=rules.GetFloat("goalEnergy",80)&&data.GetFloat("prosperity")>=rules.GetFloat("goalProsperity",60))data.SetBool("cleared",true);
    }
    void QueueEvent(const string &in text){
        Json@ old=data.GetJson("notifications");Json@ next=Json();next.Parse("[]");
        if(old !is null)for(uint i=old.Size()>7?old.Size()-7:0;i<old.Size();++i)next.PushJson(old.At(i));
        Json@ event=Json();event.SetString("text",text);next.PushJson(event);data.SetJson("notifications",next);
    }
    Json@ TakeEvents(){
        Json@ list=data.GetJson("notifications");if(list is null){@list=Json();list.Parse("[]");}
        if(list.Size()>0){Json@ empty=Json();empty.Parse("[]");data.SetJson("notifications",empty);Save();}return list;
    }
    bool Occupied(int x, int z) {
        Json@ list = Buildings();
        for (uint i = 0; i < list.Size(); ++i) {
            Json@ b = list.At(i);
            if (b.GetInt("x") == x && b.GetInt("z") == z) return true;
        }
        return false;
    }
    void AddResources(int wood, int stone, int crystal) {
        data.SetInt("wood", data.GetInt("wood") + wood);
        data.SetInt("stone", data.GetInt("stone") + stone);
        data.SetInt("crystal", data.GetInt("crystal") + crystal);
    }
    string BuildProblem(int kind, int x, int z) {
        int size = int(rules.GetInt("gridSize", 16));
        if (x < 0 || z < 0 || x >= size || z >= size) return TownTr("建築可能エリアの外です。", "Outside the building area.");
        if (Occupied(x, z)) return TownTr("このマスには既に施設があります。", "This cell is occupied.");
        Json@ d = Definition(kind);
        if (d is null) return TownTr("施設を選択してください。", "Select a building.");
        if (data.GetInt("wood") < d.GetInt("wood") || data.GetInt("stone") < d.GetInt("stone") ||
            data.GetInt("crystal") < d.GetInt("crystal")) return TownTr("素材が足りません。", "Not enough materials.");
        if (kind == 5) {
            if (data.GetBool("cleared")) return TownTr("光の塔は完成しています。", "The Tower of Light is already complete.");
            if (WorkingFacilities() < int(rules.GetInt("goalBuildingCount", 8))) return TownTr("先に稼働中の施設を8棟用意してください。", "Activate 8 facilities first.");
            if (data.GetFloat("energy") < rules.GetFloat("goalEnergy", 80)) return TownTr("光エネルギーが80必要です。", "80 light energy required.");
            if (data.GetFloat("prosperity") < rules.GetFloat("goalProsperity", 60)) return TownTr("繁栄度が60必要です。", "60 prosperity required.");
        }
        return "";
    }
    bool Build(int kind, int x, int z) {
        string problem = BuildProblem(kind, x, z);
        if (problem != "") { message = problem; return false; }
        Json@ d = Definition(kind);
        AddResources(-int(d.GetInt("wood")), -int(d.GetInt("stone")), -int(d.GetInt("crystal")));
        Json@ list = Buildings(); Json@ b = Json();
        b.SetInt("kind", kind); b.SetInt("x", x); b.SetInt("z", z);
        list.PushJson(b); data.SetJson("buildings", list);
        infrastructureDirty = true; TryClear();
        message = data.GetBool("cleared") ? TownTr("光の塔が完成しました！ 街づくりクリアです。", "The Tower of Light is complete! Town cleared!") : TownBuildingName(d) + TownTr("を建築しました。", " constructed.");
        Save(); return true;
    }
    string DeleteProblem(int x,int z){Json@ b=BuildingAt(x,z);if(b is null)return TownTr("削除する施設がありません。","No facility to remove.");if(b.GetInt("kind")==4&&b.GetBool("dispatchActive"))return TownTr("NPCが帰還してからギルドを削除してください。","Wait for the NPC to return before removing this guild.");return "";}
    bool RemoveBuilding(int x,int z){
        string problem=DeleteProblem(x,z);if(problem!=""){message=problem;return false;}
        Json@ list=Buildings();Json@ next=Json();next.Parse("[]");for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);if(b.GetInt("x")!=x||b.GetInt("z")!=z)next.PushJson(b);}data.SetJson("buildings",next);infrastructureDirty=true;EnsureInfrastructure();SyncDispatchSummary();message=TownTr("施設を削除しました。建築資材の返金はありません。","Facility removed. Construction materials are not refunded.");Save();return true;
    }
    string ProductProblem(uint index) {
        Json@ items=rules.GetJson("shopItems");Json@ item=items is null?null:items.At(index);
        if(item is null)return TownTr("商品がありません。","Product not found.");
        if(item.GetString("action")=="transformation"&&!TownTransformationsEnabled())return TownTr("変身機能は現在利用できません。","Transformation is currently unavailable.");
        if(WorkingCount(3)==0)return TownTr("道路に隣接し街灯に照らされたショップが必要です。","A road-connected, illuminated shop is required.");
        if(item.GetString("action")!="transformation")return TownTr("この商品は準備中です。","This product is not available yet.");
        if(data.GetBool("transformationUnlocked"))return TownTr("購入済みです。","Already purchased.");
        if(data.GetInt("wood")<item.GetInt("wood")||data.GetInt("stone")<item.GetInt("stone")||data.GetInt("crystal")<item.GetInt("crystal"))return TownTr("購入に必要な資材が足りません。","Not enough resources.");
        return "";
    }
    bool BuyProduct(uint index) {
        string problem=ProductProblem(index);if(problem!=""){message=problem;return false;}
        Json@ item=rules.GetJson("shopItems").At(index);
        AddResources(-int(item.GetInt("wood")),-int(item.GetInt("stone")),-int(item.GetInt("crystal")));
        data.SetBool("transformationUnlocked",true);
        message=TownTr("変身を解放しました！遠征中にEで切り替えられます。","Transformation unlocked! Press E during expeditions.");Save();return true;
    }
    void BuyTransformation() { BuyProduct(0); }
    bool DispatchAt(int x,int z){
        Json@ guild=BuildingAt(x,z);
        if(guild is null||guild.GetInt("kind")!=4||!IsWorking(x,z)){message=TownTr("道路に隣接し街灯に照らされたギルドが必要です。","A road-connected, illuminated guild is required.");return false;}
        if(guild.GetBool("dispatchActive")){message=TownTr("このギルドのNPCは現在遠征中です。","This guild's NPC is already away.");return false;}
        float duration=TownClamp(float(rules.GetFloat("dispatchSeconds",45)),1,86400);
        guild.SetBool("dispatchActive",true);guild.SetFloat("dispatchRemaining",duration);guild.SetFloat("dispatchDuration",duration);
        StoreBuilding(guild);
        SyncDispatchSummary();message=TownTr("NPCが素材集めに出発しました。遠征中も時間が進みます。","NPC dispatched! The timer continues during your expedition.");Save();return true;
    }
    // 旧スクリプト呼び出しとの互換用。画面操作はセルを指定するDispatchAtを使う。
    void Dispatch(){Json@ list=Buildings();for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);if(b.GetInt("kind")==4&&IsWorking(int(b.GetInt("x")),int(b.GetInt("z")))){DispatchAt(int(b.GetInt("x")),int(b.GetInt("z")));return;}}message=TownTr("稼働中のギルドをマップで選択してください。","Select an operational guild on the map.");}
    // 街・遠征のいずれか1シーンから更新。終了中やエディター停止中の時間は含めない。
    void Tick(float dt) {
        if (dt <= 0) return;
        bool persist = false;
        Json@ guilds=Buildings();Json@ nextBuildings=Json();nextBuildings.Parse("[]");
        for(uint i=0;i<guilds.Size();++i){
            Json@ guild=guilds.At(i);if(guild.GetInt("kind")!=4||!guild.GetBool("dispatchActive")){nextBuildings.PushJson(guild);continue;}
            float remaining = float(guild.GetFloat("dispatchRemaining")) - dt;
            guild.SetFloat("dispatchRemaining", remaining > 0 ? remaining : 0);
            if (remaining <= 0) {
                guild.SetBool("dispatchActive", false);
                AddResources(int(rules.GetInt("dispatchWood", 20)), int(rules.GetInt("dispatchStone", 15)), int(rules.GetInt("dispatchCrystal", 6)));
                message = TownTr("NPCが帰還しました。素材を受け取りました。", "NPC returned. Materials received."); persist = true;
            }
            nextBuildings.PushJson(guild);
        }
        data.SetJson("buildings",nextBuildings);
        SyncDispatchSummary();
        float clock = float(data.GetFloat("productionClock")) + dt;
        float interval = ProductionInterval();
        while (clock >= interval) { clock -= interval; Produce(); persist = true; }
        data.SetFloat("productionClock", clock);
        float eventClock = float(data.GetFloat("eventClock")) + dt;
        if (eventClock >= float(rules.GetFloat("eventSeconds", 40))) {
            eventClock = 0; Event(); persist = true;
        }
        data.SetFloat("eventClock", eventClock);
        if (persist) Save();
    }
    void Produce() {
        int workshops = WorkingCount(2);
        AddResources(int(rules.GetInt("passiveWood", 4)), int(rules.GetInt("passiveStone", 3)), int(rules.GetInt("passiveCrystal", 1)));
        float energy = TownClamp(float(data.GetFloat("energy")) + 4 + WorkingCount(0) * 12 - workshops * 4 - WorkingCount(3) * 2 - WorkingCount(4) * 3, 0, TownEnergyLimit());
        data.SetFloat("energy", energy);
        if (energy >= 10) AddResources(workshops * 6, workshops * 4, workshops * 2);
        float happiness = TownClamp(float(data.GetFloat("happiness")) + (energy >= 20 ? 1 : -3) + WorkingCount(1), 0, 100);
        float safety = TownClamp(float(data.GetFloat("safety")) + WorkingCount(0) * 2 - (energy < 20 ? 3 : 0), 0, 100);
        data.SetFloat("happiness", happiness); data.SetFloat("safety", safety);
        data.SetFloat("prosperity", TownClamp(float(WorkingFacilities()) * 5 + happiness * 0.2f + safety * 0.2f, 0, 100));
        TryClear();
    }
    void Event() {
        float roll = Random::Float(0, 1);
        float safety = float(data.GetFloat("safety")), happiness = float(data.GetFloat("happiness"));
        if (roll < (100 - safety) / 250.0f) {
            int loss = int(data.GetInt("wood")); if (loss > 8) loss = 8;
            AddResources(-loss, 0, 0); message = TownTr("資材置き場で盗難。街灯を建てて治安を上げましょう。", "Materials stolen. Add streetlights to improve safety.");
        } else if (roll < 0.55f && happiness >= 60) {
            AddResources(8, 6, 3); message = TownTr("幸福な住民から寄付が届きました！", "Happy residents donated materials!");
        } else if (data.GetFloat("energy") >= 70 && data.GetFloat("prosperity") >= 45) {
            AddResources(4, 4, 8); message = TownTr("光の祭りが開催され、光結晶が集まりました。", "A light festival brought more light crystals.");
        } else {
            data.SetFloat("happiness", TownClamp(happiness + 3, 0, 100)); message = TownTr("穏やかな一日。幸福度が少し上がりました。", "A peaceful day. Happiness increased.");
        }
        QueueEvent(message);
    }
}
