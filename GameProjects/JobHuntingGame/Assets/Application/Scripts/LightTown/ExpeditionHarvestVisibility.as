#include "ExpeditionTraversal.as"

// 遠征中の地形の足場は変化しないため、回転と空間分割を先に一度だけ計算する。
// 動くゲートはコントローラーが別途判定する。
class ExpeditionHarvestObstacle {
    Vector3 position, halfSize;
    float cosine=1, sine=0;
    bool Contains(const Vector3 &in p) const {
        Vector3 d=p-position;
        return Abs(d.x*cosine+d.y*sine)<halfSize.x && Abs(-d.x*sine+d.y*cosine)<halfSize.y;
    }
}
class ExpeditionHarvestVisibility {
    array<ExpeditionHarvestObstacle@>@ obstacles=array<ExpeditionHarvestObstacle@>();
    array<array<uint>@>@ buckets=array<array<uint>@>();
    int columns=0,rows=0;
    void Build(const array<ExpeditionPlatform@> &in platforms,uint width,uint height){
        columns=int(width/8)+1;rows=int(height/8)+1;
        obstacles.resize(0);buckets.resize(uint(columns*rows));
        for(uint i=0;i<buckets.length();++i)@buckets[i]=array<uint>();
        for(uint i=0;i<platforms.length();++i){
            ExpeditionPlatform@ p=platforms[i];ExpeditionHarvestObstacle@ o=ExpeditionHarvestObstacle();
            o.position=p.position;o.halfSize=p.size*.5f;o.cosine=Cos(p.angle);o.sine=Sin(p.angle);
            float x=Abs(o.cosine)*o.halfSize.x+Abs(o.sine)*o.halfSize.y;
            float y=Abs(o.sine)*o.halfSize.x+Abs(o.cosine)*o.halfSize.y;
            obstacles.insertLast(o);
            int x0=Max(0,int(Floor((p.position.x-x)/8))),x1=Min(columns-1,int(Floor((p.position.x+x)/8)));
            int y0=Max(0,int(Floor((p.position.y-y)/8))),y1=Min(rows-1,int(Floor((p.position.y+y)/8)));
            for(int by=y0;by<=y1;++by)for(int bx=x0;bx<=x1;++bx)buckets[uint(by*columns+bx)].insertLast(i);
        }
    }
    bool Blocked(const Vector3 &in p) const {
        int x=int(Floor(p.x/8)),y=int(Floor(p.y/8));
        if(x<0||y<0||x>=columns||y>=rows)return false;
        const array<uint>@ list=buckets[uint(y*columns+x)];
        for(uint i=0;i<list.length();++i)if(obstacles[list[i]].Contains(p))return true;
        return false;
    }
}
