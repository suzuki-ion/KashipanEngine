#include "Easings.h"
#include <cmath>
#include <algorithm>
#include <cassert>

static constexpr float PI = 3.14159265358979323846f;

float Normalize01(const float &value, const float &min, const float &max) {
	if (std::abs(max - min) < 1e-6f) {
		return 0.0f;
	}
    return std::clamp((value - min) / (max - min), 0.0f, 1.0f);
}

Vector2 Normalize01(const Vector2 &value, const Vector2 &min, const Vector2 &max) {
	return Vector2(
		Normalize01(value.x, min.x, max.x),
		Normalize01(value.y, min.y, max.y)
	);
}

Vector3 Normalize01(const Vector3 &value, const Vector3 &min, const Vector3 &max) {
	return Vector3(
		Normalize01(value.x, min.x, max.x),
		Normalize01(value.y, min.y, max.y),
		Normalize01(value.z, min.z, max.z)
	);
}

Vector4 Normalize01(const Vector4 &value, const Vector4 &min, const Vector4 &max) {
	return Vector4(
		Normalize01(value.x, min.x, max.x),
		Normalize01(value.y, min.y, max.y),
		Normalize01(value.z, min.z, max.z),
		Normalize01(value.w, min.w, max.w)
	);
}

float Lerp(const float& start, const float& end, const float& t) {
	//return (1.0f - t) * start + t * end;
	return start + t * (end - start);
}

Vector2 Lerp(const Vector2 &start, const Vector2 &end, const float &t) {
    return Vector2(start.x + t * (end.x - start.x), start.y + t * (end.y - start.y));
}

Vector3 Lerp(const Vector3& start, const Vector3& end, const float& t) {
	return Vector3(start.x + t * (end.x - start.x), start.y + t * (end.y - start.y) ,start.z + t * (end.z - start.z));
}

Vector4 Lerp(const Vector4& start, const Vector4& end, const float& t) {
	return Vector4(start.x + t * (end.x - start.x), start.y + t * (end.y - start.y), start.z + t * (end.z - start.z), start.w + t * (end.w - start.w));
}

float EaseIn(const float& t) {
	return t * t;
}

float EaseOut(const float& t) {
	return 1.0f - std::powf(1.0f - t, 2.0f);
}

float EaseInOut(const float& t) {
	if (t < 0.5f) {
		return 2.0f * t * t;
	} else {
		return 1.0f - std::powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
	}
}

