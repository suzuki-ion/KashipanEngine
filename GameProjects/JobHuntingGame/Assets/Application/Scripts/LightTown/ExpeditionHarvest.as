#include "TownPresentation.as"
#include "TownLanguage.as"

string ExpeditionResourceName(int kind){return kind==0?TownTr("木材","Wood"):(kind==1?TownTr("石材","Stone"):TownTr("光結晶","Crystal"));}
string ExpeditionResourceModel(int kind){return "Application/Models/Resources/"+(kind==0?"Timber":(kind==1?"Rock":"Crystal"))+".obj";}
int ExpeditionResourceHP(int kind){return kind==0?3:(kind==1?4:5);}
bool ExpeditionAttackRange(const Vector3 &in player,const Vector3 &in target,float facing){
    float x=(target.x-player.x)*facing;
    return x>=-.2f&&x<=2.1f&&Abs(target.y-player.y)<1.25f&&Abs(target.z-player.z)<1;
}
class ExpeditionResource {
    Object@ model;Object@ bar;Object@ back;
    int kind=0,hp=0,maxHP=0;float flash=0,barAge=0;
    Vector3 base;
    Vector4 color;
    void Initialize(Scene@ scene,int resource,const Vector3 &in floor) {
        kind=resource;hp=maxHP=ExpeditionResourceHP(kind);base=floor;
        color=kind==0?Vector4(.68f,.38f,.16f,1):(kind==1?Vector4(.62f,.66f,.7f,1):Vector4(.18f,.78f,1,1));
        @model=TownMesh(scene,"HarvestResource",ExpeditionResourceModel(kind),base,Vector3(1,1,1),color);
        @back=TownMesh(scene,"ResourceHPBackground","PrimitiveMesh-Box",base+Vector3(0,1.85f,-.9f),Vector3(1.5f,.14f,.04f),Vector4(.05f,.06f,.08f,1));
        @bar=TownMesh(scene,"ResourceHP","PrimitiveMesh-Box",base+Vector3(0,1.85f,-.93f),Vector3(1.4f,.08f,.04f),Vector4(.3f,1,.35f,1));
        array<Object@> markers={bar,back};for(uint i=0;i<markers.length();++i){MeshRenderer@ r;markers[i].GetComponent(@r);r.SetMaterialName("TownGridUnlit");r.SetCastShadows(false);markers[i].SetActive(false);}
    }
    bool Damage() {
        if(hp<=0)return false;--hp;flash=.16f;barAge=2.5f;return hp==0;
    }
    void Update(float dt) {
        if(hp<=0){model.SetActive(false);bar.SetActive(false);back.SetActive(false);return;}
        flash-=dt;barAge-=dt;MeshRenderer@ r;model.GetComponent(@r);r.SetInstanceColor(flash>0?Vector4(1,.65f,.3f,1):color);
        model.GetTransform().SetTranslate(base+Vector3(flash>0?Sin(flash*95)*.07f:0,0,0));
        bar.SetActive(barAge>0);back.SetActive(barAge>0);
        float fraction=float(hp)/float(maxHP);bar.GetTransform().SetScale(Vector3(1.4f*fraction,.08f,.04f));
        bar.GetTransform().SetTranslate(base+Vector3(-.7f*(1-fraction),1.85f,-.93f));
    }
}
