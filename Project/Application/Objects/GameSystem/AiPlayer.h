#pragma once
namespace Application {
	class AiPlayer {
	public:
		void Initialize(int npc);
		void Update(float delta);

		bool GetIsSelecting() const { return isSelecting_; }
		bool GetIsMoveUp() const { return isMoveUp_; }
		bool GetIsMoveDown() const { return isMoveDown_; }
		bool GetIsMoveLeft() const { return isMoveLeft_; }
		bool GetIsMoveRight() const { return isMoveRight_; }
		bool GetIsSend() const { return isSend_; }

	private:
		int npcType_ = 0; // NPCのタイプを管理する変数
		float actionTimer_ = 0.0f; // 行動のタイマー
		float actionInterval_ = 1.0f; // 行動の間隔（例: 1秒ごとに行動する）

		bool isSelecting_ = false; 
		bool isMoveUp_ = false;
		bool isMoveDown_ = false;
		bool isMoveLeft_ = false;
		bool isMoveRight_ = false;
		bool isSend_ = false;
	};
}
