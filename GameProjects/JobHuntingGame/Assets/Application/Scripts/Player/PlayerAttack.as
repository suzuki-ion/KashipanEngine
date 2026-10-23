// 押下1回につき一振り。命中通知は振り始めから0.09秒後に一度だけ発行する。
class PlayerAttack {
    float facing=1, swingFacing=1, cooldown=0, age=1;
    bool emitted=false;
    int sequence=0;
    bool Update(float dt,float movement,bool pressed) {
        if(dt<=0)return false;
        cooldown-=dt;
        if(age>=.3f&&Abs(movement)>.01f)facing=movement<0?-1.0f:1.0f;
        if(pressed&&cooldown<=0){age=0;cooldown=.45f;emitted=false;swingFacing=facing;}
        age+=dt;
        if(age>=.09f&&age<1&&!emitted){emitted=true;++sequence;return true;}
        return false;
    }
    bool Active(){return age<.3f;}
    void Reset(){age=1;cooldown=0;emitted=true;}
}
