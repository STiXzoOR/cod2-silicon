#include "shader_setup.h"
#include "arb_to_msl.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <CommonCrypto/CommonDigest.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>

namespace {
std::string Digest(NSData *data)
{
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, static_cast<CC_LONG>(data.length), hash);
    std::ostringstream out;
    for (unsigned char byte : hash) out << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    return out.str();
}

uint32_t U32(NSData *data, size_t offset)
{
    if (offset > data.length || data.length - offset < 4) throw cod2::arb::Error("Truncated constant metadata");
    const auto *p = static_cast<const unsigned char *>(data.bytes) + offset;
    return p[0] | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

void CheckMetadata(NSData *data)
{
    if (!data || data.length < 28 || U32(data, 0) != 28) throw cod2::arb::Error("Invalid constant metadata");
    size_t count = U32(data, 12), offset = U32(data, 16);
    if (count > 256 || offset > data.length || count > (data.length - offset) / 20) throw cod2::arb::Error("Invalid constant records");
    for (size_t n = 0; n < count; ++n) {
        size_t record = offset + n * 20;
        size_t name = U32(data, record), type = U32(data, record + 12);
        if (name >= data.length || !memchr(static_cast<const char *>(data.bytes) + name, 0, data.length - name) ||
            type > data.length || data.length - type < 16) throw cod2::arb::Error("Invalid constant offsets");
        /* Register/count share one little-endian word (CTAB retains ILP32 disk layout). */
        uint32_t range = U32(data, record + 4);
        unsigned start = range >> 16, size = U32(data, record + 8) & 65535;
        if (start + size > 256) throw cod2::arb::Error("Invalid constant register range");
    }
}

bool InsideRepository(std::filesystem::path output)
{
    for (; !output.empty(); output = output.parent_path()) {
        if (std::filesystem::exists(output / "docs/macos-port/PLAN.md")) return true;
        if (output == output.parent_path()) break;
    }
    return false;
}
}

int MacMetal_PrepareShaderCache(const char *input, const char *output, MacMetalShaderResult callback, void *context)
{
    auto notify = [&](const std::string &name, int status, const std::string &detail) {
        if (callback) callback(name.c_str(), status, detail.c_str(), context);
    };
    @autoreleasepool {
        try {
            if (!input || !output) throw cod2::arb::Error("Input and output cache paths are required");
            std::filesystem::path destination = std::filesystem::weakly_canonical(output);
            if (InsideRepository(destination)) throw cod2::arb::Error("Refusing game-derived outputs inside a repository");
            auto support = std::filesystem::weakly_canonical([NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support"].fileSystemRepresentation);
            auto relative = destination.lexically_relative(support);
            if (relative.empty() || relative == ".") throw cod2::arb::Error("Shader outputs require an Application Support subdirectory");
            for (const auto &part : relative)
                if (part == "..") throw cod2::arb::Error("Shader outputs require an Application Support subdirectory");
            NSString *root = [NSString stringWithUTF8String:input];
            NSData *manifestData = [NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"manifest.json"]];
            if (!manifestData) { notify("cache", 77, "SKIP: local licensed shader cache is absent"); return 77; }
            NSError *error = nil;
            id manifest = [NSJSONSerialization JSONObjectWithData:manifestData options:0 error:&error];
            id assets = [manifest isKindOfClass:NSDictionary.class] ? manifest[@"assets"] : nil;
            if (![assets isKindOfClass:NSDictionary.class] || [assets count] != 834) throw cod2::arb::Error("Expected an 834-payload shader manifest");
            NSRegularExpression *namePattern = [NSRegularExpression regularExpressionWithPattern:@"^[A-Za-z_0-9]+\\.(vsa|pse|vc|pc)$" options:0 error:nil];
            unsigned programs = 0, constants = 0;
            for (id name in assets) {
                if (![name isKindOfClass:NSString.class] || [namePattern numberOfMatchesInString:name options:0 range:NSMakeRange(0, [name length])] != 1) throw cod2::arb::Error("Invalid manifest path");
                NSData *data = [NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:name]];
                id expected = assets[name];
                if (!data || ![expected isKindOfClass:NSString.class] || Digest(data) != [expected UTF8String]) throw cod2::arb::Error("Cache digest mismatch: " + std::string([name UTF8String]));
                if ([name hasSuffix:@".vc"] || [name hasSuffix:@".pc"]) { CheckMetadata(data); ++constants; }
                else {
                    NSString *peer = [[name substringToIndex:[name length] - 2] stringByAppendingString:@"c"];
                    if (!assets[peer]) throw cod2::arb::Error("Missing paired constant metadata");
                    ++programs;
                }
            }
            if (programs != 417 || constants != 417) throw cod2::arb::Error("Expected 417 programs and 417 constant tables");
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            if (!device) throw cod2::arb::Error("No Metal device available");
            std::filesystem::create_directories(destination);
            unsigned failures = 0;
            for (NSString *name in [[assets allKeys] sortedArrayUsingSelector:@selector(compare:)]) {
                if (![name hasSuffix:@".vsa"] && ![name hasSuffix:@".pse"]) continue;
                @autoreleasepool {
                    try {
                        NSData *data = [NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:name]];
                        auto translation = cod2::arb::Translate(std::string(static_cast<const char *>(data.bytes), data.length));
                        bool vertex = [name hasSuffix:@".vsa"];
                        if (vertex != (translation.program.stage == cod2::arb::Stage::Vertex)) throw cod2::arb::Error("Program stage does not match cache extension");
                        NSString *source = [[NSString alloc] initWithBytes:translation.source.data() length:translation.source.size() encoding:NSUTF8StringEncoding];
                        MTLCompileOptions *options = [MTLCompileOptions new];
                        options.languageVersion = MTLLanguageVersion3_0;
                        options.fastMathEnabled = NO;
                        for (int test = 0; test < 2; ++test) {
                            options.preprocessorMacros = test ? @{@"ARB_SEMANTIC_TEST": @1} : @{};
                            id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
                            if (!library) throw cod2::arb::Error(std::string(test ? "Test: " : "Render: ") + error.localizedDescription.UTF8String);
                            NSString *entry = test ? @"arb_test" : vertex ? @"arb_vertex" : @"arb_fragment";
                            if (![library newFunctionWithName:entry]) throw cod2::arb::Error("Missing compiled entry point");
                        }
                        std::ofstream file(destination / (std::string(name.UTF8String) + ".metal"));
                        if (!(file << translation.source)) throw cod2::arb::Error("Could not cache translated source");
                        std::string detail = "render + semantic libraries compiled";
                        for (const auto &assumption : translation.program.assumptions) detail += "; UNVERIFIED: " + assumption;
                        notify(name.UTF8String, 0, detail);
                    } catch (const std::exception &e) {
                        ++failures;
                        notify(name.UTF8String, 1, e.what());
                    }
                }
            }
            notify("summary", failures ? 1 : 0, std::to_string(programs - failures) + "/417 programs; 417/417 metadata; failures=" + std::to_string(failures));
            return failures ? 1 : 0;
        } catch (const std::exception &e) {
            notify("setup", 1, e.what());
            return 1;
        }
    }
}
