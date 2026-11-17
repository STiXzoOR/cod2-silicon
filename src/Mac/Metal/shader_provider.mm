#include "shader_provider.h"
#include "shader_setup.h"
#include "arb_to_msl.h"
#include "shader_fixed.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <CommonCrypto/CommonDigest.h>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

namespace {
const char revision[] = "ws32-arb-msl-3";
struct Prepared {
    uint32_t stage;
    std::string name, sourceHash, metadataHash, path, entry;
    std::vector<std::string> states;
    std::map<unsigned, std::string> textures;
    NSData *reflection;
    id<MTLLibrary> library;
    id<MTLFunction> function;
};
struct Payload {
    std::shared_ptr<Prepared> prepared;
    MacMetalFloat4 local[256] = {};
};
struct FixedPrepared {
    id<MTLLibrary> library;
    std::map<std::string, id<MTLFunction>> functions;
    std::string sourceHash;
};
std::map<std::string, std::shared_ptr<Prepared>> ready;
std::map<unsigned, FixedPrepared> fixedReady;
std::string cacheNamespace, lastError;
int lastStatus;
bool sealed;
uint64_t factories;
std::string Hash(NSData *data);

id<MTLDevice> Device()
{
    id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(MacMetal_GPU());
    return device ? device : MTLCreateSystemDefaultDevice();
}

bool FixedKey(const uint32_t *types, unsigned &key)
{
    key = 0;
    for (unsigned n = 0; n < 8; ++n) {
        unsigned type = types ? types[n] : 0;
        if (type > 2) return false;
        key |= type << (n * 2);
    }
    return true;
}

FixedPrepared CompileFixed(id<MTLDevice> device, const char *source)
{
    if (!device || !source) throw cod2::arb::Error("No fixed shader source or Metal device");
    MTLCompileOptions *options = [MTLCompileOptions new];
    options.languageVersion = MTLLanguageVersion3_0; options.fastMathEnabled = NO;
    NSError *error = nil;
    FixedPrepared p;
    p.sourceHash = Hash([NSData dataWithBytes:source length:strlen(source)]);
    ++factories;
    p.library = [device newLibraryWithSource:@(source) options:options error:&error];
    for (const char *entry : {MAC_METAL_FIXED_VERTEX, MAC_METAL_FIXED_POST_VERTEX, MAC_METAL_FIXED_FRAGMENT,
                            MAC_METAL_FIXED_COLORED, MAC_METAL_FIXED_SOLID, MAC_METAL_FIXED_COPY, MAC_METAL_FIXED_GAMMA}) {
        id<MTLFunction> function = [p.library newFunctionWithName:@(entry)];
        if (!function) throw cod2::arb::Error(error ? error.localizedDescription.UTF8String : "Missing fixed shader entry");
        p.functions.emplace(entry, function);
    }
    return p;
}

std::string Hash(NSData *data)
{
    if (!data || data.length > UINT32_MAX) throw cod2::arb::Error("Invalid shader hash input");
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, static_cast<CC_LONG>(data.length), digest);
    static const char digits[] = "0123456789abcdef";
    std::string result;
    for (unsigned char byte : digest) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result;
}

