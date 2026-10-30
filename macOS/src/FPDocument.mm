#import "FPDocument.h"
#import "WindowController.h"
#include "platform.hpp"
#import "Trace.h"

@implementation FPDocument {
    std::unique_ptr<Document> _pdf;
}

// The Mac version only reads documents so far; it never writes them back.
+ (BOOL)autosavesInPlace { return NO; }
- (BOOL)isDocumentEdited { return NO; }

- (Document*)pdf { return _pdf.get(); }

- (void)makeWindowControllers {
    FPTrace(@"makeWindowControllers %@", self.displayName);
    @try {
        [self addWindowController:[[FPWindowController alloc] init]];
    } @catch (NSException* e) {
        FPTrace(@"window could not be made: %@ %@\n%@", e.name, e.reason, e.callStackSymbols);
        @throw;
    }
    FPTrace(@"window controllers %lu", (unsigned long)self.windowControllers.count);
}

- (void)showWindows {
    FPTrace(@"showWindows %@", self.displayName);
    [super showWindows];
}

// Asks for the password of a protected file. nil when the user cancels.
static NSString* askPassword(NSString* name, BOOL again) {
    NSAlert* a = [[NSAlert alloc] init];
    a.messageText = again ? [NSString stringWithFormat:@"That password did not open “%@”.", name]
                          : [NSString stringWithFormat:@"“%@” is protected.", name];
    a.informativeText = @"Enter the password to open it.";
    [a addButtonWithTitle:@"Open"];
    [a addButtonWithTitle:@"Cancel"];
    NSSecureTextField* field = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 260, 24)];
    a.accessoryView = field;
    a.window.initialFirstResponder = field;
    return [a runModal] == NSAlertFirstButtonReturn ? field.stringValue : nil;
}

- (BOOL)readFromURL:(NSURL*)url ofType:(NSString*)type error:(NSError**)error {
    auto doc = std::make_unique<Document>();
    const std::wstring path = platform::wide(url.fileSystemRepresentation);
    std::string password;
    OpenResult r = doc->open(path, password);
    while (r == OpenResult::PasswordNeeded || r == OpenResult::WrongPassword) {
        NSString* pw = askPassword(url.lastPathComponent, r == OpenResult::WrongPassword);
        if (!pw) {
            if (error) *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSUserCancelledError userInfo:nil];
            return NO;
        }
        password = pw.UTF8String;
        r = doc->open(path, password);
    }
    if (r != OpenResult::Ok) {
        // The same words as the Windows app (Document::describe).
        NSString* why = [NSString stringWithUTF8String:platform::utf8(Document::describe(r, path)).c_str()];
        if (error) *error = [NSError errorWithDomain:@"FeatherPDF" code:static_cast<NSInteger>(r)
                                            userInfo:@{NSLocalizedDescriptionKey : why ?: @"The file could not be opened."}];
        return NO;
    }
    _pdf = std::move(doc);
    return YES;
}

@end
