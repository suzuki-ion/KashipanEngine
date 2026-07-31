class LightTestScript : ScriptComponentBehavior {
    [SerializeField]
    uint32 lightNumX = 0;
    [SerializeField]
    uint32 lightNumY = 0;
    [SerializeField]
    uint32 objectNumX = 0;
    [SerializeField]
    uint32 objectNumY = 0;
    [SerializeField]
    bool enableCastShadow = false;

    bool spawned = false;

    void Update() {
        if (spawned) return;

        for (uint32 i = 0; i < lightNumY; ++i) {
            for (uint32 j = 0; j < lightNumX; ++j) {
                Object@ lightObject = GetScene().CreateObject("light");
                Light@ light;
                LightRenderer@ lightRenderer;
                lightObject.AddComponent(@light);
                lightObject.AddComponent(@lightRenderer);

                light.SetCastShadows(enableCastShadow);
                light.SetType(LightType::Point);
                light.SetRadius(4.0f);

                lightRenderer.SetPipelineName("Object3D.Solid.BlendNormal");
                
                Transform@ tr = lightObject.GetTransform();
                tr.SetTranslate(Vector3(j * 4.0f, 2.0f, i * 4.0f));
            }
        }
        for (uint32 i = 0; i < objectNumY; ++i) {
            for (uint32 j = 0; j < objectNumX; ++j) {
                Object@ boxObject = GetScene().CreateObject("box");
                MeshFilter@ meshFilter;
                MeshRenderer@ meshRenderer;
                boxObject.AddComponent(@meshFilter);
                boxObject.AddComponent(@meshRenderer);

                meshFilter.SetMeshHandle(7);
                meshRenderer.SetPipelineName("Object3D.Solid.BlendNormal");
                
                Transform@ tr = boxObject.GetTransform();
                tr.SetTranslate(Vector3(j * 4.0f, 0.0f, i * 4.0f));
            }
        }

        spawned = true;
    }
}