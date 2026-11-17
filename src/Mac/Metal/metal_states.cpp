#include "metal_states.h"
#include <algorithm>
#include <cmath>
#include <cstring>

static bool CanonicalFloat(float value, float *out)
{
    if (!std::isfinite(value)) return false;
    *out = value == 0 ? 0 : value;
    return true;
}
static uint32_t FloatBits(float value)
{
    uint32_t bits; std::memcpy(&bits, &value, 4); return bits;
}
static float BitsFloat(uint32_t value)
{
    float f; std::memcpy(&f, &value, 4); return f;
}
void MacMetalStates_Defaults(MacMetalState *state)
{
    if (!state) return;
    std::memset(state, 0, sizeof(*state));
    state->render[7] = state->render[14] = 1;
    state->render[8] = 3; state->render[9] = 2;
    state->render[19] = 2; state->render[20] = 1;
    state->render[22] = 3; state->render[23] = 4; state->render[25] = 8;
    state->render[53] = state->render[54] = state->render[55] = 1;
    state->render[56] = 8; state->render[58] = state->render[59] = 255;
    state->render[60] = 0xffffffff; state->render[168] = state->render[190] = state->render[191] = state->render[192] = 15;
    state->render[171] = state->render[209] = 1;
    state->render[186] = state->render[187] = state->render[188] = 1; state->render[189] = 8;
    state->render[207] = 2; state->render[208] = 1;
    state->maxDepth = 1;
    for (unsigned i = 0; i < 512; ++i)
        for (unsigned j = 0; j < 4; ++j) state->transform[i][j * 5] = 1;
    for (unsigned i = 0; i < 16; ++i) {
        state->sampler[i][1] = state->sampler[i][2] = state->sampler[i][3] = 1;
        state->sampler[i][5] = state->sampler[i][6] = 1; state->sampler[i][10] = 1;
    }
    for (unsigned i = 0; i < 8; ++i) {
        state->textureStage[i][1] = i ? 1 : 4; state->textureStage[i][4] = i ? 1 : 2;
        state->textureStage[i][2] = state->textureStage[i][5] = 2;
        state->textureStage[i][3] = state->textureStage[i][6] = 1;
        state->textureStage[i][11] = i;
    }
}
static bool RenderValid(unsigned type, uint32_t value)
{
    switch (type) {
    case 7: return value <= 2; // D3DZB_USEW is retained, requires wrapper emulation.
    case 8: return value >= 1 && value <= 3;
    case 22: return value >= 1 && value <= 3;
    case 23: case 25: case 56: case 189: return value >= 1 && value <= 8;
    case 19: case 20: case 207: case 208: return value >= 1 && value <= 17;
    case 171: case 209: return value >= 1 && value <= 5;
    case 53: case 54: case 55: case 186: case 187: case 188: return value >= 1 && value <= 8;
    case 36: case 37: case 38: case 154: case 155: case 158: case 159: case 160:
    case 166: case 170: case 175: case 178: case 179: case 180: case 182: case 183: case 195:
        return std::isfinite(BitsFloat(value));
    default: return true;
    }
}
static bool SamplerValid(unsigned type, uint32_t value)
{
    switch (type) {
    case 1: case 2: case 3: return value >= 1 && value <= 5;
    case 5: case 6: return value >= 1 && value <= 3;
    case 7: return value <= 2;
    case 8: return std::isfinite(BitsFloat(value));
    default: return true;
    }
}
int MacMetalStates_Normalize(const MacMetalState *raw, MacMetalNormalizedState *out)
{
    if (!raw || !out) return 0;
    MacMetalNormalizedState n = {};
    std::memcpy(n.render, raw->render, sizeof(n.render));
    for (unsigned i = 0; i < 256; ++i) if (!RenderValid(i, n.render[i])) return 0;
    const unsigned booleans[] = { 14,15,16,26,27,28,29,48,52,136,137,141,142,143,156,157,161,167,174,176,184,185,194,206 };
    for (unsigned i : booleans) n.render[i] = n.render[i] != 0;
    n.render[24] &= 255; n.render[57] &= 255; n.render[58] &= 255; n.render[59] &= 255;
    for (unsigned i : { 168u,190u,191u,192u }) n.render[i] &= 15;
    for (unsigned i : { 36u,37u,38u,154u,155u,158u,159u,160u,166u,170u,175u,178u,179u,180u,182u,183u,195u }) n.render[i] = FloatBits(BitsFloat(n.render[i]) == 0 ? 0 : BitsFloat(n.render[i]));
    std::memcpy(n.textureStage, raw->textureStage, sizeof(n.textureStage));
    for (auto &stage : n.textureStage) for (unsigned i : { 7u,8u,9u,10u,22u,23u }) {
        float value; if (!CanonicalFloat(BitsFloat(stage[i]), &value)) return 0; stage[i] = FloatBits(value);
    }
    for (unsigned i = 0; i < 16; ++i) {
        const uint32_t *s = raw->sampler[i];
        for (unsigned j = 1; j < 14; ++j) if (!SamplerValid(j, s[j])) return 0;
        MacMetalSamplerState &d = n.sampler[i];
        for (unsigned j = 0; j < 3; ++j) d.address[j] = s[j + 1];
        // Retain which filter requests anisotropy: min-only and mag-only
        // are distinct D3D states even with the same Metal maximum degree.
        d.minFilter = s[6]; d.magFilter = s[5]; d.mipFilter = s[7];
        d.anisotropy = s[6] == 3 || s[5] == 3 ? std::max(1u, std::min(16u, s[10])) : 1;
        d.borderColor = s[4]; d.maxMipLevel = s[9]; d.srgb = s[11] != 0; d.elementIndex = s[12]; d.dmapOffset = s[13];
        if (!CanonicalFloat(BitsFloat(s[8]), &d.lodBias)) return 0;
        bool border = false; for (unsigned j = 0; j < 3; ++j) border |= d.address[j] == 4;
        if (!border) d.borderColor = 0;
    }
    for (unsigned i = 0; i < 512; ++i)
        for (unsigned j = 0; j < 16; ++j) if (!CanonicalFloat(raw->transform[i][j], &n.transform[i][j])) return 0;
    if (!CanonicalFloat(raw->minDepth, &n.minDepth) || !CanonicalFloat(raw->maxDepth, &n.maxDepth) || n.minDepth < 0 || n.maxDepth > 1 || n.minDepth > n.maxDepth) return 0;
    std::memcpy(n.viewport, raw->viewport, sizeof(n.viewport));
    if (uint64_t(n.viewport[0]) + n.viewport[2] > UINT32_MAX || uint64_t(n.viewport[1]) + n.viewport[3] > UINT32_MAX) return 0;
    std::memcpy(n.textures, raw->textures, sizeof(n.textures)); std::memcpy(n.streams, raw->streams, sizeof(n.streams));
    std::memcpy(n.offsets, raw->offsets, sizeof(n.offsets)); std::memcpy(n.strides, raw->strides, sizeof(n.strides));
    std::memcpy(n.color, raw->color, sizeof(n.color));
    n.depth = raw->depth; n.indices = raw->indices; n.declaration = raw->declaration; n.vs = raw->vs; n.ps = raw->ps;
    *out = n; return 1;
}

