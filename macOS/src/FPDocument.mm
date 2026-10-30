#import "FPDocument.h"
#import "WindowController.h"
#include "platform.hpp"
#import "Trace.h"
#import "Settings.h"

NSNotificationName const FPDocumentReloaded = @"FPDocumentReloaded";

@implementation FPDocument {
    std::unique_ptr<Document> _pdf;
    NSString* _name;   // an unsaved new document's name
}

+ (FPDocument*)openNewWithBytes:(std::vector<unsigned char>)bytes name:(NSString*)name {
    NSError* e = nil;
    FPDocument* doc = [[FPDocument alloc] initWithType:@"com.adobe.pdf" error:&e];
    if (!doc) return nil;
    auto pdf = std::make_unique<Document>();
    if (pdf->load(std::move(bytes), {}) != OpenResult::Ok) return nil;
    pdf->setName(platform::wide(name.UTF8String ?: "New PDF.pdf"));
    pdf->markModified();
    doc->_pdf = std::move(pdf);
    doc->_name = name;
    [[NSDocumentController sharedDocumentController] addDocument:doc];
    [doc makeWindowControllers];
    [doc showWindows];
    [doc changed];
    return doc;
}

- (NSString*)displayName { return self.fileURL || !_name ? [super displayName] : _name; }

// Saving as on Windows: Save writes the file in place, Save As to a new one, and closing or quitting with changes asks
// first. (Not macOS's automatic saving: a PDF changes only when the user saves it.)
+ (BOOL)autosavesInPlace { return NO; }
- (BOOL)isDocumentEdited { return _pdf && _pdf->dirty(); }
- (BOOL)hasUndoManager { return NO; }   // undo is the engine's own (30 steps, as on Windows)

- (void)changed { [self updateChangeCount:NSChangeDone]; }

- (NSData*)dataOfType:(NSString*)type error:(NSError**)error {
    std::vector<unsigned char> bytes;
    if (!_pdf || !_pdf->bytesToSave(bytes)) {
        if (error) *error = [NSError errorWithDomain:@"FeatherPDF" code:1
                                            userInfo:@{NSLocalizedDescriptionKey : @"The document could not be written. Nothing was saved."}];
        return nil;
    }
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}

- (void)saveToURL:(NSURL*)url ofType:(NSString*)type forSaveOperation:(NSSaveOperationType)op completionHandler:(void (^)(NSError*))done {
    [super saveToURL:url ofType:type forSaveOperation:op completionHandler:^(NSError* e) {
        if (!e && op != NSSaveToOperation && self->_pdf) self->_pdf->savedAs(platform::wide(url.fileSystemRepresentation));
        if (!e) { [self updateChangeCount:NSChangeCleared]; FPRecoveryForget(self); }
        done(e);
    }];
}

- (BOOL)revertToContentsOfURL:(NSURL*)url ofType:(NSString*)type error:(NSError**)error {
    if (![super revertToContentsOfURL:url ofType:type error:error]) return NO;
    [[NSNotificationCenter defaultCenter] postNotificationName:FPDocumentReloaded object:self];
    return YES;
}

- (Document*)pdf { return _pdf.get(); }

- (void)close {
    FPRecoveryForget(self);
    [super close];
}

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