float EaseInSine(float x1, float x2, float t) {
	float easedT = 1.0f - cosf((t * PI) / 2.0f);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutSine(float x1, float x2, float t) {
	float easedT = sinf((t * PI) / 2.0f);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutSine(float x1, float x2, float t) {
	float easedT = -(cosf(PI * t) - 1.0f) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInSine(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? 0.5f * sinf(PI * t)
		: 1.0f - 0.5f * cosf(PI * (t - 0.5f)) * 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInQuad(float x1, float x2, float t) {
	float easedT = t * t;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutQuad(float x1, float x2, float t) {
	float easedT = 1.0f - (1.0f - t) * (1.0f - t);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutQuad(float x1, float x2, float t) {
	float easedT = t < 0.5f ? 2.0f * t * t
		: 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInQuad(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? 0.5f * (1.0f - (1.0f - 2.0f * t) * (1.0f - 2.0f * t))
		: 0.5f * ((2.0f * t - 1.0f) * (2.0f * t - 1.0f)) + 0.5f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInCubic(float x1, float x2, float t) {
	float easedT = t * t * t;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutCubic(float x1, float x2, float t) {
	float easedT = 1.0f - powf(1.0f - t, 3.0f);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutCubic(float x1, float x2, float t) {
	float easedT = t < 0.5f ? 4.0f * t * t * t
		: 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInCubic(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? 0.5f * (1.0f - powf(1.0f - 2.0f * t, 3.0f))
		: 0.5f * powf(2.0f * t - 1.0f, 3.0f) + 0.5f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInQuart(float x1, float x2, float t) {
	float easedT = t * t * t * t;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutQuart(float x1, float x2, float t) {
	float easedT = 1.0f - powf(1.0f - t, 4.0f);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutQuart(float x1, float x2, float t) {
	float easedT = t < 0.5f ? 8.0f * t * t * t * t
		: 1.0f - powf(-2.0f * t + 2.0f, 4.0f) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInQuart(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? 0.5f * (1.0f - powf(1.0f - 2.0f * t, 4.0f))
		: 0.5f * powf(2.0f * t - 1.0f, 4.0f) + 0.5f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInQuint(float x1, float x2, float t) {
	float easedT = t * t * t * t * t;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutQuint(float x1, float x2, float t) {
	float easedT = 1.0f - powf(1.0f - t, 5.0f);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutQuint(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? 16.0f * t * t * t * t * t
		: 1.0f - powf(-2.0f * t + 2.0f, 5.0f) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInQuint(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? 0.5f * (1.0f - powf(1.0f - 2.0f * t, 5.0f))
		: 0.5f * powf(2.0f * t - 1.0f, 5.0f) + 0.5f;
	return (1.0f - easedT) * x1 + easedT * x2;
}


float EaseInExpo(float x1, float x2, float t) {
	float easedT = t == 0.0f ? 0.0f
		: powf(2.0f, 10.0f * t - 10.0f);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutExpo(float x1, float x2, float t) {
	float easedT = t == 1.0f ? 1.0f
		: 1.0f - powf(2.0f, -10.0f * t);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutExpo(float x1, float x2, float t) {
	float easedT = t == 0.0f ? 0.0f
		: t == 1.0f ? 1.0f
		: t < 0.5f ? powf(2.0f, 20.0f * t - 10.0f) / 2.0f
		: (2.0f - powf(2.0f, -20.0f * t + 10.0f)) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInExpo(float x1, float x2, float t) {
	float easedT = t == 0.0f ? 0.0f
		: t == 1.0f ? 1.0f
		: t < 0.5f ? (1.0f - powf(2.0f, -20.0f * t + 10.0f)) / 2.0f
		: (powf(2.0f, 20.0f * (t - 0.5f) - 10.0f) + 1.0f) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInCirc(float x1, float x2, float t) {
	float easedT = 1.0f - sqrtf(1.0f - powf(t, 2.0f));
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutCirc(float x1, float x2, float t) {
	float easedT = sqrtf(1.0f - powf(t - 1.0f, 2.0f));
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutCirc(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? (1.0f - sqrtf(1.0f - powf(2.0f * t, 2.0f))) / 2.0f
		: (sqrtf(1.0f - powf(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInCirc(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? 0.5f * (1.0f - sqrtf(1.0f - powf(1.0f - 2.0f * t, 2.0f)))
		: 0.5f * (sqrtf(1.0f - powf(2.0f * t - 1.0f, 2.0f)) + 1.0f);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInBack(float x1, float x2, float t) {
	static const float c1 = 1.70158f;
	static const float c3 = c1 + 1.0f;

	float easedT = c3 * t * t * t - c1 * t * t;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutBack(float x1, float x2, float t) {
	static const float c1 = 1.70158f;
	static const float c3 = c1 + 1.0f;

	float easedT = 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutBack(float x1, float x2, float t) {
	static const float c1 = 1.70158f;
	static const float c2 = c1 * 1.525f;

	float easedT = t < 0.5f
		? (powf(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
		: (powf(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInBack(float x1, float x2, float t) {
	static const float c1 = 1.70158f;
	static const float c2 = c1 * 1.525f;

	float easedT = t < 0.5f
		? 0.5f * ((1.0f - powf(1.0f - 2.0f * t, 2.0f) * ((c2 + 1.0f) * (1.0f - 2.0f * t) - c2)))
		: 0.5f * (powf(2.0f * t - 1.0f, 2.0f) * ((c2 + 1.0f) * (2.0f * t - 1.0f) + c2) + 1.0f);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInElastic(float x1, float x2, float t) {
	static const float c4 = (2.0f * PI) / 3.0f;

	float easedT = t == 0.0f ? 0.0f
		: t == 1.0f ? 1.0f
		: -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * c4);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutElastic(float x1, float x2, float t) {
	static const float c4 = (2.0f * PI) / 3.0f;

	float easedT = t == 0.0f ? 0.0f
		: t == 1.0f ? 1.0f
		: powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutElastic(float x1, float x2, float t) {
	static const float c5 = (2.0f * PI) / 4.5f;

	float easedT = t == 0.0f ? 0.0f
		: t == 1.0f ? 1.0f
		: t < 0.5f
		? -(powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f
		: (powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInElastic(float x1, float x2, float t) {
	static const float c5 = (2.0f * PI) / 4.5f;

	float easedT = t == 0.0f ? 0.0f
		: t == 1.0f ? 1.0f
		: t < 0.5f
		? 0.5f * (powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * c5) + 1.0f)
		: 0.5f * (-powf(2.0f, 20.0f * (t - 0.5f) - 10.0f) * sinf((20.0f * (t - 0.5f) - 11.125f) * c5)) + 0.5f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInBounce(float x1, float x2, float t) {
	float easedT = 1.0f - EaseOutBounce(0.0f, 1.0f, 1.0f - t);
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutBounce(float x1, float x2, float t) {
	static const float n1 = 7.5625f;
	static const float d1 = 2.75f;

	float easedT = 0.0f;
	if (t < 1.0f / d1) {
		easedT = n1 * t * t;

	} else if (t < 2 / d1) {
		easedT = n1 * (t -= 1.5f / d1) * t + 0.75f;

	} else if (t < 2.5 / d1) {
		easedT = n1 * (t -= 2.25f / d1) * t + 0.9375f;

	} else {
		easedT = n1 * (t -= 2.625f / d1) * t + 0.984375f;
	}

	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseInOutBounce(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? (1.0f - EaseOutBounce(0.0f, 1.0f, 1.0f - 2.0f * t)) / 2.0f
		: (1.0f + EaseOutBounce(0.0f, 1.0f, 2.0f * t - 1.0f)) / 2.0f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

float EaseOutInBounce(float x1, float x2, float t) {
	float easedT = t < 0.5f
		? 0.5f * EaseOutBounce(0.0f, 1.0f, 2.0f * t)
		: 0.5f * (1.0f - EaseOutBounce(0.0f, 1.0f, 2.0f * (1.0f - t))) + 0.5f;
	return (1.0f - easedT) * x1 + easedT * x2;
}

static inline Vector2 ApplyScalar(float (*fn)(float, float, float), const Vector2 &a, const Vector2 &b, float t) {
	return Vector2{ fn(a.x, b.x, t), fn(a.y, b.y, t) };
}

Vector2 EaseInSine(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInSine, x1, x2, t); }
Vector2 EaseOutSine(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutSine, x1, x2, t); }
Vector2 EaseInOutSine(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutSine, x1, x2, t); }
Vector2 EaseOutInSine(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInSine, x1, x2, t); }

Vector2 EaseInQuad(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInQuad, x1, x2, t); }
Vector2 EaseOutQuad(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutQuad, x1, x2, t); }
Vector2 EaseInOutQuad(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutQuad, x1, x2, t); }
Vector2 EaseOutInQuad(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInQuad, x1, x2, t); }

Vector2 EaseInCubic(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInCubic, x1, x2, t); }
Vector2 EaseOutCubic(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutCubic, x1, x2, t); }
Vector2 EaseInOutCubic(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutCubic, x1, x2, t); }
Vector2 EaseOutInCubic(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInCubic, x1, x2, t); }

Vector2 EaseInQuart(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInQuart, x1, x2, t); }
Vector2 EaseOutQuart(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutQuart, x1, x2, t); }
Vector2 EaseInOutQuart(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutQuart, x1, x2, t); }
Vector2 EaseOutInQuart(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInQuart, x1, x2, t); }

Vector2 EaseInQuint(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInQuint, x1, x2, t); }
Vector2 EaseOutQuint(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutQuint, x1, x2, t); }
Vector2 EaseInOutQuint(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutQuint, x1, x2, t); }
Vector2 EaseOutInQuint(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInQuint, x1, x2, t); }

Vector2 EaseInExpo(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInExpo, x1, x2, t); }
Vector2 EaseOutExpo(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutExpo, x1, x2, t); }
Vector2 EaseInOutExpo(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutExpo, x1, x2, t); }
Vector2 EaseOutInExpo(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInExpo, x1, x2, t); }

Vector2 EaseInCirc(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInCirc, x1, x2, t); }
Vector2 EaseOutCirc(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutCirc, x1, x2, t); }
Vector2 EaseInOutCirc(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutCirc, x1, x2, t); }
Vector2 EaseOutInCirc(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInCirc, x1, x2, t); }

Vector2 EaseInBack(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInBack, x1, x2, t); }
Vector2 EaseOutBack(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutBack, x1, x2, t); }
Vector2 EaseInOutBack(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutBack, x1, x2, t); }
Vector2 EaseOutInBack(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInBack, x1, x2, t); }

Vector2 EaseInElastic(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInElastic, x1, x2, t); }
Vector2 EaseOutElastic(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutElastic, x1, x2, t); }
Vector2 EaseInOutElastic(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutElastic, x1, x2, t); }
Vector2 EaseOutInElastic(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInElastic, x1, x2, t); }

Vector2 EaseInBounce(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInBounce, x1, x2, t); }
Vector2 EaseOutBounce(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutBounce, x1, x2, t); }
Vector2 EaseInOutBounce(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseInOutBounce, x1, x2, t); }
Vector2 EaseOutInBounce(Vector2 x1, Vector2 x2, float t) { return ApplyScalar(EaseOutInBounce, x1, x2, t); }

static inline Vector3 ApplyScalar(float (*fn)(float, float, float), const Vector3 &a, const Vector3 &b, float t) {
	return Vector3{ fn(a.x, b.x, t), fn(a.y, b.y, t), fn(a.z, b.z, t) };
}

Vector3 EaseInSine(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInSine, x1, x2, t); }
Vector3 EaseOutSine(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutSine, x1, x2, t); }
Vector3 EaseInOutSine(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutSine, x1, x2, t); }
Vector3 EaseOutInSine(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInSine, x1, x2, t); }

Vector3 EaseInQuad(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInQuad, x1, x2, t); }
Vector3 EaseOutQuad(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutQuad, x1, x2, t); }
Vector3 EaseInOutQuad(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutQuad, x1, x2, t); }
Vector3 EaseOutInQuad(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInQuad, x1, x2, t); }

