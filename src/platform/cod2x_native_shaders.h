/* Shader files derived from the player's own game are written only into the
 * player's home directory. */
#import <Foundation/Foundation.h>

/* The cache holds a complete shader set whose files match its manifest. */
BOOL Cod2xShadersVerify(NSString *cache);

/* Make the cache hold the original shaders. With a game folder, they are
 * built from the game's own HLSL by the bundled generator, replacing a cache
 * from another source. If that is not possible, a cache that already
 * verifies is kept, and otherwise the shaders are taken from the first of
 * the given Mac 1.3 executables that has them. data may be nil to skip
 * building. */
BOOL Cod2xShadersSetup(NSString *data, NSArray<NSString *> *binaries, NSString *cache);
