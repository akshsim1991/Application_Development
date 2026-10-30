// The thumbnails' page menu, as on Windows: right-click a thumbnail (or several selected) to delete, turn, copy, cut and
// paste pages, insert blank pages or the pages of a file, merge, crop, extract, or select every page. Dragging
// thumbnails moves their pages; the Delete key deletes them.
#import <Cocoa/Cocoa.h>
#import "WindowController.h"
#import "PageView.h"
#import "Sidebar.h"
#import "Form.h"
#include <vector>
#include "document.hpp"

// A menu item that runs a block (kept by the item itself).
@interface FPMenuBlock : NSObject
@property(nonatomic, copy) void (^run)(void);
- (void)fire:(id)sender;
@end
@implementation FPMenuBlock
- (void)fire:(id)sender { if (self.run) self.run(); }
@end

namespace {
std::vector<unsigned char> g_pageClip;   // copied pages, for pasting into any open document

std::vector<int> pagesOf(NSArray<NSNumber*>* list) {
    std::vector<int> out;
    for (NSNumber* n in list) out.push_back(n.intValue);
    return out;
}
} // namespace

@interface FPWindowController (Tools)
- (IBAction)insertPagesFromFile:(id)sender;
- (IBAction)extractPages:(id)sender;
@end
@interface FPWindowController (PagesMore)
- (IBAction)cropPages:(id)sender;
- (IBAction)mergeFiles:(id)sender;
@end

@interface FPWindowController (ThumbMenu) <FPSidebarDelegate>
@end

@implementation FPWindowController (ThumbMenu)

- (void)thumbWarn:(const std::wstring&)t { form::message(self.window, t, true); }

- (void)deleteThumbPages:(const std::vector<int>&)pages {
    Document* d = self.pdf;
    if (!d || pages.empty()) return;
    if (static_cast<int>(pages.size()) >= d->pageCount()) {
        form::message(self.window, L"A document needs at least one page, so not every page can be deleted.");
        return;
    }
    if (d->deletePages(pages)) [self afterChange];
}

- (void)copyThumbPages:(const std::vector<int>&)pages {
    Document* d = self.pdf;
    std::vector<unsigned char> b;
    if (d && d->pagesToBytes(pages, b)) g_pageClip = std::move(b);
    else [self thumbWarn:L"The pages could not be copied."];
}

- (void)pastePagesAt:(int)at {
    Document* d = self.pdf;
    if (!d || g_pageClip.empty()) return;
    Document src;
    std::vector<unsigned char> copy = g_pageClip;
    if (src.load(std::move(copy), {}) != OpenResult::Ok) return;
    std::vector<int> all(static_cast<size_t>(src.pageCount()));
    for (int i = 0; i < src.pageCount(); ++i) all[static_cast<size_t>(i)] = i;
    if (d->importPages(src, all, at)) {
        [self afterChange];
        [self.pageView showPage:at];
    } else [self thumbWarn:L"The pages could not be pasted."];
}

- (void)sidebarFillMenu:(NSMenu*)menu pages:(NSArray<NSNumber*>*)list {
    Document* d = self.pdf;
    if (!d || !list.count) return;
    const std::vector<int> pages = pagesOf(list);
    const bool many = pages.size() > 1, clip = !g_pageClip.empty(), notAll = static_cast<int>(pages.size()) < d->pageCount();
    const int first = pages.front(), last = pages.back();
    __weak FPWindowController* weak = self;
    auto add = [&](NSString* title, bool on, void (^run)(void)) {
        NSMenuItem* item = [menu addItemWithTitle:title action:on ? @selector(fire:) : nil keyEquivalent:@""];
        FPMenuBlock* b = [[FPMenuBlock alloc] init];
        b.run = run;
        item.target = b;
        item.representedObject = b;   // the item keeps its block
        item.enabled = on;
    };
    auto sep = [&] { [menu addItem:[NSMenuItem separatorItem]]; };
    menu.autoenablesItems = NO;
    add(many ? @"Delete Pages" : @"Delete Page", notAll, ^{ [weak deleteThumbPages:pages]; });
    add(@"Rotate Clockwise", true, ^{ if (weak.pdf && weak.pdf->rotatePages(pages, 1)) [weak afterChange]; });
    add(@"Rotate Counterclockwise", true, ^{ if (weak.pdf && weak.pdf->rotatePages(pages, 3)) [weak afterChange]; });
    sep();
    add(many ? @"Copy Pages" : @"Copy Page", true, ^{ [weak copyThumbPages:pages]; });
    add(many ? @"Cut Pages" : @"Cut Page", notAll, ^{ [weak copyThumbPages:pages]; [weak deleteThumbPages:pages]; });
    add(@"Paste Pages After", clip, ^{ [weak pastePagesAt:last + 1]; });
    add(@"Paste Pages Before", clip, ^{ [weak pastePagesAt:first]; });
    add(@"Paste Pages at the Start", clip, ^{ [weak pastePagesAt:0]; });
    add(@"Paste Pages at the End", clip, ^{ [weak pastePagesAt:weak.pdf ? weak.pdf->pageCount() : 0]; });
    sep();
    add(@"Insert Blank Page After", true, ^{
        if (weak.pdf && weak.pdf->insertBlankPage(last, last)) { [weak afterChange]; [weak.pageView showPage:last + 1]; }
    });
    add(@"Insert Blank Page Before", true, ^{
        if (weak.pdf && weak.pdf->insertBlankPage(first - 1, first)) { [weak afterChange]; [weak.pageView showPage:first]; }
    });
    // these use the Pages menu's windows, starting from the first selected page
    add(@"Insert Pages from a File…", true, ^{ [weak.pageView showPage:last]; [weak insertPagesFromFile:nil]; });
    add(@"Merge PDFs into This Document…", true, ^{ [weak mergeFiles:nil]; });
    sep();
    add(@"Crop Pages…", true, ^{ [weak.pageView showPage:first]; [weak cropPages:nil]; });
    sep();
    add(@"Extract Pages…", true, ^{ [weak.pageView showPage:first]; [weak extractPages:nil]; });
    add(@"Select All Pages", true, ^{ [weak selectAllThumbs]; });
}

- (void)sidebarMovePages:(NSArray<NSNumber*>*)list before:(int)before {
    Document* d = self.pdf;
    const std::vector<int> pages = pagesOf(list);
    if (!d || !d->movePages(pages, before)) return;
    [self afterChange];
    int moved = 0;   // where the first moved page is now
    for (int p : pages) if (p < before) ++moved;
    [self.pageView showPage:before - moved];
}

- (void)sidebarDeletePages:(NSArray<NSNumber*>*)list { [self deleteThumbPages:pagesOf(list)]; }

// Pages > Delete Selected Pages (⌘⌫, as in the Finder): the thumbnails selected, else the page shown. Undo brings them back.
- (IBAction)deleteSelectedPages:(id)sender { [self deleteThumbPages:pagesOf([self selectedThumbPages])]; }

@end
