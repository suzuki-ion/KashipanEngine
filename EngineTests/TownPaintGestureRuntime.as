    bool qaFake=false,qaGestureDone=false,qaEligible=true,qaShift=false,qaPress=false,qaHeld=false,qaRelease=false;
    int qaGestureFrame=0,qaX=2,qaZ=4;

    bool QACmd(const string &in command){
        if(!qaFake)return IsCommandTriggered(command);
        if(command=="TownBuildContinuous")return qaShift;
        if(command=="TownPlaceHold")return qaHeld;
        if(command=="TownPlacePress")return qaPress;
        if(command=="TownPlace")return qaRelease;
        return false;
    }
    void QABefore(){
        if(qaGestureDone||!ready||transition.alpha>0)return;
        if(!qaFake){
            qaFake=true;state.NewGame();QAFunds();
            selected=6;tier=0;recipe=0;selectedOption=0;buildMenu.Select(0);
            buildMenu.visible=true;buildMenu.amount=1;world.arrivalDone=true;
            strokeStarted=false;strokeBlocked=false;
        }
        qaEligible=true;qaShift=false;qaPress=false;qaHeld=true;qaRelease=false;
        cameraControls.manipulating=false;
        if(qaGestureFrame!=15)cameraControls.previousMouse.x+=1;
        if(qaGestureFrame==0){qaPress=true;qaX=2;qaZ=4;}
        if(qaGestureFrame==1)qaX=13;
        if(qaGestureFrame==2)qaEligible=false;
        if(qaGestureFrame==3){qaX=4;qaZ=6;}
        if(qaGestureFrame==4){qaX=6;cameraControls.manipulating=true;}
        if(qaGestureFrame==6||qaGestureFrame==8||qaGestureFrame==11||qaGestureFrame==13||qaGestureFrame==19){qaHeld=false;qaRelease=true;}
        if(qaGestureFrame==7)qaX=8;
        if(qaGestureFrame==9){
            state.NewGame();QAFunds();
            Json@ targets=Json();
            targets.Parse("[{\"kind\":6,\"x\":1,\"z\":8},{\"kind\":6,\"x\":2,\"z\":8},{\"kind\":6,\"x\":3,\"z\":8},{\"kind\":1,\"tier\":2,\"x\":4,\"z\":7},{\"kind\":4,\"x\":6,\"z\":8,\"dispatchActive\":true,\"dispatchRemaining\":45},{\"kind\":6,\"x\":7,\"z\":8},{\"kind\":6,\"x\":8,\"z\":8},{\"kind\":6,\"x\":10,\"z\":8}]");
            state.SetBuildings(targets);state.infrastructureDirty=true;state.SyncDispatchSummary();
            selected=8;selectedOption=int(buildMenu.catalog.length());buildMenu.Select(selectedOption);
            qaPress=true;qaX=1;qaZ=8;
        }
        if(qaGestureFrame==10)qaX=2;
        if(qaGestureFrame==12){qaPress=true;qaX=1;}
        if(qaGestureFrame==14){qaShift=true;qaPress=true;qaX=2;}
        if(qaGestureFrame==15){qaShift=true;qaX=3;}
        if(qaGestureFrame==16){qaShift=true;qaX=7;}
        if(qaGestureFrame==17)qaX=8;
        if(qaGestureFrame==18){qaShift=true;qaX=10;}
    }
    void QAAfter(){
        if(!qaFake)return;
        if(qaGestureFrame==0)QAAssert(state.IsRoad(2,4),"road paints on press without Shift");
        if(qaGestureFrame==1)for(int x=2;x<=13;++x)QAAssert(state.IsRoad(x,4),"fast cursor jump fills road cells");
        if(qaGestureFrame==3)QAAssert(state.IsRoad(4,6)&&!state.IsRoad(8,5),"UI gap resumes without bridging");
        if(qaGestureFrame==4)QAAssert(!state.IsRoad(6,6),"camera manipulation pauses painting");
        if(qaGestureFrame==5)QAAssert(state.IsRoad(6,6)&&!state.IsRoad(5,6),"held painting resumes after camera manipulation");
        if(qaGestureFrame==6)QAAssert(!strokeStarted,"release ends capture");
        if(qaGestureFrame==7)QAAssert(state.IsRoad(8,6),"held road paint recovers when a press edge was missed");
        if(qaGestureFrame==11)QAAssert(state.IsRoad(1,8)&&state.IsRoad(2,8),"unmodified drag does not erase");
        if(qaGestureFrame==13)QAAssert(!state.IsRoad(1,8)&&state.IsRoad(2,8),"unmodified click erases one cell");
        if(qaGestureFrame==14)QAAssert(!state.IsRoad(2,8),"Shift drag erases on press");
        if(qaGestureFrame==15)QAAssert(state.IsRoad(3,8),"stationary cursor does not erase the newly exposed cell");
        if(qaGestureFrame==16){
            QAAssert(!state.Occupied(3,8)&&!state.Occupied(4,7)&&!state.Occupied(5,8)&&!state.Occupied(7,8),"fast erase clears intermediate cells and the entire large facility");
            QAAssert(state.BuildingAt(6,8).GetBool("dispatchActive"),"active dispatch blocks erasing the guild");
        }
        if(qaGestureFrame==17)QAAssert(state.IsRoad(8,8),"releasing Shift pauses erasing");
        if(qaGestureFrame==18)QAAssert(state.IsRoad(8,8)&&!state.IsRoad(10,8),"resuming Shift erases without bridging the gap");
        if(qaGestureFrame==19){
            QAAssert(!strokeStarted,"release ends the erase stroke");
            Json@ saved=LoadJsonFile("SaveData/LightTown.json");
            QAAssert(saved.GetJson("buildings").Size()==2,"erased cells are saved");
            qaFake=false;qaGestureDone=true;QARules();
            Json@ result=Json();result.SetBool("passed",true);SaveJsonFile("Results/PaintGesture.json",result);
        }
        ++qaGestureFrame;
    }
