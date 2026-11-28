// Delete header and footer, as the Windows app's window (Windows/src/hfdialog.cpp): the page with what was found
// outlined (headers blue, footers orange), before or after, page by page; what to delete and on which pages; the list
// of what was found, each of which can be left out; what will be removed and the warnings; then Delete, as one
// undoable change. The finding and the removing are the shared engine's (shared/headfoot.cpp).
#import "WindowController.h"
#import "Render.h"
#import "Form.h"
#import "Trace.h"
#include "headfoot.hpp"
#include "pdfium_api.hpp"
#include "platform.hpp"
#include <algorithm>
#include <memory>

@interface FPHeaderFooterSheet : NSObject
- (instancetype)initWithController:(FPWindowController*)wc document:(Document*)d page:(int)page;
- (void)show;
@end

@implementation FPHeaderFooterSheet {
    __weak FPWindowController* _wc;
    Document* _d;
    int _page, _current;
    headfoot::Detection _det;
    headfoot::Plan _plan;
    NSWindow* _sheet;
    NSImageView* _image;
    NSSegmentedControl* _when;
    NSTextField* _pageLabel;
    NSPopUpButton* _what;
    NSPopUpButton* _which;
    NSTextField* _spec;
    NSStackView* _list;
    NSTextField* _summary;
    NSButton* _delete;
    FPHeaderFooterSheet* _keep;   // (alive while the sheet is up)
}

static NSString* ns(const std::wstring& s) { return [NSString stringWithUTF8String:platform::utf8(s).c_str()]; }

- (instancetype)initWithController:(FPWindowController*)wc document:(Document*)d page:(int)page {
    if ((self = [super init])) { _wc = wc; _d = d; _page = _current = page; }
    return self;
}

static NSTextField* label(NSString* s) { NSTextField* t = [NSTextField labelWithString:s]; t.lineBreakMode = NSLineBreakByWordWrapping; return t; }

- (void)show {
    _det = headfoot::detect(*_d);
    headfoot::selectFor(_det, 2);
    _sheet = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 820, 620) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:YES];
    _sheet.title = @"Delete header and footer";
    NSView* c = _sheet.contentView;

    _image = [NSImageView imageViewWithImage:[NSImage new]];
    _image.imageScaling = NSImageScaleProportionallyUpOrDown;
    _image.wantsLayer = YES;
    _image.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;
    _when = [NSSegmentedControl segmentedControlWithLabels:@[ @"Before", @"After" ] trackingMode:NSSegmentSwitchTrackingSelectOne target:self action:@selector(redraw)];
    _when.selectedSegment = 0;
    NSButton* prev = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"chevron.left" accessibilityDescription:@"Previous page"] target:self action:@selector(previousPage)];
    NSButton* next = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"chevron.right" accessibilityDescription:@"Next page"] target:self action:@selector(nextPage)];
    _pageLabel = label(@"");
    NSStackView* nav = [NSStackView stackViewWithViews:@[ _when, prev, _pageLabel, next ]];
    NSStackView* left = [NSStackView stackViewWithViews:@[ _image, nav ]];
    left.orientation = NSUserInterfaceLayoutOrientationVertical;
    [_image.widthAnchor constraintEqualToConstant:400].active = YES;
    [_image.heightAnchor constraintEqualToConstant:520].active = YES;

    NSTextField* intro = label(@"What repeats at the top and bottom of the pages (running titles, dates, page numbers, rules, logos) is outlined: headers in blue, footers in orange. It is removed for good, not covered.");
    intro.preferredMaxLayoutWidth = 370;
    _what = [NSPopUpButton new];
    [_what addItemsWithTitles:@[ @"Header", @"Footer", @"Header and footer", @"Page numbers only", @"Only what is ticked below" ]];
    [_what selectItemAtIndex:2];
    _what.target = self; _what.action = @selector(whatChanged);
    _which = [NSPopUpButton new];
    [_which addItemsWithTitles:@[ @"All pages", @"This page", @"Odd pages", @"Even pages", @"These pages:" ]];
    _which.target = self; _which.action = @selector(replan);
    _spec = [NSTextField textFieldWithString:@""];
    _spec.placeholderString = @"1-3, 7";
    _spec.target = self; _spec.action = @selector(replan);
    [_spec.widthAnchor constraintEqualToConstant:110].active = YES;
    NSGridView* grid = [NSGridView gridViewWithViews:@[ @[ label(@"Delete:"), _what ], @[ label(@"Pages:"), [NSStackView stackViewWithViews:@[ _which, _spec ]] ] ]];
    grid.rowSpacing = 8;
    _list = [NSStackView new];
    _list.orientation = NSUserInterfaceLayoutOrientationVertical;
    _list.alignment = NSLayoutAttributeLeading;
    _list.spacing = 4;
    NSScrollView* scroll = [NSScrollView new];
    scroll.hasVerticalScroller = YES;
    scroll.borderType = NSBezelBorder;
    NSView* doc = [NSView new];
    doc.translatesAutoresizingMaskIntoConstraints = NO;
    [doc addSubview:_list];
    _list.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[ [_list.topAnchor constraintEqualToAnchor:doc.topAnchor constant:6], [_list.leadingAnchor constraintEqualToAnchor:doc.leadingAnchor constant:6],
                                               [_list.trailingAnchor constraintLessThanOrEqualToAnchor:doc.trailingAnchor], [_list.bottomAnchor constraintEqualToAnchor:doc.bottomAnchor constant:-6] ]];
    scroll.documentView = doc;
    [doc.widthAnchor constraintEqualToAnchor:scroll.contentView.widthAnchor].active = YES;
    [scroll.heightAnchor constraintEqualToConstant:200].active = YES;
    [scroll.widthAnchor constraintEqualToConstant:370].active = YES;
    _summary = label(@"");
    _summary.preferredMaxLayoutWidth = 370;
    NSButton* cancel = [NSButton buttonWithTitle:@"Cancel" target:self action:@selector(cancel)];
    cancel.keyEquivalent = @"\033";
    _delete = [NSButton buttonWithTitle:@"Delete" target:self action:@selector(deleteNow)];
    _delete.keyEquivalent = @"\r";
    NSStackView* buttons = [NSStackView stackViewWithViews:@[ cancel, _delete ]];
    NSStackView* right = [NSStackView stackViewWithViews:@[ intro, grid, label(@"Found (untick what should stay):"), scroll, _summary, buttons ]];
    right.orientation = NSUserInterfaceLayoutOrientationVertical;
    right.alignment = NSLayoutAttributeLeading;
    right.spacing = 10;
    NSStackView* all = [NSStackView stackViewWithViews:@[ left, right ]];
    all.alignment = NSLayoutAttributeTop;
    all.spacing = 18;
    all.edgeInsets = NSEdgeInsetsMake(16, 16, 16, 16);
    all.translatesAutoresizingMaskIntoConstraints = NO;
    [c addSubview:all];
    [NSLayoutConstraint activateConstraints:@[ [all.topAnchor constraintEqualToAnchor:c.topAnchor], [all.leadingAnchor constraintEqualToAnchor:c.leadingAnchor],
                                               [all.trailingAnchor constraintEqualToAnchor:c.trailingAnchor], [all.bottomAnchor constraintEqualToAnchor:c.bottomAnchor] ]];
    [self fillList];
    [self replan];
    _keep = self;
    [_wc.window beginSheet:_sheet completionHandler:^(NSModalResponse) { self->_keep = nil; }];
    FPTrace(@"header and footer window: %lu found, %d objects on %lu pages", (unsigned long)_det.elements.size(), _plan.objects, (unsigned long)_plan.pages.size());
}

