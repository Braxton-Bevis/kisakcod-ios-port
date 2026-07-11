// iOS path providers. Objective-C++ because NSFileManager/NSBundle is the
// supported way to resolve sandbox directories; results are cached as C strings
// so the engine can hold the pointers for its whole lifetime (it stores
// fs_basepath/fs_homepath dvar strings once at startup).
#import <Foundation/Foundation.h>
#include "sys_ios.h"

static char s_bundlePath[1024];
static char s_documentsPath[1024];
static char s_cachesPath[1024];

static const char *CachePath(char *dst, size_t dstSize, NSString *path)
{
    if (!dst[0] && path)
    {
        [path getCString:dst maxLength:dstSize encoding:NSUTF8StringEncoding];
    }
    return dst;
}

const char *Sys_iOS_BundlePath(void)
{
    return CachePath(s_bundlePath, sizeof(s_bundlePath), [[NSBundle mainBundle] resourcePath]);
}

static NSString *SearchPath(NSSearchPathDirectory dir)
{
    NSArray<NSString *> *paths = NSSearchPathForDirectoriesInDomains(dir, NSUserDomainMask, YES);
    return paths.count ? paths[0] : nil;
}

const char *Sys_iOS_DocumentsPath(void)
{
    return CachePath(s_documentsPath, sizeof(s_documentsPath), SearchPath(NSDocumentDirectory));
}

const char *Sys_iOS_CachesPath(void)
{
    return CachePath(s_cachesPath, sizeof(s_cachesPath), SearchPath(NSCachesDirectory));
}
