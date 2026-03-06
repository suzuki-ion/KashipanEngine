#include "Thermometer.h"

void Application::Thermometer::SetTemperature(float temperature)
{
	temperature_ = temperature;
	// 目標温度と外気温も同じ値に初期化する
	targetTemperature_ = temperature;
	outsideTemperature_ = temperature;
}

float Application::Thermometer::GetTemperatureRatio() const {
	if (maxTemperature_ <= 0.0f) {
		return 0.0f; // 最大温度が0以下の場合は0を返す
	}
	return temperature_ / maxTemperature_; // 温度の割合を計算して返す
}

void Application::Thermometer::Update(float deltaTime)
{
	// 温度を目標温度に向かって変化させる
	if (temperature_ < targetTemperature_) {
		temperature_ += temperatureChangeSpeed_ * deltaTime;
		if (temperature_ > targetTemperature_) {
			temperature_ = targetTemperature_;
		}
	}
	else if (temperature_ > targetTemperature_) {
		temperature_ -= temperatureChangeSpeed_ * deltaTime;
		if (temperature_ < targetTemperature_) {
			temperature_ = targetTemperature_;
		}
	}

	// 目標温度を外気温に向かって変化させる
	if (targetTemperature_ < outsideTemperature_) {
		targetTemperature_ += temperatureChangeSpeed_ * deltaTime * 0.5f; // 目標温度の変化速度は温度の半分にする
		if (targetTemperature_ > outsideTemperature_) {
			targetTemperature_ = outsideTemperature_;
		}
	}
	else if (targetTemperature_ > outsideTemperature_) {
		targetTemperature_ -= temperatureChangeSpeed_ * deltaTime * 0.5f; // 目標温度の変化速度は温度の半分にする
		if (targetTemperature_ < outsideTemperature_) {
			targetTemperature_ = outsideTemperature_;
		}
	}
}
