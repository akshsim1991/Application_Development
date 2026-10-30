// Editing on the page, as the Windows app does it: text boxes, a line or a paragraph edited where it is (the box opens
// over it in its size; Return or a click elsewhere applies, Esc cancels), find and replace, pictures and the picture
// tools, moving and resizing what is selected, and tables (found on the page and edited in a grid, or new ones).
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "PageView.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <objc/runtime.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include "pageobjects.hpp"
#include "picture.hpp"
#include "platform.hpp"
#include "tables.hpp"

using form::Field;
using form::Fields;
using form::ns;
using form::ws;

namespace {
constexpr COLORREF kPalette[] = {RGB(0xFF, 0xE0, 0x00), RGB(0x66, 0xDD, 0x66), RGB(0x55, 0xB8, 0xFF), RGB(0xFF, 0x8A, 0xC8),
                                 RGB(0xFF, 0x8C, 0x00), RGB(0xE5, 0x39, 0x35), RGB(0x20, 0x20, 0x20)};
constexpr double kTextSizes[] = {8, 10, 12, 14, 18, 24, 36, 48, 72};

// The style of new text (Style of New Text...), kept between runs: the Windows app's defaults.
pageobj::TextStyle newTextStyle() {
    NSUserDefaults* u = NSUserDefaults.standardUserDefaults;
    pageobj::TextStyle s;
    s.family = static_cast<pageobj::Family>(std::clamp<NSInteger>([u integerForKey:@"textFont"], 0, 2));
    s.bold = [u boolForKey:@"textBold"];
    s.italic = [u boolForKey:@"textItalic"];
    const double size = [u doubleForKey:@"textSize"];
    s.sizePt = size > 0 ? size : 14;
    NSNumber* c = [u objectForKey:@"textColour"];
    s.color = kPalette[c ? std::clamp(c.intValue, 0, 6) : 6];
    return s;
}

NSFont* fontFor(const pageobj::TextStyle& s, double size) {
    NSFont* f = s.family == pageobj::Family::Serif ? [NSFont fontWithName:@"Times New Roman" size:size]
              : s.family == pageobj::Family::Mono ? [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightRegular]
                                                  : [NSFont fontWithName:@"Helvetica" size:size];
    if (!f) f = [NSFont systemFontOfSize:size];
    NSFontTraitMask t = (s.bold ? NSBoldFontMask : 0) | (s.italic ? NSItalicFontMask : 0);
    if (t) f = [NSFontManager.sharedFontManager convertFont:f toHaveTrait:t];
    return f;
}

NSColor* colourOf(COLORREF c) { return [NSColor colorWithSRGBRed:GetRValue(c) / 255.0 green:GetGValue(c) / 255.0 blue:GetBValue(c) / 255.0 alpha:1]; }

// What is being edited in place, and what is selected.
struct EditState {
    pageobj::ParagraphHit hit;
    bool paragraph = false;
    bool cancelled = false;
};
struct Selection {
    int page = -1;
    pageobj::GroupHit group;
};
Selection g_sel;
std::wstring g_picturePath;            // the picture to put on the page (Picture tool)
double g_pictureWidth = 0;             // its width on the page in points (0: its own size)
int g_cropPicPage = -1, g_cropPicObj = -1;   // a picture being cropped (the crop tool then crops it, not the page)
pageobj::TableSpec g_newTable;
} // namespace

// The box over the page while a line or a paragraph is edited: Return (a line) or ⌘Return (a paragraph) applies,
// Esc cancels, leaving it (a click elsewhere) applies.
@interface FPInlineEditor : NSObject <NSTextViewDelegate, NSTextFieldDelegate>
@property(nonatomic, weak) FPWindowController* owner;
@property(nonatomic, strong) NSView* box;          // the text field, or the scroll view with the text view
@property(nonatomic, strong) NSStackView* bar;     // the paragraph's style bar
@property(nonatomic, strong) NSTextView* textView;
@property(nonatomic, strong) NSTextField* field;
@property(nonatomic, strong) NSPopUpButton *family, *size, *colour, *align;
@property(nonatomic, strong) NSButton *bold, *italic;
@property(nonatomic) BOOL finished;
@end

@interface FPWindowController (Editing)
- (void)inlineFinished:(FPInlineEditor*)ed apply:(BOOL)apply;
@end
@interface FPWindowController (TableSheet)
- (void)editTable:(std::shared_ptr<tables::Table>)t;   // TableSheet.mm
@end

