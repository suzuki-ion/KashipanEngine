#pragma once

namespace KashipanEngine {

enum class TutorialStage {
    None,           // チュートリアル未開始
    Move,           // 移動チュートリアル
    BombPlace,      // 爆弾設置チュートリアル
    BombChain,      // 爆弾連鎖チュートリアル
    Enemy,          // 敵との戦闘チュートリアル
    Complete        // チュートリアル完了
};

} // namespace KashipanEngine