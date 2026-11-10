#if defined(__APPLE__) && defined(COD2_X64) && COD2_X64
#import "cod2x_native_shaders.h"
#include <CommonCrypto/CommonDigest.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static uint32_t Read32(const unsigned char *bytes)
{
    return bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static uint32_t ReadBig32(const unsigned char *bytes)
{
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

/* Mac 1.3 shipped as a Universal Binary for PowerPC and Intel. Use its i386
 * slice; a file that is not universal is passed through unchanged. */
static NSData *ShaderIntelSlice(NSData *data)
{
    const unsigned char *bytes = data.bytes;
    NSUInteger length = data.length;
    if (length < 8 || ReadBig32(bytes) != 0xcafebabe) return data;
    uint32_t count = ReadBig32(bytes + 4);
    if (!count || count > 16 || 8 + (uint64_t)count * 20 > length) return nil;
    for (uint32_t i = 0; i < count; ++i) {
        const unsigned char *arch = bytes + 8 + i * 20;
        uint32_t offset = ReadBig32(arch + 8), size = ReadBig32(arch + 12);
        if (ReadBig32(arch) == 7 && (uint64_t)offset + size <= length)
            return [data subdataWithRange:NSMakeRange(offset, size)];
    }
    return nil;
}

static void Write16(NSMutableData *data, NSUInteger offset, unsigned value)
{
    unsigned char *bytes = data.mutableBytes;
    bytes[offset] = value; bytes[offset + 1] = value >> 8;
}

static void Write32(NSMutableData *data, NSUInteger offset, uint32_t value)
{
    Write16(data, offset, value); Write16(data, offset + 2, value >> 16);
}

static NSString *ShaderSHA256(NSData *data)
{
    if (!data) return nil;
    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);
    const unsigned char *bytes = data.bytes;
    NSUInteger left = data.length;
    while (left) {
        CC_LONG count = (CC_LONG)MIN(left, (NSUInteger)UINT32_MAX);
        CC_SHA256_Update(&context, bytes, count);
        bytes += count; left -= count;
    }
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Final(digest, &context);
    char hex[65];
    for (unsigned i = 0; i < sizeof(digest); ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return [NSString stringWithUTF8String:hex];
}

static BOOL ShaderMatch(id value, NSString *pattern)
{
    if (![value isKindOfClass:NSString.class]) return NO;
    return [value rangeOfString:pattern options:NSRegularExpressionSearch].location != NSNotFound;
}

static BOOL ShaderName(id name)
{
    return ShaderMatch(name, @"\\A[A-Za-z_0-9]+\\.(vsa|pse|vc|pc)\\z");
}

BOOL Cod2xShadersVerify(NSString *cache)
{
    NSData *json = [NSData dataWithContentsOfFile:[cache stringByAppendingPathComponent:@"manifest.json"]];
    id manifest = json ? [NSJSONSerialization JSONObjectWithData:json options:0 error:nil] : nil;
    if (![manifest isKindOfClass:NSDictionary.class]) return NO;
    id assets = manifest[@"assets"];
    if (![assets isKindOfClass:NSDictionary.class] || [assets count] != 834 ||
        !ShaderMatch(manifest[@"binary_sha256"], @"\\A[0-9a-f]{64}\\z")) return NO;
    for (id name in assets) {
        if (!ShaderName(name) || !ShaderMatch(assets[name], @"\\A[0-9a-f]{64}\\z")) return NO;
        if (![ShaderSHA256([NSData dataWithContentsOfFile:[cache stringByAppendingPathComponent:name]])
                isEqualToString:assets[name]]) return NO;
        if (([name hasSuffix:@".vsa"] || [name hasSuffix:@".pse"]) &&
            !assets[[[name substringToIndex:[name length] - 2] stringByAppendingString:@"c"]]) return NO;
    }
    return YES;
}

static NSData *ShaderConstants(NSString *text)
{
    NSMutableArray *tokens = [NSMutableArray array];
    for (NSString *word in [text componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet])
        if (word.length) [tokens addObject:word];
    if (!tokens.count || !ShaderMatch(tokens[0], @"\\A[0-9]+\\z")) return nil;
    unsigned count = [tokens[0] intValue];
    if (count > 256 || tokens.count != 1 + count * 6) return nil;
    NSMutableData *result = [NSMutableData dataWithLength:28 + count * 36];
    Write32(result, 0, 28); Write32(result, 12, count); Write32(result, 16, 28);
    for (unsigned i = 0; i < count; ++i) {
        NSString *name = tokens[1 + i * 6];
        if (!ShaderMatch(name, @"\\A[A-Za-z_][A-Za-z_0-9]*\\z")) return nil;
        unsigned fields[5];
        for (unsigned j = 0; j < 5; ++j) {
            NSString *word = tokens[2 + i * 6 + j];
            if (!ShaderMatch(word, @"\\A[0-9]+\\z") || word.length > 5 || word.intValue > 65535) return nil;
            fields[j] = word.intValue;
        }
        unsigned type = 28 + count * 20 + i * 16, record = 28 + i * 20;
        Write32(result, record, (uint32_t)result.length);
        Write16(result, record + 6, fields[0]); Write16(result, record + 8, fields[1]);
        Write32(result, record + 12, type);
        Write16(result, type, fields[2]); Write16(result, type + 2, fields[3]);
        Write16(result, type + 8, fields[4]);
        [result appendData:[name dataUsingEncoding:NSASCIIStringEncoding]];
        const char zero = 0; [result appendBytes:&zero length:1];
    }
    return result;
}

typedef struct { uint32_t address, size, offset; } ShaderSection;

static NSString *ShaderCString(NSData *data, ShaderSection strings, uint32_t address)
{
    if (address < strings.address || (uint64_t)address >= (uint64_t)strings.address + strings.size) return @"";
    NSUInteger offset = strings.offset + (NSUInteger)(address - strings.address);
    const char *bytes = (const char *)data.bytes + offset;
    const char *end = memchr(bytes, 0, strings.size - (address - strings.address));
    if (!end) return nil;
    return [[NSString alloc] initWithBytes:bytes length:end - bytes encoding:NSASCIIStringEncoding];
}

static NSDictionary *ShaderExtract(NSData *data)
{
    const unsigned char *bytes = data.bytes;
    NSUInteger length = data.length;
    if (length < 28 || Read32(bytes) != 0xfeedface || Read32(bytes + 4) != 7) return nil;
    uint32_t commands = Read32(bytes + 16), commandBytes = Read32(bytes + 20);
    if ((uint64_t)28 + commandBytes > length) return nil;
    ShaderSection strings = {0}, text = {0};
    uint32_t textIndex = 0, sectionIndex = 0, symoff = 0, symbols = 0, stroff = 0, strsize = 0;
    NSUInteger offset = 28;
    for (uint32_t i = 0; i < commands; ++i) {
        if (offset + 8 > 28 + commandBytes) return nil;
        uint32_t cmd = Read32(bytes + offset), size = Read32(bytes + offset + 4);
        if (size < 8 || size > 28 + commandBytes - offset) return nil;
        if (cmd == 1) {
            if (size < 56) return nil;
            uint32_t count = Read32(bytes + offset + 48);
            if ((uint64_t)56 + count * (uint64_t)68 > size) return nil;
            for (uint32_t j = 0; j < count; ++j) {
                const unsigned char *s = bytes + offset + 56 + j * 68;
                ShaderSection section = {Read32(s + 32), Read32(s + 36), Read32(s + 40)};
                ++sectionIndex;
                if (!strncmp((const char *)s, "__text", 16) || !strncmp((const char *)s, "__cstring", 16)) {
                    if ((uint64_t)section.offset + section.size > length) return nil;
                    if (!strncmp((const char *)s, "__text", 16)) { text = section; textIndex = sectionIndex; }
                    else strings = section;
                }
            }
        } else if (cmd == 2) {
            if (size < 24) return nil;
            symoff = Read32(bytes + offset + 8); symbols = Read32(bytes + offset + 12);
            stroff = Read32(bytes + offset + 16); strsize = Read32(bytes + offset + 20);
        }
        offset += size;
    }
    if (!text.size || !strings.size || !symbols || (uint64_t)symoff + (uint64_t)symbols * 12 > length ||
        (uint64_t)stroff + strsize > length) return nil;
    uint32_t address = 0, end = UINT32_MAX;
    for (uint32_t i = 0; i < symbols; ++i) {
        const unsigned char *s = bytes + symoff + (NSUInteger)i * 12;
        if ((s[4] & 0xe0) || (s[4] & 0xe) != 0xe) continue;
        uint32_t name = Read32(s);
        if (name >= strsize || !memchr(bytes + stroff + name, 0, strsize - name)) return nil;
        if (!strcmp((const char *)bytes + stroff + name, "_D3DXCompileShader")) address = Read32(s + 8);
    }
    if (address < text.address || (uint64_t)address >= (uint64_t)text.address + text.size) return nil;
    for (uint32_t i = 0; i < symbols; ++i) {
        const unsigned char *s = bytes + symoff + (NSUInteger)i * 12;
        uint32_t value = Read32(s + 8);
        if (!(s[4] & 0xe0) && (s[4] & 0xe) == 0xe && s[5] == textIndex && value > address && value < end) end = value;
    }
    if (end == UINT32_MAX || (uint64_t)end > (uint64_t)text.address + text.size) return nil;
    NSUInteger start = text.offset + (NSUInteger)(address - text.address), stop = start + (end - address);
    NSMutableDictionary *assets = [NSMutableDictionary dictionary];
    uint32_t previous = 0;
    for (NSUInteger i = start; i + 8 <= stop; ++i) {
        if (memcmp(bytes + i, "\xc7\x44\x24\x04", 4)) continue;
        uint32_t current = Read32(bytes + i + 4);
        NSString *name = ShaderCString(data, strings, current);
        if (ShaderName(name)) {
            NSString *payload = ShaderCString(data, strings, previous);
            if (!payload || ([name hasSuffix:@".vsa"] && ![payload hasPrefix:@"!!ARBvp1.0"]) ||
                ([name hasSuffix:@".pse"] && ![payload hasPrefix:@"!!ARBfp1.0"])) return nil;
            NSData *value = ([name hasSuffix:@".vc"] || [name hasSuffix:@".pc"]) ?
                ShaderConstants(payload) : [payload dataUsingEncoding:NSASCIIStringEncoding];
            if (!value || (assets[name] && ![assets[name] isEqualToData:value])) return nil;
            assets[name] = value;
        }
        previous = current;
        i += 7; /* Python's instruction-pattern matches do not overlap. */
    }
    if (assets.count != 834) return nil;
    for (NSString *name in assets)
        if (([name hasSuffix:@".vsa"] || [name hasSuffix:@".pse"]) &&
            !assets[[[name substringToIndex:name.length - 2] stringByAppendingString:@"c"]]) return nil;
    return assets;
}

/* Where the bundled generator lives: in the game app, which the launcher app
 * contains, or beside a development build's executable. */
static NSString *ShaderGenerator(void)
{
    NSMutableArray<NSString *> *candidates = [NSMutableArray array];
    const char *override = getenv("COD2_SHADERGEN");
    if (override && *override) [candidates addObject:[NSString stringWithUTF8String:override]];
    NSString *bundle = NSBundle.mainBundle.bundlePath;
    [candidates addObject:[bundle stringByAppendingPathComponent:@"Contents/Helpers/cod2-shadergen"]];
    [candidates addObject:[bundle stringByAppendingPathComponent:@"Contents/Helpers/CoD2 Game.app/Contents/Helpers/cod2-shadergen"]];
    NSString *folder = NSBundle.mainBundle.executablePath.stringByDeletingLastPathComponent;
    if (folder) [candidates addObject:[folder stringByAppendingPathComponent:@"cod2-shadergen"]];
    for (NSString *candidate in candidates)
        if ([NSFileManager.defaultManager isExecutableFileAtPath:candidate]) return candidate;
    return nil;
}

/* The generator's standard output, or nil if it did not run to a clean exit. */
static NSString *ShaderRun(NSString *tool, NSArray<NSString *> *arguments)
{
    NSTask *task = [[NSTask alloc] init];
    NSPipe *pipe = [NSPipe pipe];
    task.executableURL = [NSURL fileURLWithPath:tool];
    task.arguments = arguments;
    task.standardOutput = pipe;
    if (![task launchAndReturnError:nil]) return nil;
    NSData *output = [pipe.fileHandleForReading readDataToEndOfFile];
    [task waitUntilExit];
    if (task.terminationReason != NSTaskTerminationReasonExit || task.terminationStatus != 0) return nil;
    NSString *text = [[NSString alloc] initWithData:output encoding:NSUTF8StringEncoding];
    return [text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
}

/* Which generator version built the cache; nil for one taken from a Mac executable. */
static NSString *ShaderCacheGenerator(NSString *cache)
{
    NSData *json = [NSData dataWithContentsOfFile:[cache stringByAppendingPathComponent:@"manifest.json"]];
    id manifest = json ? [NSJSONSerialization JSONObjectWithData:json options:0 error:nil] : nil;
    id generator = [manifest isKindOfClass:NSDictionary.class] ? manifest[@"generator"] : nil;
    return [generator isKindOfClass:NSString.class] ? generator : nil;
}

/* Swap a verified staging folder into place, keeping the old cache if the swap fails. */
static BOOL ShaderInstall(NSString *staging, NSString *cache)
{
    NSFileManager *files = NSFileManager.defaultManager;
    if (!Cod2xShadersVerify(staging)) return NO;
    NSString *backup = [staging stringByAppendingString:@"-old"];
    BOOL existed = [files fileExistsAtPath:cache], installed = NO;
    if (!existed || [files moveItemAtPath:cache toPath:backup error:nil]) {
        installed = [files moveItemAtPath:staging toPath:cache error:nil];
        if (!installed && existed) [files moveItemAtPath:backup toPath:cache error:nil];
        if (installed) [files removeItemAtPath:backup error:nil];
    }
    return installed;
}

BOOL Cod2xShadersSetup(NSString *data, NSArray<NSString *> *binaries, NSString *cache)
{
    NSFileManager *files = NSFileManager.defaultManager;
    NSString *parent = cache.stringByDeletingLastPathComponent;
    if (![files createDirectoryAtPath:parent withIntermediateDirectories:YES attributes:nil error:nil]) return NO;
    int lock = open([parent stringByAppendingPathComponent:@".shader-setup.lock"].fileSystemRepresentation, O_CREAT | O_RDWR, 0600);
    if (lock < 0) return NO;
    BOOL verified = NO;
    if (flock(lock, LOCK_EX) != 0) { close(lock); return NO; }
    if (Cod2xShadersVerify(cache)) verified = YES;
    /* Build from the game's own HLSL, as the Windows game does when it loads. */
    NSString *tool = data ? ShaderGenerator() : nil;
    NSString *version = tool ? ShaderRun(tool, @[@"--version"]) : nil;
    if (version.length && !(verified && [ShaderCacheGenerator(cache) isEqualToString:version])) {
        NSString *staging = [parent stringByAppendingPathComponent:[@".shaders-" stringByAppendingString:NSUUID.UUID.UUIDString]];
        if (ShaderRun(tool, @[[data stringByAppendingPathComponent:@"main"], staging]) && ShaderInstall(staging, cache)) {
            verified = YES;
            fprintf(stderr, "CoD2 Silicon: built the shaders from the game's HLSL (%s).\n", version.UTF8String);
        } else {
            fprintf(stderr, "CoD2 Silicon: the shaders could not be built from the game's HLSL.\n");
        }
        [files removeItemAtPath:staging error:nil];
    }
    for (NSString *binary in binaries) {
        if (verified) break;
        NSData *input = [NSData dataWithContentsOfFile:binary options:NSDataReadingMappedIfSafe error:nil];
        NSData *slice = input ? ShaderIntelSlice(input) : nil;
        NSDictionary *assets = slice ? ShaderExtract(slice) : nil;
        if (!assets) continue;
        NSString *staging = [parent stringByAppendingPathComponent:[@".shaders-" stringByAppendingString:NSUUID.UUID.UUIDString]];
        if (![files createDirectoryAtPath:staging withIntermediateDirectories:NO attributes:nil error:nil]) continue;
        NSArray *names = [[assets allKeys] sortedArrayUsingSelector:@selector(compare:)];
        /* Match the developer tool's manifest byte-for-byte as well as its payloads. */
        NSMutableString *manifest = [NSMutableString stringWithFormat:@"{\n  \"binary_sha256\": \"%@\",\n  \"assets\": {\n", ShaderSHA256(input)];
        BOOL written = YES;
        for (NSUInteger i = 0; i < names.count; ++i) {
            NSString *name = names[i];
            written &= [assets[name] writeToFile:[staging stringByAppendingPathComponent:name] atomically:YES];
            [manifest appendFormat:@"    \"%@\": \"%@\"%@\n", name, ShaderSHA256(assets[name]), i + 1 < names.count ? @"," : @""];
        }
        [manifest appendString:@"  }\n}\n"];
        written &= [manifest writeToFile:[staging stringByAppendingPathComponent:@"manifest.json"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
        if (written) verified = ShaderInstall(staging, cache);
        [files removeItemAtPath:staging error:nil];
    }
    flock(lock, LOCK_UN); close(lock);
    if (verified) fprintf(stderr, "CoD2 Silicon: verified 834 shader/constant files with SHA-256.\n");
    return verified;
}
#endif
