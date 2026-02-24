#pragma once
namespace Application
{
	/// 温度計を管理するクラス
	class Thermometer final
	{
	public:
		/// 温度計の値を設定します。
		void SetTemperature(float temperature);
		/// 目標温度を設定します。
		void SetTargetTemperature(float targetTemperature) { targetTemperature_ = targetTemperature; }
		/// 外気温を設定します。
		void SetOutsideTemperature(float outsideTemperature) { outsideTemperature_ = outsideTemperature; }
		/// 温度の変化速度を設定します。
		void SetTemperatureChangeSpeed(float speed) { temperatureChangeSpeed_ = speed; }
		/// 温度の最大値を設定します。
		void SetMaxTemperature(float maxTemperature) { maxTemperature_ = maxTemperature; }

		/// 温度計の値を取得します。
		float GetTemperature() const { return temperature_; }
		/// 最大値に対する温度の割合を取得します。0.0fから1.0fの範囲で返します。
		float GetTemperatureRatio() const;
		/// 温度が最大値を超えているかどうかを返します。
		bool IsOverheated() const { return temperature_ >= maxTemperature_; }

		void AddTemperature(float delta) { SetTemperature(targetTemperature_ + delta); }

		/// 温度を更新
		void Update(float deltaTime);

	private:
		float temperature_; // 温度計の値
		float temperatureChangeSpeed_; // 温度の変化速度
		float targetTemperature_; // 目標温度
		float outsideTemperature_; // 外気温

		float maxTemperature_; // 温度の最大値
	};
}