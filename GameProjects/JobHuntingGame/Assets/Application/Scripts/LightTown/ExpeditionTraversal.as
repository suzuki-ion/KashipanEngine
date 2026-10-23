// 生成結果と同じ固定コライダーを使った、通常形態の保守的な到達判定。
class ExpeditionPlatform {
    Vector3 position, size;
    float angle = 0;
    bool ramp = false;
}
class ExpeditionTraversal {
    array<bool>@ solid;
    array<ExpeditionPlatform@>@ platforms = array<ExpeditionPlatform@>();
    array<Vector3>@ nodes = array<Vector3>();
    array<bool>@ reached = array<bool>();
    array<array<uint>@>@ platformRows=array<array<uint>@>();
    array<Vector3>@ halfBounds=array<Vector3>();
    uint width, height;
    float jump = 20, gravity = 48, speed = 6;
    float halfWidth = .51f, halfHeight = 1.01f;
    uint testedEdges = 0;
    bool Solid(int x,int y) {
        return x<0||y<0||x>=int(width)||y>=int(height)||solid[uint(y)*width+uint(x)];
    }
    bool Clear(const Vector3 &in p) {
        int x0=int(Floor(p.x-halfWidth)),x1=int(Floor(p.x+halfWidth));
        int y0=int(Floor(p.y-halfHeight)),y1=int(Floor(p.y+halfHeight));
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)if(Solid(x,y))return false;
        int row=int(Floor(p.y));if(row<0||row>=int(platformRows.length()))return false;
        for(uint j=0;j<platformRows[uint(row)].length();++j){uint i=platformRows[uint(row)][j];
            ExpeditionPlatform@ s=platforms[i];
            // 回転Boxは外接矩形で判定する。実形状より厳しくすることで偽の通過を避ける。
            float w=halfBounds[i].x,h=halfBounds[i].y;
            if(Abs(p.x-s.position.x)<w+halfWidth&&Abs(p.y-s.position.y)<h+halfHeight)return false;
        }
        return true;
    }
    bool Travel(const Vector3 &in a,const Vector3 &in b) {
        float dx=b.x-a.x,dy=b.y-a.y;
        if(Abs(dx)>7||dy>jump*jump/(2*gravity)||dy< -6)return false;
        if(!Clear(a)||!Clear(b))return false;
        if(dy<-.1f){
            // 下段へはジャンプだけでなく、縁まで歩いてから降りる経路も調べる。
            float direction=dx<0?-1.0f:1.0f;
            for(uint step=1;step<=30;++step){Vector3 edge=a+Vector3(direction*float(step)*.12f,0,0);
                if(!Clear(edge))break;
                if(!Clear(edge-Vector3(0,.12f,0)))continue;
                float time=Sqrt(-2*dy/gravity);if(Abs(b.x-edge.x)/time>speed)continue;
                bool drop=true;uint samples=uint(Ceil(time*120));
                for(uint k=1;k<=samples;++k){float t=time*float(k)/float(samples);
                    if(!Clear(Vector3(edge.x+(b.x-edge.x)*t/time,edge.y-.5f*gravity*t*t,0))){drop=false;break;}
                }
                if(drop)return true;
            }
        }
        if(Abs(dy)<.02f){
            bool walk=true;uint steps=uint(Ceil(Abs(dx)/.15f))+1;
            for(uint i=1;i<=steps;++i){Vector3 p=a+(b-a)*(float(i)/float(steps));if(!Clear(p)||Clear(p-Vector3(0,.12f,0))){walk=false;break;}}
            if(walk)return true;
        }
        float disc=jump*jump-2*gravity*dy;if(disc<0)return false;
        float duration=(jump+Sqrt(disc))/gravity;if(duration<=0||Abs(dx)/duration>speed)return false;
        uint samples=uint(Ceil(duration*120));
        float average=Abs(dx)/duration,boost=Min(average,speed-average),direction=dx<0?-1.0f:1.0f;
        // 一定速、先に横へ抜ける跳躍、上昇後に横へ進む跳躍を試す。
        for(int profile=-1;profile<=1;++profile){bool clear=true;
            for(uint i=1;i<samples;++i){float t=duration*float(i)/float(samples);
                float x=a.x+dx*t/duration+direction*float(profile)*boost*duration/3.14159265f*Sin(3.14159265f*t/duration);
                if(!Clear(Vector3(x,a.y+jump*t-.5f*gravity*t*t,0))){clear=false;break;}
            }
            if(clear)return true;
        }
        return false;
    }
    void BuildNodes() {
        platformRows.resize(height);for(uint y=0;y<height;++y)@platformRows[y]=array<uint>();
        halfBounds.resize(platforms.length());
        for(uint i=0;i<platforms.length();++i){ExpeditionPlatform@ s=platforms[i];
            float w=(Abs(Cos(s.angle))*s.size.x+Abs(Sin(s.angle))*s.size.y)*.5f;
            float h=(Abs(Sin(s.angle))*s.size.x+Abs(Cos(s.angle))*s.size.y)*.5f;
            halfBounds[i]=Vector3(w,h,0);
            for(int y=int(Floor(s.position.y-h-halfHeight));y<=int(Floor(s.position.y+h+halfHeight));++y)if(y>=0&&y<int(height))platformRows[uint(y)].insertLast(i);
        }
        nodes.resize(0);
        // 地形の立てる位置。1セル刻みで段差・着地面・頭上の隙間を確認。
        for(uint y=1;y<height-1;++y){int first=-1,last=-1;
            for(uint x=1;x<width;++x){Vector3 p(float(x)+.5f,float(y)+1.06f,0);
                bool stand=x<width-1&&Solid(int(x),int(y)-1)&&!Solid(int(x),int(y))&&Clear(p);
                if(stand){if(first<0)first=int(x);last=int(x);continue;}
                if(first>=0){for(int k=first;k<=last;k+=2)nodes.insertLast(Vector3(float(k)+.5f,float(y)+1.06f,0));
                    if((last-first)%2!=0)nodes.insertLast(Vector3(float(last)+.5f,float(y)+1.06f,0));first=-1;
                }
            }
        }
        for(uint i=0;i<platforms.length();++i){ExpeditionPlatform@ s=platforms[i];if(s.ramp)continue;
            // 高さ2mの体でも上段の床を避けて跳べるよう、足場の端寄りも出発点に含める。
            for(int side=-1;side<=1;++side){Vector3 p=s.position+Vector3(float(side)*(s.size.x*.5f-.15f),s.size.y*.5f+1.06f,0);if(Clear(p))nodes.insertLast(p);}
        }
    }
    bool Flood(const Vector3 &in start,bool reverse=false) {
        BuildNodes();reached.resize(nodes.length());for(uint i=0;i<reached.length();++i)reached[i]=false;
        array<uint> queue;testedEdges=0;
        for(uint i=0;i<nodes.length();++i)if(Abs(nodes[i].x-start.x)<7&&Abs(nodes[i].y-start.y)<.2f&&Travel(start,nodes[i])){reached[i]=true;queue.insertLast(i);}
        for(uint head=0;head<queue.length();++head){Vector3 a=nodes[queue[head]];
            for(uint j=0;j<nodes.length();++j){if(reached[j])continue;Vector3 b=nodes[j];
                if(Abs(a.x-b.x)>7||b.y-a.y>3.5f||b.y-a.y< -6)continue;
                ++testedEdges;if(reverse?Travel(b,a):Travel(a,b)){reached[j]=true;queue.insertLast(j);}
            }
        }
        return queue.length()>0;
    }
    bool Reachable(const Vector3 &in target) {
        for(uint i=0;i<nodes.length();++i)if(reached[i]&&Abs(nodes[i].x-target.x)<7&&Abs(nodes[i].y-target.y)<.2f&&Travel(nodes[i],target))return true;
        return false;
    }
}
