// 人型の表示だけを子オブジェクトで回転させる。物理側のBoxは直立を維持する。
string PlayerLocomotionClip(float speed,bool grounded,float vertical=0) {
    if(!grounded)return vertical>0?"Jump_Start":"Fall_Loop";
    if(Abs(speed)>=5)return "UAL_Sprint_Loop";
    return Abs(speed)>.15f?"UAL_Walk_Loop":"UAL_Idle_Loop";
}
class PlayerAnimation {
    Player@ owner;
    Object@ visual;
    Animator@ animator;
    SkinnedMeshRenderer@ skin;
    float facing=1;
    bool wasAttacking=false;
    string clip,attackClip="UAL_Punch_Cross";
    PlayerAnimation(Player@ player){@owner=player;}
    void Initialize(){
        @visual=owner.humanoidVisual;
        if(visual is null)return;
        visual.GetComponent(@animator);visual.GetComponent(@skin);
        // Awake時には参照先のコンポーネントが未生成の場合がある。初回Updateでも解決する。
        if(animator is null||skin is null)return;
        Select("UAL_Idle_Loop",true,1);
    }
    void Select(const string &in name,bool loop,float speed){
        if(animator is null)return;
        if(clip!=name){animator.SetClipName(name);clip=name;}
        animator.SetLoop(loop);animator.SetPlaybackSpeed(speed);animator.Play();
    }
    void Update(){
        if(visual is null||animator is null||skin is null)Initialize();
        if(visual is null||animator is null||skin is null)return;
        bool humanoid=owner.transformation.form==PlayerForm::Sphere;
        GetScene().SetVariable("Player.Humanoid",humanoid);
        visual.SetActive(humanoid);
        if(!humanoid){animator.Stop();wasAttacking=false;return;}
        float horizontal=owner.movement.velocity.x;
        bool attacking=owner.attack.Active();
        if(!attacking&&Abs(owner.input.moveDirection)>.01f)facing=owner.input.moveDirection<0?-1.0f:1.0f;
        if(attacking){
            facing=owner.attack.swingFacing;
            if(!wasAttacking){bool harvest=false;GetScene().GetVariable("Player.HarvestTarget",harvest);attackClip=harvest?"UAL_Sword_Attack":"UAL_Punch_Cross";}
            Select(attackClip,false,(attackClip=="UAL_Sword_Attack"?1.5f:1.0f)/.3f);
        }else{
            string next=PlayerLocomotionClip(horizontal,owner.movement.IsGrounded(),owner.movement.velocity.y);
            float speed=next=="UAL_Walk_Loop"?Clamp(Abs(horizontal)/3,.65f,1.4f):(next=="UAL_Sprint_Loop"?Clamp(Abs(horizontal)/6,.8f,1.5f):1);
            Select(next,next!="Jump_Start",speed);
        }
        wasAttacking=attacking;
        GetScene().SetVariable("Player.WeaponSwing",attacking&&attackClip=="UAL_Sword_Attack");
        // glTFの前方+Zは左手系への読み込みで-Zになる。
        visual.GetTransform().SetRotate(Vector3(0,facing>0?-1.5707963f:1.5707963f,0));
        bool flash=owner.damageFlash.flashTimer>0&&int(owner.damageFlash.flashTimer/owner.damageFlashInterval)%2==0;
        skin.SetInstanceColor(flash?owner.damageFlashColor:Vector4(1,1,1,1));skin.SetInstanceColorBlendMode(flash?0:1);
        GetScene().SetVariable("Player.AnimationClip",clip);
    }
}
