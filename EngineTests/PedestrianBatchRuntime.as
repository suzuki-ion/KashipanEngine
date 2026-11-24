// 分離した Release 実行環境の実際の CityController に組み込む。
void QAPedestrianBatch(){
    bool old=GetSettingBool("town.people",true);SetSettingBool("town.people",true);
    TownState local;@local.scene=GetScene();@local.rules=state.rules;local.NewGame();
    local.data.SetInt("wood",100000);local.data.SetInt("stone",100000);local.data.SetInt("crystal",100000);
    for(int x=4;x<=11;++x)QAAssert(local.Build(6,x,8,-1,false),"batch pedestrian road fixture");
    QAAssert(local.Build(0,5,9,0,false)&&local.Build(0,10,9,0,false),"batch pedestrian lighting fixture");
    QAAssert(local.Build(1,5,7,2,false)&&local.Build(1,10,7,2,false),"batch pedestrian homes fixture");
    QAAssert(local.Build(3,7,9,0,false)&&local.Build(2,8,9,0,false)&&local.Build(4,9,9,0,false),"batch pedestrian destinations fixture");
    TownWorld terrain;@terrain.state=local;terrain.arrivalDone=true;
    GetScene().SetGlobalVariable("LightTown.Pedestrians","");
    TownPedestrians people;people.Initialize(GetScene(),local,terrain);people.Update(0);
    QAAssert(people.walkers.length()==12&&people.bodies.GetWorldInstanceCount()==0&&people.heads.GetWorldInstanceCount()==0,"residents start indoors without fallback meshes");
    QAAssert(GetScene().GetObjects("TownPedestrian").length()==0,"no per-resident scene objects");
    bool entered=false,shop=false,factory=false,guild=false,spread=false;
    for(int frame=0;frame<1200;++frame){people.Update(.2f);uint visible=0;
        for(uint i=0;i<people.walkers.length();++i){TownWalker@ w=people.walkers[i];
            if(w.phase==3){entered=true;if(w.building==9*16+7)shop=true;if(w.building==9*16+8)factory=true;if(w.building==9*16+9)guild=true;}
            if(w.visible&&w.scale>0){
                QAAssert((people.bodies.GetWorldInstancePosition(visible)-(w.position+Vector3(0,.30f*w.scale,0))).Length()<.0001f,"batch body follows resident and entry scale");
                QAAssert((people.heads.GetWorldInstancePosition(visible)-(w.position+Vector3(0,.61f*w.scale,0))).Length()<.0001f,"batch head follows resident and entry scale");++visible;
            }
            if(i>0&&w.visible&&people.walkers[0].visible&&(w.position-people.walkers[0].position).Length()>.4f)spread=true;
        }
        QAAssert(people.bodies.GetWorldInstanceCount()==visible&&people.heads.GetWorldInstanceCount()==visible,"visible residents match both instance arrays");
    }
    QAAssert(entered&&shop&&factory&&guild&&spread,"batched residents enter and visit all destination types and spread out");
    SetSettingBool("town.people",false);people.Update(.1f);
    QAAssert(people.bodies.GetWorldInstanceCount()==0&&people.heads.GetWorldInstanceCount()==0,"display switch clears both batches");SetSettingBool("town.people",true);people.Update(.1f);
    people.Save();TownPedestrians resumed;resumed.Initialize(GetScene(),local,terrain);resumed.Update(0);
    QAAssert(resumed.walkers.length()==people.walkers.length(),"batch population restores");
    for(uint i=0;i<people.walkers.length();++i)QAAssert(resumed.walkers[i].phase==people.walkers[i].phase&&Abs(resumed.walkers[i].progress-people.walkers[i].progress)<.001f,"individual travel state restores with batch rendering");
    QAAssert(local.RemoveBuilding(10,7),"remove batched resident home");for(int f=0;f<1200;++f)people.Update(.2f);
    for(uint i=9;i<people.walkers.length();++i)QAAssert(!people.walkers[i].visible&&people.walkers[i].phase==0,"surplus residents retire into homes");
    for(int x=4;x<=11;++x)local.RemoveBuilding(x,8);people.Update(.1f);
    QAAssert(people.bodies.GetWorldInstanceCount()==0&&people.heads.GetWorldInstanceCount()==0,"removing roads empties batches");
    array<Vector3> p={Vector3(3,4,5)},s={Vector3(2,3,4)};array<Quaternion> q={Quaternion(0,0,0,1)};array<Vector4> c={Vector4(.2f,.4f,.6f,1)};
    QAAssert(people.bodies.SetWorldMeshInstances(p,q,s,c),"generic mesh submission accepted");p[0]=Vector3(9,9,9);
    QAAssert((people.bodies.GetWorldInstancePosition(0)-Vector3(3,4,5)).Length()<.0001f,"mesh API owns submitted arrays");
    s.resize(0);QAAssert(!people.bodies.SetWorldMeshInstances(p,q,s,c)&&people.bodies.GetWorldInstanceCount()==1,"invalid lengths preserve previous batch");
    s.insertLast(Vector3(1,1,1));q[0]=Quaternion(0,0,0,0);QAAssert(!people.bodies.SetWorldMeshInstances(p,q,s,c),"zero quaternion rejected");
    for(uint i=0;i<people.walkers.length();++i){people.Show(people.walkers[i],false);resumed.Show(resumed.walkers[i],false);}people.Submit();resumed.Submit();
    SetSettingBool("town.people",old);GetScene().SetGlobalVariable("LightTown.Pedestrians","");
    Json@ result=Json();result.SetBool("passed",true);SaveJsonFile("Results/PedestrianBatchRegression.json",result);
}
