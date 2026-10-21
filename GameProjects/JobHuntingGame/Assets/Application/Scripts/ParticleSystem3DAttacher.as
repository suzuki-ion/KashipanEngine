// 指定したオブジェクト群へ、開始時にParticleSystem3Dコンポーネントを付与するスクリプト。
// このコンポーネント自体は任意の管理用オブジェクト（付与先とは別のオブジェクトでよい）に付ける。
//
// 注意: エンジンに"ParticleManager3D"という型は存在しないため、パーティクル用コンポーネントである
// ParticleSystem3D（Objects/Components/ParticleSystem3D.h）を付与する処理として実装している。

class ParticleSystem3DAttacher : ScriptComponentBehavior {
    [SerializeField, Tooltip("ParticleSystem3Dを付与する対象オブジェクト群")]
    array<Object@>@ targetObjects = null;

    void Start() {
        for (uint i = 0; i < targetObjects.length(); i++) {
            Object@ target = targetObjects[i];
            if (target is null) continue;

            // 既に付与済みなら二重に追加しない
            ParticleSystem3D@ existing;
            if (target.GetComponent(@existing)) continue;

            ParticleSystem3D@ added;
            if (!target.AddComponent(@added)) {
                Log("ParticleSystem3DAttacher: failed to add ParticleSystem3D to " + target.GetName());
            }
        }
    }
}
