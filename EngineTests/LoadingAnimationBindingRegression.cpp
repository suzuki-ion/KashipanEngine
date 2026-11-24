// エンジンを起動せず、実際の Scene、コンポーネントのライフサイクル、パラメーターのバインドを検証する。
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/Transform.h"
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace KashipanEngine;
namespace {
int checks=0;
void Check(bool v,const char *message) { ++checks; if(!v)throw std::runtime_error(message); }
const auto componentKey=std::bit_cast<Passkey<EmptyObject>>(std::uint8_t{});
void Target(KeyFrameAnimator *animator,const std::string &id) {
    auto json=animator->SaveToJsonInterface(componentKey);
    json["customData"]["animations"][0]["bindings"][0]["targetObjectID"]=id;
    Check(animator->LoadFromJsonInterface(componentKey,json),"target binding JSON loads");
}
}
int main() {
    try {
        Scene scene(std::string("LoadingAnimationBindingRegression")); auto *context=scene.GetSceneContext();
        auto *owner=context->CreateEmptyObject("owner"); auto *child=context->CreateEmptyObject("child");
        child->GetComponent<Transform>()->SetParentObject(owner);
        auto *animator=owner->AddComponent<KeyFrameAnimator>();
        const JSON clip={{"keyframes",JSON::array({{{"time",0},{"value",0}},{{"time",1},{"value",100}}})}};
        Check(animator->AddAnimationData("move",clip),"embedded clip added");
        Check(animator->AddBinding("move","Transform","translate_",0),"legacy self binding added");
        Check(animator->SetCurrentTime("move",.5f) && owner->GetComponent<Transform>()->GetTranslate().x==50,"legacy self binding preserved");
        const auto childID=child->GetObjectID().ToString(); Target(animator,childID);
        Check(animator->SetCurrentTime("move",.25f) && child->GetComponent<Transform>()->GetTranslate().x==25,"real animator applies to another object");
        Check(owner->GetComponent<Transform>()->GetTranslate().x==50,"other-object application leaves owner unchanged");
        auto *clone=context->CloneObject(owner,"clone",true); auto *clonedAnimator=clone->GetComponent<KeyFrameAnimator>();
        const auto clonedTarget=clonedAnimator->GetBindings("move")->front().targetObjectID;
        Check(clonedTarget!=childID,"subtree clone remaps animated target UUID");
        auto *clonedChild=context->GetSceneObject(UUID128(clonedTarget)); Check(clonedChild!=nullptr,"cloned target exists");
        Check(clonedAnimator->SetCurrentTime("move",.75f) && clonedChild->GetComponent<Transform>()->GetTranslate().x==75,"cloned animator applies to cloned child");
        Check(child->GetComponent<Transform>()->GetTranslate().x==25,"cloned animation leaves original unchanged");
        auto *externalClone=context->CloneObject(owner,"external clone",false);
        Check(externalClone->GetComponent<KeyFrameAnimator>()->GetBindings("move")->front().targetObjectID==childID,"external references retained by single-object clone");
        Check(context->DeleteObject(child),"target deleted");
        Check(animator->SetCurrentTime("move",1),"deleted target safely ignored by animator");
        auto binding=animator->GetBindings("move")->front();
        Check(!ApplyParameterBinding(owner->GetObjectContext(),binding,9,animator),"missing UUID returns false");
        Target(animator,owner->GetObjectID().ToString());
        auto *selfClone=context->CloneObject(owner,"self clone",false);
        auto *selfAnimator=selfClone->GetComponent<KeyFrameAnimator>();
        Check(selfAnimator->GetBindings("move")->front().targetObjectID==selfClone->GetObjectID().ToString(),"explicit self UUID remapped");
        Check(selfAnimator->SetCurrentTime("move",.8f) && selfClone->GetComponent<Transform>()->GetTranslate().x==80,"explicit self clone receives values");
        std::cout<<checks<<" live binding and clone checks passed\n"; return 0;
    }catch(const std::exception &e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
