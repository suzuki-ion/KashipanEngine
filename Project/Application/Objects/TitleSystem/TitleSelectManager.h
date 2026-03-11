#pragma once
#include <map>
#include "SelectNumderManager.h"
#include "TitleSection.h"

namespace Application {

	/// タイトル画面でのキャラクター選択やモード選択などの管理を行うクラス
	class TitleSelectManager {
	public:
		void Initialize(
			std::function<bool()> upNumberFunc,
			std::function<bool()> downNumberFunc,
			std::function<bool()> submitNumberFunc,
			std::function<bool()> cancelNumberFunc,
			std::function<bool()> multiplayerSubmitFunc,
			std::function<bool()> multiplayerCancelFunc);

		void Update(float delta);

		/// モード選択が完了して遷移すべきか
		bool GetModeSelectSubmitted() const { return modeSelectSubmitted_; }
		/// 選ばれているモードを取得する
		TitleSection GetCurrentSection() const { return currentSection_; }
		/// 現在のセクションで選ばれている数字を取得する
		int GetCurrentSelectNumber() const {
			if (currentSection_ == TitleSection::ModeSelect) {
				return modeSelectNumberManager_.GetSelectNumber();
			}
			else if (currentSection_ == TitleSection::AISelect) {
				return aiSelectNumberManager_.GetSelectNumber();
			}
			return 0;
		}

		/// 現在1pのコントローラの猶予時間を取得する
		float Get1PTriggerGraceTime() const { return triggered1PTimer_; }
		/// 現在2pのコントローラの猶予時間を取得する
		float Get2PTriggerGraceTime() const { return triggered2PTimer_; }

	private:
		float deltaTime_;

		// 現在のセクション
		TitleSection currentSection_;

		// モード選択で選ばれているモードの番号
		SelectNumderManager modeSelectNumberManager_;
		// AI選択で選ばれているAIの番号
		SelectNumderManager aiSelectNumberManager_;

		// セクションごとの処理
		std::map<TitleSection, std::function<void()>> sectionUpdateFunctions_;

		// 決定とキャンセルの関数（セクション共通で使用）
		std::function<bool()> submitFunc_;
		std::function<bool()> cancelFunc_;

		// マルチプレイ用の2Pの決定とキャンセルの関数（マルチプレイヤー選択セクションで使用）
		std::function<bool()> multiplayerSubmitFunc_;
		std::function<bool()> multiplayerCancelFunc_;
		
		// 遷移をさせるためのフラグ
		bool modeSelectSubmitted_;

		// 同時押し判定用のタイマー
		float triggered1PTimer_ = 0.0f;
		float triggered2PTimer_ = 0.0f;

		// 使用するSEハンドルのマップ
		std::map<std::string, uint32_t> seMap_;
		float seVolume_ = 0.9f;

	private:
		// タイトルコールセクションの処理
		void UpdateTitleCallSection();
		// モード選択セクションの処理
		void UpdateModeSelectSection();
		// AI選択セクションの処理
		void UpdateAISelectSection();
		 // マルチプレイヤー選択セクションの処理
		void UpdateMultiplayerSelectSection();

	};
}
