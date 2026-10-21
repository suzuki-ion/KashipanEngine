#include "VectorMap.h"

namespace MyStd {

template<class T>
class NameMap {
public:
    using key_type = std::string;
    using mapped_type = T;
    using entry_type = Entry<key_type, mapped_type>;
    entry_type &operator[](std::string_view name) { return storage_[key_type(name)]; }
    mapped_type &Set(std::string_view name, const mapped_type &v) { auto &e = storage_[key_type(name)]; e = v; return e.value; }
    template<class... Args> mapped_type &Emplace(std::string_view name, Args&&... args) { auto &e = storage_[key_type(name)]; e = mapped_type(std::forward<Args>(args)...); return e.value; }
    mapped_type *TryGet(std::string_view name) { auto it = storage_.find(key_type(name)); return it == storage_.end() ? nullptr : &it->value; }
    const mapped_type *TryGet(std::string_view name) const {
        auto &s = const_cast<Storage &>(storage_);
        auto it = s.find(key_type(name));
        return it == s.end() ? nullptr : &it->value;
    }
    mapped_type &Get(std::string_view name) { return storage_.at(key_type(name)).value; }
    const mapped_type &Get(std::string_view name) const { return const_cast<NameMap *>(this)->Get(name); }
    bool Contains(std::string_view name) const { return TryGet(name) != nullptr; }
    void Erase(std::string_view name) { storage_.erase(key_type(name)); }
    void Erase(size_t index) { storage_.erase(index); }
    size_t Size() const { return storage_.size(); }
    bool Empty() const { return storage_.empty(); }
    void Clear() { storage_.clear(); }
    entry_type &At(size_t index) { return storage_.at(index); }
    const entry_type &At(size_t index) const { return const_cast<NameMap *>(this)->At(index); }
    size_t IndexOf(std::string_view name) { return storage_.at(key_type(name)).index; }
    auto begin() { return storage_.begin(); }
    auto end() { return storage_.end(); }
    auto begin() const { return const_cast<NameMap *>(this)->begin(); }
    auto end() const { return const_cast<NameMap *>(this)->end(); }
private:
    using Storage = VectorMap<key_type, mapped_type>;
    Storage storage_;
};

} // namespace MyStd