#pragma once
#include "Utilities/ValueType.h"

template <typename T> struct is_vector : std::false_type {};
template <typename T, typename Alloc> struct is_vector<std::vector<T, Alloc>> : std::true_type {};
template <typename T> inline constexpr bool is_vector_v = is_vector<T>::value;

template <typename T> struct is_unordered_map : std::false_type {};
template <typename K, typename V, typename Hash, typename KeyEqual, typename Alloc> struct is_unordered_map<std::unordered_map<K, V, Hash, KeyEqual, Alloc>> : std::true_type {};
template <typename T> inline constexpr bool is_unordered_map_v = is_unordered_map<T>::value;

template <typename T> struct is_pair : std::false_type {};
template <typename T1, typename T2> struct is_pair<std::pair<T1, T2>> : std::true_type {};
template <typename T> inline constexpr bool is_pair_v = is_pair<T>::value;

/// @brief カスタムAnyクラス
class MyAny {
public:
    struct BaseHolder {
        virtual ~BaseHolder() {}
        virtual BaseHolder *clone() const = 0;
        bool isType(const ValueType &valueType) const {
            return valueType == ValueType::Any || valueType == typeInfo.GetBaseType();
        }
        bool isType(const std::vector<TypeInfo> &templateArgs) const {
            if (typeInfo.GetBaseType() == ValueType::Any) return true;
            if (typeInfo.GetTemplateArguments().size() != templateArgs.size()) return false;
            for (size_t i = 0; i < templateArgs.size(); ++i) {
                if (!isType(templateArgs[i])) return false;
            }
            return true;
        }
        bool isType(const TypeInfo &typeInfoT) const {
            return typeInfoT.GetBaseType() == ValueType::Any ||
                (typeInfoT.GetBaseType() == typeInfo.GetBaseType() && isType(typeInfoT.GetTemplateArguments()));
        }
        template<typename T>
        bool isType() const {
            TypeInfo typeInfoT = GetValueType<T>();
            return typeInfoT.GetBaseType() == ValueType::Any || isType(typeInfoT);
        }
        TypeInfo typeInfo = TypeInfo(ValueType::None);
    };

    MyAny() : content(nullptr) {}
    template <typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, MyAny>>>
    MyAny(const T &value);
    template <typename T>
    MyAny(const T &value, const TypeInfo &explicitTypeInfo) : content(std::make_unique<Holder<T>>(value)) {
        content->typeInfo = explicitTypeInfo;
    }
    MyAny(const MyAny &other) : content(other.content ? other.content->clone() : nullptr) {}
    MyAny(MyAny &&other) noexcept : content(std::move(other.content)) {}
    ~MyAny() = default;
    MyAny &operator=(const MyAny &other) {
        if (this != &other) {
            content.reset(other.content ? other.content->clone() : nullptr);
        }
        return *this;
    }
    MyAny &operator=(MyAny &&other) noexcept {
        if (this != &other) {
            content = std::move(other.content);
        }
        return *this;
    }
    bool operator==(const MyAny &other) const {
        if (content && other.content) {
            return GetRawPointer() == other.GetRawPointer();
        }
        return !content && !other.content;
    }
    bool operator!=(const MyAny &other) const {
        return !(*this == other);
    }
    template <typename T>
    bool IsType() const {
        return content && content->isType<T>();
    }
    template<typename T>
    T AnyCast() const {
        if (!content || !content->isType<T>()) {
            throw std::bad_cast();
        }
        return static_cast<Holder<T>*>(content.get())->value;
    }
    template <typename T>
    T *AnyCastPtr() const {
        if (!content || !content->isType<T>()) {
            return nullptr;
        }
        return &(static_cast<Holder<T>*>(content.get())->value);
    }
    void *GetRawPointer() const {
        if (!content) {
            return nullptr;
        }
        return static_cast<void *>(content.get());
    }
    bool IsEmpty() const {
        return !content;
    }
    const TypeInfo &GetTypeInfo() const {
        if (!content) {
            static TypeInfo noneType(ValueType::None);
            return noneType;
        }
        return content->typeInfo;
    }
private:
    template<typename T>
    struct Holder : BaseHolder {
        Holder(const T &value) : value(value) {
            typeInfo = GetValueType<T>();
        }
        BaseHolder *clone() const override { return new Holder(value); }
        T value;
    };
    std::unique_ptr<BaseHolder> content;
};

namespace std {
template<>
struct hash<MyAny> {
    std::size_t operator()(const MyAny &a) const noexcept {
        return std::hash<void *>()(a.GetRawPointer());
    }
};
} // namespace std

template <typename T, typename>
MyAny::MyAny(const T &value) : content(std::make_unique<Holder<T>>(value)) {
    if constexpr (is_vector_v<T>) {
        // すでに std::vector<MyAny> であればそのまま保持
        if constexpr (std::is_same_v<typename T::value_type, MyAny>) {
            content = std::make_unique<Holder<T>>(value);
        } else {
            // std::vector<int> 等なら、要素を再帰的に MyAny 化して std::vector<MyAny> に変換
            std::vector<MyAny> anyVec;
            anyVec.reserve(value.size());
            for (const auto &item : value) {
                anyVec.push_back(MyAny(item)); // 再帰呼び出し
            }
            content = std::make_unique<Holder<std::vector<MyAny>>>(anyVec);
            content->typeInfo = GetValueType<T>(); // 本来の型情報をセット
        }
    } else if constexpr (is_unordered_map_v<T>) {
        if constexpr (std::is_same_v<typename T::key_type, MyAny> && std::is_same_v<typename T::mapped_type, MyAny>) {
            content = std::make_unique<Holder<T>>(value);
        } else {
            std::unordered_map<MyAny, MyAny> anyMap;
            for (const auto &[k, v] : value) {
                anyMap[MyAny(k)] = MyAny(v);
            }
            content = std::make_unique<Holder<std::unordered_map<MyAny, MyAny>>>(anyMap);
            content->typeInfo = GetValueType<T>();
        }
    } else if constexpr (is_pair_v<T>) {
        if constexpr (std::is_same_v<typename T::first_type, MyAny> && std::is_same_v<typename T::second_type, MyAny>) {
            content = std::make_unique<Holder<T>>(value);
        } else {
            auto anyPair = std::make_pair(MyAny(value.first), MyAny(value.second));
            content = std::make_unique<Holder<decltype(anyPair)>>(anyPair);
            content->typeInfo = GetValueType<T>();
        }
    } else {
        // プリミティブ型や数学系、通常のクラスはそのまま保持
        content = std::make_unique<Holder<T>>(value);
    }
}

namespace KashipanEngine {

/// @brief MyAny を JSON へ保存するための関数
/// @param value 保存したい MyAny
/// @return JSON オブジェクト
JSON SaveAnyToJson(const MyAny &value);

/// @brief JSON から任意の MyAny を復元するための関数
/// @param json JSON オブジェクト
/// @param typeInfo 型情報
/// @return 復元された MyAny
MyAny LoadAnyFromJson(const JSON &json, const TypeInfo &typeInfo);

} // namespace KashipanEngine