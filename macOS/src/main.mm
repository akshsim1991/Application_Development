// Feather PDF for macOS. Started with one of the test commands it runs it and ends (no window), so the Mac build is
// checked with the same tests as the Windows app:
//   --render in.pdf page out.png [zoom] [normal|dark|dimmed]   draws a page as the window does, into a PNG
//   --tables / --tableedit / --hfremove ...                     the engine's test commands (shared/selftest.hpp)
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import "AppDelegate.h"
#import "Settings.h"
#import "Render.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "document.hpp"
#include "pdfium_api.hpp"
#include "platform.hpp"
#include "cli.hpp"

void FPInstallOcrEngine();   // OcrVision.mm
void FPInstallSignSystem();  // Signatures.mm

// Microsoft Office keeps its fonts (Cambria, Calibri, Aptos...) inside its own apps, where other apps do not see them.
// PDFs made with Office use them, so edited text can only keep its font if Feather PDF finds them: they are made known
// to this app alone (nothing is installed or changed on the Mac).
static void registerOfficeFonts() {
    NSFileManager* fm = NSFileManager.defaultManager;
    for (NSString* app in @[ @"Microsoft Word", @"Microsoft Excel", @"Microsoft PowerPoint", @"Microsoft Outlook", @"Microsoft OneNote" ]) {
        NSString* dir = [NSString stringWithFormat:@"/Applications/%@.app/Contents/Resources/DFonts", app];
        NSMutableArray<NSURL*>* urls = [NSMutableArray array];
        for (NSString* f in [fm contentsOfDirectoryAtPath:dir error:nil]) {
            NSString* ext = f.pathExtension.lowercaseString;
            if ([@[ @"ttf", @"ttc", @"otf" ] containsObject:ext]) [urls addObject:[NSURL fileURLWithPath:[dir stringByAppendingPathComponent:f]]];
        }
        if (urls.count) CTFontManagerRegisterFontURLs((__bridge CFArrayRef)urls, kCTFontManagerScopeProcess, true, nil);
    }
}

static int renderCommand(const std::vector<std::string>& a) {
    if (a.size() < 4) return 2;
    Document doc;
    if (doc.open(platform::wide(a[1])) != OpenResult::Ok) return 3;
    const int page = std::atoi(a[2].c_str()) - 1;
    if (page < 0 || page >= doc.pageCount()) return 5;
    const double zoom = a.size() > 4 ? std::atof(a[4].c_str()) : 1.0;
    const pagecolours::Mode mode = a.size() > 5 && a[5] == "dark" ? pagecolours::Mode::Dark
                                 : a.size() > 5 && a[5] == "dimmed" ? pagecolours::Mode::Dimmed : pagecolours::Mode::Normal;
    const int w = static_cast<int>(std::lround(doc.pageSize(page).w * zoom));
    const int h = static_cast<int>(std::lround(doc.pageSize(page).h * zoom));
    CGImageRef img = FPRenderPage(doc, page, w, h, 0, 0, w, h, mode);
    const bool ok = FPWritePNG(img, a[3].c_str());
    if (img) CGImageRelease(img);
    return ok ? 0 : 4;
}

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
        // macOS adds "-psn_..." (and "-NS..." settings) when it starts the app; those are not files or commands.
        std::vector<std::string> plain;
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i].rfind("-psn_", 0) == 0) continue;
            // "-key value": a setting for this run (macOS reads it into the user defaults), e.g. -pageColours 1
            if (args[i].size() > 1 && args[i][0] == '-' && args[i][1] != '-') { ++i; continue; }
            plain.push_back(args[i]);
        }

        registerOfficeFonts();   // before the installed fonts are first listed
        FPInstallOcrEngine();
        FPInstallSignSystem();
        std::wstring err;
        const bool loaded = pdfium::load(err);
        const bool command = !plain.empty() && plain[0].rfind("--", 0) == 0;
        if (command) {
            if (!loaded) { std::fprintf(stderr, "%s\n", platform::utf8(err).c_str()); return 10; }
            if (plain[0] == "--render") return renderCommand(plain);
            std::vector<std::wstring> w;
            for (const auto& s : plain) w.push_back(platform::wide(s));
            const int rc = cli::run(w);
            if (rc < 0) { std::fprintf(stderr, "unknown command %s\n", plain[0].c_str()); return 2; }
            return rc;
        }

        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        if (!loaded) {
            NSAlert* a = [[NSAlert alloc] init];
            a.messageText = @"Feather PDF cannot start";
            a.informativeText = [NSString stringWithUTF8String:platform::utf8(err).c_str()];
            [a runModal];
            return 1;
        }
        (void)[[FPDocumentController alloc] init];   // the first one made is the app's document controller (Settings.mm)
        FPAppDelegate* delegate = [[FPAppDelegate alloc] init];
        NSMutableArray<NSString*>* files = [NSMutableArray array];
        for (const auto& s : plain) [files addObject:[NSString stringWithUTF8String:s.c_str()]];
        delegate.filesToOpen = files;
        app.delegate = delegate;
        app.mainMenu = [delegate buildMainMenu];
        [app run];
    }
    return 0;
}
