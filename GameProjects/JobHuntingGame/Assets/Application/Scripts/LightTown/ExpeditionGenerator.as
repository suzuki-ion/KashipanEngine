#include "ExpeditionTraversal.as"
// 接続グラフ→出入口別テンプレート→通常ジャンプ検証。WFCはバイオームに使用。
class ExpeditionRoom {
    int left, bottom, width, height;
    int templateKind = 0, ports = 0;
    float CenterX() { return float(left) + float(width) * 0.5f; }
}
class ExpeditionLink {
    uint a, b;
    bool vertical;
    float shaftX;
}
class ExpeditionGenerator {
    string profile="classic";
    bool SetProfile(const string &in value){if(value!="classic"&&value!="forest"&&value!="quarry"&&value!="grotto")return false;profile=value;return true;}
    array<string>@ names = {"Meadow", "Forest", "Quarry", "Crystal"};
    array<int>@ levels = {0, 0, 1, 2};
    WaveFunctionCollapse@ wfc;
    uint columns = 4, rows = 4, width = 66, height = 60;
    uint startRoom = 0, goalRoom = 15, seed = 1, random = 1;
    bool usedFallback = false;
    bool routeValidated = false;
    uint repairCount = 0;
    string routeProblem;
    ExpeditionTraversal@ traversal = ExpeditionTraversal();
    array<uint>@ mainRoute = array<uint>();
    array<ExpeditionRoom@>@ rooms = array<ExpeditionRoom@>();
    array<ExpeditionLink@>@ links = array<ExpeditionLink@>();
    array<bool>@ solid = array<bool>();
    array<bool>@ caveSupports=array<bool>();
    array<int>@ forestSurface=array<int>();
    int ForestFloor(int x){if(forestSurface.length()==0)return 3;x=x<0?0:(x>=int(forestSurface.length())?int(forestSurface.length())-1:x);return forestSurface[uint(x)];}
    void ForestTerrain(){
        forestSurface.resize(width);
        for(uint x=0;x<width;++x)forestSurface[x]=int(8+3*CaveNoise(float(x)*.055f,19)+2*Sin(float(x)*.13f+float(seed%17)));
        // Stable gathering clearings on a rolling surface.
        for(uint column=0;column<columns;++column){int left=rooms[column].left,level=ForestFloor(left+1);for(int x=left-1;x<=left+10;++x)if(x>0&&x<int(width)-1)forestSurface[uint(x)]=level;}
        for(uint x=1;x<width;++x)if(forestSurface[x]>forestSurface[x-1]+1)forestSurface[x]=forestSurface[x-1]+1;
        for(int x=int(width)-2;x>=0;--x)if(forestSurface[uint(x)]>forestSurface[uint(x+1)]+1)forestSurface[uint(x)]=forestSurface[uint(x+1)]+1;
        // Two-cell terraces provide a real landing surface for the 1m-wide body.
        for(uint x=0;x+1<width;x+=2)forestSurface[x+1]=forestSurface[x];
        for(uint y=0;y<height;++y)for(uint x=0;x<width;++x)solid[y*width+x]=x==0||x==width-1||y==height-1||int(y)<forestSurface[x];
        for(uint row=0;row<rows;++row)for(uint column=0;column<columns;++column){ExpeditionRoom@ room=rooms[row*columns+column];room.bottom=row==0?ForestFloor(room.left+1):12+int(row)*12;}
        // Broken branches connect neighboring crowns. Openings at climbing trunks
        // prevent a solid shelf from blocking the ascent from below.
        for(uint row=1;row<rows;++row)for(uint column=0;column+1<columns;++column){ExpeditionRoom@ a=rooms[row*columns+column];ExpeditionRoom@ b=rooms[row*columns+column+1];int from=a.left+1,to=b.left+1;
            for(int x=from;x<=to;++x){float t=float(x-from)/float(to-from);int floor=int(float(a.bottom)+float(b.bottom-a.bottom)*t);bool gap=false;
                for(uint i=0;i<links.length();++i){ExpeditionLink@ link=links[i];if(link.vertical&&link.b/columns==row&&Abs(float(x)-link.shaftX-.5f)<3.5f)gap=true;}
                bool nearTrunk=false;for(uint i=0;i<links.length();++i)if(links[i].vertical&&(links[i].b/columns==row||links[i].a/columns==row)&&Abs(float(x)-links[i].shaftX-.5f)<7)nearTrunk=true;
                if(!nearTrunk&&x>from+4&&x<to-3&&(x+int(row)*2)%7<2)gap=true;
                if(!gap)solid[uint(floor-1)*width+uint(x)]=true;
            }
        }
        for(uint i=columns;i<rooms.length();++i){ExpeditionRoom@ room=rooms[i];for(int x=room.left-1;x<room.left+3;++x)if(x>0&&x<int(width)-1)solid[uint(room.bottom-1)*width+uint(x)]=true;}
    }
    void MovementProfile(float jumpPower,float gravityPerFrame,float maxSpeed) {
        traversal.jump=jumpPower>0?jumpPower:20;traversal.gravity=gravityPerFrame>0?gravityPerFrame*60:48;traversal.speed=maxSpeed>0?maxSpeed*.75f:6;
    }
    uint Next(uint limit) { random = random * 1664525 + 1013904223; return (random >> 8) % limit; }
    bool IsSolid(int x, int y) {
        if (x < 0 || y < 0 || x >= int(width) || y >= int(height)) return true;
        return solid[uint(y) * width + uint(x)];
    }
    bool CanPlaceFluorescent(ExpeditionRoom@ room) {
        // The original fixture is 5.2m wide. Require a continuous ceiling
        // above its full footprint, including openings carved by shafts.
        int ceiling=room.bottom+room.height;
        for(int x=int(Floor(room.CenterX()-2.6f));x<int(Ceil(room.CenterX()+2.6f));++x)
            if(!IsSolid(x,ceiling))return false;
        return true;
    }
    void Carve(int left, int bottom, int right, int top) {
        for (int y = bottom; y < top; ++y) for (int x = left; x < right; ++x)
            if (x > 0 && y > 0 && x < int(width) - 1 && y < int(height) - 1) solid[uint(y) * width + uint(x)] = false;
    }
    float CaveHash(int x,int y){uint n=uint(x)*374761393+uint(y)*668265263+seed*1442695041;n=(n^(n>>13))*1274126177;return float((n^(n>>16))&65535)/65535.0f;}
    float CaveNoise(float x,float y){int ix=int(Floor(x)),iy=int(Floor(y));float tx=x-float(ix),ty=y-float(iy);tx=tx*tx*(3-2*tx);ty=ty*ty*(3-2*ty);return (CaveHash(ix,iy)*(1-tx)+CaveHash(ix+1,iy)*tx)*(1-ty)+(CaveHash(ix,iy+1)*(1-tx)+CaveHash(ix+1,iy+1)*tx)*ty;}
    void CavePocket(float cx,float cy,float rx,float ry){
        for(int y=int(Floor(cy-ry-1));y<=int(Ceil(cy+ry+1));++y)for(int x=int(Floor(cx-rx-1));x<=int(Ceil(cx+rx+1));++x){
            if(x<=0||y<=0||x>=int(width)-1||y>=int(height)-1)continue;
            if(caveSupports.length()==solid.length()&&caveSupports[uint(y)*width+uint(x)])continue;
            float dx=(float(x)+.5f-cx)/rx,dy=(float(y)+.5f-cy)/ry;
            if(dx*dx+dy*dy<1+(CaveNoise(float(x)*.32f,float(y)*.32f)-.5f)*.28f)Carve(x,y,x+1,y+1);
        }
    }
    float CaveShaftX(ExpeditionLink@ link,float t){return link.shaftX+.5f+Sin(t*3.14159265f)*(CaveHash(int(link.a),53)<.5f?-1.5f:1.5f);}
    void CaveRoom(ExpeditionRoom@ room,uint index){
        float cx=room.CenterX()+(CaveHash(int(index),7)-.5f)*3,cy=float(room.bottom)+4+(CaveHash(int(index),9)-.5f)*2;
        CavePocket(cx,cy,5.5f+CaveHash(int(index),11)*1.3f,4.6f+CaveHash(int(index),23)*1.8f);
        // Attached lobes produce irregular large chambers without isolated holes.
        CavePocket(cx-2.5f,cy+2,3.5f,3+CaveHash(int(index),31)*2);
        if(index%3==0)CavePocket(cx+3,cy+1,4.2f,4.5f);
    }
    void CavePassage(ExpeditionLink@ link){
        ExpeditionRoom@ a=rooms[link.a];ExpeditionRoom@ b=rooms[link.b];
        if(link.vertical){
            float rise=float(b.bottom-a.bottom);for(int y=a.bottom;y<=b.bottom+3;++y){float t=float(y-a.bottom)/rise;t=t<0?0:(t>1?1:t);CavePocket(CaveShaftX(link,t),float(y)+.5f,4.1f,3.2f);}
        }else{
            int from=int(a.CenterX()),to=int(b.CenterX());float bend=CaveHash(int(link.a),71)<.5f?-1.5f:1.5f;
            for(int x=from;x<=to;++x){float t=float(x-from)/float(to-from),floorY=float(a.bottom)+float(b.bottom-a.bottom)*t+Sin(t*3.14159265f)*bend;CavePocket(float(x)+.5f,floorY+3.2f,2.7f,3.4f);}
        }
    }
    void OrganicCave(){
        // Preserve the original walkable rock shelves, with varying thickness.
        // Expanding air around this backbone cannot destroy mandatory landings.
        caveSupports.resize(solid.length());for(uint i=0;i<caveSupports.length();++i)caveSupports[i]=false;
        for(int y=1;y<int(height)-1;++y)for(int x=1;x<int(width)-1;++x)if(IsSolid(x,y)&&!IsSolid(x,y+1)){
            int depth=CaveHash(x,y)<.5f?2:3;for(int d=0;d<depth&&y-d>0;++d)if(IsSolid(x,y-d))caveSupports[uint(y-d)*width+uint(x)]=true;
        }
        for(uint i=0;i<rooms.length();++i)CaveRoom(rooms[i],i);
        for(uint i=0;i<links.length();++i)CavePassage(links[i]);
    }
    bool Linked(uint a, uint b) {
        for (uint i = 0; i < links.length(); ++i) if ((links[i].a == a && links[i].b == b) || (links[i].b == a && links[i].a == b)) return true;
        return false;
    }
    void Connect(uint a, uint b) {
        if (Linked(a, b)) return;
        ExpeditionLink@ link = ExpeditionLink(); link.a = a < b ? a : b; link.b = a < b ? b : a;
        link.vertical = link.b - link.a == columns;
        link.shaftX = float((link.a % columns)*16+8);
        links.insertLast(link);
    }
    void ForestGraph(){
        // Broad horizontal groves joined by staggered climbing routes.
        for(uint y=0;y<rows;++y)for(uint x=0;x+1<columns;++x)Connect(y*columns+x,y*columns+x+1);
        for(uint y=0;y+1<rows;++y){uint x=Next(columns);Connect(y*columns+x,(y+1)*columns+x);if(y%2==0){uint other=(x+1+Next(columns-1))%columns;Connect(y*columns+other,(y+1)*columns+other);}}
    }
    void QuarryGraph(){
        // Parallel extraction shafts, crosscuts and a broad upper work gallery.
        for(uint x=0;x<columns;++x)for(uint y=0;y+2<rows;++y)Connect(y*columns+x,(y+1)*columns+x);
        for(uint x=0;x+1<columns;++x){Connect(x,x+1);Connect((rows-1)*columns+x,(rows-1)*columns+x+1);}
        uint shaft=Next(columns);Connect((rows-2)*columns+shaft,(rows-1)*columns+shaft);
        uint crosscut=Next(columns-1);Connect(columns+crosscut,columns+crosscut+1);
    }
    void TunnelGraph(){
        // A randomized spanning tree retains dead ends; extra edges add detours.
        array<bool> visited(rooms.length(),false);array<uint> stack;
        uint first=Next(rooms.length());stack.insertLast(first);visited[first]=true;
        while(stack.length()>0){uint a=stack[stack.length()-1];array<uint> options;
            if(a%columns>0&&!visited[a-1])options.insertLast(a-1);if(a%columns+1<columns&&!visited[a+1])options.insertLast(a+1);
            if(a>=columns&&!visited[a-columns])options.insertLast(a-columns);if(a+columns<rooms.length()&&!visited[a+columns])options.insertLast(a+columns);
            if(options.length()==0){stack.removeLast();continue;}uint b=options[Next(options.length())];Connect(a,b);visited[b]=true;stack.insertLast(b);
        }
        bool loopAdded=false;uint loopChance=profile=="grotto"?3:5;
        for(uint a=0;a<rooms.length();++a){
            if(a%columns+1<columns&&!Linked(a,a+1)&&(!loopAdded||Next(loopChance)==0)){Connect(a,a+1);loopAdded=true;}
            if(a+columns<rooms.length()&&!Linked(a,a+columns)&&Next(loopChance)==0){Connect(a,a+columns);loopAdded=true;}
        }
    }
    bool Solve(uint count, uint value, uint layers=4) {
        seed = value; random = value; usedFallback = false; routeValidated=false;repairCount=0;routeProblem="";
        columns = count / 12; columns = columns < 3 ? 3 : (columns > 10 ? 10 : columns); rows = layers < 4 ? 4 : (layers > 10 ? 10 : layers);
        width = columns * 16 + 2; height = rows * 14 + 4;
        rooms.resize(0); links.resize(0); solid.resize(width * height);caveSupports.resize(0);forestSurface.resize(0);
        for (uint i = 0; i < solid.length(); ++i) solid[i] = true;
        @wfc = WaveFunctionCollapse(); wfc.SetGridSize(columns, rows, 1);
        for (uint i = 0; i < names.length(); ++i) wfc.RegisterTile(names[i]);
        for (uint i = 0; i < names.length(); ++i) for (uint j = 0; j < names.length(); ++j) {
            if (Abs(float(levels[i] - levels[j])) > 1) continue;
            wfc.AddTileConnection(names[i], WFCDirection::Left, names[j]); wfc.AddTileConnection(names[i], WFCDirection::Right, names[j]);
            wfc.AddTileConnection(names[i], WFCDirection::Up, names[j]); wfc.AddTileConnection(names[i], WFCDirection::Down, names[j]);
        }
        wfc.FixTile(0, 0, 0, "Forest"); wfc.FixTile(1, 0, 0, "Quarry"); wfc.FixTile(2, 0, 0, "Crystal");
        bool solved = false;
        for (uint retry = 0; retry < 4 && !solved; ++retry) { wfc.SetSeed(seed + retry); solved = wfc.Solve(); }
        usedFallback = !solved;
        for (uint y = 0; y < rows; ++y) for (uint x = 0; x < columns; ++x) {
            ExpeditionRoom@ room = ExpeditionRoom();
            room.left = int(x * 16 + 3); room.bottom = int(y * 14 + 3);
            room.width = 12; room.height = int(8 + Next(2)); rooms.insertLast(room);
            room.templateKind=int(Next(4));
            if(profile=="forest"){room.left=int(x*16+2);room.width=14;room.height=11;room.templateKind=Next(3)==0?1:2;}
            else if(profile=="quarry"){room.height=10;room.templateKind=y==rows-1?3:(Next(3)==0?2:3);}
            else if(profile=="grotto"){room.width=10;room.height=8;}
            Carve(room.left, room.bottom, room.left + room.width, room.bottom + room.height);
        }
        if(profile=="forest")ForestGraph();else if(profile=="quarry")QuarryGraph();else TunnelGraph();
        for (uint i = 0; i < links.length(); ++i) {
            ExpeditionLink@ link = links[i]; ExpeditionRoom@ a = rooms[link.a]; ExpeditionRoom@ b = rooms[link.b];
            if (link.vertical) Carve(int(link.shaftX) - 3, a.bottom, int(link.shaftX) + 4, b.bottom + 5);
            else {
                int from = int(a.CenterX()), to = int(b.CenterX());
                for (int x = from; x <= to; ++x) {
                    int floorY = int(float(a.bottom) + float(b.bottom - a.bottom) * float(x - from) / float(to - from));
                    Carve(x, floorY, x + 1, floorY + (profile=="forest"?7:(profile=="grotto"?4:5)));
                }
            }
        }
        // 接続通路が床を削った部屋にも、配置用の小さな支持床を残す。
        for (uint i = 0; i < rooms.length(); ++i) {
            ExpeditionRoom@ room = rooms[i];
            Carve(room.left, room.bottom, room.left + 3, room.bottom + 4);
            for (int x = room.left; x < room.left + 3; ++x) solid[uint(room.bottom - 1) * width + uint(x)] = true;
        }
        if(profile=="grotto")OrganicCave();
        if(profile=="forest")ForestTerrain();
        startRoom = Next(rooms.length());
        array<int>@ distance = Distances(startRoom); goalRoom = startRoom;
        for (uint i = 0; i < rooms.length(); ++i) if (distance[i] > distance[goalRoom]) goalRoom = i;
        SetPortsAndRoute();
        @traversal.solid=solid;traversal.width=width;traversal.height=height;
        MakePlatforms(false);
        routeValidated=ValidateRoutes();
        // 不成立なら追加の段差・斜面を外し、検証用の基本テンプレートへ戻す。
        if(!routeValidated){repairCount=1;MakePlatforms(true);routeValidated=ValidateRoutes();}
        if(!routeValidated){repairCount=2;SafeFallback();routeValidated=ValidateRoutes();usedFallback=true;}
        if(routeValidated&&profile=="quarry")PlanWallLamps();
        return routeValidated;
    }
    void LampPlatform(const Vector3 &in position) {
        Platform(position,Vector3(1,1.5f,1));
        traversal.platforms[traversal.platforms.length()-1].wallLamp=true;
    }
    void PlanWallLamps() {
        array<Vector3> candidates;
        for(uint i=0;i<rooms.length();++i)candidates.insertLast(Vector3(float(rooms[i].left)+1.5f,float(rooms[i].bottom)+4.6f,0));
        for(uint i=0;i<links.length();++i){ExpeditionLink@ link=links[i];ExpeditionRoom@ a=rooms[link.a];ExpeditionRoom@ b=rooms[link.b];
            candidates.insertLast(link.vertical?Vector3(link.shaftX+.5f,float(a.bottom+b.bottom)*.5f+3,0):Vector3((a.CenterX()+b.CenterX())*.5f,float(a.bottom+b.bottom)*.5f+3,0));
        }
        // Bound expensive placement validation on expanded maps. Fixtures are
        // sampled across the whole map rather than all clustered at its bottom.
        if(rooms.length()>16){array<Vector3> distributed;uint count=12;
            for(uint i=0;i<count;++i){ExpeditionRoom@ room=rooms[i*(rooms.length()-1)/(count-1)];
                distributed.insertLast(Vector3(float(room.left+room.width)-1,float(room.bottom)+5.5f,0));
            }
            candidates=distributed;
        }
        uint base=traversal.platforms.length();
        for(uint i=0;i<candidates.length();++i)LampPlatform(candidates[i]);
        if(!ValidateRoutes()){
            // Keep only fixtures that preserve both outbound and return routes.
            traversal.platforms.resize(base);
            for(uint i=0;i<candidates.length();++i){LampPlatform(candidates[i]);if(!ValidateRoutes())traversal.platforms.removeLast();}
            routeValidated=ValidateRoutes();
        }
        routeProblem="";
    }
    void SetPortsAndRoute() {
        for(uint i=0;i<rooms.length();++i)rooms[i].ports=0;
        for(uint i=0;i<links.length();++i){ExpeditionLink@ l=links[i];rooms[l.a].ports|=l.vertical?8:2;rooms[l.b].ports|=l.vertical?4:1;}
        array<int> parent(rooms.length(),-1);array<uint> queue={startRoom};parent[startRoom]=int(startRoom);
        for(uint h=0;h<queue.length();++h)for(uint i=0;i<links.length();++i){uint a=queue[h],b=links[i].a==a?links[i].b:(links[i].b==a?links[i].a:a);if(parent[b]<0){parent[b]=int(a);queue.insertLast(b);}}
        mainRoute.resize(0);uint at=goalRoom;mainRoute.insertLast(at);while(at!=startRoom){at=uint(parent[at]);mainRoute.insertAt(0,at);}
    }
    void Platform(const Vector3 &in p,const Vector3 &in size,float angle=0) {
        ExpeditionPlatform@ s=ExpeditionPlatform();s.position=p;s.size=size;s.angle=angle;s.ramp=Abs(angle)>.001f;traversal.platforms.insertLast(s);
    }
    void MakePlatforms(bool repair) {
        traversal.platforms.resize(0);
        for(uint i=0;i<links.length();++i){ExpeditionLink@ l=links[i];if(!l.vertical)continue;
            float bottom=float(rooms[l.a].bottom),rise=float(rooms[l.b].bottom)-bottom;
            uint steps=profile=="forest"?(rise<=13?4:6):uint(Ceil(rise/2.6f));
            for(uint s=1;s<=steps;++s){float top=bottom+rise*float(s)/float(steps);
                uint parity=s+(profile=="forest"?(l.a/columns)*steps:0);
                float x=l.shaftX+.5f+(parity%2==0?1.5f:-1.5f);
                Platform(Vector3(x,top-.15f,0),Vector3(1.8f,.3f,3));
            }
        }
        if(repair)return;
        for(uint i=0;i<rooms.length();++i){ExpeditionRoom@ r=rooms[i];
            // 出入口の幹線と左側の安全な採集場所を避けたテンプレート部品。
            if(r.templateKind==1)Platform(Vector3(float(r.left+r.width)-2,float(r.bottom)+2.8f,0),Vector3(2.6f,.3f,3));
            if(r.templateKind==2){
                // Forest slopes are optional side terrain, outside through exits.
                if(profile=="forest"&&(r.ports&2)!=0)continue;
                float x=float(r.left+r.width)-2.5f;bool support=true;
                for(int k=int(x)-2;k<=int(x)+2;++k)support=support&&IsSolid(k,r.bottom-1);
                if(support)for(uint side=0;side<2;++side){float angle=Atan2(side==0?1.0f:-1.0f,2);
                    Platform(Vector3(x+(side==0?-1:1)+Sin(angle)*.15f,float(r.bottom)+.5f-Cos(angle)*.15f,0),Vector3(Sqrt(5.0f),.3f,3),angle);
                }
            }
            if(r.templateKind==3)Platform(Vector3(float(r.left+r.width)-2,float(r.bottom)+3.2f,0),Vector3(3,.3f,3));
        }
    }
    bool ValidateRoutes() {
        if(!traversal.Flood(StartPosition())){routeProblem="spawn blocked";return false;}
        for(uint i=0;i<rooms.length();++i)if(!traversal.Reachable(RoomAnchor(i))){
            routeProblem="outbound room "+i;return false;
        }
        // 片道の落下だけでは合格にしない。各採集部屋からゴールへ戻れることも確認する。
        if(!traversal.Flood(GoalPosition(),true))return false;
        for(uint i=0;i<rooms.length();++i)if(!traversal.Reachable(RoomAnchor(i))){routeProblem="return room "+i;return false;}
        return true;
    }
    void SafeFallback() {
        // 試行回数を制限。最後は段差のない確認用の連結廊下を生成し、同じ検証を通す。
        width=uint(rooms.length())*16+2;height=18;solid.resize(width*height);
        for(uint i=0;i<solid.length();++i)solid[i]=true;
        links.resize(0);traversal.platforms.resize(0);
        for(uint i=0;i<rooms.length();++i){rooms[i].left=int(i*16+3);rooms[i].bottom=3;rooms[i].width=12;rooms[i].height=9;rooms[i].templateKind=0;}
        Carve(2,3,int(width)-2,13);
        for(uint i=1;i<rooms.length();++i){ExpeditionLink@ l=ExpeditionLink();l.a=i-1;l.b=i;l.vertical=false;links.insertLast(l);}
        SetPortsAndRoute();@traversal.solid=solid;traversal.width=width;traversal.height=height;
    }
    array<int>@ Distances(uint origin) {
        array<int>@ distance = array<int>(rooms.length(), -1); array<uint> queue = {origin}; distance[origin] = 0;
        for (uint head = 0; head < queue.length(); ++head) {
            uint a = queue[head];
            for (uint i = 0; i < links.length(); ++i) {
                uint b = links[i].a == a ? links[i].b : (links[i].b == a ? links[i].a : a);
                if (b != a && distance[b] < 0) { distance[b] = distance[a] + 1; queue.insertLast(b); }
            }
        }
        return distance;
    }
    int Kind(uint x, uint row = 0) {
        if (usedFallback) return int((x + row) % 4);
        string name; if (!wfc.TryGetResolvedTile(x, row, 0, name)) return 0;
        for (uint i = 0; i < names.length(); ++i) if (name == names[i]) return int(i);
        return 0;
    }
    Vector3 RoomAnchor(uint index) {
        ExpeditionRoom@ room = rooms[index];
        for (int x = room.left; x < room.left + room.width - 1; ++x)
            if (IsSolid(x, room.bottom - 1) && IsSolid(x + 1, room.bottom - 1) && !IsSolid(x, room.bottom) && !IsSolid(x, room.bottom + 2))
                return Vector3(float(x) + 1.0f, float(room.bottom) + 1.06f, 0);
        return Vector3(room.CenterX(), float(room.bottom) + 1.06f, 0);
    }
    Vector3 StartPosition() { return RoomAnchor(startRoom); }
    Vector3 GoalPosition() { return RoomAnchor(goalRoom); }
    uint ShortcutEntrance() { return (startRoom + 3) % rooms.length(); }
    uint ShortcutExit() { return (ShortcutEntrance() + rooms.length() / 2) % rooms.length(); }
}