static EditState g_edit;
static FPInlineEditor* g_editor = nil;

@implementation FPInlineEditor
- (void)end:(BOOL)apply {
    if (self.finished) return;
    self.finished = YES;
    [self.owner inlineFinished:self apply:apply];
}
- (BOOL)control:(NSControl*)c textView:(NSTextView*)tv doCommandBySelector:(SEL)cmd {
    if (cmd == @selector(insertNewline:)) { [self end:YES]; return YES; }
    if (cmd == @selector(cancelOperation:)) { [self end:NO]; return YES; }
    return NO;
}
- (void)controlTextDidEndEditing:(NSNotification*)n { [self end:YES]; }
- (void)applyClicked:(id)sender { [self end:YES]; }
- (BOOL)textView:(NSTextView*)tv doCommandBySelector:(SEL)cmd {
    if (cmd == @selector(cancelOperation:)) { [self end:NO]; return YES; }
    if (cmd == @selector(insertNewline:) && (NSApp.currentEvent.modifierFlags & NSEventModifierFlagCommand)) { [self end:YES]; return YES; }
    return NO;
}
- (void)textDidEndEditing:(NSNotification*)n {
    // the style bar takes the keyboard for a moment: only a click outside both ends the editing
    NSResponder* fr = self.textView.window.firstResponder;
    if ([fr isKindOfClass:[NSView class]] && [(NSView*)fr isDescendantOf:self.bar]) return;
    [self end:YES];
}
@end

@implementation FPWindowController (Editing)

- (void)edNote:(const std::wstring&)t { form::message(self.window, t); }

// ---- choosing a tool ----

- (IBAction)chooseEditTool:(NSMenuItem*)sender {
    if (!self.pdf) return;
    const FPTool t = static_cast<FPTool>(sender.tag);
    self.pageView.tool = t;
    g_sel = Selection{};
    switch (t) {
        case FPToolTextBox: [self edNote:L"Click where the text should start."]; break;
        case FPToolEditText: [self edNote:L"Click a line of text to change it. Return applies, Esc cancels."]; break;
        case FPToolEditPara: [self edNote:L"Click a paragraph to change it. ⌘Return applies (Return starts a new line), Esc cancels."]; break;
        case FPToolSelectObject: [self edNote:L"Click a picture, a paragraph or a table to select it. Drag it to move it, its corner to resize it; Delete removes it."]; break;
        case FPToolEditTable: [self edNote:L"Click a table on the page to edit it in a grid."]; break;
        default: break;
    }
}

- (IBAction)textStyleDialog:(id)sender {
    const pageobj::TextStyle s = newTextStyle();
    Fields f(5);
    f[0] = {Field::Choice, L"Font:", {L"Sans-serif (Helvetica)", L"Serif (Times)", L"Monospaced (Courier)"}, L"", static_cast<int>(s.family)};
    std::vector<std::wstring> sizes;
    int at = 3;
    for (int i = 0; i < 9; ++i) { sizes.push_back(std::to_wstring(static_cast<int>(kTextSizes[i])) + L" pt"); if (kTextSizes[i] == s.sizePt) at = i; }
    f[1] = {Field::Choice, L"Size:", sizes, L"", at};
    int col = 6;
    for (int i = 0; i < 7; ++i) if (kPalette[i] == s.color) col = i;
    f[2] = {Field::Choice, L"Colour:", {L"Yellow", L"Green", L"Blue", L"Pink", L"Orange", L"Red", L"Black"}, L"", col};
    f[3] = {Field::Check, L"Bold", {}, L"", 0, s.bold};
    f[4] = {Field::Check, L"Italic", {}, L"", 0, s.italic};
    form::show(self.window, L"Style of new text", f, L"OK", nullptr, [](const Fields& v) {
        NSUserDefaults* u = NSUserDefaults.standardUserDefaults;
        [u setInteger:v[0].choice forKey:@"textFont"];
        [u setDouble:kTextSizes[std::clamp(v[1].choice, 0, 8)] forKey:@"textSize"];
        [u setInteger:v[2].choice forKey:@"textColour"];
        [u setBool:v[3].checked forKey:@"textBold"];
        [u setBool:v[4].checked forKey:@"textItalic"];
    });
}

