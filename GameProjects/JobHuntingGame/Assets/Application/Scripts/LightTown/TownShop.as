#include "TownState.as"
#include "TownPresentation.as"
#include "TownCityHUD.as"
class TownShopMenu {
    Scene@ scene;TownState@ state;TownUI@ ui=TownUI();bool visible=false;
    uint page=0,shown=0;array<TextRenderer@>@ prices=array<TextRenderer@>();
    TextRenderer@ wallet;TextRenderer@ status;array<TownResourceBurst@>@ deltas=array<TownResourceBurst@>();
    void Initialize(Scene@ owner,TownState@ data){@scene=owner;@state=data;Rebuild();Show(false);}
    void Rebuild(){
        for(uint i=0;i<ui.objects.length();++i)if(IsValidObject(ui.objects[i]))scene.DeleteObject(ui.objects[i]);
        @ui=TownUI();ui.Initialize(scene,"TownShopScreen","TownShopCamera","TownShopPanelTemplate");ui.layer=30;prices.resize(0);
        ui.Rect(640,360,1260,700,Vector4(.035f,.085f,.09f,.9f));ui.Text(TownTr("ショップ","SHOP"),70,50,34);ui.Text(TownTr("商品名","Product"),80,126,23);ui.Text(TownTr("価格","Price"),600,126,23);
        @wallet=ui.Text("",560,65,20);@status=ui.Text("",80,590,18);
        Json@ items=state.rules.GetJson("shopItems");shown=items.Size()>page*5?items.Size()-page*5:0;if(shown>5)shown=5;
        if(shown==0)ui.Text(TownTr("現在販売中の商品はありません。","No products are currently available."),80,200,23);
        for(uint i=0;i<shown;++i){Json@ item=items.At(page*5+i);float y=190+float(i)*75;
            ui.Text(TownBuildingName(item),80,y,25);
            prices.insertLast(ui.Text(TownTr("木 ","Wood ")+item.GetInt("wood")+" / "+TownTr("石 ","Stone ")+item.GetInt("stone")+" / "+TownTr("光結晶 ","Crystals ")+item.GetInt("crystal"),600,y,21));
            ui.Button(TownTr("購入","Buy"),1060,y);
        }
        ui.Button(TownTr("前へ","Previous"),80,640);ui.Button(TownTr("次へ","Next"),300,640);ui.Button(TownTr("閉じる [Esc]","Close [Esc]"),880,640);
        ui.Show(visible);
    }
    void Show(bool show){visible=show;ui.Show(show);scene.GetObject("TownShopScreen").SetActive(show);scene.GetObject("TownShopCamera").SetActive(show);scene.GetObject("TownShopBackground").SetActive(show);scene.GetObject("TownShopDisplay").SetActive(show);if(!show){for(uint i=0;i<deltas.length();++i)if(IsValidObject(deltas[i].object))scene.DeleteObject(deltas[i].object);deltas.resize(0);}}
    bool Open(){if(state.WorkingCount(3)==0){state.message=TownTr("稼働中のショップが必要です（道路＋街灯）。","An operational shop is required (road + streetlight).");return false;}Show(true);return true;}
    void Update(){
        if(!visible)return;int clicked=ui.Clicked();
        if(clicked>=0&&clicked<int(shown))Purchase(page*5+uint(clicked));
        wallet.SetText(TownTr("所持 木 ","Owned: Wood ")+state.data.GetInt("wood")+TownTr(" / 石 "," / Stone ")+state.data.GetInt("stone")+TownTr(" / 光結晶 "," / Crystals ")+state.data.GetInt("crystal"));status.SetText(state.message);
        for(int i=int(deltas.length())-1;i>=0;--i)if(TownAdvanceBurst(scene,deltas[uint(i)],GetDeltaTime()))deltas.removeAt(uint(i));
        for(uint i=0;i<shown;++i){Json@ item=state.rules.GetJson("shopItems").At(page*5+i);bool owned=item.GetString("action")=="transformation"&&state.data.GetBool("transformationUnlocked");ui.buttonTexts[i].SetText(owned?TownTr("購入済み","Purchased"):TownTr("購入","Buy"));}
        if(clicked==int(shown)&&page>0){--page;Rebuild();}
        if(clicked==int(shown)+1&&(page+1)*5<state.rules.GetJson("shopItems").Size()){++page;Rebuild();}
        if(clicked==int(shown)+2||IsCommandTriggered("Cancel"))Show(false);
    }
    bool Purchase(uint index){
        if(!state.BuyProduct(index))return false;
        Json@ item=state.rules.GetJson("shopItems").At(index);array<string> keys={"wood","stone","crystal"};
        for(uint i=0;i<keys.length();++i){int cost=int(item.GetInt(keys[i]));if(cost>0)deltas.insertLast(TownCreateBurst(scene,scene.GetObject("TownShopScreen"),-cost,650+float(i)*160,105));}return true;
    }
}
