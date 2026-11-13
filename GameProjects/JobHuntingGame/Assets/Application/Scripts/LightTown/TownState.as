#include "TownLanguage.as"
#include "TownFeatures.as"
// 街と遠征で共有するセーブデータとゲームルール。
float TownClamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}
float TownEnergyLimit() { return 3.402823e38f; }
// Facing 0=-Z, 1=+X, 2=+Z, 3=-X. Imported models face +Z before the base half-turn.
int TownFacing(int rotation){return (rotation%4+4)%4;}
int TownFrontX(int rotation){int r=TownFacing(rotation);return r==1?1:(r==3?-1:0);}
int TownFrontZ(int rotation){int r=TownFacing(rotation);return r==0?-1:(r==2?1:0);}
float TownBuildingYaw(int rotation){return 3.14159265f-float(TownFacing(rotation))*1.57079633f;}
float TownModelYaw(int kind,int rotation){return TownBuildingYaw(rotation)+(kind==0?-1.57079633f:0);}
float TownStreetlightRadius(){return 4.5f;}
bool TownStreetlightCovers(int dx,int dz,float spacing){float x=float(dx)*spacing,z=float(dz)*spacing;return x*x+z*z<=TownStreetlightRadius()*TownStreetlightRadius()+.0001f;}

class TownState {
    Json@ data;
    Json@ rules;
    Scene@ scene;
    double lastRealTime = 0;
    string message = TownTr("街灯や工房を建てて、光の街を育てましょう。", "Build streetlights and workshops to grow your town.");
    bool saveFailed = false;
    bool infrastructureDirty = true;
    array<float>@ placedParameterDeltas=array<float>(3,0);
    array<bool>@ roadCells = array<bool>();
    array<bool>@ litCells = array<bool>();
    array<bool>@ workingCells = array<bool>();
    array<int>@ workingCounts = array<int>();
    // Cell indices point into an owned JSON copy. At() still returns a copy so
    // callers must explicitly StoreBuilding() to persist their edits.
    Json@ buildingSnapshot;
    int facilityCount=0;uint infrastructureVersion=0;
    array<int>@ buildingIndices = array<int>();
    void InvalidateBuildings(){@buildingSnapshot=null;}
    void SetBuildings(Json@ list){data.SetJson("buildings",list);InvalidateBuildings();}
    void EnsureBuildings(){
        if(buildingSnapshot !is null)return;
        @buildingSnapshot=Buildings();int size=GridSize();buildingIndices.resize(uint(size*size));facilityCount=0;
        for(uint i=0;i<buildingIndices.length();++i)buildingIndices[i]=-1;
        for(uint i=0;i<buildingSnapshot.Size();++i){Json@ b=buildingSnapshot.At(i);int x=int(b.GetInt("x")),z=int(b.GetInt("z"));if(b.GetInt("kind")>=0&&b.GetInt("kind")<=7&&b.GetInt("kind")!=6)++facilityCount;int width=Footprint(b);for(int dz=0;dz<width;++dz)for(int dx=0;dx<width;++dx)if(InBounds(x+dx,z+dz))buildingIndices[uint((z+dz)*size+x+dx)]=int(i);}
    }
    uint BuildingCount(){EnsureBuildings();return buildingSnapshot.Size();}
    bool HasFacilities(){EnsureBuildings();return facilityCount>0;}

