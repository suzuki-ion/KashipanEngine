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
    void MovementProfile(float jumpPower,float gravityPerFrame,float maxSpeed) {
        traversal.jump=jumpPower>0?jumpPower:20;traversal.gravity=gravityPerFrame>0?gravityPerFrame*60:48;traversal.speed=maxSpeed>0?maxSpeed*.75f:6;
    }
    uint Next(uint limit) { random = random * 1664525 + 1013904223; return (random >> 8) % limit; }
    bool IsSolid(int x, int y) {
        if (x < 0 || y < 0 || x >= int(width) || y >= int(height)) return true;
        return solid[uint(y) * width + uint(x)];
    }
    void Carve(int left, int bottom, int right, int top) {
        for (int y = bottom; y < top; ++y) for (int x = left; x < right; ++x)
            if (x > 0 && y > 0 && x < int(width) - 1 && y < int(height) - 1) solid[uint(y) * width + uint(x)] = false;
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
    bool Solve(uint count, uint value) {
        seed = value; random = value; usedFallback = false; routeValidated=false;repairCount=0;routeProblem="";
        columns = count / 12; columns = columns < 3 ? 3 : (columns > 5 ? 5 : columns); rows = 4;
        width = columns * 16 + 2; height = rows * 14 + 4;
        rooms.resize(0); links.resize(0); solid.resize(width * height);
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
            Carve(room.left, room.bottom, room.left + room.width, room.bottom + room.height);
        }
        // ランダムな全域木に枝を追加。全室をつなぎ、行き止まりと迂回路を残す。
        array<bool> visited(rooms.length(), false); array<uint> stack;
        uint first = Next(rooms.length()); stack.insertLast(first); visited[first] = true;
        while (stack.length() > 0) {
            uint a = stack[stack.length() - 1]; array<uint> options;
            if (a % columns > 0 && !visited[a - 1]) options.insertLast(a - 1);
            if (a % columns + 1 < columns && !visited[a + 1]) options.insertLast(a + 1);
            if (a >= columns && !visited[a - columns]) options.insertLast(a - columns);
            if (a + columns < rooms.length() && !visited[a + columns]) options.insertLast(a + columns);
            if (options.length() == 0) { stack.removeLast(); continue; }
            uint b = options[Next(options.length())]; Connect(a, b); visited[b] = true; stack.insertLast(b);
        }
        bool loopAdded = false;
        for (uint a = 0; a < rooms.length(); ++a) {
            if (a % columns + 1 < columns && !Linked(a, a + 1) && (!loopAdded || Next(5) == 0)) { Connect(a, a + 1); loopAdded = true; }
            if (a + columns < rooms.length() && !Linked(a, a + columns) && Next(5) == 0) { Connect(a, a + columns); loopAdded = true; }
        }
        for (uint i = 0; i < links.length(); ++i) {
            ExpeditionLink@ link = links[i]; ExpeditionRoom@ a = rooms[link.a]; ExpeditionRoom@ b = rooms[link.b];
            if (link.vertical) Carve(int(link.shaftX) - 3, a.bottom, int(link.shaftX) + 4, b.bottom + 5);
            else {
                int from = int(a.CenterX()), to = int(b.CenterX());
                for (int x = from; x <= to; ++x) {
                    int floorY = int(float(a.bottom) + float(b.bottom - a.bottom) * float(x - from) / float(to - from));
                    Carve(x, floorY, x + 1, floorY + 5);
                }
            }
        }
        // 接続通路が床を削った部屋にも、配置用の小さな支持床を残す。
        for (uint i = 0; i < rooms.length(); ++i) {
            ExpeditionRoom@ room = rooms[i];
            Carve(room.left, room.bottom, room.left + 3, room.bottom + 4);
            for (int x = room.left; x < room.left + 3; ++x) solid[uint(room.bottom - 1) * width + uint(x)] = true;
        }
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
        return routeValidated;
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
            uint steps=uint(Ceil(rise/2.6f));
            for(uint s=1;s<=steps;++s){float top=bottom+rise*float(s)/float(steps);
                float x=l.shaftX+.5f+(s%2==0?1.5f:-1.5f);
                Platform(Vector3(x,top-.15f,0),Vector3(1.8f,.3f,3));
            }
        }
        if(repair)return;
        for(uint i=0;i<rooms.length();++i){ExpeditionRoom@ r=rooms[i];
            // 出入口の幹線と左側の安全な採集場所を避けたテンプレート部品。
            if(r.templateKind==1)Platform(Vector3(float(r.left+r.width)-2,float(r.bottom)+2.8f,0),Vector3(2.6f,.3f,3));
            if(r.templateKind==2){
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