// ---- what the editing tools did (Annotate.mm passes the events on) ----

- (void)editEvent:(const FPToolEvent&)e {
    Document* d = self.pdf;
    if (!d || e.pts.empty()) return;
    const annot::PointD p = e.pts.front();
    switch (self.pageView.tool) {
        case FPToolTextBox: {
            if (e.kind != FPToolEvent::Click) break;
            const int page = e.page;
            Fields f(2);
            f[0] = {Field::Note, L"", {}, L"Type the text to put on the page. Use \\n for a new line."};
            f[1] = {Field::Text, L"Text:", {}, L""};
            form::show(self.window, L"Add text", f, L"Add", [](const Fields& v) -> std::wstring {
                return pageobj::isLatin(v[1].text) ? L"" : L"New text can use letters, numbers and common symbols of Western languages. Please remove other characters.";
            }, [self, d, page, p](const Fields& v) {
                std::wstring t = v[1].text;
                for (size_t i; (i = t.find(L"\\n")) != std::wstring::npos;) t.replace(i, 2, L"\n");
                if (t.empty()) return;
                if (pageobj::addTextBox(*d, page, p.x, p.y, t, newTextStyle())) { d->resetText(page); [self afterChange]; }
                else [self edNote:L"The text could not be added."];
            });
            break;
        }
        case FPToolEditText: case FPToolEditPara: {
            if (e.kind != FPToolEvent::Click) break;
            const bool para = self.pageView.tool == FPToolEditPara;
            const pageobj::ParagraphHit hit = pageobj::paragraphAt(*d, e.page, p.x, p.y, !para);
            if (!hit.found || !hit.upright) {
                [self edNote:L"No text there. Click directly on a line of text (scanned pages are pictures: use text recognition first)."];
                break;
            }
            [self beginInlineEdit:hit paragraph:para];
            break;
        }
        case FPToolSelectObject: {
            if (e.drag != FPToolEvent::None && g_sel.group.found && e.pts.size() >= 2) {   // moved or resized
                const auto& g = g_sel.group;
                const double dx = e.pts[1].x - e.pts[0].x, dy = e.pts[1].y - e.pts[0].y;
                double tl = g.left, tb = g.bottom, tr = g.right, tt = g.top;
                if (e.drag == FPToolEvent::Move) { tl += dx; tr += dx; tb += dy; tt += dy; }
                else { tr = std::max(g.left + 8, g.right + dx); tb = std::min(g.top - 8, g.bottom + dy); }
                if (pageobj::transformGroup(*d, e.page, g.objects, g.left, g.bottom, g.right, g.top, tl, tb, tr, tt)) {
                    [self afterChange];
                    g_sel.group.left = tl; g_sel.group.bottom = tb; g_sel.group.right = tr; g_sel.group.top = tt;
                    [self.pageView setObjectBox:e.page rect:annot::RectD{tl, tb, tr, tt}];
                }
                break;
            }
            if (e.kind != FPToolEvent::Click) break;
            g_sel.page = e.page;
            g_sel.group = pageobj::groupAt(*d, e.page, p.x, p.y);
            if (g_sel.group.found) [self.pageView setObjectBox:e.page rect:annot::RectD{g_sel.group.left, g_sel.group.bottom, g_sel.group.right, g_sel.group.top}];
            else { g_sel.page = -1; [self.pageView setObjectBox:-1 rect:annot::RectD{}]; }
            break;
        }
        case FPToolPicture: {
            if (e.kind != FPToolEvent::Click) break;
            std::wstring err;
            if (pageobj::addPicture(*d, e.page, p.x, p.y, g_picturePath, err, g_pictureWidth)) [self afterChange];
            else [self edNote:err];
            self.pageView.tool = FPToolNone;
            break;
        }
        case FPToolEditTable: {
            if (e.kind != FPToolEvent::Click) break;
            auto t = std::make_shared<tables::Table>();
            std::wstring why;
            if (!tables::tableAt(*d, e.page, p.x, p.y, *t, &why)) { [self edNote:why.empty() ? L"There is no table there." : why]; break; }
            [self editTable:t];
            break;
        }
        case FPToolPlaceTable:
            if (e.kind != FPToolEvent::Click) break;
            if (pageobj::addTable(*d, e.page, p.x, p.y, g_newTable)) [self afterChange];
            else [self edNote:L"The table does not fit there. Click higher or further left, or make it smaller."];
            self.pageView.tool = FPToolNone;
            break;
        default: break;
    }
}