- (void)fillList {
    for (NSView* v in _list.arrangedSubviews.copy) [v removeFromSuperview];
    for (size_t i = 0; i < _det.elements.size(); ++i) {
        const auto& e = _det.elements[i];
        NSString* title = [NSString stringWithFormat:@"%@: %@ (%d page%@)%@", e.band == headfoot::Band::Header ? @"Header" : @"Footer", ns(e.label),
                           e.pageCount(), e.pageCount() == 1 ? @"" : @"s", e.locked ? @" (part of the page's template: kept)" : @""];
        NSButton* b = [NSButton checkboxWithTitle:title target:self action:@selector(ticked:)];
        b.tag = static_cast<NSInteger>(i);
        b.state = e.selected && !e.locked ? NSControlStateValueOn : NSControlStateValueOff;
        b.enabled = !e.locked;
        b.contentTintColor = e.band == headfoot::Band::Header ? NSColor.systemBlueColor : NSColor.systemOrangeColor;
        [_list addArrangedSubview:b];
    }
    if (_det.elements.empty())
        [_list addArrangedSubview:label(_d->pageCount() < 2 ? @"With one page nothing can be seen to repeat." : @"Nothing that repeats was found at the top or bottom of the pages.")];
}

- (void)whatChanged {
    if (_what.indexOfSelectedItem < 4) headfoot::selectFor(_det, static_cast<int>(_what.indexOfSelectedItem));
    [self fillList];
    [self replan];
}

- (void)ticked:(NSButton*)b {
    if (b.tag < 0 || b.tag >= static_cast<NSInteger>(_det.elements.size())) return;
    _det.elements[static_cast<size_t>(b.tag)].selected = b.state == NSControlStateValueOn;
    [_what selectItemAtIndex:4];   // (chosen one by one now)
    [self replan];
}