bool SupportPath(const char *path)
{
    if (!path) return false;
    auto directory = std::filesystem::weakly_canonical(path);
    auto support = std::filesystem::weakly_canonical([NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support"].fileSystemRepresentation);
    auto relative = directory.lexically_relative(support);
    if (relative.empty() || relative == ".") return false;
    for (const auto &part : relative) if (part == "..") return false;
    return true;
}

void Notify(const char *name, int status, const char *detail, void *)
{
    if (status) { lastStatus = status; lastError = std::string(name) + ": " + detail; }
}

bool Equal(const char *text, const std::string &expected)
{ return text && expected == text; }

void Describe(const Prepared &p, MacMetalShaderLibrary *out)
{
    *out = {MAC_METAL_ABI_VERSION, p.stage, p.name.c_str(), p.sourceHash.c_str(),
            p.metadataHash.c_str(), revision, p.path.c_str(), p.entry.c_str(),
            p.reflection.bytes, p.reflection.length};
}

void Release(void *pointer) { delete static_cast<Payload *>(pointer); }

void *AssociateReady(MacShimSidecar *side, const MacMetalShaderLibrary *desc)
{
    if (!side || !desc || desc->abiVersion != MAC_METAL_ABI_VERSION || !desc->name) return nullptr;
    auto it = ready.find(desc->name);
    if (it == ready.end()) return nullptr;
    auto p = it->second;
    MacShimKind kind = MacMetal_SidecarKind(side);
    if ((p->stage == 0 ? kind != MAC_SHIM_VERTEX_SHADER : kind != MAC_SHIM_PIXEL_SHADER) ||
        desc->stage != p->stage || !Equal(desc->sourceSHA256, p->sourceHash) ||
        !Equal(desc->metadataSHA256, p->metadataHash) || !Equal(desc->translatorRevision, revision) ||
        !Equal(desc->libraryPath, p->path) || !Equal(desc->entryPoint, p->entry) ||
        desc->reflectionSize != p->reflection.length || !desc->reflection ||
        memcmp(desc->reflection, p->reflection.bytes, p->reflection.length)) return nullptr;
    id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(MacMetal_GPU());
    if (device && p->library.device.registryID != device.registryID) return nullptr;
    auto *payload = new Payload;
    payload->prepared = p;
    MacMetal_SetSidecarPayload(side, payload, Release);
    return payload;
}
void *Load(MacShimSidecar *side, const MacMetalShaderLibrary *desc)
{
    return sealed ? nullptr : AssociateReady(side, desc);
}
}

int MacMetalShaders_Prepare(const char *sourceCache, const char *translatedCache)
{
    @autoreleasepool {
        lastStatus = 1; lastError.clear();
        if (sealed) { lastError = "Shader preparation requested after loading was sealed"; return 0; }
        int lock = -1;
        try {
            if (!sourceCache || !SupportPath(translatedCache)) throw cod2::arb::Error("Translated cache must be in Application Support");
            NSString *output = [NSString stringWithUTF8String:translatedCache];
            [NSFileManager.defaultManager createDirectoryAtPath:output withIntermediateDirectories:YES attributes:nil error:nil];
            lock = open([output stringByAppendingPathComponent:@".prepare.lock"].fileSystemRepresentation, O_CREAT | O_RDWR, 0600);
            if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) throw cod2::arb::Error("Shader setup cache is busy");
            NSString *input = [NSString stringWithUTF8String:sourceCache];
            NSData *initialManifest = [NSData dataWithContentsOfFile:[input stringByAppendingPathComponent:@"manifest.json"]];
            int status = MacMetal_PrepareShaderCache(sourceCache, translatedCache, Notify, nullptr);
            if (status) { lastStatus = status; if (lastError.empty()) lastError = "Shader translation failed"; throw cod2::arb::Error(lastError); }
            NSData *manifestData = [NSData dataWithContentsOfFile:[input stringByAppendingPathComponent:@"manifest.json"]];
            if (![initialManifest isEqualToData:manifestData]) throw cod2::arb::Error("Shader manifest changed during preparation");
            NSDictionary *manifest = [NSJSONSerialization JSONObjectWithData:manifestData options:0 error:nil];
            NSDictionary *assets = manifest[@"assets"];
            id<MTLDevice> device = Device(); /* First-run setup has no transport yet. */
            if (!device) throw cod2::arb::Error("No Metal device for shader preparation");
            std::map<std::string, std::shared_ptr<Prepared>> pending;
            NSMutableArray *records = [NSMutableArray array];
            MTLCompileOptions *options = [MTLCompileOptions new];
            options.languageVersion = MTLLanguageVersion3_0;
            options.fastMathEnabled = NO;
            for (NSString *name in [[assets allKeys] sortedArrayUsingSelector:@selector(compare:)]) {
                if (![name hasSuffix:@".vsa"] && ![name hasSuffix:@".pse"]) continue;
                @autoreleasepool {
                    NSData *original = [NSData dataWithContentsOfFile:[input stringByAppendingPathComponent:name]];
                    auto translated = cod2::arb::Translate(std::string(static_cast<const char *>(original.bytes), original.length));
                    NSString *path = [output stringByAppendingPathComponent:[name stringByAppendingString:@".metal"]];
                    NSData *msl = [NSData dataWithContentsOfFile:path];
                    if (msl.length != translated.source.size() || memcmp(msl.bytes, translated.source.data(), msl.length)) throw cod2::arb::Error("Translated source identity mismatch");
                    NSString *peer = [[name substringToIndex:name.length - 2] stringByAppendingString:@"c"];
                    NSData *reflection = [NSData dataWithContentsOfFile:[input stringByAppendingPathComponent:peer]];
                    if (Hash(original) != [assets[name] UTF8String] || Hash(reflection) != [assets[peer] UTF8String]) throw cod2::arb::Error("Shader inputs changed during preparation");
                    auto p = std::make_shared<Prepared>();
                    p->name = name.UTF8String; p->stage = [name hasSuffix:@".vsa"] ? 0 : 1;
                    p->sourceHash = Hash(original); p->metadataHash = Hash(reflection);
                    p->path = path.fileSystemRepresentation; p->entry = p->stage ? "arb_fragment" : "arb_vertex";
                    p->reflection = reflection; p->states = translated.program.stateBindings;
                    p->textures = translated.program.textures;
                    NSError *error = nil;
                    NSString *source = [[NSString alloc] initWithData:msl encoding:NSUTF8StringEncoding];
                    ++factories;
                    p->library = [device newLibraryWithSource:source options:options error:&error];
                    p->function = [p->library newFunctionWithName:@(p->entry.c_str())];
                    if (!p->function) throw cod2::arb::Error(p->name + ": " + (error ? error.localizedDescription.UTF8String : "Missing raster entry"));
                    pending.emplace(p->name, p);
                    [records addObject:@{@"name": name, @"stage": @(p->stage), @"source_sha256": @(p->sourceHash.c_str()),
                        @"metadata_sha256": @(p->metadataHash.c_str()), @"msl_sha256": @(Hash(msl).c_str()), @"entry": @(p->entry.c_str())}];
                }
            }
            if (pending.size() != 417) throw cod2::arb::Error("Expected 417 retained raster libraries");
            std::map<unsigned, FixedPrepared> nextFixed;
            nextFixed.emplace(0, CompileFixed(device, MacMetalFixed_Source()));
            NSData *fixedSource = [NSData dataWithBytes:MacMetalFixed_Source() length:strlen(MacMetalFixed_Source())];
            NSDictionary *identity = @{@"abi": @1, @"translator": @(revision), @"strict_math": @YES,
                @"language": @"MSL3.0", @"manifest_sha256": @(Hash(manifestData).c_str()),
                @"fixed_revision": @MAC_METAL_FIXED_REVISION, @"fixed_source_sha256": @(Hash(fixedSource).c_str()),
                @"registry_id": @(device.registryID), @"os_build": NSProcessInfo.processInfo.operatingSystemVersionString,
                @"programs": records};
            NSData *json = [NSJSONSerialization dataWithJSONObject:identity options:NSJSONWritingSortedKeys error:nil];
            std::string nextNamespace = Hash(json);
            if (![json writeToFile:[output stringByAppendingPathComponent:@"shader-provider-v1.json"] options:NSDataWritingAtomic error:nil]) throw cod2::arb::Error("Could not publish shader cache identity");
            ready.swap(pending); fixedReady.swap(nextFixed); cacheNamespace = nextNamespace;
            lastStatus = 0;
            flock(lock, LOCK_UN); close(lock);
            return 1;
        } catch (const std::exception &error) {
            lastError = error.what();
            if (lock >= 0) { flock(lock, LOCK_UN); close(lock); }
            return 0;
        }
    }
}

