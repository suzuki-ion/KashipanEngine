// 配布版でも利用する通常の描画コンポーネントでUI・仮モデルを生成する。
Object@ TownMesh(Scene@ scene, const string &in name, const string &in mesh, const Vector3 &in pos,
                 const Vector3 &in scale, const Vector4 &in color) {
    Object@ obj = scene.CreateObject(name);
    obj.GetTransform().SetTranslate(pos); obj.GetTransform().SetScale(scale);
    MeshFilter@ filter; MeshRenderer@ renderer;
    obj.AddComponent(@filter); obj.AddComponent(@renderer);
    filter.SetMeshHandle(GetModelHandleFromAssetPath(mesh));
    renderer.SetPipelineName("Object3D.Solid.BlendNormal"); renderer.SetTargetObject(scene.GetObject("TownScreen"));
    renderer.SetInstanceColor(color); renderer.SetAllowInstancing(true);
    return obj;
}
Light@ TownLight(Scene@ scene, const Vector3 &in pos, const Vector4 &in color, float power, const string &in name = "TownLight") {
    Object@ obj = scene.CreateObject(name); obj.GetTransform().SetTranslate(pos);
    Light@ light; LightRenderer@ renderer; obj.AddComponent(@light); obj.AddComponent(@renderer);
    light.SetType(LightType::Point); light.SetColor(color);
    light.SetIntensity(power); light.SetRadius(5); light.SetDecay(2);
    light.SetCastShadows(GetSettingBool("town.shadows", true));
    light.SetShadowMapResolution(uint(GetSettingInt("town.shadowResolution", 1024)));
    renderer.SetTargetObject(scene.GetObject("TownScreen"));
    return light;
}
class TownUI {
    Scene@ scene; Object@ window; Object@ camera;
    Object@ target;
    string panelTemplate = "TownPanelHeader";
    array<UIButton@>@ buttons = array<UIButton@>();
    array<TextRenderer@>@ buttonTexts = array<TextRenderer@>();
    array<TextRenderer@>@ labels = array<TextRenderer@>();
    array<Object@>@ objects = array<Object@>();
    int layer = 30;
    float canvasHeight=720;
    void Initialize(Scene@ owner, const string &in targetName = "TownCanvas", const string &in cameraName = "TownUICamera", const string &in panelName = "TownPanelHeader") {
        @scene = owner; @window = scene.GetObject("TownWindow"); @camera = scene.GetObject(cameraName);
        @target = scene.GetObject(targetName); panelTemplate = panelName;
    }
    TextRenderer@ Text(const string &in content, float x, float y, float size = 20) {
        Object@ obj = scene.CreateObject("TownUILabel"); objects.insertLast(obj);
        // Camera2Dは左下原点。メニュー側は左上原点の論理座標で記述する。
        obj.GetTransform().SetTranslate(Vector3(x, canvasHeight - y, 0));
        TextRenderer@ text; obj.AddComponent(@text);
        text.SetTargetObject(target); text.SetPipelineName("Text2D.BlendNormal");
        text.SetFontName("NotoSansJP-Regular"); text.SetFontSize(size);
        text.SetHorizontalAlign(TextHorizontalAlign::Left); text.SetVerticalAlign(TextVerticalAlign::Top);
        text.SetOutlineWidth(0.05f); text.SetRenderPriority(layer); text.SetText(content);
        labels.insertLast(text);
        return text;
    }
    void Button(const string &in content, float x, float y) {
        TextRenderer@ text = Text(content, x, y, 19);
        Object@ obj = objects[objects.length() - 1];
        UIButton@ button; obj.AddComponent(@button); button.SetDisplayCameraObject(camera);
        buttons.insertLast(button); buttonTexts.insertLast(text);
    }
    Object@ Rect(float x, float y, float width, float height, const Vector4 &in color) {
        // SpriteRendererに公開されていない描画先設定は、シーン内のパネルから引き継ぐ。
        Object@ obj = scene.CloneObject(scene.GetObject(panelTemplate), "TownUIRect");
        obj.SetActive(true); objects.insertLast(obj);
        obj.GetTransform().SetTranslate(Vector3(x, canvasHeight - y, 0));
        obj.GetTransform().SetScale(Vector3(width, height, 1));
        SpriteRenderer@ sprite; obj.GetComponent(@sprite);
        sprite.SetMaterialName("TownUIOverlay");
        sprite.SetInstanceColor(color); sprite.SetRenderPriority(layer - 1);
        return obj;
    }
    void Show(bool visible) {
        for (uint i = 0; i < objects.length(); ++i) if (IsValidObject(objects[i])) objects[i].SetActive(visible);
    }
    Object@ Image(const string &in path,float x,float y,float size) {
        Object@ obj=Rect(x,y,size,size,Vector4(1,1,1,1));
        TextureSource@ source;obj.AddComponent(@source);source.SetTextureAssetPath(path);return obj;
    }
    Object@ ButtonArea(const string &in content,float x,float y,float width,float height,float labelX,float labelY) {
        Object@ surface=Rect(x,y,width,height,Vector4(.04f,.1f,.12f,.8f));
        UIButton@ button;surface.AddComponent(@button);button.SetDisplayCameraObject(camera);
        TextRenderer@ label=Text(content,labelX,labelY,19);buttons.insertLast(button);buttonTexts.insertLast(label);return surface;
    }
    void RefreshLabels() {
        // 追加文字でフォントアトラスが拡張されると既存グリフのUVも変わる。
        // 試作UIは小規模なので全ラベルを再評価し、静的な見出しも最新アトラスへ追従させる。
        for (uint i = 0; i < labels.length(); ++i) labels[i].SetFontSize(labels[i].GetFontSize());
    }
    int Clicked() {
        RefreshLabels();
        int result = -1;
        for (uint i = 0; i < buttons.length(); ++i) {
            buttonTexts[i].SetColor(buttons[i].IsHovered() ? Vector4(1, 0.85f, 0.35f, 1) : Vector4(0.9f, 0.95f, 1, 1));
            if (buttons[i].IsClicked()) result = int(i);
        }
        if (result >= 0) {
            Object@ soundObject = scene.GetObject("TownUISound");
            AudioSource@ sound;
            if (soundObject !is null && soundObject.GetComponent(@sound)) sound.Play();
        }
        return result;
    }
    bool IsOverUI() {
        ScreenBufferViewport@ viewport; scene.GetObject("TownDisplayViewport").GetComponent(@viewport);
        Vector2 pos;
        if (!viewport.TryGetOffscreenMousePosition(pos)) return true;
        float x = pos.x / 1.5f, y = pos.y / 1.5f;
        return x < 320 || y < 90 || y > 630;
    }
}
