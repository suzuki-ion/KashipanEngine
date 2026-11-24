// 分離した QA アプリケーション内でのみ、実際の CityController に組み込む。
    bool qaRulesPassed=false;
    void QAAssert(bool ok,const string &in what){if(ok)return;Json@ result=Json();result.SetBool("passed",false);result.SetString("error",what);SaveJsonFile("Results/TownExpansionRules.json",result);throw(what);}
    void QAFunds(){state.AddResources(100000,100000,100000);state.data.SetInt("planks",100000);state.data.SetInt("masonry",100000);state.data.SetInt("refinedCrystal",100000);state.data.SetFloat("energy",1000000);}
    void QARules(){
        for(int r=0;r<4;++r){Vector3 front=Math::MakeRotateEuler(Vector3(0,TownModelYaw(0,r),0)).RotateVector(Vector3(1,0,0));QAAssert((front-Vector3(float(TownFrontX(r)),0,float(TownFrontZ(r)))).Length()<.001f,"streetlight arm agrees with entrance arrow");}
        QAAssert(buildMenu.catalog.length()==22,"22 categorized building choices");
        QAAssert(settings.ui.buttons.length()==11&&settings.rowLabels.length()==11,"settings controls initialized");
        state.NewGame();QAFunds();state.data.SetFloat("energy",1000000000);state.Build(6,2,3);state.Build(0,2,2,2);state.Produce();
        QAAssert(state.data.GetFloat("energy")>1000000000,"light reserve grows beyond old cap and float integer precision");state.Save();TownState loaded;QAAssert(loaded.Initialize(GetScene())&&loaded.data.GetFloat("energy")==state.data.GetFloat("energy"),"unlimited reserve persists");
        int wood=int(state.data.GetInt("wood")),stone=int(state.data.GetInt("stone")),crystal=int(state.data.GetInt("crystal"));for(int i=0;i<20;++i){state.Produce();state.Event();}
        QAAssert(state.data.GetInt("wood")==wood&&state.data.GetInt("stone")==stone&&state.data.GetInt("crystal")==crystal,"passive production and events cannot create raw materials");
        state.NewGame();QAFunds();state.Build(6,5,7);state.Build(0,4,7,1);double safety=state.data.GetFloat("safety");
        QAAssert(state.Build(1,5,5,2,true,2),"large house construction");QAAssert(state.BuildingAt(6,6).GetInt("x")==5&&state.Occupied(5,6)&&state.Occupied(6,5),"all four cells share the same building");
        QAAssert(state.IsWorking(6,6),"large footprint entrance activation");QAAssert(state.data.GetFloat("safety")<safety&&state.Residents(state.BuildingAt(5,5))==30,"large housing population and safety cost");
        QAAssert(!state.Build(6,6,6)&&!state.Build(1,15,15,0,true,2),"footprint overlap and map bounds rejected");
        QAAssert(state.RemoveBuilding(6,6)&&!state.Occupied(5,5)&&!state.Occupied(6,6),"delete via any occupied cell clears the footprint");
        safety=state.data.GetFloat("safety");state.Build(7,7,7);QAAssert(state.data.GetFloat("safety")==safety+18,"police grant more safety than streetlights");
        state.NewGame();QAFunds();state.Build(1,5,5);double happiness=state.data.GetFloat("happiness");state.Build(2,6,5,0,true,0,0);
        QAAssert(state.data.GetFloat("happiness")<happiness&&state.placedParameterDeltas[0]<0,"nearby factory penalizes happiness once and reports the delta");double polluted=state.data.GetFloat("happiness");state.Produce();QAAssert(state.data.GetFloat("happiness")==polluted,"pollution does not compound each production tick");state.RemoveBuilding(6,5);QAAssert(state.data.GetFloat("happiness")==happiness,"removing a factory restores its neighborhood effect");
        state.NewGame();state.data.SetFloat("happiness",1);Json@ dense=Json();dense.Parse("[{\"kind\":1,\"x\":5,\"z\":5},{\"kind\":2,\"x\":6,\"z\":5,\"tier\":2}]");state.SetBuildings(dense);state.ReconcilePollution();QAAssert(state.data.GetFloat("happiness")==0,"pollution clips at zero");dense.Parse("[{\"kind\":1,\"x\":5,\"z\":5}]");state.SetBuildings(dense);state.ReconcilePollution();QAAssert(state.data.GetFloat("happiness")==1,"removing pollution only restores actual prior loss");
        state.NewGame();QAFunds();for(int x=2;x<14;++x)state.Build(6,x,8);state.Build(0,3,7,2);
        for(int r=0;r<3;++r)QAAssert(state.Build(2,4+r,7,2,true,0,r),"three dedicated processing recipes");
        wood=int(state.data.GetInt("wood"));stone=int(state.data.GetInt("stone"));crystal=int(state.data.GetInt("crystal"));int planks=int(state.data.GetInt("planks"));state.Produce();
        QAAssert(state.data.GetInt("wood")==wood-2&&state.data.GetInt("stone")==stone-2&&state.data.GetInt("crystal")==crystal-2,"processing consumes matching raw materials");
        QAAssert(state.data.GetInt("planks")==planks+1,"processed output produced");state.data.SetInt("wood",0);planks=int(state.data.GetInt("planks"));state.Produce();QAAssert(state.data.GetInt("planks")==planks,"missing raw input stops only its recipe");
        state.NewGame();QAFunds();for(int x=2;x<14;++x)state.Build(6,x,8);state.Build(0,3,9,0);state.Build(0,7,7,2);
        state.Build(4,3,7,2);QAAssert(state.Build(4,8,6,2,true,2),"large guild built");
        state.DispatchAll();QAAssert(state.data.GetInt("dispatchCount")==2,"bulk dispatch starts both guilds");
        QAAssert(state.BuildingAt(3,7).GetFloat("dispatchDuration")==state.BuildingAt(9,7).GetFloat("dispatchDuration"),"guild tiers use identical duration");
        wood=int(state.data.GetInt("wood"));state.Tick(45);QAAssert(state.data.GetInt("wood")==wood+20*9,"guild rewards scale by 1 and 8 without changing duration");
        QAAssert(TownFireflyCount(64)==8&&TownFireflyCount(16384)==2048&&TownFireflyCount(1000000000)==2048,"8 reserve light per visible firefly, fixed render cap");
        fireflies.Update(0,0);fireflies.Update(0,8);float radius=fireflies.commonLight.GetRadius(),intensity=fireflies.intensityScale;fireflies.Update(1,16384);QAAssert(fireflies.commonLight.GetRadius()>radius&&fireflies.intensityScale>intensity,"reserve increases light range and intensity");
        // 描画と操作を確認するため、さまざまな施設が混在する街を作る。
        state.NewGame();QAFunds();for(int x=2;x<14;++x)state.Build(6,x,8);for(int z=2;z<13;++z)state.Build(6,12,z);
        state.Build(0,3,7,2);state.Build(0,7,7,2);state.Build(0,11,7,2);state.Build(1,3,9,0);state.Build(1,5,9,0,true,1);state.Build(1,8,9,0,true,2);state.Build(2,4,7,2,true,0,0);state.Build(2,5,7,2,true,1,1);state.Build(2,8,6,2,true,2,2);state.Build(7,10,7,2);state.Build(3,11,9,0);state.Build(4,2,7,2);
        world.Rebuild();world.arrivalDone=true;for(uint i=0;i<world.roots.length();++i)world.roots[i].GetTransform().SetTranslate(Vector3());
        bool oldPeople=GetSettingBool("town.people",true),oldFireflies=GetSettingBool("town.fireflies",true),oldSimple=GetSettingBool("town.simpleModels",false);int oldBars=GetSettingInt("town.bars",0);
        SetSettingBool("town.people",false);pedestrians.Update(.1f);for(uint i=0;i<pedestrians.walkers.length();++i)QAAssert(!pedestrians.walkers[i].visible,"people visibility switch hides batched residents");SetSettingBool("town.people",oldPeople);
        SetSettingBool("town.fireflies",false);fireflies.Update(0,100000);QAAssert(fireflies.batchLights.GetPointInstanceCount()==0&&fireflies.batchMarkers.GetWorldInstanceCount()==0,"floating light switch clears GPU batches without spending reserve");SetSettingBool("town.fireflies",oldFireflies);
        SetSettingBool("town.simpleModels",true);world.Rebuild();MeshFilter@ simple;world.models[0].GetComponent(@simple);QAAssert(simple.GetMeshHandle()==GetModelHandleFromAssetPath("PrimitiveMesh-Box"),"simple building mode uses box mesh");SetSettingBool("town.simpleModels",oldSimple);world.Rebuild();
        indicators.Rebuild();SetSettingInt("town.bars",1);indicators.Update(.1f,false,false);for(uint i=0;i<indicators.bars.length();++i)if(indicators.bars[i].visible)QAAssert(!indicators.bars[i].ui.objects[0].IsActive()&&!indicators.bars[i].fill.IsActive(),"seconds-only bars hide geometry");SetSettingInt("town.bars",2);indicators.Update(0,false,false);for(uint i=0;i<indicators.bars.length();++i)QAAssert(!indicators.bars[i].visible,"hidden bars do not render");SetSettingInt("town.bars",oldBars);
        TownApplySettings(GetScene());buildMenu.Select(5);selectedOption=5;selected=2;tier=0;recipe=0;buildMenu.SetVisible(true);
        Json@ result=Json();result.SetBool("passed",true);result.SetInt("choices",int(buildMenu.catalog.length()));SaveJsonFile("Results/TownExpansionRules.json",result);qaRulesPassed=true;
    }


