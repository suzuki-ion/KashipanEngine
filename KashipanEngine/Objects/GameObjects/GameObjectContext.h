#pragma once
#include <string>
#include <span>
#include "Objects/GameObjects/GameObject2DBase.h"
#include "Objects/GameObjects/GameObject3DBase.h"

namespace KashipanEngine {

/// @brief ゲームオブジェクトコンテキストインターフェースクラス
class IGameObjectContext {
public:
    virtual ~IGameObjectContext() = default;

    virtual const std::string &GetName() const = 0;

    template<typename T>
    std::span<T> GetVertexData() const { return GetVertexDataImpl<T>(); }

    template<typename T>
    void SetVertexData(const std::span<T> &data) { SetVertexDataImpl<T>(data); }

protected:
    template<typename T>
    std::span<T> GetVertexDataImpl() const;

    template<typename T>
    void SetVertexDataImpl(const std::span<T> &data);
};

/// @brief 2Dゲームオブジェクトコンテキストクラス
class GameObject2DContext : public IGameObjectContext {
public:
    GameObject2DContext(Passkey<GameObject2DBase>, GameObject2DBase *owner) : owner_(owner) {}
    ~GameObject2DContext() = default;

    GameObject2DContext(const GameObject2DContext &) = delete;
    GameObject2DContext &operator=(const GameObject2DContext &) = delete;
    GameObject2DContext(GameObject2DContext &&) = delete;
    GameObject2DContext &operator=(GameObject2DContext &&) = delete;

    /// @brief オブジェクト名の取得
    const std::string &GetName() const override;
    /// @brief 頂点データの取得
    template<typename T>
    std::span<T> GetVertexData() const { return owner_->GetVertexData<T>(); }
    /// @brief 頂点データの設定
    template<typename T>
    void SetVertexData(const std::span<T> &data) { owner_->SetVertexData<T>(data); }
    /// @brief 他コンポーネントの取得
    std::vector<IGameObjectComponent2D *> GetComponents(const std::string &componentName) const;
    /// @brief コンポーネントの存在チェック
    /// @return コンポーネントの個数
    size_t HasComponents(const std::string &componentName) const;

private:
    GameObject2DBase *owner_ = nullptr;
};

/// @brief 3Dゲームオブジェクトコンテキストクラス
class GameObject3DContext : public IGameObjectContext {
public:
    GameObject3DContext(Passkey<GameObject3DBase>, GameObject3DBase *owner) : owner_(owner) {}
    ~GameObject3DContext() = default;

    GameObject3DContext(const GameObject3DContext &) = delete;
    GameObject3DContext &operator=(const GameObject3DContext &) = delete;
    GameObject3DContext(GameObject3DContext &&) = delete;
    GameObject3DContext &operator=(GameObject3DContext &&) = delete;

    /// @brief オブジェクト名の取得
    const std::string &GetName() const override;
    /// @brief 頂点データの取得
    template<typename T>
    std::span<T> GetVertexData() const { return owner_->template GetVertexData<T>(); }
    /// @brief 頂点データの設定
    template<typename T>
    void SetVertexDataImpl(const std::span<T> &data) { owner_->template SetVertexData<T>(data); }
    /// @brief 他コンポーネントの取得
    std::vector<IGameObjectComponent3D *> GetComponents(const std::string &componentName) const;
    /// @brief コンポーネントの存在チェック
    /// @return コンポーネントの個数
    size_t HasComponents(const std::string &componentName) const;

private:
    GameObject3DBase *owner_ = nullptr;
};

} // namespace KashipanEngine
