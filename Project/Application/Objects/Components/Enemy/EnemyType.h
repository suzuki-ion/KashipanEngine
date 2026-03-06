#pragma once

/// <summary>
/// 敵の種類
/// </summary>
enum class EnemyType {

	Basic,  // 向いている方向2拍毎に移動
	Speedy, // Basicより1拍速く移動する
	
	Count // 敵の種類数
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