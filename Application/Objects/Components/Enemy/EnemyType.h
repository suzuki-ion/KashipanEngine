#pragma once

/// <summary>
/// 敵の種類
/// </summary>
enum class EnemyType {

	Basic, // 向いている方向にしか移動しない
	
};

/// <summary>
/// 敵の初期向き
/// </summary>
enum class EnemyDirection {
	Up,
	Down,
	Left,
	Right
};