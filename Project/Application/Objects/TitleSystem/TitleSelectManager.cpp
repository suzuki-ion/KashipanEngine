#include "TitleSelectManager.h"

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
	simultaneousSubmitGraceTime_ = 0.0f;
	modeSelectSubmitted_ = false;
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
	}
}

void Application::TitleSelectManager::UpdateModeSelectSection()
{
	modeSelectNumberManager_.Update();
	if (submitFunc_()) {
		if(modeSelectNumberManager_.GetSelectNumber() == 0) {
			currentSection_ = TitleSection::AISelect;
		}
		else {
			currentSection_ = TitleSection::MultiplayerSelect;
		}
	}
	else if (cancelFunc_()) {
		currentSection_ = TitleSection::TitleCall;
	}
}

void Application::TitleSelectManager::UpdateAISelectSection()
{
	aiSelectNumberManager_.Update();
	if (submitFunc_()) {
		modeSelectSubmitted_ = true;
	}
	else if (cancelFunc_()) {
		currentSection_ = TitleSection::ModeSelect;
	}
}

void Application::TitleSelectManager::UpdateMultiplayerSelectSection()
{
	// 同時押し判定の猶予時間がある場合は、両方の決定がされたか、どちらかが先に決定したかをチェック
	if(simultaneousSubmitGraceTime_ > 0.0f) {
		// 1Pと2Pの同時決定の猶予時間中は、両方の決定がされたか、どちらかが先に決定したかをチェック
		if (multiplayerSubmitFunc_() && submitFunc_()) {
			modeSelectSubmitted_ = true;
		}
		simultaneousSubmitGraceTime_ -= deltaTime_;
	}
	else {
		// 同時押し判定のクールダウン中は、どちらかが決定しても同時押し判定の猶予時間は開始せず、クールダウンが終わるまで待つ
		if (simultaneousSubmitCooldown_ > 0.0f) {
			simultaneousSubmitCooldown_ -= deltaTime_;
		}

		// 同時押し判定のクールダウンが終わっている状態で、どちらかが決定したら同時押し判定の猶予時間を開始
		if (submitFunc_() || multiplayerSubmitFunc_()) {
			if (simultaneousSubmitCooldown_ <= 0.0f) {
				simultaneousSubmitCooldown_ = 1.0f; // 同時押し判定のクールダウンを開始
				simultaneousSubmitGraceTime_ = 0.5f; // どちらかが決定したら、もう一方が同時に決定する猶予時間を開始
			}
		}
	}

	// 猶予時間中であっても、どちらかがキャンセルしたらモード選択に戻る
	if (multiplayerCancelFunc_() || cancelFunc_()) {
		simultaneousSubmitGraceTime_ = 0.0f;
		currentSection_ = TitleSection::ModeSelect;
	}
}
