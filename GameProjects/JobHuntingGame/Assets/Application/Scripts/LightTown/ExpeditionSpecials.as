#include "ExpeditionSpecialLayout.as"
#include "ExpeditionObjects.as"

class ExpeditionAnnexView {
    Object@ gate;Object@ seal;Object@ key;
}
class ExpeditionSpecials {
    ExpeditionSpecialLayout@ layout=ExpeditionSpecialLayout();
    array<ExpeditionAnnexView@>@ views=array<ExpeditionAnnexView@>();
    float clock=0;
    Vector4 Color(uint i){return i==0?Vector4(1,.25f,.2f,1):Vector4(.2f,.55f,1,1);}
    string KeyName(uint i){return i==0?TownTr("赤い鍵","Red key"):TownTr("青い鍵","Blue key");}
    void Unlit(Object@ object){MeshRenderer@ r;object.GetComponent(@r);r.SetMaterialName("TownGridUnlit");r.SetCastShadows(false);}
    void Build(Scene@ scene){
        views.resize(0);
        for(uint i=0;i<layout.zones.length();++i){ExpeditionAnnex@ zone=layout.zones[i];ExpeditionAnnexView@ view=ExpeditionAnnexView();views.insertLast(view);
            Vector4 color=zone.locked?Color(i):Vector4(.3f,.8f,.4f,1);
            Object@ wall=TownMesh(scene,"SpecialRoomBackdrop","PrimitiveMesh-Box",Vector3(zone.room.CenterX(),float(zone.room.bottom)+4,4),Vector3(14,8,.3f),Vector4(.15f,.2f,.23f,1));
            if(zone.locked){
                @view.gate=ExpeditionFloor(scene,"LockedPassage",zone.door,Vector3(.6f,4,3),color);
                @view.seal=TownMesh(scene,"DoorKeySeal","PrimitiveMesh-Box",zone.door+Vector3(0,0,-1.6f),Vector3(.8f,.8f,.1f),color);Unlit(view.seal);
                @view.key=scene.CreateObject("ExpeditionKey");view.key.GetTransform().SetTranslate(zone.keyPosition);
                array<Vector3> offsets={Vector3(0,.3f,0),Vector3(0,-.15f,0),Vector3(.15f,-.42f,0)};
                array<Vector3> sizes={Vector3(.55f,.55f,.12f),Vector3(.12f,.6f,.12f),Vector3(.3f,.12f,.12f)};
                for(uint part=0;part<3;++part){Object@ object=TownMesh(scene,"ExpeditionKeyPart",part==0?"PrimitiveMesh-UVSphere":"PrimitiveMesh-Box",offsets[part],sizes[part],color);object.GetTransform().SetParentObject(view.key);Unlit(object);}
            }
            Light@ light=TownLight(scene,zone.Anchor()+Vector3(4,2,-1),color,2,"SpecialRoomLight");light.SetRadius(9);light.SetCastShadows(false);
            Object@ marker=TownMesh(scene,"ResourceRoomMarker","PrimitiveMesh-Octahedron",Vector3(zone.room.CenterX(),float(zone.room.bottom)+6,2),Vector3(.7f,.7f,.7f),color);Unlit(marker);
        }
    }
    string Update(const Vector3 &in player,float dt){
        clock+=dt;string message;
        for(uint i=0;i<layout.zones.length();++i){ExpeditionAnnex@ zone=layout.zones[i];if(!zone.locked)continue;
            ExpeditionAnnexView@ view=views[i];
            if(!zone.keyTaken){view.key.GetTransform().SetTranslate(zone.keyPosition+Vector3(0,Sin(clock*2)*.2f,0));view.key.GetTransform().SetRotate(Vector3(0,clock,0));
                if((player-zone.keyPosition).LengthSquared()<1.7f){layout.TakeKey(i);view.key.SetActive(false);message=KeyName(i)+TownTr("を入手しました。対応する扉に近づくと解錠します。"," acquired. Approach its matching gate to unlock it.");}
            }
            if(!zone.opened&&Abs(player.x-zone.door.x)<2&&Abs(player.y-(zone.door.y-1))<1.5f){
                if(layout.Open(i)){view.gate.SetActive(false);view.seal.SetActive(false);message=TownTr("解錠しました！奥は資材庫です。","Unlocked! The resource cache is inside.");}
                else message=KeyName(i)+TownTr("が必要です。通常区画を探索してください。"," required. Search the main area.");
            }
        }
        return message;
    }
    string Status(){return TownTr("赤鍵：","Red key: ")+(layout.HasKey(0)?TownTr("取得済み","found"):TownTr("未取得","missing"))+"  /  "+TownTr("青鍵：","Blue key: ")+(layout.HasKey(2)?TownTr("取得済み","found"):TownTr("未取得","missing"));}
    bool BlocksRay(const Vector3 &in p){for(uint i=0;i<layout.zones.length();++i){ExpeditionAnnex@ zone=layout.zones[i];if(zone.locked&&!zone.opened&&Abs(p.x-zone.door.x)<.3f&&Abs(p.y-zone.door.y)<2)return true;}return false;}
}
