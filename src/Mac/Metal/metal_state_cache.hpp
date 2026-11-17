#ifndef COD2_METAL_STATE_CACHE_HPP
#define COD2_METAL_STATE_CACHE_HPP
#include "metal_states.h"
#include <unordered_map>
#include <vector>
#include <cstring>

struct MacMetalCacheKey {
    std::vector<unsigned char> owned;
    const unsigned char *borrowed;
    size_t length;
    explicit MacMetalCacheKey(const MacMetalPipelineKey &key) : borrowed(key.bytes), length(key.length) {}
    const unsigned char *data() const { return owned.empty() ? borrowed : owned.data(); }
    size_t size() const { return length; }
    MacMetalPipelineKey view() const { return { data(), length, MAC_METAL_STATE_KEY_SCHEMA }; }
    bool operator==(const MacMetalCacheKey &other) const
    { return length == other.length && !std::memcmp(data(), other.data(), length); }
};

struct MacMetalByteHash {
    size_t operator()(const MacMetalPipelineKey &key) const
    {
        return (size_t)MacMetalStates_KeyHash(&key);
    }
};
/* Loading owns key bytes; ready lookups borrow bytes only for the synchronous
 * find. Hashes choose a bucket; equality still compares every versioned byte. */
template<class Value, class Hash = MacMetalByteHash> class MacMetalByteCache {
    struct KeyHash {
        size_t operator()(const MacMetalCacheKey &key) const { return Hash{}(key.view()); }
    };
    std::unordered_map<MacMetalCacheKey, Value, KeyHash> entries;
public:
    static bool valid(const MacMetalPipelineKey &key)
    {
        return key.schemaVersion == MAC_METAL_STATE_KEY_SCHEMA && key.bytes && key.length >= 8 && key.length <= MAC_METAL_KEY_CAPACITY &&
            key.bytes[0] == 'M' && key.bytes[1] == 'P' && key.bytes[2] == 'S' && key.bytes[3] >= 1 && key.bytes[3] <= 3 &&
            key.bytes[4] == MAC_METAL_STATE_KEY_SCHEMA && key.bytes[5] == 0 && key.bytes[6] == 0 && key.bytes[7] == 0;
    }
    const Value *find(const MacMetalPipelineKey &key) const
    {
        if (!valid(key)) return nullptr;
        auto i = entries.find(MacMetalCacheKey(key));
        return i == entries.end() ? nullptr : &i->second;
    }
    bool insert(const MacMetalPipelineKey &key, Value value)
    {
        if (!valid(key)) return false;
        MacMetalCacheKey stored(key);
        stored.owned.assign(key.bytes, key.bytes + key.length);
        stored.borrowed = nullptr;
        return entries.emplace(std::move(stored), value).second;
    }
    size_t size() const { return entries.size(); }
    void clear() { entries.clear(); }
    const auto &all() const { return entries; }
};
#endif