- (void)replan {
    std::wstring why;
    const std::vector<int> pages = headfoot::choosePages(*_d, static_cast<int>(_which.indexOfSelectedItem), _current, form::ws(_spec.stringValue ?: @""), &why);
    _spec.enabled = _which.indexOfSelectedItem == 4;
    _plan = pages.empty() ? headfoot::Plan{} : headfoot::plan(*_d, _det, pages);
    NSMutableString* s = [NSMutableString string];
    if (pages.empty()) [s appendString:ns(why.empty() ? L"No pages are chosen." : why)];
    else if (_plan.objects == 0 && _plan.areas == 0) [s appendString:@"Nothing of that kind on those pages: nothing will be deleted."];
    else [s appendFormat:@"%d piece%@ will be deleted on %lu page%@.", _plan.objects + _plan.areas, _plan.objects + _plan.areas == 1 ? @"" : @"s",
                         (unsigned long)_plan.pages.size(), _plan.pages.size() == 1 ? @"" : @"s"];
    if (_plan.sharedKept) [s appendFormat:@"\n%d piece%@ belong to the page's template with the body text and stay.", _plan.sharedKept, _plan.sharedKept == 1 ? @"" : @"s"];
    if (_plan.crossing) [s appendFormat:@"\n%d object%@ cross the edge of the area and stay.", _plan.crossing, _plan.crossing == 1 ? @"" : @"s"];
    if (_plan.bodyTaken) [s appendFormat:@"\nCareful: %d piece%@ of the body would go too (\"%@\").", _plan.bodyTaken, _plan.bodyTaken == 1 ? @"" : @"s", ns(_plan.bodySample)];
    if (!_det.scannedNoText.empty() && std::count(_det.scannedNoText.begin(), _det.scannedNoText.end(), 1))
        [s appendString:@"\nScanned pages without recognised text: their header and footer cannot be found (Tools > Recognise Text first)."];
    _summary.stringValue = s;
    _delete.enabled = _plan.objects + _plan.areas > 0;
    [self redraw];
}

- (void)previousPage { if (_page > 0) { --_page; [self redraw]; } }
- (void)nextPage { if (_page + 1 < _d->pageCount()) { ++_page; [self redraw]; } }

/** The page: before (what is found outlined) or after (a copy of the page with its plan carried out). */
- (void)redraw {
    _pageLabel.stringValue = [NSString stringWithFormat:@"Page %d of %d", _page + 1, _d->pageCount()];
    const bool after = _when.selectedSegment == 1;
    std::unique_ptr<Document> copy;
    Document* src = _d;
    int index = _page;
    if (after) {
        std::vector<unsigned char> bytes;
        copy = std::make_unique<Document>();
        if (_d->pagesToBytes({_page}, bytes) && copy->load(std::move(bytes), {}) == OpenResult::Ok) {
            for (const auto& pp : _plan.pages) if (pp.page == _page) { headfoot::PagePlan c2 = pp; c2.page = 0; headfoot::applyPage(*copy, 0, c2); }
            src = copy.get(); index = 0;
        }
    }
    const PageSize ps = _d->pageSize(_page);
    const double scale = std::min(800.0 / std::max(1.0f, ps.w), 1040.0 / std::max(1.0f, ps.h));
    const int w = std::max(1, static_cast<int>(ps.w * scale)), h = std::max(1, static_cast<int>(ps.h * scale));
    CGImageRef page = FPRenderPage(*src, index, w, h, 0, 0, w, h, pagecolours::Mode::Normal);
    if (!page) return;
    NSImage* img = [[NSImage alloc] initWithSize:NSMakeSize(w, h)];
    [img lockFocus];
    CGContextRef cg = NSGraphicsContext.currentContext.CGContext;
    CGContextDrawImage(cg, CGRectMake(0, 0, w, h), page);
    if (!after) {   // what was found on this page, outlined (ticked ones solid, the others dashed)
        for (const auto& e : _det.elements)
            for (const auto& it : e.items) {
                if (it.page != _page) continue;
                const NSRect r = NSMakeRect(it.box.l * scale - 2, h - it.box.b * scale - 2, (it.box.r - it.box.l) * scale + 4, (it.box.b - it.box.t) * scale + 4);
                NSBezierPath* p = [NSBezierPath bezierPathWithRect:r];
                p.lineWidth = 2;
                if (!e.selected) { const CGFloat dash[2] = {5, 3}; [p setLineDash:dash count:2 phase:0]; }
                [(e.band == headfoot::Band::Header ? NSColor.systemBlueColor : NSColor.systemOrangeColor) setStroke];
                [p stroke];
            }
    }
    [img unlockFocus];
    CGImageRelease(page);
    _image.image = img;
}

- (void)cancel { [_wc.window endSheet:_sheet]; }

- (void)deleteNow {
    std::wstring why;
    std::wstring found;
    for (const auto& e : _det.elements)
        if (e.selected && !e.locked) found += L"  - " + std::wstring(e.band == headfoot::Band::Header ? L"Header: " : L"Footer: ") + e.label + L"\n";
    const headfoot::Plan plan = _plan;
    [_wc.window endSheet:_sheet];
    if (!headfoot::apply(*_d, plan, &why)) { form::message(_wc.window, L"Nothing was deleted. " + why, true); return; }
    [_wc afterChange];
    FPTrace(@"header and footer deleted: %d objects on %lu pages", plan.objects, (unsigned long)plan.pages.size());
    form::message(_wc.window, L"Deleted on " + std::to_wstring(plan.pages.size()) + L" page(s):\n" + found + L"\nUndo (⌘Z) brings them back.");
}

@end

@implementation FPWindowController (HeaderFooter)
- (void)showHeaderFooterWindow {
    Document* d = self.pdf;
    if (!d) return;
    [[[FPHeaderFooterSheet alloc] initWithController:self document:d page:[self.pageView currentPage]] show];
}
@end