int MacMetalShaders_SetupAfterExtraction(const char *sourceCache)
{
    @autoreleasepool {
        NSString *destination = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/CoD2 Silicon/metal/shaders-abi1"];
        int success = MacMetalShaders_Prepare(sourceCache, destination.fileSystemRepresentation);
        if (success) fprintf(stderr, "CoD2 Silicon: prepared 417 local Metal shader libraries (ABI 1).\n");
        else fprintf(stderr, "CoD2 Silicon: Metal shader preparation: %s\n", MacMetalShaders_LastError());
        return success;
    }
}

void MacMetalShaders_BeginLoading(void) { sealed = false; }
void MacMetalShaders_SealLoading(void) { sealed = true; }
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
/* Lazy engine material registration only attaches an already verified library.
 * It cannot prepare a cache, create a function or compile a shader. */
void *MacMetalShaders_AssociateReady(MacShimSidecar *side, const MacMetalShaderLibrary *desc)
{ return AssociateReady(side, desc); }
#endif
unsigned int MacMetalShaders_Count(void) { return static_cast<unsigned>(ready.size()); }
int MacMetalShaders_Describe(unsigned int index, MacMetalShaderLibrary *out)
{
    if (!out || index >= ready.size()) return 0;
    auto it = ready.begin(); std::advance(it, index); Describe(*it->second, out); return 1;
}
int MacMetalShaders_Find(const char *name, MacMetalShaderLibrary *out)
{
    if (!name || !out) return 0;
    auto it = ready.find(name);
    if (it == ready.end()) return 0;
    Describe(*it->second, out); return 1;
}
const char *MacMetalShaders_CacheNamespace(void) { return cacheNamespace.c_str(); }
const char *MacMetalShaders_LastError(void) { return lastError.c_str(); }
int MacMetalShaders_LastStatus(void) { return lastStatus; }
uint64_t MacMetalShaders_FactoryCount(void) { return factories; }
int MacMetalShaders_PrepareFixed(const uint32_t *types)
{
    @autoreleasepool {
        unsigned key;
        if (sealed || !FixedKey(types, key)) return 0;
        auto found = fixedReady.find(key);
        if (found != fixedReady.end()) return found->second.library.device.registryID == Device().registryID;
        try {
            std::unique_ptr<char, decltype(&free)> source(types ? MacMetalFixed_SourceForTypes(types) : strdup(MacMetalFixed_Source()), free);
            fixedReady.emplace(key, CompileFixed(Device(), source.get()));
            return 1;
        } catch (const std::exception &error) { lastError = error.what(); return 0; }
    }
}
void *MacMetalShaders_FixedLibrary(const uint32_t *types)
{
    unsigned key;
    if (!FixedKey(types, key)) return nullptr;
    auto found = fixedReady.find(key);
    return found == fixedReady.end() ? nullptr : (__bridge void *)found->second.library;
}
void *MacMetalShaders_FixedFunction(const uint32_t *types, const char *entry)
{
    unsigned key;
    if (!entry || !FixedKey(types, key)) return nullptr;
    auto found = fixedReady.find(key);
    if (found == fixedReady.end()) return nullptr;
    auto function = found->second.functions.find(entry);
    return function == found->second.functions.end() ? nullptr : (__bridge void *)function->second;
}
const char *MacMetalShaders_FixedSourceSHA256(const uint32_t *types)
{
    unsigned key;
    if (!FixedKey(types, key)) return nullptr;
    auto found = fixedReady.find(key);
    return found == fixedReady.end() ? nullptr : found->second.sourceHash.c_str();
}
void *MacMetalShaders_Library(const void *pointer)
{ return pointer ? (__bridge void *)static_cast<const Payload *>(pointer)->prepared->library : nullptr; }
void *MacMetalShaders_Function(const void *pointer)
{ return pointer ? (__bridge void *)static_cast<const Payload *>(pointer)->prepared->function : nullptr; }
int MacMetalShaders_Description(const void *pointer, MacMetalShaderLibrary *out)
{
    if (!pointer || !out) return 0;
    Describe(*static_cast<const Payload *>(pointer)->prepared, out); return 1;
}
int MacMetalShaders_TextureType(const void *pointer, unsigned int slot)
{
    if (!pointer || slot >= 16) return -1;
    const auto &textures = static_cast<const Payload *>(pointer)->prepared->textures;
    auto found = textures.find(slot);
    if (found == textures.end()) return -1;
    return found->second == "CUBE" ? 1 : found->second == "3D" ? 2 : 0;
}
const char *MacMetalShaders_StateBinding(const void *pointer, unsigned int slot)
{
    if (!pointer) return nullptr;
    const auto &states = static_cast<const Payload *>(pointer)->prepared->states;
    return slot < states.size() ? states[slot].c_str() : nullptr;
}
int MacMetalShaders_WriteLocal(void *pointer, unsigned int first, const float *values, unsigned int count)
{
    if (!pointer || first > 256 || count > 256 - first || (!values && count)) return 0;
    if (count) memcpy(static_cast<Payload *>(pointer)->local + first, values, count * sizeof(MacMetalFloat4));
    return 1;
}
int MacMetalShaders_Snapshot(const void *pointer, const MacMetalARBConstants *stage, MacMetalARBConstants *snapshot)
{
    if (!pointer || !stage || !snapshot) return 0;
    *snapshot = *stage;
    memcpy(snapshot->local, static_cast<const Payload *>(pointer)->local, sizeof(snapshot->local));
    return 1;
}
int MacMetalShaders_SnapshotState(const void *pointer, const MacMetalState *state, MacMetalARBConstants *snapshot)
{
    if (!pointer || !state || !snapshot) return 0;
    const auto &p = static_cast<const Payload *>(pointer)->prepared;
    for (const auto &binding : p->states) if (binding != "state.fog.color") return 0;
    if (!MacMetalShaders_Snapshot(pointer, p->stage ? &state->fragment : &state->vertex, snapshot)) return 0;
    for (unsigned slot = 0; slot < p->states.size(); ++slot) {
        uint32_t color = state->render[34];
        snapshot->state[slot] = {{float((color >> 16) & 255) / 255.f, float((color >> 8) & 255) / 255.f,
                                 float(color & 255) / 255.f, float(color >> 24) / 255.f}};
    }
    return 1;
}
__attribute__((constructor)) static void InstallShaders(void)
{
    const MacMetalShaderOps ops = {MAC_METAL_ABI_VERSION, Load, MacMetalShaders_Prepare};
    if (!MacMetal_InstallShaders(&ops)) abort();
}
