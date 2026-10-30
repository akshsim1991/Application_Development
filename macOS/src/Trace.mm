#import "Trace.h"
#include <cstdio>
#include <cstdlib>

void FPTrace(NSString* format, ...) {
    const char* dir = getenv("FEATHER_UI_CHECK");
    if (!dir) return;
    va_list args;
    va_start(args, format);
    NSString* line = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);
    NSString* path = [[NSString stringWithUTF8String:dir] stringByAppendingPathComponent:@"trace.txt"];
    [[NSFileManager defaultManager] createDirectoryAtPath:[path stringByDeletingLastPathComponent] withIntermediateDirectories:YES
                                               attributes:nil error:nil];
    if (FILE* f = fopen(path.fileSystemRepresentation, "a")) {
        fprintf(f, "%s\n", line.UTF8String);
        fclose(f);
    }
}