// ---- editing a line or a paragraph in place ----

- (void)beginInlineEdit:(const pageobj::ParagraphHit&)hit paragraph:(BOOL)para {
    [self endInlineEdit:YES];
    g_edit = EditState{hit, static_cast<bool>(para), false};
    FPPageView* v = self.pageView;
    const double zoom = v.zoom;
    const double height = para ? std::max(hit.lineHeight * hit.lines, hit.lineHeight) : hit.lineHeight;
    NSRect r = [v viewRectOnPage:hit.page rect:annot::RectD{hit.left, hit.top - height, hit.left + hit.width, hit.top}];
    r = NSInsetRect(r, -4, -3);
    r.size.width = std::max(r.size.width, 160.0);
    FPInlineEditor* ed = [[FPInlineEditor alloc] init];
    ed.owner = self;
    NSFont* font = fontFor(hit.style, hit.style.sizePt * zoom);
    if (!para) {
        NSTextField* f = [[NSTextField alloc] initWithFrame:r];
        f.stringValue = ns(hit.text);
        f.font = font;
        f.textColor = colourOf(hit.style.color);
        f.delegate = ed;
        f.focusRingType = NSFocusRingTypeExterior;
        ed.field = f;
        ed.box = f;
    } else {
        NSScrollView* sv = [[NSScrollView alloc] initWithFrame:NSInsetRect(r, 0, -std::max(0.0, hit.lineHeight * zoom))];
        sv.hasVerticalScroller = YES;
        sv.borderType = NSLineBorder;
        NSTextView* tv = [[NSTextView alloc] initWithFrame:sv.bounds];
        tv.string = ns(hit.text);
        tv.font = font;
        tv.textColor = colourOf(hit.style.color);
        tv.delegate = ed;
        tv.autoresizingMask = NSViewWidthSizable;
        tv.richText = NO;
        sv.documentView = tv;
        ed.textView = tv;
        ed.box = sv;
        // the style bar: font, size, bold, italic, colour, alignment
        ed.family = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
        [ed.family addItemsWithTitles:@[ @"Sans-serif", @"Serif", @"Monospaced" ]];
        [ed.family selectItemAtIndex:static_cast<NSInteger>(hit.style.family)];
        ed.size = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
        for (double s : {6.0, 7.0, 8.0, 9.0, 10.0, 10.5, 11.0, 12.0, 14.0, 16.0, 18.0, 20.0, 24.0, 28.0, 36.0})
            [ed.size addItemWithTitle:[NSString stringWithFormat:@"%g pt", s]];
        NSString* cur = [NSString stringWithFormat:@"%g pt", hit.style.sizePt];
        if (![ed.size itemWithTitle:cur]) [ed.size addItemWithTitle:cur];
        [ed.size selectItemWithTitle:cur];
        ed.bold = [NSButton checkboxWithTitle:@"Bold" target:nil action:nil];
        ed.bold.state = hit.style.bold;
        ed.italic = [NSButton checkboxWithTitle:@"Italic" target:nil action:nil];
        ed.italic.state = hit.style.italic;
        ed.colour = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
        [ed.colour addItemsWithTitles:@[ @"As it is", @"Black", @"Red", @"Blue", @"Green" ]];
        ed.align = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
        [ed.align addItemsWithTitles:@[ @"Left", @"Centre", @"Right", @"Justify" ]];
        [ed.align selectItemAtIndex:static_cast<NSInteger>(hit.align)];
        NSButton* apply = [NSButton buttonWithTitle:@"Apply" target:ed action:@selector(applyClicked:)];
        ed.bar = [NSStackView stackViewWithViews:@[ ed.family, ed.size, ed.bold, ed.italic, ed.colour, ed.align, apply ]];
        ed.bar.spacing = 6;
        ed.bar.wantsLayer = YES;
        ed.bar.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;
        ed.bar.edgeInsets = NSEdgeInsetsMake(3, 6, 3, 6);
        ed.bar.frame = NSMakeRect(NSMinX(sv.frame), NSMinY(sv.frame) - 32, 560, 30);
        [v addSubview:ed.bar];
    }
    [v addSubview:ed.box];
    [v.window makeFirstResponder:para ? (NSView*)ed.textView : (NSView*)ed.field];
    g_editor = ed;
}

