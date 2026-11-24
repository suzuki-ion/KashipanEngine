#include "ExpeditionGenerator.as"
#include "ExpeditionObjects.as"

// 装飾メッシュにはコライダーを付けない。衝突判定のある壁灯は生成処理で配置を決める。
class ExpeditionTheme {
    Scene@ scene;Json@ definition;string name="grotto";ExpeditionGenerator@ terrain;
    Vector4 ground=Vector4(1,1,1,1),background=Vector4(.2f,.25f,.3f,1);
    void Initialize(Scene@ owner,Json@ data){
        @scene=owner;@definition=data;name=data.GetString("theme","grotto");
        ground=data.GetVector4("groundColor",ground);background=data.GetVector4("backgroundColor",background);
        ScreenBufferObject@ screen;scene.GetObject("TownScreen").GetComponent(@screen);
        Vector4 clear=data.GetVector4("clearColor",Vector4(.03f,.04f,.07f,1));clear.w=1;screen.SetClearColor(clear);
        Object@ sun=scene.GetObject("SceneLight");Light@ light;
        if(sun !is null&&sun.GetComponent(@light)){light.SetColor(name=="forest"?Vector4(.82f,1,.78f,1):(name=="quarry"?Vector4(1,.88f,.7f,1):Vector4(.45f,.65f,1,1)));light.SetIntensity(name=="forest"?.4f:.24f);}
    }
    Object@ Decor(const string &in type,const string &in mesh,const Vector3 &in p,const Vector3 &in size,const Vector4 &in color,bool glowing=false){
        Object@ object=TownMesh(scene,"Theme"+type,mesh,p,size,color);MeshRenderer@ renderer;object.GetComponent(@renderer);renderer.SetCastShadows(false);
        if(glowing)renderer.SetMaterialName("TownGridUnlit");else if(name=="forest")renderer.SetMaterialName("TownFacility");return object;
    }
    void Glow(const Vector3 &in p,const Vector4 &in color,float power,float radius){Light@ light=TownLight(scene,p,color,power,"ExpeditionThemeLight");light.SetRadius(radius);light.SetCastShadows(false);}
    void Backdrop(float width,float height){
        Vector4 color=name=="grotto"?background:definition.GetVector4("clearColor",Vector4(.03f,.04f,.07f,1));
        Decor("Backdrop","PrimitiveMesh-Box",Vector3(width*.5f,height*.5f,12),Vector3(width+16,height+16,.2f),color,true);
        if(name=="grotto")for(int layer=0;layer<9;++layer){float y=float(layer)*7+2;Vector4 band=background;band.x*=.78f;band.y*=.8f;band.z*=.83f;Object@ seam=Decor("RockLayer","PrimitiveMesh-Box",Vector3(width*.5f,y,11),Vector3(width+10,.25f+float(layer%3)*.2f,.1f),band,true);seam.GetTransform().SetRotate(Vector3(0,0,layer%2==0?.025f:-.02f));}
        if(name=="forest"&&terrain !is null){
            for(int i=0;i<14;++i){float x=float(i)*(width+10)/13-5,size=18+terrain.CaveHash(i,111)*16;Tree(x,float(terrain.ForestFloor(int(x))),9.5f,size,true);}
            for(uint column=0;column<terrain.columns;++column){float x=float(column*16)+8.5f;Tree(x,float(terrain.ForestFloor(int(x))),3.5f,float(height)-18+terrain.CaveHash(int(column),113)*3,false);}
        }
    }
    void Tree(float x,float y,float z,float size,bool distant){
        string prefix=distant?"FarTree":"Jungle";float thickness=distant?.65f:1.15f;
        Decor(prefix+"Trunk","PrimitiveMesh-Cylinder-Closed",Vector3(x,y+size*.45f,z),Vector3(thickness,size*.9f,thickness),distant?Vector4(.12f,.23f,.19f,1):Vector4(.38f,.23f,.11f,1),distant);
        for(int crown=0;crown<5;++crown){float offset=float(crown-2)*1.9f,height=size*(crown%2==0?.88f:.98f);Vector4 leaves=distant?Vector4(.12f,.29f,.23f,1):Vector4(.18f+.015f*float(crown),.46f+.025f*float(crown%2),.15f,1);
            Decor(prefix+"Canopy","PrimitiveMesh-UVSphere",Vector3(x+offset,y+height,z),Vector3(distant?6:7,distant?4:4.8f,3),leaves,distant);
        }
        if(!distant)for(int side=-1;side<=1;side+=2){Object@ root=Decor("ButtressRoot","PrimitiveMesh-Box",Vector3(x+float(side)*.65f,y+.65f,z),Vector3(1.7f,.7f,1),Vector4(.38f,.23f,.11f,1));root.GetTransform().SetRotate(Vector3(0,0,float(side)*.7f));}
    }
    void Room(ExpeditionRoom@ room,uint index){
        float bottom=float(room.bottom),top=bottom+float(room.height),center=room.CenterX(),left=float(room.left),right=left+float(room.width);
        Vector4 wall=name=="forest"?definition.GetVector4("clearColor",background):background;
        if(name=="quarry")Decor("Wall","PrimitiveMesh-Box",Vector3(center,(bottom+top)*.5f,8),Vector3(float(room.width),float(room.height),.4f),wall,false);
        if(name=="forest"){
            bool groundLevel=terrain !is null&&index<terrain.columns;
            if(groundLevel){
                for(int i=0;i<7;++i){float x=left+float(i)*2,y=terrain is null?bottom:float(terrain.ForestFloor(int(x)));Decor("Grass","PrimitiveMesh-Box",Vector3(x,y+.05f,-1.6f),Vector3(1.7f,.16f,.35f),Vector4(.3f,.5f,.15f,1));
                    if(i%2==0){Decor("Fern","PrimitiveMesh-UVSphere",Vector3(x,y+.4f,2.5f),Vector3(1.5f,.8f,1),Vector4(.16f,.42f,.12f,1));Decor("Flower","PrimitiveMesh-UVSphere",Vector3(x+.4f,y+.65f,2.2f),Vector3(.2f,.2f,.2f),i%4==0?Vector4(1,.65f,.25f,1):Vector4(.9f,.3f,.5f,1));}
                }
            }else{
                for(int side=0;side<2;++side){float x=left+(side==0?1:10),length=3+float((index+uint(side))%4);
                    Decor("Vine","PrimitiveMesh-Box",Vector3(x,bottom-.5f-length*.5f,2.5f),Vector3(.09f,length,.09f),Vector4(.18f,.38f,.1f,1));
                    Decor("HangingLeaves","PrimitiveMesh-UVSphere",Vector3(x,bottom-.2f,2.7f),Vector3(3.4f,1.7f,2),Vector4(.18f,.46f,.14f,1));
                }
            }
            Glow(Vector3(center,bottom+4,-1),Vector4(1,.85f,.55f,1),2,10);
        }else if(name=="quarry"){
            if(terrain !is null&&terrain.CanPlaceFluorescent(room))ExpeditionFactoryLight(scene,Vector3(center,top-.5f,0),true);
            Vector4 metal(.4f,.48f,.55f,1);
            for(int side=0;side<2;++side)Decor("Support","PrimitiveMesh-Box",Vector3(side==0?left+.4f:right-.4f,(bottom+top)*.5f,3),Vector3(.28f,float(room.height),.4f),metal);
            Decor("Beam","PrimitiveMesh-Box",Vector3(center,top-.7f,3),Vector3(float(room.width),.35f,.5f),metal);
            Decor("Pipe","PrimitiveMesh-Cylinder-Closed",Vector3(right-1,bottom+2.5f,4),Vector3(.5f,5,.5f),Vector4(.55f,.33f,.17f,1));
            Decor("Machine","PrimitiveMesh-Box",Vector3(right-2,bottom+1.1f,3),Vector3(2,2.2f,1),Vector4(.55f,.4f,.18f,1));
            Decor("Rail","PrimitiveMesh-Box",Vector3(center,bottom+.1f,2),Vector3(float(room.width),.16f,.25f),metal);
        }else{
            for(int i=0;i<4;++i){float x=left+.6f+float(i)*(float(room.width)-1.2f)/3;
                float ceiling=top;if(terrain !is null){int y=int(bottom)+3;while(y<int(terrain.height)-1&&!terrain.IsSolid(int(x),y))++y;ceiling=float(y);}
                Object@ rock=Decor("Stalactite","PrimitiveMesh-Cone",Vector3(x,ceiling-.65f,3.4f),Vector3(1.3f,1.8f,1.3f),Vector4(.24f,.29f,.4f,1));rock.GetTransform().SetRotate(Vector3(0,0,3.14159265f));
                float crystalX=center+2-float(i)*.32f,floor=bottom;if(terrain !is null){int y=int(bottom)+3;while(y>0&&!terrain.IsSolid(int(crystalX),y-1))--y;floor=float(y);}
                Decor("Crystal","PrimitiveMesh-Octahedron",Vector3(crystalX,floor+.6f+float(i%2)*.35f,2.4f),Vector3(.4f,1.1f+float(i%2)*.8f,.4f),Vector4(.12f,.5f,.85f,1),true);
            }
            Decor("CaveRock","PrimitiveMesh-Octahedron",Vector3(left+.6f,bottom+1,4),Vector3(1.8f,2.2f,1),background);
            Glow(Vector3(center,bottom+3,-1),Vector4(.25f,.6f,1,1),3.5f,9);
        }
    }
    void Passage(ExpeditionLink@ link,ExpeditionRoom@ low,ExpeditionRoom@ high){
        Vector3 p=link.vertical?Vector3(link.shaftX+.5f,float(low.bottom+high.bottom)*.5f+3,0):Vector3((low.CenterX()+high.CenterX())*.5f,float(low.bottom+high.bottom)*.5f+3,0);
        if(name!="quarry")Glow(p+Vector3(0,0,1),name=="forest"?Vector4(1,.85f,.55f,1):Vector4(.25f,.6f,1,1),1.5f,7);
    }
}