class KeyWriter {
    MacMetalKeyStorage *key;
public:
    explicit KeyWriter(MacMetalKeyStorage *k, unsigned kind) : key(k)
    {
        key->length = 0; key->schemaVersion = MAC_METAL_STATE_KEY_SCHEMA;
        const unsigned char magic[] = { 'M','P','S',(unsigned char)kind };
        bytes(magic, 4); word(MAC_METAL_STATE_KEY_SCHEMA);
    }
    void bytes(const void *p, size_t n)
    {
        if (n <= MAC_METAL_KEY_CAPACITY - key->length) {
            std::memcpy(key->bytes + key->length, p, n); key->length += n;
        }
    }
    void word(uint32_t v)
    {
        unsigned char b[] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) }; bytes(b, 4);
    }
    bool shader(const MacMetalShaderIdentity &s)
    {
        size_t length = strnlen(s.entryPoint, sizeof(s.entryPoint));
        if (!length || length == sizeof(s.entryPoint) || s.registerABI != MAC_METAL_ABI_VERSION || s.compilerOptions != MAC_METAL_STRICT_MATH) return false;
        bytes(s.source, 32); bytes(s.metadata, 32); bytes(s.interfaceSignature, 32);
        size_t revisionLength = strnlen(s.translatorRevision, sizeof(s.translatorRevision));
        if (!revisionLength || revisionLength == sizeof(s.translatorRevision)) return false;
        word((uint32_t)revisionLength); bytes(s.translatorRevision, revisionLength);
        word(s.registerABI); word(s.compilerOptions);
        word((uint32_t)length); bytes(s.entryPoint, length); return true;
    }
};
int MacMetalStates_PipelineKey(const MacMetalNormalizedState *n, const MacMetalPipelineRecipe *r, MacMetalKeyStorage *out)
{
    if (!n || !r || !out || r->attributeCount > 32 || r->specializationLength > 256 || !r->samples || r->topology < 1 || r->topology > 3 || r->hardwareVertexFetch > 1) return 0;
    KeyWriter w(out, 1);
    if (!w.shader(r->vertex) || !w.shader(r->fragment)) return 0;
    w.word(r->hardwareVertexFetch); w.word(r->attributeCount);
    MacMetalVertexAttribute sorted[32]; std::memcpy(sorted, r->attributes, r->attributeCount * sizeof(*sorted));
    std::sort(sorted, sorted + r->attributeCount, [](const auto &a, const auto &b) { return a.attribute < b.attribute; });
    for (unsigned i = 0; i < r->attributeCount; ++i) {
        const auto &a = sorted[i];
        // Metal's hardware vertex descriptor has 31 slots (0..30). The
        // larger logical recipe capacity also supports buffer-based fetch.
        if (a.attribute >= 32 || (r->hardwareVertexFetch && a.attribute >= 31) || a.stream >= 16 || !a.format || (i && sorted[i - 1].attribute == a.attribute)) return 0;
        w.word(a.attribute); w.word(a.stream); w.word(a.offset); w.word(a.format); w.word(a.colorOrder);
    }
    for (unsigned s : r->strides) w.word(s);
    for (unsigned f : r->colorFormats) w.word(f);
    w.word(r->depthFormat); w.word(r->stencilFormat); w.word(r->samples); w.word(r->topology);
    w.word(n->render[181]); // Vendor alpha-to-coverage token, never guessed.
    for (unsigned i = 0; i < 4; ++i) {
        w.word(r->colorFormats[i] ? n->render[i ? 189 + i : 168] : 0);
        bool blend = n->render[27] && r->colorFormats[i];
        w.word(blend);
        w.word(blend ? n->render[19] : 2); w.word(blend ? n->render[20] : 1); w.word(blend ? n->render[171] : 1);
        bool separate = blend && n->render[206];
        w.word(blend ? n->render[separate ? 207 : 19] : 2); w.word(blend ? n->render[separate ? 208 : 20] : 1); w.word(blend ? n->render[separate ? 209 : 171] : 1);
    }
    w.word(r->fixedFunctionVariant); w.word(r->specializationLength); w.bytes(r->specialization, r->specializationLength);
    if (r->fixedFunctionVariant) for (const auto &stage : n->textureStage) for (unsigned value : stage) w.word(value);
    return 1;
}
int MacMetalStates_DepthKey(const MacMetalNormalizedState *n, MacMetalKeyStorage *out)
{
    if (!n || !out) return 0;
    KeyWriter w(out, 2);
    w.word(n->render[7] ? n->render[23] : 8); w.word(n->render[7] ? n->render[14] : 0);
    w.word(n->render[52]);
    for (unsigned face = 0; face < 2; ++face) {
        unsigned base = face && n->render[185] ? 186 : 53;
        w.word(n->render[52] ? n->render[base + 3] : 8);
        for (unsigned i = 0; i < 3; ++i) w.word(n->render[52] ? n->render[base + i] : 1);
        w.word(n->render[52] ? n->render[58] : 255); w.word(n->render[52] ? n->render[59] : 0);
    }
    return 1;
}
int MacMetalStates_SamplerKey(const MacMetalSamplerState *s, MacMetalKeyStorage *out)
{
    if (!s || !out || !std::isfinite(s->lodBias)) return 0;
    KeyWriter w(out, 3);
    for (unsigned a : s->address) w.word(a);
    w.word(s->minFilter); w.word(s->magFilter); w.word(s->mipFilter); w.word(s->anisotropy);
    w.word(s->borderColor); w.word(s->maxMipLevel); w.word(s->srgb); w.word(s->elementIndex); w.word(s->dmapOffset); w.word(FloatBits(s->lodBias == 0 ? 0 : s->lodBias));
    return 1;
}
MacMetalPipelineKey MacMetalStates_KeyView(const MacMetalKeyStorage *s)
{
    return s ? MacMetalPipelineKey{ s->bytes, s->length, s->schemaVersion } : MacMetalPipelineKey{};
}
int MacMetalStates_KeyEqual(const MacMetalKeyStorage *a, const MacMetalKeyStorage *b)
{
    return a && b && a->length <= MAC_METAL_KEY_CAPACITY && b->length <= MAC_METAL_KEY_CAPACITY && a->schemaVersion == b->schemaVersion && a->length == b->length && !std::memcmp(a->bytes, b->bytes, a->length);
}
uint64_t MacMetalStates_KeyHash(const MacMetalPipelineKey *key)
{
    if (!key || !key->bytes || key->length > MAC_METAL_KEY_CAPACITY) return 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < key->length; ++i) hash = (hash ^ key->bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

int MacMetalStates_Update(MacMetalState *state, const char *method, const void *const *args, void *result, size_t size)
{
    if (!state || !method || !args || !result || size != sizeof(int32_t)) return 0;
    const char *prefix = "CDirect3DDevice_";
    if (std::strncmp(method, prefix, std::strlen(prefix))) return 0;
    const char *op = method + std::strlen(prefix);
    // ABI 1 starts its shadow zeroed, before native CreateDevice supplies
    // targets. Initialize numeric D3D defaults once, retaining those bindings.
    if (!state->render[8] && !state->render[23] && !state->render[25]) {
        MacMetalState defaults; MacMetalStates_Defaults(&defaults);
        std::memcpy(state->render, defaults.render, sizeof(state->render));
        std::memcpy(state->sampler, defaults.sampler, sizeof(state->sampler));
        std::memcpy(state->textureStage, defaults.textureStage, sizeof(state->textureStage));
        std::memcpy(state->transform, defaults.transform, sizeof(state->transform));
        state->minDepth = 0; state->maxDepth = 1;
    }
    bool set = !std::strncmp(op, "Set", 3), get = !std::strncmp(op, "Get", 3);
    if (!set && !get) return 0;
    auto u = [args](unsigned i) { return *static_cast<const uint32_t *>(args[i]); };
    auto p = [args](unsigned i) { return *static_cast<void *const *>(args[i]); };
    uint32_t *slot = nullptr; unsigned valueIndex = 0; bool valid = true;
    if (!std::strcmp(op + 3, "RenderState")) {
        valid = u(1) < 256; if (valid) slot = &state->render[u(1)]; valueIndex = 2;
        if (valid && set) valid = RenderValid(u(1), u(2));
    } else if (!std::strcmp(op + 3, "SamplerState")) {
        valid = u(1) < 16 && u(2) < 16; if (valid) slot = &state->sampler[u(1)][u(2)]; valueIndex = 3;
        if (valid && set) valid = SamplerValid(u(2), u(3));
    } else if (!std::strcmp(op + 3, "TextureStageState")) {
        valid = u(1) < 8 && u(2) < 33; if (valid) slot = &state->textureStage[u(1)][u(2)]; valueIndex = 3;
    } else if (!std::strcmp(op + 3, "Transform")) {
        valid = u(1) < 512 && p(2);
        float values[16];
        if (valid && set) for (unsigned i = 0; i < 16; ++i) valid &= CanonicalFloat(static_cast<float *>(p(2))[i], &values[i]);
        if (valid) std::memcpy(set ? state->transform[u(1)] : p(2), set ? p(2) : state->transform[u(1)], 64);
    } else if (!std::strcmp(op + 3, "Viewport")) {
        valid = p(1);
        if (valid && set) {
            const uint32_t *v = static_cast<const uint32_t *>(p(1)); float depth[2];
            std::memcpy(depth, v + 4, 8);
            valid = std::isfinite(depth[0]) && std::isfinite(depth[1]) && depth[0] >= 0 && depth[1] <= 1 && depth[0] <= depth[1] && uint64_t(v[0]) + v[2] <= UINT32_MAX && uint64_t(v[1]) + v[3] <= UINT32_MAX;
            if (valid) { std::memcpy(state->viewport, v, 16); state->minDepth = depth[0]; state->maxDepth = depth[1]; }
        } else if (valid) {
            std::memcpy(p(1), state->viewport, 16); std::memcpy(static_cast<char *>(p(1)) + 16, &state->minDepth, 4); std::memcpy(static_cast<char *>(p(1)) + 20, &state->maxDepth, 4);
        }
    } else return 0; // Object binding ownership remains with ABI 1 providers.
    if (slot && valid) {
        if (set) *slot = u(valueIndex);
        else if (p(valueIndex)) *static_cast<uint32_t *>(p(valueIndex)) = *slot;
        else valid = false;
    }
    *static_cast<int32_t *>(result) = valid ? 0 : (int32_t)0x8876086c;
    return 1;
}