- (void)endInlineEdit:(BOOL)apply {
    if (g_editor && !g_editor.finished) [g_editor end:apply];
}

- (void)inlineFinished:(FPInlineEditor*)ed apply:(BOOL)apply {
    Document* d = self.pdf;
    const std::wstring text = ws(ed.textView ? ed.textView.string : ed.field.stringValue);
    pageobj::TextStyle style = g_edit.hit.style;
    pageobj::Align align = g_edit.hit.align;
    if (ed.bar) {
        style.family = static_cast<pageobj::Family>(ed.family.indexOfSelectedItem);
        style.sizePt = ed.size.titleOfSelectedItem.doubleValue;
        style.bold = ed.bold.state == NSControlStateValueOn;
        style.italic = ed.italic.state == NSControlStateValueOn;
        static const COLORREF cols[] = {0, RGB(0x20, 0x20, 0x20), RGB(0xE5, 0x39, 0x35), RGB(0x25, 0x6F, 0xD1), RGB(0x2E, 0x9E, 0x4A)};
        if (ed.colour.indexOfSelectedItem > 0) style.color = cols[ed.colour.indexOfSelectedItem];
        align = static_cast<pageobj::Align>(ed.align.indexOfSelectedItem);
    }
    [ed.box removeFromSuperview];
    [ed.bar removeFromSuperview];
    if (g_editor == ed) g_editor = nil;
    [self.window makeFirstResponder:self.pageView];
    const bool changed = text != g_edit.hit.text || style.family != g_edit.hit.style.family || style.sizePt != g_edit.hit.style.sizePt ||
                         style.bold != g_edit.hit.style.bold || style.italic != g_edit.hit.style.italic || style.color != g_edit.hit.style.color ||
                         align != g_edit.hit.align;
    if (!apply || !d || !changed) return;
    std::wstring why;
    if (pageobj::replaceParagraph(*d, g_edit.hit, text, style, align, &why, !g_edit.paragraph)) [self afterChange];
    else [self edNote:why.empty() ? L"The text could not be changed." : why];
}

// ---- find and replace ----

- (IBAction)findAndReplace:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(4);
    f[0] = {Field::Text, L"Find:", {}, L""};
    f[1] = {Field::Text, L"Replace with:", {}, L""};
    f[2] = {Field::Check, L"Match case"};
    f[3] = {Field::Text, L"Pages (empty = all):", {}, L""};
    form::show(self.window, L"Find and replace text", f, L"Replace all", [](const Fields& v) -> std::wstring {
        if (v[0].text.empty()) return L"Type the text to find.";
        return pageobj::isLatin(v[1].text) ? L"" : L"The new text can use letters, numbers and common symbols of Western languages.";
    }, [self, d](const Fields& v) {
        std::vector<int> pages;
        if (v[3].text.find_first_not_of(L' ') == std::wstring::npos) for (int i = 0; i < d->pageCount(); ++i) pages.push_back(i);
        else pages = d->parseRange(v[3].text);
        const int n = pageobj::replaceAll(*d, v[0].text, v[1].text, v[2].checked, pages);
        if (n > 0) [self afterChange];
        [self edNote:n > 0 ? std::to_wstring(n) + L" place(s) replaced. Undo (⌘Z) takes it back." : L"The text was not found (text split across lines or styles cannot be replaced)."];
    });
}

// ---- pictures ----

- (IBAction)insertPicture:(id)sender {
    if (!self.pdf) return;
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypeImage ];
    p.title = @"Choose a picture to put on the page";
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        g_picturePath = platform::wide(p.URL.fileSystemRepresentation);
        g_pictureWidth = 0;
        self.pageView.tool = FPToolPicture;
        [self edNote:L"Click on the page where the top-left corner of the picture should go. Esc cancels."];
    }];
}

// A picture to put where the next click on the page lands (handwritten signatures: Signatures.mm).
- (void)placePictureFile:(const std::wstring&)path width:(double)widthPt note:(const std::wstring&)note {
    g_picturePath = path;
    g_pictureWidth = widthPt;
    self.pageView.tool = FPToolPicture;
    [self edNote:note];
}

