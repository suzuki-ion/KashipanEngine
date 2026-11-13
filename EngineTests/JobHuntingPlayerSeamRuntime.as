// Included only by an isolated QA copy of Player.as. Uses the real player update
// and native collision callbacks; the runner creates the matching floor course.
class PlayerSeamRuntimeRegression {
    float clock=0;
    int current=-1;
    bool done=false;
    array<float>@ positions=array<float>();array<float>@ heights=array<float>();
    void Update(Player@ player) {
        if(done||GetScene().GetObject("SeamA") is null)return;
        int next=int(clock/2.0f);clock+=GetDeltaTime();
        if(next!=current){
            if(current>=0){positions.insertLast(GetTransform().GetTranslate().x);heights.insertLast(GetTransform().GetTranslate().y);}
            current=next;
            if(next>=5){
                Json@ result=Json();
                result.SetFloat("wallY",heights[2]);result.SetFloat("ceilingY",heights[4]);result.SetFloat("right",positions[0]);result.SetFloat("left",positions[1]);result.SetFloat("wall",positions[2]);
                result.SetFloat("highStep",positions[3]);result.SetFloat("lowCeiling",positions[4]);
                result.SetBool("passed",positions[0]>1007.5f&&positions[1]<1002&&
                    positions[2]>1009&&positions[2]<=1009.51f&&
                    positions[3]>1101.4f&&positions[3]<1101.6f&&positions[4]>1201.4f&&positions[4]<1201.6f);
                SaveJsonFile("Results/SeamMovement.json",result);done=true;return;
            }
            float x=next==1?1008.5f:(next==2?1008:(next==3?1100.8f:(next==4?1200.8f:1000.8f)));
            GetTransform().SetTranslate(Vector3(x,11,0));
            player.movement.ResetVelocities();player.movement.velocity.x=next==1?-6:6;
            player.movement.hasGroundContact=true;player.movement.groundHitNormal=Vector3(0,1,0);
            player.movement.airborneTime=0;player.movement.state=PlayerGroundState::Grounded;
        }
        player.input.moveDirection=current==1?-1:1;player.input.jumpTriggered=false;player.input.attackTriggered=false;
    }
}
