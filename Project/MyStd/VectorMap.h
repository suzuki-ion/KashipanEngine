#pragma once
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <utility>

namespace MyStd {

template<typename Key, typename Value>
struct Entry {
    Entry() = delete;
    Entry(const Key &setKey, size_t setIndex) : key(setKey), index(setIndex), value(Value{}) {}
    Entry(const Key &setKey, size_t setIndex, const Value &setValue) : key(setKey), index(setIndex), value(setValue) {}

    operator Value &() { return value; }
    operator const Value &() const { return value; }

    Entry &operator=(const Value &newValue) { value = newValue; return *this; }
    Entry &operator+=(const Value &newValue) { value += newValue; return *this; }
    Entry &operator-=(const Value &newValue) { value -= newValue; return *this; }
    Entry &operator*=(const Value &newValue) { value *= newValue; return *this; }
    Entry &operator/=(const Value &newValue) { if constexpr(std::is_arithmetic_v<Value>) { if (newValue == 0) { value = 0; } else { value /= newValue; } } return *this; }
    Entry &operator+(const Value &newValue) { value += newValue; return *this; }
    Entry &operator-(const Value &newValue) { value -= newValue; return *this; }
    Entry &operator*(const Value &newValue) { value *= newValue; return *this; }
    Entry &operator/(const Value &newValue) { if constexpr(std::is_arithmetic_v<Value>) { if (newValue == 0) { value = 0; } else { value /= newValue; } } return *this; }
    Entry &operator++() { ++value; return *this; }
    Entry &operator--() { --value; return *this; }

    Key key;      // no longer const to allow reindexing swaps
    size_t index; // updated when elements are moved
    Value value;
};

template<typename Key, typename Value>
class VectorMap {
public:
    VectorMap() = default;
    ~VectorMap() = default;

    Entry<Key, Value> &operator[](const Key &key) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            return vec_[it->second];
        }
        vec_.emplace_back(key, vec_.size());
        map_[key] = vec_.size() - 1;
        return vec_.back();
    }
    Entry<Key, Value> &operator[](size_t index) { return at(index); }

    void push_back(const Key &key, const Value &value) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            vec_.emplace_back(key, vec_.size(), value);
            map_[key] = vec_.size() - 1;
        } else {
            vec_[it->second].value = value;
        }
    }
    void push_back(const std::pair<Key, Value> &pair) { push_back(pair.first, pair.second); }
    void pop_back() {
        if (vec_.empty()) return;
        Key key = vec_.back().key;
        map_.erase(key);
        vec_.pop_back();
    }
    Entry<Key, Value> &front() { if (vec_.empty()) throw std::out_of_range("Vector is empty"); return vec_.front(); }
    Entry<Key, Value> &back() { if (vec_.empty()) throw std::out_of_range("Vector is empty"); return vec_.back(); }

    size_t size() const { return vec_.size(); }
    bool empty() const { return vec_.empty(); }
    void clear() { vec_.clear(); map_.clear(); }

    void erase(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end()) return;
        erase(it->second);
    }
    void erase(size_t index) {
        if (index >= vec_.size()) return;
        Key key = vec_[index].key;
        map_.erase(key);
        if (index != vec_.size() - 1) {
            std::swap(vec_[index], vec_.back());
            vec_[index].index = index;
            map_[vec_[index].key] = index;
        }
        vec_.pop_back();
    }

    Entry<Key, Value> &at(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end()) throw std::out_of_range("Key not found");
        return vec_[it->second];
    }
    Entry<Key, Value> &at(size_t index) { if (index >= vec_.size()) throw std::out_of_range("Index out of range"); return vec_[index]; }

    auto find(const Key &key) { auto it = map_.find(key); return it == map_.end() ? vec_.end() : vec_.begin() + it->second; }
    auto find(size_t index) { return index >= vec_.size() ? vec_.end() : vec_.begin() + index; }

    auto begin() { return vec_.begin(); }
    auto end() { return vec_.end(); }

private:
    std::vector<Entry<Key, Value>> vec_;
    std::unordered_map<Key, size_t> map_;
};

} // namespace MyStd