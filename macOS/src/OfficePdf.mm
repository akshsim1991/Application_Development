#import "OfficePdf.h"
#import <AppKit/AppKit.h>
#import "Trace.h"
#include <algorithm>

@implementation FPOfficePdf

+ (FPOfficeKind)kindOf:(NSURL*)url {
    NSString* e = url.pathExtension.lowercaseString;
    if ([@[ @"doc", @"docx", @"docm", @"dot", @"dotx", @"rtf", @"rtfd", @"odt", @"pages", @"wpd" ] containsObject:e]) return FPOfficeText;
    if ([@[ @"xls", @"xlsx", @"xlsm", @"xlsb", @"ods", @"numbers" ] containsObject:e]) return FPOfficeSheet;
    if ([@[ @"ppt", @"pptx", @"pptm", @"pps", @"ppsx", @"odp", @"key" ] containsObject:e]) return FPOfficeSlides;
    return FPOfficeNone;
}

static NSURL* appWithId(NSString* bundleId) { return [NSWorkspace.sharedWorkspace URLForApplicationWithBundleIdentifier:bundleId]; }

static NSString* quoted(NSString* s) {   // an AppleScript string
    return [NSString stringWithFormat:@"\"%@\"", [[s stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"] stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""]];
}

static BOOL fileMade(NSURL* out) {
    NSDictionary* a = [NSFileManager.defaultManager attributesOfItemAtPath:out.path error:nil];
    if (!a || a.fileSize < 200) return NO;
    NSData* head = [[NSFileHandle fileHandleForReadingAtPath:out.path] readDataOfLength:5];
    return head.length == 5 && memcmp(head.bytes, "%PDF-", 5) == 0;
}

/** An AppleScript run; NO with its error message. */
static BOOL runScript(NSString* source, NSString** error) {
    NSDictionary* err = nil;
    NSAppleScript* s = [[NSAppleScript alloc] initWithSource:source];
    [s executeAndReturnError:&err];
    if (err && error) *error = err[NSAppleScriptErrorMessage] ?: @"the program did not answer";
    return err == nil;
}

/** Microsoft Office for Mac: the document opened, saved as PDF, closed (nothing else of the user's is touched). */
static BOOL viaMicrosoft(FPOfficeKind kind, NSURL* in, NSURL* out, NSString** program, NSString** error) {
    NSString* app = kind == FPOfficeText ? @"com.microsoft.Word" : kind == FPOfficeSheet ? @"com.microsoft.Excel" : @"com.microsoft.Powerpoint";
    if (!appWithId(app)) return NO;
    NSString* script;
    if (kind == FPOfficeText)
        script = [NSString stringWithFormat:@"tell application id \"com.microsoft.Word\"\n"
                   "set d to open file name %@ with read only\n"
                   "save as d file name %@ file format format PDF\n"
                   "close d saving no\nend tell", quoted(in.path), quoted(out.path)];
    else if (kind == FPOfficeSheet)
        script = [NSString stringWithFormat:@"tell application id \"com.microsoft.Excel\"\n"
                   "set w to open workbook workbook file name %@ read only true\n"
                   "save workbook as w filename %@ file format PDF file format\n"
                   "close w saving no\nend tell", quoted(in.path), quoted(out.path)];
    else
        script = [NSString stringWithFormat:@"tell application id \"com.microsoft.Powerpoint\"\n"
                   "open %@\n"
                   "set p to active presentation\n"
                   "save p in %@ as save as PDF\n"
                   "close p saving no\nend tell", quoted(in.path), quoted(out.path)];
    NSString* why = nil;
    if (runScript(script, &why) && fileMade(out)) {
        if (program) *program = kind == FPOfficeText ? @"Microsoft Word" : kind == FPOfficeSheet ? @"Microsoft Excel" : @"Microsoft PowerPoint";
        return YES;
    }
    if (error && why) *error = why;
    return NO;
}

/** LibreOffice, run without a window into a folder of its own, then the PDF moved where it belongs. */
static BOOL viaLibreOffice(NSURL* in, NSURL* out, NSString** program, NSString** error) {
    NSURL* app = appWithId(@"org.libreoffice.script");
    NSString* soffice = app ? [app.path stringByAppendingPathComponent:@"Contents/MacOS/soffice"] : nil;
    if (!soffice || ![NSFileManager.defaultManager isExecutableFileAtPath:soffice]) return NO;
    NSString* dir = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSUUID UUID].UUIDString];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    NSTask* t = [NSTask new];
    t.executableURL = [NSURL fileURLWithPath:soffice];
    // (a profile of its own: a LibreOffice the user has open does not get in the way)
    t.arguments = @[ [@"-env:UserInstallation=file://" stringByAppendingString:[dir stringByAppendingPathComponent:@"profile"]],
                     @"--headless", @"--convert-to", @"pdf", @"--outdir", dir, in.path ];
    t.standardOutput = NSFileHandle.fileHandleWithNullDevice;
    t.standardError = NSFileHandle.fileHandleWithNullDevice;
    NSError* e = nil;
    if (![t launchAndReturnError:&e]) { if (error) *error = e.localizedDescription; return NO; }
    NSDate* until = [NSDate dateWithTimeIntervalSinceNow:120];
    while (t.running && [until timeIntervalSinceNow] > 0) [NSThread sleepForTimeInterval:0.2];
    if (t.running) { [t terminate]; if (error) *error = @"LibreOffice took too long"; return NO; }
    NSURL* made = [[NSURL fileURLWithPath:dir] URLByAppendingPathComponent:[in.lastPathComponent.stringByDeletingPathExtension stringByAppendingPathExtension:@"pdf"]];
    BOOL ok = fileMade(made) && [NSFileManager.defaultManager moveItemAtURL:made toURL:out error:nil];
    [NSFileManager.defaultManager removeItemAtPath:dir error:nil];
    if (ok && program) *program = @"LibreOffice";
    return ok;
}

/** Pages, Numbers or Keynote: the file opened (they read Word, Excel and PowerPoint files), exported as PDF, closed. */
static BOOL viaIWork(FPOfficeKind kind, NSURL* in, NSURL* out, NSString** program, NSString** error) {
    NSString* app = kind == FPOfficeText ? @"com.apple.iWork.Pages" : kind == FPOfficeSheet ? @"com.apple.iWork.Numbers" : @"com.apple.iWork.Keynote";
    if (!appWithId(app)) return NO;
    NSString* script = [NSString stringWithFormat:@"tell application id \"%@\"\n"
                         "set d to open (POSIX file %@)\n"
                         "export d to (POSIX file %@) as PDF\n"
                         "close d saving no\nend tell", app, quoted(in.path), quoted(out.path)];
    NSString* why = nil;
    if (runScript(script, &why) && fileMade(out)) {
        if (program) *program = kind == FPOfficeText ? @"Pages" : kind == FPOfficeSheet ? @"Numbers" : @"Keynote";
        return YES;
    }
    if (error && why) *error = why;
    return NO;
}

/** The Mac's own text system: Word (.docx, .doc), RTF and OpenDocument text, laid out on the document's paper size. */
static BOOL viaTextSystem(NSURL* in, NSURL* out, NSString** program, NSString** error) {
    NSString* e = in.pathExtension.lowercaseString;
    if (![@[ @"doc", @"docx", @"docm", @"dot", @"dotx", @"rtf", @"rtfd", @"odt" ] containsObject:e]) return NO;
    NSDictionary* attrs = nil;
    NSError* err = nil;
    NSAttributedString* text = [[NSAttributedString alloc] initWithURL:in options:@{} documentAttributes:&attrs error:&err];
    if (!text) { if (error) *error = err.localizedDescription ?: @"the document could not be read"; return NO; }
    NSPrintInfo* pi = [[NSPrintInfo alloc] initWithDictionary:@{}];
    NSValue* paper = attrs[NSPaperSizeDocumentAttribute];
    pi.paperSize = paper ? paper.sizeValue : NSMakeSize(595.28, 841.89);   // the document's, else A4
    pi.leftMargin = [attrs[NSLeftMarginDocumentAttribute] doubleValue] ?: 72;
    pi.rightMargin = [attrs[NSRightMarginDocumentAttribute] doubleValue] ?: 72;
    pi.topMargin = [attrs[NSTopMarginDocumentAttribute] doubleValue] ?: 72;
    pi.bottomMargin = [attrs[NSBottomMarginDocumentAttribute] doubleValue] ?: 72;
    pi.horizontalPagination = NSPrintingPaginationModeFit;
    pi.verticalPagination = NSPrintingPaginationModeAutomatic;
    pi.verticallyCentered = NO;
    pi.horizontallyCentered = NO;
    const CGFloat width = pi.paperSize.width - pi.leftMargin - pi.rightMargin;
    NSTextView* view = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, width, pi.paperSize.height)];
    view.textContainerInset = NSZeroSize;
    view.textContainer.lineFragmentPadding = 0;
    view.backgroundColor = NSColor.whiteColor;
    [view.textStorage setAttributedString:text];
    [view.layoutManager ensureLayoutForTextContainer:view.textContainer];
    NSRect used = [view.layoutManager usedRectForTextContainer:view.textContainer];
    [view setFrameSize:NSMakeSize(width, std::max<CGFloat>(used.size.height + 1, pi.paperSize.height - pi.topMargin - pi.bottomMargin))];
    NSPrintOperation* op = [NSPrintOperation PDFOperationWithView:view insideRect:view.bounds toPath:out.path printInfo:pi];
    op.showsPrintPanel = NO;
    op.showsProgressPanel = NO;
    if (![op runOperation] || !fileMade(out)) { if (error) *error = @"the PDF could not be written"; return NO; }
    if (program) *program = @"the Mac's text system";
    return YES;
}

