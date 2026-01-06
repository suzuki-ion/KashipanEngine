#pragma once
#include <vector>
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

float Normalize01(const float &value, const float &min, const float &max);
Vector2 Normalize01(const Vector2 &value, const Vector2 &min, const Vector2 &max);
Vector3 Normalize01(const Vector3 &value, const Vector3 &min, const Vector3 &max);
Vector4 Normalize01(const Vector4 &value, const Vector4 &min, const Vector4 &max);

float Lerp(const float& start, const float& end, const float& t);
Vector2 Lerp(const Vector2 &start, const Vector2 &end, const float &t);
Vector3 Lerp(const Vector3& start, const Vector3& end, const float& t);
Vector4 Lerp(const Vector4& start, const Vector4& end, const float& t);

float EaseInSine(float x1, float x2, float t);
float EaseOutSine(float x1, float x2, float t);
float EaseInOutSine(float x1, float x2, float t);
float EaseOutInSine(float x1, float x2, float t);

float EaseInQuad(float x1, float x2, float t);
float EaseOutQuad(float x1, float x2, float t);
float EaseInOutQuad(float x1, float x2, float t);
float EaseOutInQuad(float x1, float x2, float t);

float EaseInCubic(float x1, float x2, float t);
float EaseOutCubic(float x1, float x2, float t);
float EaseInOutCubic(float x1, float x2, float t);
float EaseOutInCubic(float x1, float x2, float t);

float EaseInQuart(float x1, float x2, float t);
float EaseOutQuart(float x1, float x2, float t);
float EaseInOutQuart(float x1, float x2, float t);
float EaseOutInQuart(float x1, float x2, float t);

float EaseInQuint(float x1, float x2, float t);
float EaseOutQuint(float x1, float x2, float t);
float EaseInOutQuint(float x1, float x2, float t);
float EaseOutInQuint(float x1, float x2, float t);

float EaseInExpo(float x1, float x2, float t);
float EaseOutExpo(float x1, float x2, float t);
float EaseInOutExpo(float x1, float x2, float t);
float EaseOutInExpo(float x1, float x2, float t);

float EaseInCirc(float x1, float x2, float t);
float EaseOutCirc(float x1, float x2, float t);
float EaseInOutCirc(float x1, float x2, float t);
float EaseOutInCirc(float x1, float x2, float t);

float EaseInBack(float x1, float x2, float t);
float EaseOutBack(float x1, float x2, float t);
float EaseInOutBack(float x1, float x2, float t);
float EaseOutInBack(float x1, float x2, float t);

float EaseInElastic(float x1, float x2, float t);
float EaseOutElastic(float x1, float x2, float t);
float EaseInOutElastic(float x1, float x2, float t);
float EaseOutInElastic(float x1, float x2, float t);

float EaseInBounce(float x1, float x2, float t);
float EaseOutBounce(float x1, float x2, float t);
float EaseInOutBounce(float x1, float x2, float t);
float EaseOutInBounce(float x1, float x2, float t);

Vector2 EaseInSine(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutSine(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutSine(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInSine(Vector2 x1, Vector2 x2, float t);

Vector2 EaseInQuad(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutQuad(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutQuad(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInQuad(Vector2 x1, Vector2 x2, float t);

Vector2 EaseInCubic(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutCubic(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutCubic(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInCubic(Vector2 x1, Vector2 x2, float t);

Vector2 EaseInQuart(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutQuart(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutQuart(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInQuart(Vector2 x1, Vector2 x2, float t);

Vector2 EaseInQuint(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutQuint(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutQuint(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInQuint(Vector2 x1, Vector2 x2, float t);

Vector2 EaseInExpo(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutExpo(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutExpo(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInExpo(Vector2 x1, Vector2 x2, float t);

Vector2 EaseInCirc(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutCirc(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutCirc(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInCirc(Vector2 x1, Vector2 x2, float t);

Vector2 EaseInBack(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutBack(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutBack(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInBack(Vector2 x1, Vector2 x2, float t);

Vector2 EaseInElastic(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutElastic(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutElastic(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInElastic(Vector2 x1, Vector2 x2, float t);

Vector2 EaseInBounce(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutBounce(Vector2 x1, Vector2 x2, float t);
Vector2 EaseInOutBounce(Vector2 x1, Vector2 x2, float t);
Vector2 EaseOutInBounce(Vector2 x1, Vector2 x2, float t);

Vector3 EaseInSine(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutSine(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutSine(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInSine(Vector3 x1, Vector3 x2, float t);

Vector3 EaseInQuad(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutQuad(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutQuad(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInQuad(Vector3 x1, Vector3 x2, float t);

Vector3 EaseInCubic(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutCubic(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutCubic(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInCubic(Vector3 x1, Vector3 x2, float t);

Vector3 EaseInQuart(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutQuart(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutQuart(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInQuart(Vector3 x1, Vector3 x2, float t);

Vector3 EaseInQuint(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutQuint(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutQuint(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInQuint(Vector3 x1, Vector3 x2, float t);

Vector3 EaseInExpo(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutExpo(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutExpo(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInExpo(Vector3 x1, Vector3 x2, float t);

Vector3 EaseInCirc(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutCirc(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutCirc(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInCirc(Vector3 x1, Vector3 x2, float t);

Vector3 EaseInBack(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutBack(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutBack(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInBack(Vector3 x1, Vector3 x2, float t);

Vector3 EaseInElastic(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutElastic(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutElastic(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInElastic(Vector3 x1, Vector3 x2, float t);

Vector3 EaseInBounce(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutBounce(Vector3 x1, Vector3 x2, float t);
Vector3 EaseInOutBounce(Vector3 x1, Vector3 x2, float t);
Vector3 EaseOutInBounce(Vector3 x1, Vector3 x2, float t);
