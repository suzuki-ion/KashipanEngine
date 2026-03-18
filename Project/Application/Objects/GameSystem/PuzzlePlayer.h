#pragma once
#include <functional>

namespace Application {
	class PuzzlePlayer {
	public:
		virtual ~PuzzlePlayer() = default;
		void Initialize();

		void SetMoveUpFunction(std::function<bool()> func) { moveUpFunction_ = func; }
		void SetMoveDownFunction(std::function<bool()> func) { moveDownFunction_ = func; }
		void SetMoveLeftFunction(std::function<bool()> func) { moveLeftFunction_ = func; }
		void SetMoveRightFunction(std::function<bool()> func) { moveRightFunction_ = func; }
		void SetSelectFunction(std::function<bool()> func) { selectFunction_ = func; }
		void SetSendFunction(std::function<bool()> func) { sendFunction_ = func; }

		bool IsMoveUp() const {
			return moveUpFunction_ ? moveUpFunction_() : false;
		}
		bool IsMoveDown() const {
			return moveDownFunction_ ? moveDownFunction_() : false;
		}
		bool IsMoveLeft() const {
			return moveLeftFunction_ ? moveLeftFunction_() : false;
		}
		bool IsMoveRight() const {
			return moveRightFunction_ ? moveRightFunction_() : false;
		}
		bool IsSelect() const {
			return selectFunction_ ? selectFunction_() : false;
		}
		bool IsSend() const {
			return sendFunction_ ? sendFunction_() : false;
		}

	private:
		std::function<bool()> moveUpFunction_;
		std::function<bool()> moveDownFunction_;
		std::function<bool()> moveLeftFunction_;
		std::function<bool()> moveRightFunction_;
		std::function<bool()> selectFunction_;
		std::function<bool()> sendFunction_;
	};
}