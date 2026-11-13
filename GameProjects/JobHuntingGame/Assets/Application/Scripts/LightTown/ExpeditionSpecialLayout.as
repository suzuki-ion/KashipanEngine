#include "ExpeditionGenerator.as"

class ExpeditionAnnex {
    ExpeditionRoom@ room=ExpeditionRoom();
    uint host=0,keyRoom=0;
    bool locked=false,opened=false,keyTaken=false;
    Vector3 door,keyPosition;
    Vector3 Anchor(){return Vector3(float(room.left)+2,float(room.bottom)+1.06f,0);}
    uint ResourceCount(){return locked?6:5;}
}

// Optional sealed chambers are outside the normal graph. Closing every gate
// must leave all normal gathering rooms, keys and the return gate accessible.
class ExpeditionSpecialLayout {
    array<ExpeditionAnnex@>@ zones=array<ExpeditionAnnex@>();
    uint baseWidth=0;
    string problem;
    bool Fail(const string &in text){problem=text;return false;}
    bool Build(ExpeditionGenerator@ terrain){
        zones.resize(0);problem="";baseWidth=terrain.width;
        uint expanded=baseWidth+22;array<bool> original=terrain.solid;
        terrain.solid.resize(expanded*terrain.height);
        for(uint y=0;y<terrain.height;++y)for(uint x=0;x<expanded;++x)
            terrain.solid[y*expanded+x]=x<baseWidth?original[y*baseWidth+x]:true;
        terrain.width=expanded;terrain.traversal.width=expanded;
        for(uint i=0;i<4;++i){
            ExpeditionAnnex@ zone=ExpeditionAnnex();uint row=i*(terrain.rows-1)/3;
            zone.host=row*terrain.columns+terrain.columns-1;
            ExpeditionRoom@ host=terrain.rooms[zone.host];
            zone.room.left=int(baseWidth)+4;zone.room.bottom=host.bottom;zone.room.width=14;zone.room.height=8;
            zone.locked=i%2==0;zone.opened=!zone.locked;
            zone.keyRoom=(terrain.startRoom+3+i*11)%terrain.rooms.length();
            while(zone.keyRoom==terrain.startRoom||zone.keyRoom==terrain.goalRoom)zone.keyRoom=(zone.keyRoom+1)%terrain.rooms.length();
            zone.keyPosition=terrain.RoomAnchor(zone.keyRoom)+Vector3(0,.3f,-.3f);
            zone.door=Vector3(float(baseWidth)+.5f,float(host.bottom)+2,0);
            terrain.Carve(zone.room.left,host.bottom,zone.room.left+zone.room.width,host.bottom+zone.room.height);
            int from=host.left+host.width-2;
            terrain.Carve(from,host.bottom,zone.room.left,host.bottom+4);
            int support=terrain.profile=="forest"?host.left+2:from;
            for(int x=support;x<zone.room.left+zone.room.width;++x){
                bool shaft=false;
                if(terrain.profile=="forest")for(uint link=0;link<terrain.links.length();++link){ExpeditionLink@ l=terrain.links[link];if(l.vertical&&l.b==zone.host&&Abs(float(x)-l.shaftX-.5f)<3.5f)shaft=true;}
                if(!shaft)terrain.solid[uint(host.bottom-1)*expanded+uint(x)]=true;
            }
            zones.insertLast(zone);
        }
        if(!terrain.ValidateRoutes())return Fail("main route: "+terrain.routeProblem);
        if(!terrain.traversal.Flood(terrain.StartPosition()))return Fail("spawn");
        for(uint i=0;i<zones.length();++i)if(!terrain.traversal.Reachable(zones[i].Anchor()))return Fail("open cache "+i+" host="+zones[i].host);
        if(!terrain.traversal.Flood(terrain.GoalPosition(),true))return Fail("return gate");
        for(uint i=0;i<zones.length();++i)if(!terrain.traversal.Reachable(zones[i].Anchor()))return Fail("cache return "+i);
        uint count=terrain.traversal.platforms.length();
        for(uint i=0;i<zones.length();++i)if(zones[i].locked)terrain.Platform(zones[i].door,Vector3(.6f,4,3));
        bool valid=terrain.ValidateRoutes();
        if(valid){valid=terrain.traversal.Flood(terrain.StartPosition());
            for(uint i=0;i<zones.length();++i){
                if(!terrain.traversal.Reachable(terrain.RoomAnchor(zones[i].keyRoom))){problem="key "+i;valid=false;}
                if(terrain.traversal.Reachable(zones[i].Anchor())!=!zones[i].locked){problem="gate isolation "+i;valid=false;}
            }
        }
        terrain.traversal.platforms.resize(count);terrain.routeValidated=valid;
        return valid;
    }
    bool CanHarvest(int zone){return zone<0||(uint(zone)<zones.length()&&zones[uint(zone)].opened);}
    bool HasKey(uint zone){return zone<zones.length()&&zones[zone].locked&&zones[zone].keyTaken;}
    bool TakeKey(uint zone){if(zone>=zones.length()||!zones[zone].locked||zones[zone].keyTaken)return false;zones[zone].keyTaken=true;return true;}
    bool Open(uint zone){if(zone>=zones.length()||zones[zone].opened||!HasKey(zone))return false;zones[zone].opened=true;return true;}
}
