#include "TownPresentation.as"
#include "TownState.as"

string TownBuildingMesh(int kind) {
    array<string> models = {"Streetlight", "House", "Workshop", "Shop", "Guild", "Tower", "Road", "Police"};
    return "Application/Models/Town/" + models[uint(kind)] + ".obj";
}
Object@ TownBuildingModel(Scene@ scene, int kind, const Vector3 &in position, const Vector4 &in color) {
    Object@ model = TownMesh(scene, "TownFacilityModel", TownBuildingMesh(kind), position, Vector3(1, 1, 1), color);
    // OBJの座標系変換後も、入口・庇・看板を街カメラ側（-Z）へ向ける。
    model.GetTransform().SetRotate(Vector3(0, 3.14159265f, 0));
    MeshRenderer@ renderer; model.GetComponent(@renderer); renderer.SetMaterialName("TownFacility");
    return model;
}

string TownRoadMesh(int mask,bool details=false){return "Application/Models/Town/Roads/Road"+(mask<10?"0":"")+mask+(details?"Details.obj":"Surface.obj");}

string TownVariantMesh(int kind,int tier,int recipe=0){if(kind==1&&tier>0)return "Application/Models/Town/"+(tier==1?"Apartment":"Highrise")+".obj";if(kind==3&&tier>0)return "Application/Models/Town/"+(tier==1?"Supermarket":"Mall")+".obj";if(kind==4&&tier>0)return "Application/Models/Town/"+(tier==1?"GuildMedium":"GuildLarge")+".obj";if(kind==2)return "Application/Models/Town/Factory"+recipe+"Tier"+tier+".obj";return TownBuildingMesh(kind);}