- (IBAction)pastePicture:(id)sender {
    Document* d = self.pdf;
    NSImage* img = [[NSImage alloc] initWithPasteboard:NSPasteboard.generalPasteboard];
    if (!d || !img) { [self edNote:L"The clipboard holds no picture."]; return; }
    NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:img.TIFFRepresentation];
    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    NSString* path = [NSTemporaryDirectory() stringByAppendingPathComponent:@"Feather PDF pasted picture.png"];
    if (![png writeToFile:path atomically:YES]) return;
    const int page = [self.pageView currentPage];
    std::wstring err;
    const PageSize& s = d->pageSize(page);
    if (pageobj::addPicture(*d, page, s.w * 0.1, s.h * 0.9, platform::wide(path.UTF8String), err)) [self afterChange];
    else [self edNote:err];
    [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
}

- (BOOL)selectedPicture:(int*)page obj:(int*)obj {
    Document* d = self.pdf;
    if (!d || g_sel.page < 0 || !g_sel.group.found || g_sel.group.objects.empty()) { [self edNote:L"Select a picture first: Edit > Select Object, then click the picture."]; return NO; }
    if (!pageobj::isPicture(*d, g_sel.page, g_sel.group.objects)) { [self edNote:L"The selection is not a picture."]; return NO; }
    *page = g_sel.page;
    *obj = g_sel.group.objects.front();
    return YES;
}

- (void)afterObjectChange {
    g_sel = Selection{};
    [self.pageView setObjectBox:-1 rect:annot::RectD{}];
    [self afterChange];
}

- (IBAction)objectRotateRight:(id)sender { if (g_sel.group.found && pageobj::rotateGroup(*self.pdf, g_sel.page, g_sel.group.objects, 1)) [self afterObjectChange]; }
- (IBAction)objectRotateLeft:(id)sender { if (g_sel.group.found && pageobj::rotateGroup(*self.pdf, g_sel.page, g_sel.group.objects, 3)) [self afterObjectChange]; }
- (IBAction)objectFlipHorizontal:(id)sender { if (g_sel.group.found && pageobj::flipGroup(*self.pdf, g_sel.page, g_sel.group.objects, true)) [self afterObjectChange]; }
- (IBAction)objectFlipVertical:(id)sender { if (g_sel.group.found && pageobj::flipGroup(*self.pdf, g_sel.page, g_sel.group.objects, false)) [self afterObjectChange]; }
- (IBAction)objectToFront:(id)sender { if (g_sel.group.found && pageobj::restackGroup(*self.pdf, g_sel.page, g_sel.group.objects, true)) [self afterObjectChange]; }
- (IBAction)objectToBack:(id)sender { if (g_sel.group.found && pageobj::restackGroup(*self.pdf, g_sel.page, g_sel.group.objects, false)) [self afterObjectChange]; }
- (IBAction)objectDelete:(id)sender { if (g_sel.group.found && pageobj::deleteGroup(*self.pdf, g_sel.page, g_sel.group.objects)) [self afterObjectChange]; }

- (IBAction)pictureCrop:(id)sender {
    int page, obj;
    if (![self selectedPicture:&page obj:&obj]) return;
    g_cropPicPage = page;
    g_cropPicObj = obj;
    self.pageView.tool = FPToolCrop;
    [self edNote:L"Drag a box around the part of the picture to keep. Esc cancels."];
}

// The crop tool's box when a picture is being cropped (PagesMore.mm asks first); false: it is a page crop.
- (BOOL)cropPictureWith:(const FPToolEvent&)e {
    if (g_cropPicObj < 0) return NO;
    const int page = g_cropPicPage, obj = g_cropPicObj;
    g_cropPicPage = g_cropPicObj = -1;
    self.pageView.tool = FPToolNone;
    if (e.kind != FPToolEvent::Draw || e.pts.size() < 2 || e.page != page) return YES;
    if (pageobj::cropPicture(*self.pdf, page, obj, std::min(e.pts[0].x, e.pts[1].x), std::min(e.pts[0].y, e.pts[1].y),
                             std::max(e.pts[0].x, e.pts[1].x), std::max(e.pts[0].y, e.pts[1].y))) [self afterObjectChange];
    else [self edNote:L"The picture could not be cropped there."];
    return YES;
}

