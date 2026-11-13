        if(current<0){
            Vector3 oldMin=player.minVelocity,oldMax=player.maxVelocity;player.minVelocity=Vector3(-200,-200,-200);player.maxVelocity=Vector3(200,200,200);
            player.movement.ResetVelocities();GetTransform().SetTranslate(Vector3(1008,11,0));player.movement.velocity=Vector3(200,0,0);player.movement.ApplyVelocity(.1f);float wallX=GetTransform().GetTranslate().x;
            player.movement.ResetVelocities();GetTransform().SetTranslate(Vector3(1300,14,0));player.movement.velocity=Vector3(0,-200,0);player.movement.ApplyVelocity(.1f);float floorY=GetTransform().GetTranslate().y;
            player.movement.ResetVelocities();GetTransform().SetTranslate(Vector3(1200.8f,11,0));player.movement.velocity=Vector3(0,200,0);player.movement.isJumping=true;player.movement.ApplyVelocity(.1f);float ceilingY=GetTransform().GetTranslate().y;player.movement.isJumping=false;
            player.movement.ResetVelocities();GetTransform().SetTranslate(Vector3(1397,10.4f,0));for(int step=0;step<20;++step){player.movement.velocity=Vector3(6,-4,0);player.movement.ApplyVelocity(.05f);}Vector3 slope=GetTransform().GetTranslate();
            player.minVelocity=oldMin;player.maxVelocity=oldMax;Json@ report=Json();report.SetFloat("wallX",wallX);report.SetFloat("thinFloorY",floorY);report.SetFloat("ceilingY",ceilingY);report.SetVector3("slope",slope);report.SetBool("passed",wallX>1009&&wallX<=1009.51f&&floorY>=11.02f&&floorY<11.04f&&ceilingY>=10.99f&&ceilingY<=11.01f&&slope.x>1402.8f&&slope.y>11.5f&&slope.y<13);SaveJsonFile("Results/CollisionSubsteps.json",report);
        }