+ (BOOL)convert:(NSURL*)in to:(NSURL*)out program:(NSString**)program error:(NSString**)error {
    const FPOfficeKind kind = [self kindOf:in];
    if (kind == FPOfficeNone) { if (error) *error = @"It is not a Word, Excel or PowerPoint file."; return NO; }
    [NSFileManager.defaultManager removeItemAtURL:out error:nil];
    NSString* why = nil;
    NSString* ext = in.pathExtension.lowercaseString;
    const BOOL apple = [@[ @"pages", @"numbers", @"key" ] containsObject:ext];   // (Apple's own: only its programs read them)
    if (!apple && viaMicrosoft(kind, in, out, program, &why)) return YES;
    if (!apple && viaLibreOffice(in, out, program, &why)) return YES;
    if (viaIWork(kind, in, out, program, &why)) return YES;
    if (viaTextSystem(in, out, program, &why)) return YES;
    if (error) {
        NSString* need = kind == FPOfficeText ? @"Microsoft Word, LibreOffice or Pages" : kind == FPOfficeSheet ? @"Microsoft Excel, LibreOffice or Numbers" : @"Microsoft PowerPoint, LibreOffice or Keynote";
        *error = why ? [NSString stringWithFormat:@"%@ could not make it (%@).", need, why]
                     : [NSString stringWithFormat:@"A Mac makes PDFs of these files with %@; none of them is on this Mac. LibreOffice (libreoffice.org) is free, as are Pages, Numbers and Keynote in the App Store.", need];
    }
    return NO;
}

@end