- (IBAction)pictureAdjust:(id)sender {
    int page, obj;
    if (![self selectedPicture:&page obj:&obj]) return;
    Fields f(3);
    f[0] = {Field::Text, L"Brightness (-100 to 100):", {}, L"20"};
    f[1] = {Field::Text, L"Contrast (-100 to 100):", {}, L"0"};
    f[2] = {Field::Check, L"Black and white"};
    form::show(self.window, L"Brightness and contrast", f, L"Apply", [](const Fields& v) -> std::wstring {
        for (int i = 0; i < 2; ++i) { const long n = wcstol(v[static_cast<size_t>(i)].text.c_str(), nullptr, 10); if (n < -100 || n > 100) return L"Use numbers from -100 to 100."; }
        return L"";
    }, [self, page, obj](const Fields& v) {
        if (pageobj::adjustPicture(*self.pdf, page, obj, static_cast<int>(wcstol(v[0].text.c_str(), nullptr, 10)),
                                   static_cast<int>(wcstol(v[1].text.c_str(), nullptr, 10)), v[2].checked)) [self afterObjectChange];
    });
}

- (IBAction)pictureBlackAndWhite:(id)sender {
    int page, obj;
    if ([self selectedPicture:&page obj:&obj] && pageobj::adjustPicture(*self.pdf, page, obj, 0, 0, true)) [self afterObjectChange];
}

- (IBAction)pictureReplace:(id)sender {
    int page, obj;
    if (![self selectedPicture:&page obj:&obj]) return;
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypeImage ];
    p.title = @"Replace the picture with";
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        std::wstring err;
        if (pageobj::replacePicture(*self.pdf, page, obj, platform::wide(p.URL.fileSystemRepresentation), err)) [self afterObjectChange];
        else [self edNote:err];
    }];
}

- (IBAction)pictureSave:(id)sender {
    int page, obj;
    if (![self selectedPicture:&page obj:&obj]) return;
    NSSavePanel* p = [NSSavePanel savePanel];
    p.nameFieldStringValue = @"Picture.png";
    p.allowedContentTypes = @[ UTTypePNG ];
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r == NSModalResponseOK && !pageobj::savePicture(*self.pdf, page, obj, platform::wide(p.URL.fileSystemRepresentation)))
            [self edNote:L"The picture could not be saved."];
    }];
}

// ---- tables ----

- (IBAction)addNewTable:(id)sender {
    if (!self.pdf) return;
    Fields f(5);
    f[0] = {Field::Text, L"Rows:", {}, L"4"};
    f[1] = {Field::Text, L"Columns:", {}, L"3"};
    f[2] = {Field::Text, L"Column width (points):", {}, L"110"};
    f[3] = {Field::Choice, L"Text size:", {L"9 pt", L"10 pt", L"11 pt", L"12 pt", L"14 pt"}, L"", 2};
    f[4] = {Field::Check, L"First row is a heading (bold)", {}, L"", 0, true};
    form::show(self.window, L"Add a table", f, L"Place…", [](const Fields& v) -> std::wstring {
        const long r = wcstol(v[0].text.c_str(), nullptr, 10), c = wcstol(v[1].text.c_str(), nullptr, 10), w = wcstol(v[2].text.c_str(), nullptr, 10);
        if (r < 1 || r > 60 || c < 1 || c > 15) return L"Use 1 to 60 rows and 1 to 15 columns.";
        if (w < 20 || w > 500) return L"Use a column width from 20 to 500 points.";
        return L"";
    }, [self](const Fields& v) {
        static const double sizes[] = {9, 10, 11, 12, 14};
        g_newTable = pageobj::TableSpec{};
        g_newTable.rows = static_cast<int>(wcstol(v[0].text.c_str(), nullptr, 10));
        g_newTable.cols = static_cast<int>(wcstol(v[1].text.c_str(), nullptr, 10));
        g_newTable.colWidth = wcstod(v[2].text.c_str(), nullptr);
        g_newTable.fontSize = sizes[std::clamp(v[3].choice, 0, 4)];
        g_newTable.rowHeight = g_newTable.fontSize * 2.2;
        g_newTable.header = v[4].checked;
        g_newTable.cells.assign(static_cast<size_t>(g_newTable.rows * g_newTable.cols), L"");
        self.pageView.tool = FPToolPlaceTable;
        [self edNote:L"Click on the page where the top-left corner of the table should go."];
    });
}

@end
