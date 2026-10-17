#include "Objects/Components/ShortTextExtended.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Collider/CharacterController2D.h"
#include "Objects/Components/Collider/CharacterController3D.h"
#include "Objects/Components/Collider/RigidBody2D.h"
#include "Objects/Components/Collider/RigidBody3D.h"
#include "Objects/Components/Render/TilemapRenderer.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/UI/UIButton.h"
#include "Objects/Collision/CollisionQuery.h"
#include "Input/Input.h"
#include "Input/Mouse.h"
#include "Input/Controller.h"
#include "Input/InputCommand.h"
#include <queue>
#include <numbers>

namespace KashipanEngine::ShortText {
namespace {
Value Vec(const Vector3 &v) { return Value::array({v.x,v.y,v.z}); }
Vector3 Vector(const Value &v) {
    if (!v.is_array() || v.size()<2 || v.size()>3) throw std::runtime_error("vector requires 2 or 3 numbers");
    Vector3 r{v.at(0).get<float>(),v.at(1).get<float>(),v.size()==3?v.at(2).get<float>():0};
    if (!IsFinite(r)||std::abs(r.x)>1e7f||std::abs(r.y)>1e7f||std::abs(r.z)>1e7f) throw std::runtime_error("vector must be finite and bounded");
    return r;
}
float Number(const Value &a,const char *key,float def) {
    float v=a.value(key,def); if (!std::isfinite(v) || std::abs(v)>1e7f) throw std::runtime_error("invalid numeric argument"); return v;
}
EmptyObject *Resolve(SceneContext *s,const std::string &name) {
    if (!s) return nullptr; EmptyObject *result=nullptr;
    for (auto *o:s->GetSceneObjects()) {
        if (o->GetObjectID().ToString()==name) return o;
        if (o->GetName()==name) { if (result) return nullptr; result=o; }
    } return result;
}
void StoreQueryResult(CommandContext &c,const Value &a,const Value &v) {
    if (c.memory) c.memory->extensionValues["results"][a.value("result",std::string("physics"))]=v;
}
void Move(EmptyObject *o,const Vector3 &delta,bool controller) {
    if(controller) {
        if(auto *p=o->GetComponent<CharacterController2D>()) { p->Move({delta.x,delta.y}); return; }
        if(auto *p=o->GetComponent<CharacterController3D>()) { p->Move(delta); return; }
        throw std::runtime_error("CharacterController2D/3D required");
    }
    auto *t=o->GetComponent<Transform>(); if(!t) throw std::runtime_error("Transform required");
    t->SetTranslate(t->GetTranslate()+delta);
}
Value State(const EmptyObject *o) {
    Value r=Value::object();
    if(auto *p=o->GetComponent<CharacterController2D>()) {
        auto n=p->GetGroundNormal(); r={{"grounded",p->IsGrounded()},{"wall",p->IsTouchingWall()},{"ceiling",p->IsTouchingCeiling()},
          {"normal",Value::array({n.x,n.y,0})},{"shape_supported",p->WasLastMoveShapeSupported()}};
    } else if(auto *controller3=o->GetComponent<CharacterController3D>()) r={{"grounded",controller3->IsGrounded()},{"wall",controller3->IsTouchingWall()},{"ceiling",controller3->IsTouchingCeiling()},{"normal",Vec(controller3->GetGroundNormal())},{"shape_supported",controller3->WasLastMoveShapeSupported()}};
    if(auto *p=o->GetComponent<RigidBody2D>()) {auto v=p->GetVelocity();r["velocity"]=Value::array({v.x,v.y,0});r["mass"]=p->GetMass();r["gravity"]=p->IsGravityEnabled();}
    if(auto *p=o->GetComponent<RigidBody3D>()) {r["velocity"]=Vec(p->GetVelocity());r["mass"]=p->GetMass();r["gravity"]=p->IsGravityEnabled();}
    return r;
}
template<class Hits> Value HitsJson(const Hits &hits) {
    Value result=Value::array(); for (const auto &h:hits) if(h.isHit) {
        if(result.size()>=128) break;
        result.push_back({{"id",h.otherObject?h.otherObject->GetObjectID().ToString():""},{"normal",Vec(h.normal)},{"depth",h.penetration}});
    } return result;
}
}

void ReadPhysicsValues(const EmptyObject *owner,SceneContext *scene,ProgramState &state,ProgramMemory *memory,double dt) {
    if(!owner) return;
    state.environment["physics"]=State(owner);
    auto *input=scene?scene->GetInput():nullptr; if(!input) return;
    auto &m=input->GetMouse(); auto &out=state.environment["input"];
    const auto screen=m.GetPos(static_cast<HWND>(nullptr));
    out["mouse_screen"]=Value::array({screen.x,screen.y});
    out["scroll"]=m.GetWheel(); out["delta"]=Value::array({m.GetDeltaX(),m.GetDeltaY()});
    if(memory) {
        auto &hold=memory->extensionValues["mouse_hold"]; if(!hold.is_array()||hold.size()!=3) hold=Value::array({0.0,0.0,0.0});
        for(int i=0;i<3;++i) { double t=m.IsButtonDown(i)?hold[i].get<double>()+std::clamp(dt,0.0,1.0):0;hold[i]=t;out["hold_seconds"][std::to_string(i)]=t; }
    }
    if(auto *b=owner->GetComponent<UIButton>()) {
        out["hover"]=b->IsHovered();out["drag"]=b->IsPressed()&&(m.GetDeltaX()!=0||m.GetDeltaY()!=0);
        Vector2 uv; if(b->TryGetLocalHoverPosition(uv)) out["ui_uv"]=Value::array({uv.x,uv.y});
    }
    auto &pads=input->GetController();
    for(int i=0;i<4;++i) {
        auto &pad=out["gamepad"][std::to_string(i)];pad["connected"]=pads.IsConnected(i);
        pad["left_stick"]=Value::array({pads.GetLeftStickX(i)/32767.0f,pads.GetLeftStickY(i)/32767.0f});
        pad["right_stick"]=Value::array({pads.GetRightStickX(i)/32767.0f,pads.GetRightStickY(i)/32767.0f});
        pad["left_trigger"]=pads.GetLeftTrigger(i)/255.0f;pad["right_trigger"]=pads.GetRightTrigger(i)/255.0f;
        for(const auto &name:{"A","B","X","Y","LeftShoulder","RightShoulder","Start","Back","DPadUp","DPadDown","DPadLeft","DPadRight"}) {
            auto button=InputCommand::StringToControllerButton(name);
            pad["buttons"][name]={{"held",pads.IsButtonDown(button,i)},{"pressed",pads.IsButtonTrigger(button,i)},{"released",pads.IsButtonRelease(button,i)}};
        }
    }
    if(auto *commands=scene->GetInputCommand()) for(const auto &name:commands->GetRegisteredCommandNames()) {
        auto v=commands->Evaluate(name);out["commands"][name]={{"triggered",v.Triggered()},{"value",v.Value()}};
    }
}

bool ExecutePhysicsCommand(const std::string &command,const Value &a,CommandContext &c,std::string &error) {
    const std::unordered_set<std::string> known={"controller_move","physics_state","rigid_force","rigid_impulse","rigid_velocity","rigid_settings","collision_query","raycast","look_at","accelerated_move","waypoint_move","grid_path","mouse_world","input_command","gamepad_vibration"};
    if(!known.contains(command)) return false;
    try {
        if(!a.is_object()) throw std::runtime_error("arguments must be object");
        if(!c.target) throw std::runtime_error("target required");
        if(!std::isfinite(c.dt)||c.dt<0||c.dt>1)throw std::runtime_error("frame dt must be 0..1 seconds");
        if(command=="physics_state") {StoreQueryResult(c,a,State(c.target));return true;}
        if(command=="controller_move") {auto v=Vector(a.at("velocity"));Move(c.target,v*static_cast<float>(c.dt),true);return true;}
        if(command=="rigid_settings") {
            auto *p2=c.target->GetComponent<RigidBody2D>();auto *p3=c.target->GetComponent<RigidBody3D>();
            if(!p2&&!p3) throw std::runtime_error("RigidBody2D/3D required");
            if(a.contains("mass")) {float m=Number(a,"mass",1);if(m<=0) throw std::runtime_error("mass must be positive");if(p2)p2->SetMass(m);if(p3)p3->SetMass(m);}
            if(a.contains("gravity")) {bool g=a.at("gravity").get<bool>();if(p2)p2->SetUseGravity(g);if(p3)p3->SetUseGravity(g);}
            if(a.contains("sleep")) {bool sleep=a.at("sleep").get<bool>();if(p2){auto id=p2->GetRigidBodyId();if(!b2Body_IsValid(id))throw std::runtime_error("2D body is not initialized");b2Body_SetAwake(id,!sleep);}if(p3){auto *b=p3->GetRigidBody();if(!b)throw std::runtime_error("3D body is not initialized");b->setIsSleeping(sleep);}}
            return true;
        }
        if(command=="rigid_force"||command=="rigid_impulse"||command=="rigid_velocity") {
            auto v=Vector(a.at("value"));
            if(auto *p=c.target->GetComponent<RigidBody2D>()) {
                auto id=p->GetRigidBodyId();if(!b2Body_IsValid(id))throw std::runtime_error("2D body is not initialized");
                if(command=="rigid_force")b2Body_ApplyForceToCenter(id,{v.x,v.y},true);
                else if(command=="rigid_impulse")b2Body_ApplyLinearImpulseToCenter(id,{v.x,v.y},true);
                else p->SetVelocity({v.x,v.y});
            } else if(auto *body3=c.target->GetComponent<RigidBody3D>()) {
                auto *b=body3->GetRigidBody();if(!b)throw std::runtime_error("3D body is not initialized");
                if(command=="rigid_force")b->applyWorldForceAtCenterOfMass({v.x,v.y,v.z});
                else if(command=="rigid_impulse") {float mass=body3->GetMass();if(mass<=0)throw std::runtime_error("mass must be positive");body3->SetVelocity(body3->GetVelocity()+v/mass);}
                else body3->SetVelocity(v);
            } else throw std::runtime_error("RigidBody2D/3D required");return true;
        }
        if(command=="collision_query"||command=="raycast") {
            auto *coll=c.scene?c.scene->GetComponent<SceneObjectCollider>():nullptr;if(!coll)throw std::runtime_error("SceneObjectCollider required");
            int dim=a.value("dimension",2);auto position=Vector(a.value("center",Value::array({0,0,0})));
            float radius=Number(a,"radius",0.5f);if(radius<0)throw std::runtime_error("radius must be nonnegative");
            if(dim==2) {
                CollisionShape2D shape;shape.center={position.x,position.y};shape.radius=radius;
                std::string type=a.value("shape",std::string("circle"));
                if(command=="raycast"||type=="segment") {shape.type=CollisionShapeType2D::Segment;auto start=Vector(a.at("start")),end=Vector(a.at("end"));shape.start={start.x,start.y};shape.end={end.x,end.y};}
                else if(type=="box") {shape.type=CollisionShapeType2D::Box;auto size=Vector(a.at("half_size"));shape.halfSize={size.x,size.y};shape.rotation=Number(a,"rotation",0);}
                else if(type=="capsule") {shape.type=CollisionShapeType2D::Capsule;auto start=Vector(a.at("start")),end=Vector(a.at("end"));shape.start={start.x,start.y};shape.end={end.x,end.y};}
                else if(type=="point")shape.type=CollisionShapeType2D::Point;
                else if(type!="circle")throw std::runtime_error("unknown 2D query shape");
                ColliderInfo2D query;query.shape=shape.Build();if(!IsValidQueryShape(query.shape))throw std::runtime_error("invalid shape");
                StoreQueryResult(c,a,HitsJson(coll->Query2D(query,a.value("include_triggers",false),c.target)));return true;
            }
            if(dim!=3)throw std::runtime_error("dimension must be 2 or 3");
            if(command=="raycast") {
                // Exact RP3D raycast, registered 3D bodies only. Sort by fraction.
                struct Callback final:reactphysics3d::RaycastCallback {
                    std::vector<std::pair<float,Value>> hits;SceneObjectCollider *collider;const EmptyObject *ignore;bool triggers;
                    explicit Callback(SceneObjectCollider *v,const EmptyObject *o,bool t):collider(v),ignore(o),triggers(t){}
                    reactphysics3d::decimal notifyRaycastHit(const reactphysics3d::RaycastInfo &h) override {
                        const auto *info=collider->GetCollider()->FindInfoByHandle3D(h.collider);EmptyObject *object=info?info->ownerObject:nullptr;
                        if(object==ignore||(!triggers&&h.collider->getIsTrigger()))return 1;
                        hits.push_back({h.hitFraction,{{"id",object?object->GetObjectID().ToString():""},{"normal",Value::array({h.worldNormal.x,h.worldNormal.y,h.worldNormal.z})},{"point",Value::array({h.worldPoint.x,h.worldPoint.y,h.worldPoint.z})},{"fraction",h.hitFraction}}});return 1;
                    }
                } cb(coll,c.target,a.value("include_triggers",false));
                auto *world=coll->GetCollider()->GetPhysicsWorld();if(!world)throw std::runtime_error("3D world is not initialized");
                auto start=Vector(a.at("start")),end=Vector(a.at("end"));auto delta=end-start;if(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z<1e-12f)throw std::runtime_error("ray start/end must differ");world->raycast(reactphysics3d::Ray({start.x,start.y,start.z},{end.x,end.y,end.z}),&cb);
                std::sort(cb.hits.begin(),cb.hits.end(),[](auto &x,auto &y){return x.first<y.first;});Value hits=Value::array();for(auto &h:cb.hits)if(hits.size()<128)hits.push_back(h.second);StoreQueryResult(c,a,hits);return true;
            }
            CollisionShape3D shape;shape.center=position;shape.radius=radius;std::string type=a.value("shape",std::string("sphere"));
            if(radius<=0)throw std::runtime_error("3D query radius must be positive");
            if(type=="box"){shape.type=CollisionShapeType3D::Box;shape.halfExtents=Vector(a.at("half_size"));if(shape.halfExtents.x<=0||shape.halfExtents.y<=0||shape.halfExtents.z<=0)throw std::runtime_error("half_size must be positive");}
            else if(type=="capsule"){shape.type=CollisionShapeType3D::Capsule;shape.height=Number(a,"height",1);if(shape.height<=0)throw std::runtime_error("height must be positive");}
            else if(type!="sphere")throw std::runtime_error("unknown 3D query shape");
            StoreQueryResult(c,a,HitsJson(coll->Query3D(shape.Build(),position,Quaternion::Identity(),a.value("include_triggers",false),c.target)));return true;
        }
        if(command=="look_at") {
            auto *t=c.target->GetComponent<Transform>();if(!t)throw std::runtime_error("Transform required");
            Vector3 goal;if(a.contains("position"))goal=Vector(a.at("position"));else {auto *ref=Resolve(c.scene,a.at("reference").get<std::string>());auto *rt=ref?ref->GetComponent<Transform>():nullptr;if(!rt)throw std::runtime_error("reference Transform required");goal=rt->GetWorldPosition();}
            auto delta=goal-t->GetWorldPosition();
            if(auto *parent=t->GetParentObject())if(auto *pt=parent->GetComponent<Transform>())delta=pt->GetWorldRotateQuaternion().Inverse().RotateVector(delta);
            if(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z<1e-12f)return true;
            auto r=t->GetRotate();
            if(a.value("dimension",2)==2)r.z=std::atan2(delta.y,delta.x)+Number(a,"offset_degrees",0)*std::numbers::pi_v<float>/180;
            else {r.y=std::atan2(delta.x,delta.z);r.x=-std::atan2(delta.y,std::sqrt(delta.x*delta.x+delta.z*delta.z));}
            t->SetRotate(r);return true;
        }
        if(command=="accelerated_move"||command=="waypoint_move") {
            if(!c.memory)throw std::runtime_error("program memory required");auto *t=c.target->GetComponent<Transform>();if(!t)throw std::runtime_error("Transform required");
            auto &m=c.memory->extensionValues["motion"][c.target->GetObjectID().ToString()+":"+a.value("slot",std::string("default"))];
            if(!m.is_object())m=Value::object();
            Vector3 goal;size_t index=m.value("index",size_t(0));
            if(command=="waypoint_move") {auto points=a.at("points");if(!points.is_array()||points.empty()||points.size()>256)throw std::runtime_error("1..256 waypoints required");if(index>=points.size())index=0;goal=Vector(points[index]);}
            else goal=Vector(a.at("position"));
            auto delta=goal-t->GetTranslate();float distance=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
            float tolerance=Number(a,"tolerance",0.01f),speed=Number(a,"speed",5),accel=Number(a,"acceleration",20);if(tolerance<0||speed<=0||accel<=0)throw std::runtime_error("invalid motion bounds");
            bool arrived=distance<=tolerance;float current=m.value("speed",0.0f);
            float desired=std::min(speed,std::sqrt(2*accel*distance));current=std::clamp(desired,current-accel*static_cast<float>(c.dt),current+accel*static_cast<float>(c.dt));
            if(!arrived&&distance>1e-6f) {float amount=std::min(distance,current*static_cast<float>(c.dt));Move(c.target,delta*(amount/distance),a.value("controller",false));arrived=distance-amount<=tolerance;}
            m["speed"]=arrived?0:current;
            if(arrived&&command=="waypoint_move") {size_t count=a.at("points").size();m["index"]=(index+1<count)?index+1:(a.value("loop",true)?0:index);}
            auto direction=distance>1e-6f?delta/distance:Vector3{0,0,0};
            StoreQueryResult(c,a,{{"arrived",arrived},{"waypoint",index},{"speed",arrived?0:current},{"direction",Vec(direction)},{"velocity",Vec(direction*(arrived?0:current))}});return true;
        }
        if(command=="grid_path") {
            // Bounded four-neighbour BFS on explicit grid. Tilemap solid cells or world-space collider probes block cells.
            int width=a.value("width",32),height=a.value("height",32);if(width<1||height<1||width>128||height>128)throw std::runtime_error("grid dimensions must be 1..128");
            auto start=Vector(a.at("start")),goal=Vector(a.at("goal")),origin=Vector(a.value("origin",Value::array({0,0,0})));float cell=Number(a,"cell_size",1);if(cell<=0)throw std::runtime_error("cell_size must be positive");
            auto coordinate=[&](float position,float offset,int extent){double index=std::floor((static_cast<double>(position)-offset)/cell);if(!std::isfinite(index)||index<0||index>=extent)throw std::runtime_error("start/goal outside grid");return static_cast<int>(index);};
            int sx=coordinate(start.x,origin.x,width),sy=coordinate(start.y,origin.y,height),gx=coordinate(goal.x,origin.x,width),gy=coordinate(goal.y,origin.y,height);
            auto inside=[&](int x,int y){return x>=0&&y>=0&&x<width&&y<height;};if(!inside(sx,sy)||!inside(gx,gy))throw std::runtime_error("start/goal outside grid");
            auto *mapObject=Resolve(c.scene,a.value("tilemap",std::string()));auto *map=mapObject?mapObject->GetComponent<TilemapRenderer>():nullptr;
            if(!a.value("tilemap",std::string()).empty()&&!map)throw std::runtime_error("specified TilemapRenderer not found");
            auto *coll=c.scene?c.scene->GetComponent<SceneObjectCollider>():nullptr;bool avoid=a.value("avoid_colliders",false);if(avoid&&!coll)throw std::runtime_error("SceneObjectCollider required for avoidance");
            std::vector<bool> blocked(width*height);if(a.contains("blocked"))for(auto &p:a.at("blocked")){auto v=Vector(p);int x=static_cast<int>(v.x),y=static_cast<int>(v.y);if(inside(x,y))blocked[y*width+x]=true;}
            for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
                if(map)blocked[y*width+x]=blocked[y*width+x]||map->GetTileTypeSolid(map->GetTile(x,y));
                if(avoid) {float radius=Number(a,"agent_radius",cell*0.25f);if(radius<0)throw std::runtime_error("agent_radius must be nonnegative");ColliderInfo2D q;q.shape=Math::Circle{{origin.x+(x+0.5f)*cell,origin.y+(y+0.5f)*cell},radius};blocked[y*width+x]=blocked[y*width+x]||!coll->Query2D(q,false,c.target).empty();}
            }
            std::vector<int> prev(width*height,-1);std::queue<int> queue;int begin=sy*width+sx,end=gy*width+gx;if(!blocked[end]){queue.push(begin);prev[begin]=begin;}
            while(!queue.empty()&&prev[end]<0){int id=queue.front();queue.pop();int x=id%width,y=id/width;for(auto [dx,dy]:std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){int nx=x+dx,ny=y+dy;if(!inside(nx,ny))continue;int next=ny*width+nx;if(!blocked[next]&&prev[next]<0){prev[next]=id;queue.push(next);}}}
            Value points=Value::array();if(prev[end]>=0){std::vector<int> path;for(int id=end;;id=prev[id]){path.push_back(id);if(id==begin)break;}std::reverse(path.begin(),path.end());for(int id:path)points.push_back(Value::array({origin.x+(id%width+0.5f)*cell,origin.y+(id/width+0.5f)*cell,origin.z}));}
            StoreQueryResult(c,a,{{"found",!points.empty()},{"points",points}});return true;
        }
        if(command=="mouse_world") {
            auto *camera=Resolve(c.scene,a.at("camera").get<std::string>());auto *cam=camera?camera->GetComponent<Camera2D>():nullptr;auto *tr=camera?camera->GetComponent<Transform>():nullptr;if(!cam||!tr)throw std::runtime_error("Camera2D/Transform required");
            auto uv=Vector(a.at("uv"));auto world=Vector3{uv.x*cam->GetWidth(),uv.y*cam->GetHeight(),Number(a,"depth",0)}.Transform(tr->GetWorldMatrix());StoreQueryResult(c,a,Vec(world));return true;
        }
        if(command=="input_command") {auto *input=c.scene?c.scene->GetInputCommand():nullptr;if(!input)throw std::runtime_error("InputCommand unavailable");auto name=a.at("name").get<std::string>();auto names=input->GetRegisteredCommandNames();if(std::find(names.begin(),names.end(),name)==names.end())throw std::runtime_error("InputCommand not registered");auto v=input->Evaluate(name);StoreQueryResult(c,a,{{"triggered",v.Triggered()},{"value",v.Value()}});return true;}
        if(command=="gamepad_vibration") {auto *input=c.scene?c.scene->GetInput():nullptr;if(!input)throw std::runtime_error("Input unavailable");int index=a.value("index",0);if(index<0||index>3)throw std::runtime_error("controller index 0..3 required");input->GetController().SetVibration(index,std::clamp(a.value("left",0),0,65535),std::clamp(a.value("right",0),0,65535));return true;}
    } catch(const std::exception &e) {error=command+": "+e.what();return false;}
    error=command+": command not completed";return false;
}
}

