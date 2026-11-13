#include "TownPresentation.as"
class TownTransition {
 Scene@ scene;Object@ overlay;float alpha=1;bool outgoing=false,finished=false,failed=false,blackReady=false;string loadingTarget;
 void Initialize(Scene@ owner){@scene=owner;@overlay=scene.CloneObject(scene.GetObject("TownFadeTemplate"),"TownSceneFade");overlay.SetActive(true);SpriteRenderer@ r;overlay.GetComponent(@r);r.SetMaterialName("TownUIOverlay");Layout();}
 void Layout(){Transform@ view=scene.GetObject("TownDisplayViewport").GetTransform();overlay.GetTransform().SetTranslate(view.GetTranslate());overlay.GetTransform().SetScale(view.GetScale());SpriteRenderer@ r;overlay.GetComponent(@r);r.SetInstanceColor(Vector4(0,0,0,alpha));}
 bool Begin(const string &in target=""){loadingTarget=target;outgoing=true;finished=false;blackReady=false;alpha=0;overlay.SetActive(true);return true;}
 bool Update(float dt){dt=TownClamp(dt,0,.05f);if(finished)return true;if(outgoing){alpha=Min(1.0f,alpha+dt/.35f);Layout();if(alpha>=1){if(!blackReady){blackReady=true;return true;}bool changed=loadingTarget.length()>0?scene.ChangeSceneWithLoading(loadingTarget,"LoadingScene"):scene.ChangeToNextScene();if(changed){finished=true;return true;}outgoing=false;failed=true;LogError("Scene transition failed");}return true;}alpha=Max(0.0f,alpha-dt/.35f);overlay.SetActive(alpha>0);if(alpha>0)Layout();return alpha>0;}
}
