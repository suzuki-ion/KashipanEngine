#pragma once

namespace KashipanEngine {

/// @brief エンジン組み込みのプリミティブメッシュを生成しModelManagerへ登録するクラス
/// @details 登録名は全て "PrimitiveMesh-" から始まる（例: "PrimitiveMesh-Box"）。
///          ModelManagerの構築時（LoadAllFromAssetsFolderと同じタイミング）に一度だけ呼ばれる想定。
class PrimitiveMeshGenerator {
public:
    /// @brief UVSphere/ICOSphere/Plane/Box/Triangle/Circle/Tetrahedron/CylinderClosed/CylinderOpened を登録する
    /// @details 同名で登録済みの場合は ModelManager::RegisterProceduralMesh 側で再登録がスキップされる
    static void RegisterBuiltinPrimitiveMeshes();
};

} // namespace KashipanEngine
