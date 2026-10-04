/* Licensed shader bytes are extracted only into the player's home directory. */
#import <Foundation/Foundation.h>

BOOL Cod2xShadersVerify(NSString *cache);
BOOL Cod2xShadersSetup(NSArray<NSString *> *binaries, NSString *cache);
