#include "TownState.as"

class TownStages {
    Json@ entries;
    bool Load(){
        Json@ source=LoadJsonFile("Assets/Application/Data/ExpeditionStages.json");if(source is null)return false;
        @entries=source.GetJson("stages");if(entries is null||!entries.IsArray()||entries.Size()==0)return false;
        for(uint i=0;i<entries.Size();++i){Json@ stage=entries.At(i);if(stage.GetString("id")==""||stage.GetInt("woodWeight")<0||stage.GetInt("stoneWeight")<0||stage.GetInt("crystalWeight")<0||stage.GetInt("woodWeight")+stage.GetInt("stoneWeight")+stage.GetInt("crystalWeight")<=0)return false;for(uint j=0;j<i;++j)if(entries.At(j).GetString("id")==stage.GetString("id"))return false;}
        for(uint i=0;i<entries.Size();++i){Json@ stage=entries.At(i);string generation=stage.GetString("generation","grotto"),theme=stage.GetString("theme","grotto");if(generation!="forest"&&generation!="quarry"&&generation!="grotto")return false;if(theme!="forest"&&theme!="quarry"&&theme!="grotto")return false;}
        return true;
    }
    uint Count(){return entries is null?0:entries.Size();}
    int Index(const string &in id){for(uint i=0;i<Count();++i)if(entries.At(i).GetString("id")==id)return int(i);return 0;}
    Json@ At(int index){return index>=0&&index<int(Count())?entries.At(uint(index)):null;}
    string Name(Json@ stage){return TownTr(stage.GetString("name"),stage.GetString("nameEn"));}
    Vector3 Position(Json@ stage){return Vector3(float(stage.GetFloat("x")),0,float(stage.GetFloat("z")));}
    int ResourceKind(Json@ stage,uint room){
        // すべてのステージに各素材の採取部屋を 1 つずつ確保し、残りの部屋には出現傾向を持たせる。
        if(room<3)return int(room);
        uint wood=uint(stage.GetInt("woodWeight")),stone=uint(stage.GetInt("stoneWeight")),total=wood+stone+uint(stage.GetInt("crystalWeight"));
        uint roll=(room*37+uint(stage.GetInt("seedSalt")))%total;
        return roll<wood?0:(roll<wood+stone?1:2);
    }
}