    bool Initialize(Scene@ owner) {
        InvalidateBuildings();
        @scene = owner;
        @rules = LoadJsonFile("Assets/Application/Data/LightTownRules.json");
        if (rules is null || !rules.IsObject()) return false;
        string cached;
        bool fromSession = scene.GetGlobalVariable("LightTown.State", cached);
        lastRealTime = GetMonotonicSeconds();
        if (fromSession) {
            @data = Json();
            if (!data.Parse(cached)) @data = null;
        } else @data = LoadJsonFile("SaveData/LightTown.json");
        if (data is null || data.GetInt("version") != 1 || data.GetJson("buildings") is null) NewGame();
        else if (fromSession) {
            double previous;
            if (scene.GetGlobalVariable("LightTown.LastRealTime", previous) && previous >= 0 && previous <= lastRealTime)
                lastRealTime = previous;
        }
        if(data.GetJson("notifications") is null){Json@ empty=Json();empty.Parse("[]");data.SetJson("notifications",empty);}
        if(data.GetFloat("energy")<0)data.SetFloat("energy",0);
        array<string> processed={"planks","masonry","refinedCrystal"};for(uint i=0;i<processed.length();++i)if(!data.Has(processed[i]))data.SetInt(processed[i],0);
        MigrateGuildDispatch();
        MigrateBuildingFacing();
        infrastructureDirty = true;
        ReconcilePollution();Publish();
        return true;
    }
    void NewGame() {
        lastRealTime = GetMonotonicSeconds();
        infrastructureDirty = true;
        @data = Json();
        data.SetInt("version", 1);
        data.SetInt("wood", rules.GetInt("initialWood", 70));
        data.SetInt("stone", rules.GetInt("initialStone", 55));
        data.SetInt("crystal", rules.GetInt("initialCrystal", 15));data.SetInt("planks",0);data.SetInt("masonry",0);data.SetInt("refinedCrystal",0);data.SetFloat("pollutionPenalty",0);data.SetFloat("pollutionApplied",0);
        data.SetFloat("energy", 60); data.SetFloat("happiness", 50);
        data.SetFloat("safety", 50); data.SetFloat("prosperity", 10);
        data.SetFloat("productionClock", 0); data.SetFloat("eventClock", 0);
        data.SetFloat("dispatchRemaining", 0); data.SetBool("dispatchActive", false);
        data.SetInt("guildDispatchVersion",1);
        data.SetBool("transformationUnlocked", false); data.SetBool("cleared", false);
        data.SetBool("clearAcknowledged", false);
        data.SetInt("expedition", 0);
        data.SetString("selectedStage", "forest");
        Json@ list = Json(); list.Parse("[]"); SetBuildings(list);
        Json@ notifications = Json(); notifications.Parse("[]"); data.SetJson("notifications", notifications);
    }
    void Publish() {
        scene.SetGlobalVariable("LightTown.State", data.ToString());
        // This clock belongs to the running session, so only the remaining times go on disk.
        scene.SetGlobalVariable("LightTown.LastRealTime", lastRealTime);
        scene.SetGlobalVariable("LightTown.TransformationUnlocked", data.GetBool("transformationUnlocked"));
    }
    bool Save() {
        Publish();
        saveFailed = !SaveJsonFile("SaveData/LightTown.json", data);
        if (saveFailed) LogError("LightTown: セーブ失敗。進行状況はメモリ上に保持しています。");
        return !saveFailed;
    }
    int Balance(const string &in key,int tier,int fallback){Json@ table=rules.GetJson("balance");return table is null?fallback:int(table.GetInt(key+tier,fallback));}
    Json@ Definition(int kind,int tier=0,int recipe=0) {
        Json@ d=rules.GetJson("buildings").At(uint(kind));if(d is null)return null;tier=int(TownClamp(float(tier),0,2));int multiplier=Balance("cost",tier,tier==0?1:(tier==1?3:9));
        array<string> keys={"wood","stone","crystal"};for(uint i=0;i<keys.length();++i)d.SetInt(keys[i],d.GetInt(keys[i])*multiplier);
        d.SetInt("kind",kind);d.SetInt("tier",tier);d.SetInt("recipe",recipe);d.SetInt("size",tier==2&&kind>=1&&kind<=4?2:1);
        d.SetInt("planks",Balance("planksCost",tier,tier==0?0:(tier==1?8:30)));d.SetInt("masonry",Balance("masonryCost",tier,tier==0?0:(tier==1?6:25)));d.SetInt("refinedCrystal",Balance("refinedCrystalCost",tier,tier==2?12:0));
        d.SetFloat("height",d.GetFloat("height",2)*(tier==0?1:(tier==1?1.4f:2)));
        array<string> home={"一軒家","アパート","高層マンション"},homeEn={"House","Apartment","High-rise"},shop={"小規模商店","スーパーマーケット","ショッピングモール"},shopEn={"Small shop","Supermarket","Shopping mall"},guild={"小型ギルド","中型ギルド","大型ギルド"},guildEn={"Small guild","Medium guild","Large guild"};
        if(kind==1){d.SetString("name",home[uint(tier)]);d.SetString("nameEn",homeEn[uint(tier)]);}
        if(kind==3){d.SetString("name",shop[uint(tier)]);d.SetString("nameEn",shopEn[uint(tier)]);}
        if(kind==4){d.SetString("name",guild[uint(tier)]);d.SetString("nameEn",guildEn[uint(tier)]);}
        if(kind==2){Json@ tint=Json();tint.SetFloat("x",recipe==0?.68f:(recipe==1?.58f:.18f));tint.SetFloat("y",recipe==0?.38f:(recipe==1?.64f:.7f));tint.SetFloat("z",recipe==0?.16f:(recipe==1?.7f:1));tint.SetFloat("w",1);d.SetJson("color",tint);d.SetFloat("height",2+float(tier)*.65f);array<string> raw={"木材","石材","光結晶"},rawEn={"Timber","Stone","Crystal"};d.SetString("name",(tier==0?"":(tier==1?"中規模":"大規模"))+raw[uint(recipe%3)]+"加工工場");d.SetString("nameEn",(tier==0?"Small ":(tier==1?"Medium ":"Large "))+rawEn[uint(recipe%3)]+" refinery");}
        return d;
    }
    Json@ BuildingDefinition(Json@ b){return Definition(int(b.GetInt("kind")),int(b.GetInt("tier")),int(b.GetInt("recipe")));}
    int Footprint(Json@ b){int kind=int(b.GetInt("kind"));return kind>=1&&kind<=4&&b.GetInt("tier")==2?2:1;}
    int Residents(Json@ b){int t=int(b.GetInt("tier"));return Balance("residents",t,t==0?3:(t==1?9:30));}
    int Capacity(Json@ b){int t=int(b.GetInt("tier"));return Balance("capacity",t,t==0?1:(t==1?3:8));}
    int EntranceCell(int x,int z,int rotation,int size){int nx=x,nz=z;if(rotation==0)nz=z-1;else if(rotation==1)nx=x+size;else if(rotation==2)nz=z+size;else nx=x-1;return InBounds(nx,nz)?nz*GridSize()+nx:-1;}
    bool BuildingFrontRoad(Json@ b){int cell=EntranceCell(int(b.GetInt("x")),int(b.GetInt("z")),int(b.GetInt("rotation")),Footprint(b));return cell>=0&&roadCells[uint(cell)];}
    Json@ Buildings() { return data.GetJson("buildings"); }
    Json@ BuildingAt(int x,int z){if(!InBounds(x,z))return null;EnsureBuildings();int index=buildingIndices[uint(z*GridSize()+x)];return index<0?null:buildingSnapshot.At(uint(index));}
    // Json.GetJson/Atはコピーを返すため、変更した施設を配列へ明示的に書き戻す。
    void StoreBuilding(Json@ updated){Json@ list=Buildings();Json@ next=Json();next.Parse("[]");for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);next.PushJson(b.GetInt("x")==updated.GetInt("x")&&b.GetInt("z")==updated.GetInt("z")?updated:b);}SetBuildings(next);}
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
    int AutoFacing(int x,int z,int preferred=0){
        preferred=TownFacing(preferred);if(FrontRoad(x,z,preferred))return preferred;
        float best=1e20f;int facing=preferred,size=GridSize();
        for(int rz=0;rz<size;++rz)for(int rx=0;rx<size;++rx){if(!RawRoad(rx,rz))continue;int dx=rx-x,dz=rz-z;if(dx==0&&dz==0)continue;float distance=float(dx*dx+dz*dz);
            int r=Abs(float(dx))>Abs(float(dz))?(dx>0?1:3):(dz>0?2:0);
            if(Abs(dx)==Abs(dz)&&(TownFrontX(preferred)*dx>0||TownFrontZ(preferred)*dz>0))r=preferred;
            if(distance<best||(distance==best&&r==preferred)){best=distance;facing=r;}
        }return facing;
    }
    int AutoFacingFor(int x,int z,int width,int preferred=0){EnsureInfrastructure();preferred=TownFacing(preferred);for(int n=0;n<4;++n){int r=TownFacing(preferred+n),cell=EntranceCell(x,z,r,width);if(cell>=0&&roadCells[uint(cell)])return r;}return AutoFacing(x,z,preferred);}
    void MigrateBuildingFacing(){
        // Old saves had no entrance direction. Orient each legacy building toward
        // an existing adjacent road once, then persist a fixed quarter-turn.
        Json@ list=Buildings();array<bool> legacyRoads(uint(GridSize()*GridSize()),false);
        for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);int x=int(b.GetInt("x")),z=int(b.GetInt("z"));if(b.GetInt("kind")==6&&InBounds(x,z))legacyRoads[uint(z*GridSize()+x)]=true;}
        Json@ next=Json();next.Parse("[]");bool changed=false;
        for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);if(!b.Has("rotation")){int rotation=0;for(int r=0;r<4;++r){int x=int(b.GetInt("x"))+TownFrontX(r),z=int(b.GetInt("z"))+TownFrontZ(r);if(InBounds(x,z)&&legacyRoads[uint(z*GridSize()+x)]){rotation=r;break;}}b.SetInt("rotation",rotation);changed=true;}next.PushJson(b);}
        if(changed)SetBuildings(next);
    }
    bool FrontRoad(int x,int z,int rotation){return RawRoad(x+TownFrontX(rotation),z+TownFrontZ(rotation));}
    int EntranceRoad(int x,int z){EnsureInfrastructure();Json@ b=BuildingAt(x,z);if(b is null)return -1;int c=EntranceCell(int(b.GetInt("x")),int(b.GetInt("z")),int(b.GetInt("rotation")),Footprint(b));return c>=0&&roadCells[uint(c)]?c:-1;}
    string OperationProblem(int kind,int x,int z,int rotation,int width=1){
        if(kind==6)return "";EnsureInfrastructure();
        bool adjacent=false;for(int dz=0;dz<width;++dz)for(int dx=0;dx<width;++dx)if(RoadAdjacent(x+dx,z+dz))adjacent=true;
        if(!adjacent)return TownTr("道路に隣接していません","No adjacent road");
        int entrance=EntranceCell(x,z,rotation,width);if(entrance<0||!roadCells[uint(entrance)])return TownTr("入口が道路を向いていません","Entrance does not face a road");
        if(kind!=0&&!IsLit(x,z))return TownTr("光が届いていません","Not illuminated");return "";
    }
    bool InBounds(int x,int z) { return x>=0 && z>=0 && x<GridSize() && z<GridSize(); }
    bool RawRoad(int x,int z) { return InBounds(x,z) && roadCells[uint(z*GridSize()+x)]; }
    bool RoadAdjacent(int x,int z) { return RawRoad(x-1,z)||RawRoad(x+1,z)||RawRoad(x,z-1)||RawRoad(x,z+1); }
    void EnsureInfrastructure() {
        if(!infrastructureDirty)return;
        int size=GridSize();roadCells.resize(uint(size*size));litCells.resize(uint(size*size));workingCells.resize(uint(size*size));workingCounts.resize(8);
        for(uint i=0;i<roadCells.length();++i){roadCells[i]=false;litCells[i]=false;workingCells[i]=false;}
        for(uint i=0;i<workingCounts.length();++i)workingCounts[i]=0;
        Json@ list=Buildings();
        for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);int x=int(b.GetInt("x")),z=int(b.GetInt("z"));if(InBounds(x,z)&&b.GetInt("kind")==6)roadCells[uint(z*size+x)]=true;}
        float spacing=float(rules.GetFloat("cellSize",1.5f));
        // 道路に隣接した街灯が点灯し、自身を含むセルを照らす。最初の一灯から開始できる。
        for(uint i=0;i<list.Size();++i){
            Json@ lamp=list.At(i);int lx=int(lamp.GetInt("x")),lz=int(lamp.GetInt("z"));
            if(lamp.GetInt("kind")!=0||!InBounds(lx,lz)||!BuildingFrontRoad(lamp))continue;
            for(int z=0;z<size;++z)for(int x=0;x<size;++x){
                // セル中心を基準としたXZ平面の円。光源と範囲表示も同じ中心を使う。
                if(TownStreetlightCovers(x-lx,z-lz,spacing))litCells[uint(z*size+x)]=true;
            }
        }
        for(uint i=0;i<list.Size();++i){
            Json@ b=list.At(i);int x=int(b.GetInt("x")),z=int(b.GetInt("z")),kind=int(b.GetInt("kind"));
            if(!InBounds(x,z)||kind<0||kind>7)continue;
            bool working=kind==6||(BuildingFrontRoad(b)&&litCells[uint(z*size+x)]);
            for(int dz=0;dz<Footprint(b);++dz)for(int dx=0;dx<Footprint(b);++dx)if(InBounds(x+dx,z+dz))workingCells[uint((z+dz)*size+x+dx)]=working;if(working)++workingCounts[uint(kind)];
        }
        infrastructureDirty=false;
        ++infrastructureVersion;
    }
    bool IsRoad(int x,int z){EnsureInfrastructure();return RawRoad(x,z);}
    bool IsLit(int x,int z){EnsureInfrastructure();return InBounds(x,z)&&litCells[uint(z*GridSize()+x)];}
    bool IsWorking(int x,int z){EnsureInfrastructure();return InBounds(x,z)&&workingCells[uint(z*GridSize()+x)];}
    int RoadMask(int x,int z){EnsureInfrastructure();return (RawRoad(x-1,z)?1:0)|(RawRoad(x+1,z)?2:0)|(RawRoad(x,z-1)?4:0)|(RawRoad(x,z+1)?8:0);}
    int WorkingCount(int kind){EnsureInfrastructure();return kind>=0&&kind<8?workingCounts[uint(kind)]:0;}
    int WorkingFacilities(){int total=0;for(int k=0;k<8;++k)if(k!=6)total+=WorkingCount(k);return total;}
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
        if(!InBounds(x,z))return false;EnsureBuildings();return buildingIndices[uint(z*GridSize()+x)]>=0;
    }
    void AddResources(int wood, int stone, int crystal) {
        data.SetInt("wood", data.GetInt("wood") + wood);
        data.SetInt("stone", data.GetInt("stone") + stone);
        data.SetInt("crystal", data.GetInt("crystal") + crystal);
    }
    string BuildProblem(int kind, int x, int z,int tier=0,int recipe=0) {
        int size = int(rules.GetInt("gridSize", 16));
        if (x < 0 || z < 0 || x >= size || z >= size) return TownTr("建築可能エリアの外です。", "Outside the building area.");
        if (Occupied(x, z)) return TownTr("このマスには既に施設があります。", "This cell is occupied.");
        Json@ d = Definition(kind,tier,recipe);
        if (d is null) return TownTr("施設を選択してください。", "Select a building.");
        if (data.GetInt("wood") < d.GetInt("wood") || data.GetInt("stone") < d.GetInt("stone") ||
            data.GetInt("crystal") < d.GetInt("crystal")) return TownTr("素材が足りません。", "Not enough materials.");
        int width=int(d.GetInt("size",1));for(int dz=0;dz<width;++dz)for(int dx=0;dx<width;++dx){if(!InBounds(x+dx,z+dz))return TownTr("敷地がマップ外です","Footprint outside map");if(Occupied(x+dx,z+dz))return TownTr("敷地に別の建物があります","Footprint occupied");}
        array<string> refined={"planks","masonry","refinedCrystal"};for(uint i=0;i<refined.length();++i)if(data.GetInt(refined[i])<d.GetInt(refined[i]))return TownTr("加工資材が足りません","Not enough processed materials");
        if (kind == 5) {
            if (data.GetBool("cleared")) return TownTr("光の塔は完成しています。", "The Tower of Light is already complete.");
            if (WorkingFacilities() < int(rules.GetInt("goalBuildingCount", 8))) return TownTr("先に稼働中の施設を8棟用意してください。", "Activate 8 facilities first.");
            if (data.GetFloat("energy") < rules.GetFloat("goalEnergy", 80)) return TownTr("光エネルギーが80必要です。", "80 light energy required.");
            if (data.GetFloat("prosperity") < rules.GetFloat("goalProsperity", 60)) return TownTr("繁栄度が60必要です。", "60 prosperity required.");
        }
        return "";
    }
    array<float>@ PlacementDeltas(int kind,int tier=0){
        array<float>@ delta=array<float>(3,0);if(kind<0||kind==6||kind>7)return delta;
        delta[0]=TownClamp(float(data.GetFloat("happiness"))+(kind==1?float(Balance("housingHappiness",tier,1+tier*2)):0),0,100)-float(data.GetFloat("happiness"));
        delta[1]=TownClamp(float(data.GetFloat("safety"))+(kind==0?Balance("streetlightSafety",0,2):(kind==7?Balance("policeSafety",0,18):(kind==1?Balance("housingSafety",tier,-(2+tier*tier*4)):0))),0,100)-float(data.GetFloat("safety"));
        delta[2]=TownClamp(float(data.GetFloat("prosperity"))+Balance("prosperity",tier,tier==0?5:(tier==1?12:30)),0,100)-float(data.GetFloat("prosperity"));return delta;
    }
    bool Build(int kind, int x, int z, int rotation=-1,bool persist=true,int tier=0,int recipe=0) {
        @placedParameterDeltas=array<float>(3,0);
        string problem = BuildProblem(kind, x, z,tier,recipe);
        if (problem != "") { message = problem; return false; }
        Json@ d = Definition(kind,tier,recipe);
        AddResources(-int(d.GetInt("wood")), -int(d.GetInt("stone")), -int(d.GetInt("crystal")));
        Json@ list = Buildings(); Json@ b = Json();
        EnsureInfrastructure();b.SetInt("tier",tier);b.SetInt("recipe",recipe);b.SetInt("kind", kind); b.SetInt("x", x); b.SetInt("z", z);b.SetInt("rotation",rotation<0?AutoFacing(x,z):TownFacing(rotation));
        list.PushJson(b); SetBuildings(list);
        array<string> refined={"planks","masonry","refinedCrystal"};for(uint i=0;i<refined.length();++i)data.SetInt(refined[i],data.GetInt(refined[i])-d.GetInt(refined[i]));
        float beforeHappiness=float(data.GetFloat("happiness"));@placedParameterDeltas=PlacementDeltas(kind,tier);
        array<string> parameterKeys={"happiness","safety","prosperity"};for(uint i=0;i<3;++i)data.SetFloat(parameterKeys[i],data.GetFloat(parameterKeys[i])+placedParameterDeltas[i]);
        infrastructureDirty = true; ReconcilePollution();placedParameterDeltas[0]=float(data.GetFloat("happiness"))-beforeHappiness;TryClear();
        message = data.GetBool("cleared") ? TownTr("光の塔が完成しました！ 街づくりクリアです。", "The Tower of Light is complete! Town cleared!") : TownBuildingName(d) + TownTr("を建築しました。", " constructed.");
        if(persist)Save(); return true;
    }
    string DeleteProblem(int x,int z){Json@ b=BuildingAt(x,z);if(b is null)return TownTr("削除する施設がありません。","No facility to remove.");if(b.GetInt("kind")==4&&b.GetBool("dispatchActive"))return TownTr("NPCが帰還してからギルドを削除してください。","Wait for the NPC to return before removing this guild.");return "";}
    bool RemoveBuilding(int x,int z,bool persist=true){
        Json@ chosen=BuildingAt(x,z);if(chosen !is null){x=int(chosen.GetInt("x"));z=int(chosen.GetInt("z"));}
        string problem=DeleteProblem(x,z);if(problem!=""){message=problem;return false;}
        Json@ list=Buildings();Json@ next=Json();next.Parse("[]");for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);if(b.GetInt("x")!=x||b.GetInt("z")!=z)next.PushJson(b);}SetBuildings(next);infrastructureDirty=true;EnsureInfrastructure();ReconcilePollution();SyncDispatchSummary();message=TownTr("施設を削除しました。建築資材の返金はありません。","Facility removed. Construction materials are not refunded.");if(persist)Save();return true;
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
    bool DispatchAt(int x,int z,bool persist=true){
        Json@ guild=BuildingAt(x,z);
        if(guild is null||guild.GetInt("kind")!=4||!IsWorking(x,z)){message=TownTr("道路に隣接し街灯に照らされたギルドが必要です。","A road-connected, illuminated guild is required.");return false;}
        if(guild.GetBool("dispatchActive")){message=TownTr("このギルドのNPCは現在遠征中です。","This guild's NPC is already away.");return false;}
        float duration=TownClamp(float(rules.GetFloat("dispatchSeconds",45)),1,86400);
        guild.SetBool("dispatchActive",true);guild.SetFloat("dispatchRemaining",duration);guild.SetFloat("dispatchDuration",duration);
        StoreBuilding(guild);
        SyncDispatchSummary();message=TownTr("NPCが素材集めに出発しました。遠征中も時間が進みます。","NPC dispatched! The timer continues during your expedition.");if(persist)Save();return true;
    }
    // 旧スクリプト呼び出しとの互換用。画面操作はセルを指定するDispatchAtを使う。
    void Dispatch(){Json@ list=Buildings();for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);if(b.GetInt("kind")==4&&IsWorking(int(b.GetInt("x")),int(b.GetInt("z")))){DispatchAt(int(b.GetInt("x")),int(b.GetInt("z")));return;}}message=TownTr("稼働中のギルドをマップで選択してください。","Select an operational guild on the map.");}
    void AdvanceRealTime() {
        double elapsed = GetMonotonicSeconds() - lastRealTime;
        if (elapsed <= 0) return;
        // Bound catch-up work after a long suspension. The unapplied time stays pending.
        if (elapsed > 30 && HasFacilities()) elapsed = 30;
        lastRealTime += elapsed;
        Tick(elapsed);
    }
    bool TickDispatch(double dt) {
        if (data.GetInt("dispatchCount") <= 0) return false;
        bool returned = false;
        Json@ guilds=Buildings();Json@ nextBuildings=Json();nextBuildings.Parse("[]");
        for(uint i=0;i<guilds.Size();++i){
            Json@ guild=guilds.At(i);if(guild.GetInt("kind")!=4||!guild.GetBool("dispatchActive")){nextBuildings.PushJson(guild);continue;}
            double remaining = guild.GetFloat("dispatchRemaining") - dt;
            guild.SetFloat("dispatchRemaining", remaining > 0 ? remaining : 0);
            if (remaining <= 0) {
                guild.SetBool("dispatchActive", false);
                int scale=Capacity(guild);AddResources(int(rules.GetInt("dispatchWood",20))*scale,int(rules.GetInt("dispatchStone",15))*scale,int(rules.GetInt("dispatchCrystal",6))*scale);
                message = TownTr("NPCが帰還しました。素材を受け取りました。", "NPC returned. Materials received."); returned = true;
            }
            nextBuildings.PushJson(guild);
        }
        SetBuildings(nextBuildings);
        SyncDispatchSummary();
        return returned;
    }
    void Tick(double dt) {
        if (dt <= 0) return;
        if (!HasFacilities()) {
            data.SetFloat("productionClock", 0);
            data.SetFloat("eventClock", 0);
            return;
        }
        double clock = data.GetFloat("productionClock");
        double eventClock = data.GetFloat("eventClock");
        double interval = ProductionInterval();
        double eventInterval = TownClamp(float(rules.GetFloat("eventSeconds", 40)), 1, 86400);
        bool persist = false;
        // Apply returns before each due cycle, so a later return can't fund earlier production.
        while (dt > 0) {
            double untilProduction = interval - clock;
            double untilEvent = eventInterval - eventClock;
            double step = dt;
            if (untilProduction > 0 && untilProduction < step) step = untilProduction;
            if (untilEvent > 0 && untilEvent < step) step = untilEvent;
            if (TickDispatch(step)) persist = true;
            if (step >= untilProduction) { clock = step - untilProduction; Produce(); persist = true; }
            else clock += step;
            if (step >= untilEvent) { eventClock = step - untilEvent; Event(); persist = true; }
            else eventClock += step;
            dt -= step;
        }
        data.SetFloat("productionClock", clock);
        data.SetFloat("eventClock", eventClock);
        if (persist) Save();
    }
    // The preview and payout share this calculation, including resource caps.
    // Order: wood, stone, crystal, light, happiness, safety, prosperity.
    void ReconcilePollution(){
        float penalty=0;Json@ list=Buildings();float spacing=float(rules.GetFloat("cellSize",1.5f));
        for(uint i=0;i<list.Size();++i){Json@ house=list.At(i);if(house.GetInt("kind")!=1)continue;for(uint j=0;j<list.Size();++j){Json@ factory=list.At(j);if(factory.GetInt("kind")!=2)continue;int t=int(factory.GetInt("tier"));float hx=float(house.GetInt("x")),hz=float(house.GetInt("z")),fx=float(factory.GetInt("x")),fz=float(factory.GetInt("z"));float dx=Max(0.0f,Max(hx-fx-float(Footprint(factory)-1),fx-hx-float(Footprint(house)-1))),dz=Max(0.0f,Max(hz-fz-float(Footprint(factory)-1),fz-hz-float(Footprint(house)-1)));if(dx*dx+dz*dz<=float(Balance("pollutionRadius",t,2+t)*Balance("pollutionRadius",t,2+t)))penalty+=float(Balance("pollution",t,2+t*3));}}
        float old=float(data.GetFloat("pollutionPenalty")),applied=float(data.GetFloat("pollutionApplied",old)),h=float(data.GetFloat("happiness"));if(penalty>old){float loss=Min(h,penalty-old);h-=loss;applied+=loss;}else if(penalty<old){float restore=Min(applied,old-penalty);h=Min(100.0f,h+restore);applied-=restore;}data.SetFloat("happiness",h);data.SetFloat("pollutionPenalty",penalty);data.SetFloat("pollutionApplied",applied);
    }
    array<float>@ ProductionDeltas(){
        array<float>@ delta=array<float>(10,0);if(!HasFacilities())return delta;
        float light=4+WorkingCount(0)*12-WorkingCount(3)*2-WorkingCount(4)*3-WorkingCount(7)*2;
        array<int> remaining={int(data.GetInt("wood")),int(data.GetInt("stone")),int(data.GetInt("crystal"))};float available=Max(0.0f,float(data.GetFloat("energy"))+light);
        Json@ list=Buildings();for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);if(b.GetInt("kind")!=2||!IsWorking(int(b.GetInt("x")),int(b.GetInt("z"))))continue;int recipe=int(b.GetInt("recipe"))%3,scale=Capacity(b),cost=scale*Balance("processingEnergy",0,4),input=scale*Balance("processingInput",0,2);
            if(remaining[uint(recipe)]>=input&&available>=cost){remaining[uint(recipe)]-=input;delta[uint(recipe)]-=input;delta[uint(7+recipe)]+=scale;available-=cost;light-=cost;}}
        delta[3]=light<0?-Min(-light,float(data.GetFloat("energy"))):light;
        if(data.GetFloat("energy")+delta[3]<20){delta[4]=-Min(3.0f,float(data.GetFloat("happiness")));delta[5]=-Min(3.0f,float(data.GetFloat("safety")));}return delta;
    }
    void Produce(){if(!HasFacilities())return;array<float>@ delta=ProductionDeltas();AddResources(int(delta[0]),int(delta[1]),int(delta[2]));array<string> keys={"energy","happiness","safety","prosperity","planks","masonry","refinedCrystal"};for(uint i=0;i<keys.length();++i)data.SetFloat(keys[i],data.GetFloat(keys[i])+delta[i+3]);TryClear();}
    void DispatchAll(){
        Json@ list=Buildings();int sent=0;
        for(uint i=0;i<list.Size();++i){Json@ b=list.At(i);int x=int(b.GetInt("x")),z=int(b.GetInt("z"));
            if(b.GetInt("kind")==4&&!b.GetBool("dispatchActive")&&IsWorking(x,z)){if(DispatchAt(x,z,false))++sent;}}
        message=TownTr("一括派遣：","Dispatched guilds: ")+sent;if(sent>0)Save();
    }
    void Event(){
        if(!HasFacilities())return;float roll=Random::Float(0,1);float happiness=0,safety=0,light=0;
        if(roll<.3f){happiness=-4;safety=-3;message=TownTr("騒音の苦情が届きました","Noise complaints");}
        else if(roll<.65f){happiness=5;safety=2;message=TownTr("住民の交流会が開かれました","Community gathering");}
        else{happiness=3;light=-10;message=TownTr("光の祭りが開かれました","Festival of light");}
        happiness=TownClamp(float(data.GetFloat("happiness"))+happiness,0,100)-float(data.GetFloat("happiness"));safety=TownClamp(float(data.GetFloat("safety"))+safety,0,100)-float(data.GetFloat("safety"));light=light<0?-Min(-light,float(data.GetFloat("energy"))):light;
        data.SetFloat("happiness",data.GetFloat("happiness")+happiness);data.SetFloat("safety",data.GetFloat("safety")+safety);data.SetFloat("energy",data.GetFloat("energy")+light);
        Json@ events=data.GetJson("notifications");Json@ e=Json();e.SetString("text",message);e.SetFloat("happiness",happiness);e.SetFloat("safety",safety);e.SetFloat("energy",light);events.PushJson(e);data.SetJson("notifications",events);
    }
}
