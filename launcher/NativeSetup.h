#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN
NSString *LauncherHome(void);
void LauncherMigrate(void);
NSString * _Nullable LauncherFindData(NSString * _Nullable selected);
BOOL LauncherKeyValid(NSString *key);
BOOL LauncherPrepareShaders(NSString *data, NSString * _Nullable selected);
BOOL LauncherVerifyShaders(void);
NSDictionary<NSString *, NSString *> * _Nullable LauncherParseLink(NSString *link);
NS_ASSUME_NONNULL_END
