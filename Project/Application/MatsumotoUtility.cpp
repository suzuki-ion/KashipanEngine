#include "MatsumotoUtility.h"

float Application::MatsumotoUtility::SimpleEaseIn(float from, float to, float transitionSpeed)
{
	float value = from;
	value += (to - value) * transitionSpeed;
	if (fabsf(value - to) <= 0.01f) {
		return to;
	}
	return value;
}