Vector3 EaseInCubic(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInCubic, x1, x2, t); }
Vector3 EaseOutCubic(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutCubic, x1, x2, t); }
Vector3 EaseInOutCubic(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutCubic, x1, x2, t); }
Vector3 EaseOutInCubic(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInCubic, x1, x2, t); }

Vector3 EaseInQuart(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInQuart, x1, x2, t); }
Vector3 EaseOutQuart(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutQuart, x1, x2, t); }
Vector3 EaseInOutQuart(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutQuart, x1, x2, t); }
Vector3 EaseOutInQuart(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInQuart, x1, x2, t); }

Vector3 EaseInQuint(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInQuint, x1, x2, t); }
Vector3 EaseOutQuint(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutQuint, x1, x2, t); }
Vector3 EaseInOutQuint(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutQuint, x1, x2, t); }
Vector3 EaseOutInQuint(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInQuint, x1, x2, t); }

Vector3 EaseInExpo(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInExpo, x1, x2, t); }
Vector3 EaseOutExpo(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutExpo, x1, x2, t); }
Vector3 EaseInOutExpo(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutExpo, x1, x2, t); }
Vector3 EaseOutInExpo(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInExpo, x1, x2, t); }

Vector3 EaseInCirc(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInCirc, x1, x2, t); }
Vector3 EaseOutCirc(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutCirc, x1, x2, t); }
Vector3 EaseInOutCirc(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutCirc, x1, x2, t); }
Vector3 EaseOutInCirc(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInCirc, x1, x2, t); }

Vector3 EaseInBack(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInBack, x1, x2, t); }
Vector3 EaseOutBack(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutBack, x1, x2, t); }
Vector3 EaseInOutBack(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutBack, x1, x2, t); }
Vector3 EaseOutInBack(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInBack, x1, x2, t); }

Vector3 EaseInElastic(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInElastic, x1, x2, t); }
Vector3 EaseOutElastic(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutElastic, x1, x2, t); }
Vector3 EaseInOutElastic(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutElastic, x1, x2, t); }
Vector3 EaseOutInElastic(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInElastic, x1, x2, t); }

Vector3 EaseInBounce(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInBounce, x1, x2, t); }
Vector3 EaseOutBounce(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutBounce, x1, x2, t); }
Vector3 EaseInOutBounce(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseInOutBounce, x1, x2, t); }
Vector3 EaseOutInBounce(Vector3 x1, Vector3 x2, float t) { return ApplyScalar(EaseOutInBounce, x1, x2, t); }