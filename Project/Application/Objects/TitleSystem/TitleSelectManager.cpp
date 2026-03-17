#include "TitleSelectManager.h"
#include <KashipanEngine.h>
#include <cassert>

void Application::TitleSelectManager::Initialize(std::function<bool()> upNumberFunc, std::function<bool()> downNumberFunc, std::function<bool()> submitNumberFunc, std::function<bool()> cancelNumberFunc, std::function<bool()> multiplayerSubmitFunc, std::function<bool()> multiplayerCancelFunc)
{
	// 最初はタイトルコールセクションから
	currentSection_ = TitleSection::TitleCall;
	modeSelectNumberManager_.Initialize(upNumberFunc, downNumberFunc, submitNumberFunc, cancelNumberFunc);
	aiSelectNumberManager_.Initialize(upNumberFunc, downNumberFunc, submitNumberFunc, cancelNumberFunc);
	modeSelectNumberManager_.Setup(2);
	aiSelectNumberManager_.Setup(3);
	sectionUpdateFunctions_[TitleSection::TitleCall] = [this]() { UpdateTitleCallSection(); };
	sectionUpdateFunctions_[TitleSection::ModeSelect] = [this]() { UpdateModeSelectSection(); };
	sectionUpdateFunctions_[TitleSection::AISelect] = [this]() { UpdateAISelectSection(); };
	sectionUpdateFunctions_[TitleSection::MultiplayerSelect] = [this]() { UpdateMultiplayerSelectSection(); };

	submitFunc_ = submitNumberFunc;
	cancelFunc_ = cancelNumberFunc;
	multiplayerSubmitFunc_ = multiplayerSubmitFunc;
	multiplayerCancelFunc_ = multiplayerCancelFunc;

	deltaTime_ = 0.016f;
	modeSelectSubmitted_ = false;

	// SEのハンドルを取得してマップに保存
	seMap_["SelectChange"] = KashipanEngine::AudioManager::GetSoundHandleFromFileName("menuSelect.mp3");
	seMap_["SelectDecide"] = KashipanEngine::AudioManager::GetSoundHandleFromFileName("menuDecide.mp3");
	seMap_["SelectCancel"] = KashipanEngine::AudioManager::GetSoundHandleFromFileName("cursorCancel.mp3");
	for(auto& [key, handle] : seMap_) {
		if (handle == KashipanEngine::AudioManager::kInvalidSoundHandle) {
			assert(false && "faild load SE");
		}
	}
	seVolume_ = 0.9f;

}
void Application::TitleSelectManager::Update(float delta)
{
	if (modeSelectSubmitted_) {
		return;
	}

	deltaTime_ = delta;

	// 現在のセクションに対応する処理を呼び出す
	auto it = sectionUpdateFunctions_.find(currentSection_);
	if (it != sectionUpdateFunctions_.end()) {
		it->second();
	}
}

void Application::TitleSelectManager::UpdateTitleCallSection()
{
	if (submitFunc_()) {
		currentSection_ = TitleSection::ModeSelect;
		KashipanEngine::AudioManager::Play(seMap_["SelectDecide"], seVolume_);
	}
}

void Application::TitleSelectManager::UpdateModeSelectSection()
{
	modeSelectNumberManager_.Update();
	if(modeSelectNumberManager_.IsMoving()) {
		KashipanEngine::AudioManager::Play(seMap_["SelectChange"], seVolume_);
	}

	if (submitFunc_()) {
		if (modeSelectNumberManager_.GetSelectNumber() == 0) {
			currentSection_ = TitleSection::AISelect;
		}
		else {
			currentSection_ = TitleSection::MultiplayerSelect;
		}
		KashipanEngine::AudioManager::Play(seMap_["SelectDecide"], seVolume_);
	}
	else if (cancelFunc_()) {
		currentSection_ = TitleSection::TitleCall;
		KashipanEngine::AudioManager::Play(seMap_["SelectCancel"], seVolume_);
	}
}

void Application::TitleSelectManager::UpdateAISelectSection()
{
	aiSelectNumberManager_.Update();
	if (aiSelectNumberManager_.IsMoving()) {
		KashipanEngine::AudioManager::Play(seMap_["SelectChange"], seVolume_);
	}

	if (submitFunc_()) {
		modeSelectSubmitted_ = true;
		KashipanEngine::AudioManager::Play(seMap_["SelectDecide"], seVolume_);
	}
	else if (cancelFunc_()) {
		currentSection_ = TitleSection::ModeSelect;
		KashipanEngine::AudioManager::Play(seMap_["SelectCancel"], seVolume_);
	}
}

void Application::TitleSelectManager::UpdateMultiplayerSelectSection()
{
	// 同時押し猶予時間の設定
	float simultaneousSubmitGraceTime = 0.2f;

	// 入力があった場合、猶予時間を設定
	if (submitFunc_()) {
		if (triggered1PTimer_ <= 0.0f) {
			triggered1PTimer_ = simultaneousSubmitGraceTime;
			KashipanEngine::AudioManager::Play(seMap_["SelectChange"], seVolume_,-1.0f);
		}
	}
	if (multiplayerSubmitFunc_()) {
		if (triggered2PTimer_ <= 0.0f) {
			triggered2PTimer_ = simultaneousSubmitGraceTime;
			KashipanEngine::AudioManager::Play(seMap_["SelectChange"], seVolume_, 1.0f);
		}
	}

	// どちらかの猶予時間が残っている場合、遷移の条件を満たす
	if (triggered1PTimer_ > 0.0f && triggered2PTimer_ > 0.0f) {
		// これが遷移するトリガー
		modeSelectSubmitted_ = true;

		KashipanEngine::AudioManager::Play(seMap_["SelectDecide"], seVolume_);
	}

	// 1pと2pの猶予時間を更新
	if (triggered1PTimer_ > 0.0f) {
		triggered1PTimer_ -= deltaTime_;
	}
	if (triggered2PTimer_ > 0.0f) {
		triggered2PTimer_ -= deltaTime_;
	}

	// 猶予時間中であっても、どちらかがキャンセルしたらモード選択に戻る
	if (multiplayerCancelFunc_() || cancelFunc_()) {
		triggered1PTimer_ = 0.0f;
		triggered2PTimer_ = 0.0f;

		currentSection_ = TitleSection::ModeSelect;
		KashipanEngine::AudioManager::Play(seMap_["SelectCancel"], seVolume_);
	}
}
