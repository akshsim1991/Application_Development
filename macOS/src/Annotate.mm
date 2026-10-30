// The Annotate and Redact tools: the same tools, colours, stamps, questions and messages as the Windows app
// (Windows/src/main.cpp), on the shared engine (annot, pageobjects, redact).
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "PageView.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include "annot.hpp"
#include "pageobjects.hpp"
#include "platform.hpp"
#include "redact.hpp"

using form::Field;
using form::Fields;
using form::ns;
using form::ws;

namespace {
constexpr COLORREF kPalette[] = {RGB(0xFF, 0xE0, 0x00), RGB(0x66, 0xDD, 0x66), RGB(0x55, 0xB8, 0xFF), RGB(0xFF, 0x8A, 0xC8),
                                 RGB(0xFF, 0x8C, 0x00), RGB(0xE5, 0x39, 0x35), RGB(0x20, 0x20, 0x20)};
constexpr double kWidths[] = {1, 2, 4};
const char* kStampNames[] = {"APPROVED", "FINAL", "DRAFT", "CONFIDENTIAL", "NOT APPROVED", "FOR COMMENT", "REVIEWED"};
constexpr COLORREF kStampCols[] = {RGB(0x2E, 0x9E, 0x4A), RGB(0x25, 0x6F, 0xD1), RGB(0xE0, 0x8A, 0x00), RGB(0xD0, 0x2B, 0x2B),
                                   RGB(0xD0, 0x2B, 0x2B), RGB(0x25, 0x6F, 0xD1), RGB(0x2E, 0x9E, 0x4A)};

// The choices of the Annotate menu, kept between runs (the same defaults as on Windows).
int pref(NSString* key, int def, int max) {
    NSNumber* v = [NSUserDefaults.standardUserDefaults objectForKey:key];
    return v ? std::clamp(v.intValue, 0, max) : def;
}
COLORREF drawColor() { return kPalette[pref(@"drawingColour", 5, 6)]; }
COLORREF highlightColor() { return kPalette[pref(@"highlightColour", 0, 6)]; }
double lineWidth() { return kWidths[pref(@"lineWidth", 1, 2)]; }

std::wstring dateText(const char* format) {
    const time_t now = time(nullptr);
    tm t{};
    localtime_r(&now, &t);
    char buf[64];
    strftime(buf, sizeof buf, format, &t);
    return platform::wide(buf);
}

// The pending stamp (chosen in the menu, put down with a click).
std::wstring g_stamp;
COLORREF g_stampColour = RGB(0xE5, 0x39, 0x35);
} // namespace

@interface FPWindowController (Forms)
- (void)formFieldDrawn:(const FPToolEvent&)e;   // Forms.mm
@end
@interface FPWindowController (PagesMore)
- (void)cropDrawn:(const FPToolEvent&)e;        // PagesMore.mm
@end
@interface FPWindowController (Editing)
- (void)editEvent:(const FPToolEvent&)e;        // Editing.mm
- (BOOL)cropPictureWith:(const FPToolEvent&)e;
@end

@implementation FPWindowController (Annotate)

- (void)note:(const std::wstring&)t { form::message(self.window, t); }

// Called once the window is made: the page view's tools and right-click menu come here.
- (void)setupTools {
    __weak FPWindowController* weak = self;
    self.pageView.onToolEvent = [weak](const FPToolEvent& e) { [weak toolEvent:e]; };
    self.pageView.onContextMenu = [weak](int page, annot::PointD at, int a) -> NSMenu* { return [weak contextMenuFor:page at:at annot:a]; };
    self.pageView.lineWidthPt = lineWidth();
}

// Asks for a line of text (a comment, a note's text); `done` gets it when not empty.
- (void)askText:(const std::wstring&)title prompt:(const std::wstring&)prompt initial:(const std::wstring&)initial done:(std::function<void(const std::wstring&)>)done {
    Fields f(2);
    f[0] = {Field::Note, L"", {}, prompt};
    f[1] = {Field::Text, L"Text:", {}, initial};
    form::show(self.window, title, f, L"OK", nullptr, [done](const Fields& v) { if (!v[1].text.empty()) done(v[1].text); });
}

// ---- choosing a tool ----

- (IBAction)chooseTool:(NSMenuItem*)sender {
    const FPTool t = static_cast<FPTool>(sender.tag);
    self.pageView.tool = self.pageView.tool == t ? FPToolNone : t;   // choosing it again ends it
    self.pageView.lineWidthPt = lineWidth();
    static const wchar_t* hints[] = {nullptr, L"Click where the note should go.", nullptr, nullptr, nullptr, nullptr, nullptr,
                                     L"Click the corners one by one; double-click (or Return) ends the shape.", nullptr,
                                     L"Drag from the point the arrow shows to where the text box goes.", nullptr};
    if (t < 11 && hints[t] && self.pageView.tool == t) [self note:hints[t]];
}

- (IBAction)selectTextTool:(id)sender { self.pageView.tool = FPToolNone; }

// ---- text markup: the selected text ----

- (void)markup:(annot::Markup)kind comment:(BOOL)askComment {
    Document* d = self.pdf;
    std::vector<annot::RectD> rects;
    int page = 0;
    if (!d || ![self.pageView selectionRects:rects page:&page]) { [self note:L"Select some text first: drag over it on the page."]; return; }
    const COLORREF col = kind == annot::Markup::Highlight ? highlightColor() : drawColor();
    if (!askComment) {
        if (annot::addMarkup(*d, page, kind, rects, col)) [self afterChange];
        return;
    }
    [self askText:L"Comment" prompt:L"Type your comment on the selected text." initial:L"" done:[self, d, page, rects, col](const std::wstring& t) {
        if (annot::addMarkup(*d, page, annot::Markup::Highlight, rects, col, t)) [self afterChange];
    }];
}

- (IBAction)highlightSelection:(id)sender { [self markup:annot::Markup::Highlight comment:NO]; }
- (IBAction)underlineSelection:(id)sender { [self markup:annot::Markup::Underline comment:NO]; }
- (IBAction)strikeSelection:(id)sender { [self markup:annot::Markup::Strikethrough comment:NO]; }
- (IBAction)squigglySelection:(id)sender { [self markup:annot::Markup::Squiggly comment:NO]; }
- (IBAction)commentOnSelection:(id)sender { [self markup:annot::Markup::Highlight comment:YES]; }

// ---- stamps ----

- (void)startStamp:(const std::wstring&)text colour:(COLORREF)c {
    g_stamp = text;
    g_stampColour = c;
    self.pageView.tool = FPToolStamp;
    [self note:L"Click on the page where the stamp should go (its centre lands on the click). Esc cancels."];
}

- (IBAction)chooseStamp:(NSMenuItem*)sender {
    const NSInteger i = sender.tag;
    if (i >= 0 && i < 7) { [self startStamp:platform::wide(kStampNames[i]) colour:kStampCols[i]]; return; }
    static const wchar_t* words[] = {L"", L"APPROVED ", L"RECEIVED ", L"PAID "};
    static const COLORREF cols[] = {RGB(0x25, 0x6F, 0xD1), RGB(0x2E, 0x9E, 0x4A), RGB(0x25, 0x6F, 0xD1), RGB(0xD0, 0x2B, 0x2B)};
    if (i >= 10 && i < 14) {
        std::wstring t = std::wstring(words[i - 10]) + dateText("%d %b %Y") + (i == 10 ? L" " + dateText("%H:%M") : L"");
        [self startStamp:t colour:cols[i - 10]];
        return;
    }
    Fields f(2);   // custom
    f[0] = {Field::Text, L"Stamp text:", {}, L""};
    f[1] = {Field::Choice, L"Colour:", {L"Green", L"Blue", L"Orange", L"Red"}, L"", 3};
    form::show(self.window, L"Custom stamp", f, L"Choose place", [](const Fields& v) -> std::wstring {
        if (v[0].text.empty()) return L"Type the text of the stamp.";
        return pageobj::isLatin(v[0].text) ? L"" : L"A stamp can use letters, numbers and common symbols of Western languages.";
    }, [self](const Fields& v) {
        static const COLORREF c[] = {RGB(0x2E, 0x9E, 0x4A), RGB(0x25, 0x6F, 0xD1), RGB(0xE0, 0x8A, 0x00), RGB(0xD0, 0x2B, 0x2B)};
        [self startStamp:v[0].text colour:c[std::clamp(v[1].choice, 0, 3)]];
    });
}

// ---- colours and widths ----

- (IBAction)setDrawingColour:(NSMenuItem*)sender { [NSUserDefaults.standardUserDefaults setInteger:sender.tag forKey:@"drawingColour"]; }
- (IBAction)setHighlightColour:(NSMenuItem*)sender { [NSUserDefaults.standardUserDefaults setInteger:sender.tag forKey:@"highlightColour"]; }
- (IBAction)setLineWidth:(NSMenuItem*)sender {
    [NSUserDefaults.standardUserDefaults setInteger:sender.tag forKey:@"lineWidth"];
    self.pageView.lineWidthPt = lineWidth();
}

// ---- what a tool did on the page ----

- (void)toolEvent:(const FPToolEvent&)e {
    Document* d = self.pdf;
    if (!d) return;
    if (e.kind == FPToolEvent::Delete) {
        if (e.annot >= 0 && annot::remove(*d, e.page, e.annot)) [self afterChange];
        return;
    }
    if (e.kind == FPToolEvent::DoubleClick && self.pageView.tool == FPToolNone && e.annot >= 0) { [self editComment:e.page index:e.annot]; return; }
    if (e.pts.empty()) return;
    const annot::PointD a = e.pts.front(), b = e.pts.back();
    const annot::RectD box{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
    switch (self.pageView.tool) {
        case FPToolRectangle: case FPToolEllipse:
            if (e.kind == FPToolEvent::Draw &&
                annot::addShape(*d, e.page, self.pageView.tool == FPToolRectangle ? annot::Shape::Rectangle : annot::Shape::Ellipse, box, drawColor(), lineWidth()))
                [self afterChange];
            break;
        case FPToolLine: case FPToolArrow: {
            if (e.kind != FPToolEvent::Draw) break;
            std::vector<std::vector<annot::PointD>> strokes{{a, b}};
            const double dx = b.x - a.x, dy = b.y - a.y;
            if (self.pageView.tool == FPToolArrow && std::hypot(dx, dy) > 1) {
                const double head = std::max(8.0, lineWidth() * 5), ang = 0.45, base = std::atan2(dy, dx) + M_PI;
                for (double s : {-1.0, 1.0}) strokes.push_back({b, {b.x + head * std::cos(base + s * ang), b.y + head * std::sin(base + s * ang)}});
            }
            if (annot::addInk(*d, e.page, strokes, drawColor(), lineWidth())) [self afterChange];
            break;
        }
        case FPToolPen:
            if (e.pts.size() >= 2 && annot::addInk(*d, e.page, {e.pts}, drawColor(), lineWidth())) [self afterChange];
            break;
        case FPToolPolygon:
            if (e.kind == FPToolEvent::Draw && annot::addPolygon(*d, e.page, e.pts, drawColor(), lineWidth())) [self afterChange];
            break;
        case FPToolCloud:
            if (e.kind == FPToolEvent::Draw && annot::addCloud(*d, e.page, box, drawColor(), lineWidth())) [self afterChange];
            break;
        case FPToolCallout: {
            if (e.kind != FPToolEvent::Draw) { [self note:L"Drag from the point the arrow shows to where the text box goes."]; break; }
            const int page = e.page;
            [self askText:L"Callout" prompt:L"Type the text of the callout." initial:L"" done:[self, d, page, a, b](const std::wstring& t) {
                if (annot::addCallout(*d, page, a, b, t, drawColor())) [self afterChange];
            }];
            break;
        }
        case FPToolNote: {
            const int page = e.page;
            [self askText:L"Comment" prompt:L"Type your comment." initial:L"" done:[self, d, page, a](const std::wstring& t) {
                if (annot::addNote(*d, page, a, t, highlightColor())) [self afterChange];
            }];
            break;
        }
        case FPToolStamp:
            if (pageobj::addStamp(*d, e.page, a.x, a.y, g_stamp, g_stampColour)) [self afterChange];
            else [self note:L"The stamp could not be added."];
            break;
        case FPToolRedact:
            if (e.kind == FPToolEvent::Draw) {
                d->redactions.push_back({e.page, box.left, box.bottom, box.right, box.top});
                [self.pageView setNeedsDisplay:YES];
            }
            break;
        case FPToolCrop: if (![self cropPictureWith:e]) [self cropDrawn:e]; break;
        case FPToolTextBox: case FPToolPicture: case FPToolSelectObject: case FPToolEditText: case FPToolEditPara:
        case FPToolEditTable: case FPToolPlaceTable:
            [self editEvent:e];
            break;
        default:
            if (self.pageView.tool >= FPToolFormText) [self formFieldDrawn:e];
            break;
    }
}

- (void)editComment:(int)page index:(int)index {
    Document* d = self.pdf;
    std::wstring cur;
    bool callout = false;
    for (const auto& a : annot::list(*d, page)) if (a.index == index) { cur = a.contents; callout = a.callout; }
    [self askText:callout ? L"Callout" : L"Comment" prompt:L"Edit the comment." initial:cur done:[self, d, page, index, cur, callout](const std::wstring& t) {
        if (t == cur) return;
        if (callout ? annot::setCalloutText(*d, page, index, t) : annot::setContents(*d, page, index, t)) [self afterChange];
    }];
}

- (IBAction)deleteSelectedAnnotation:(id)sender {
    Document* d = self.pdf;
    const int page = self.pageView.selectedAnnotPage, i = self.pageView.selectedAnnot;
    if (d && i >= 0 && annot::remove(*d, page, i)) { [self.pageView clearAnnotSelection]; [self afterChange]; }
}

- (IBAction)editSelectedComment:(id)sender {
    if (self.pageView.selectedAnnot >= 0) [self editComment:self.pageView.selectedAnnotPage index:self.pageView.selectedAnnot];
}

// ---- flatten, notes summary ----

- (IBAction)flattenComments:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    form::ask(self.window, L"Make every comment, drawing and form field part of the page? They can no longer be changed or deleted one by one (links keep working). Undo takes it back.",
              L"Flatten", [self, d](bool yes) {
        if (!yes) return;
        const int n = d->flattenAll();
        if (n > 0) { [self afterChange]; [self note:std::to_wstring(n) + L" page(s) flattened."]; }
        else [self note:L"There is nothing to flatten."];
    });
}

- (IBAction)notesSummary:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    const std::wstring md = annot::notesSummary(*d);
    if (md.empty()) { [self note:L"This document has no highlights or comments yet."]; return; }
    NSSavePanel* p = [NSSavePanel savePanel];
    std::wstring base = d->title();
    if (base.find_last_of(L'.') != std::wstring::npos) base.resize(base.find_last_of(L'.'));
    p.nameFieldStringValue = ns(base + L" (notes).md");
    p.title = @"Save the notes as";
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        const std::string u = "\xEF\xBB\xBF" + platform::utf8(md);
        if (Document::writeFile(platform::wide(p.URL.fileSystemRepresentation), std::vector<unsigned char>(u.begin(), u.end())) != SaveResult::Ok) {
            [self note:L"The notes could not be saved. The folder may be read-only, or the disk may be full."];
            return;
        }
        [[NSWorkspace sharedWorkspace] openURL:p.URL];
    }];
}

// ---- redaction ----

- (IBAction)markSelectionForRedaction:(id)sender {
    Document* d = self.pdf;
    std::vector<annot::RectD> rects;
    int page = 0;
    if (!d || ![self.pageView selectionRects:rects page:&page]) { [self note:L"Select the text to redact first: drag over it on the page."]; return; }
    for (const auto& r : rects) d->redactions.push_back({page, r.left, r.bottom, r.right, r.top});
    [self.pageView setNeedsDisplay:YES];
}

- (IBAction)findAndMarkRedaction:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(2);
    f[0] = {Field::Text, L"Find:", {}, L""};
    f[1] = {Field::Check, L"Match case"};
    form::show(self.window, L"Find and mark for redaction", f, L"Mark all", [](const Fields& v) -> std::wstring {
        return v[0].text.empty() ? L"Type the text to find." : L"";
    }, [self, d](const Fields& v) {
        const int n = redact::markText(*d, v[0].text, v[1].checked);
        [self.pageView setNeedsDisplay:YES];
        [self note:n ? std::to_wstring(n) + L" place(s) marked. Check the marks, then choose Apply Redactions." : L"The text was not found."];
    });
}

- (IBAction)applyRedactions:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    if (d->redactions.empty()) { [self note:L"There are no marks yet. Mark areas (Redact tool), selected text, or search results first."]; return; }
    form::ask(self.window, L"Remove for good everything under the " + std::to_wstring(d->redactions.size()) +
                           L" mark(s)? The marked pages become pictures with black boxes; their text there can no longer be found or copied. Undo takes it back until you save.",
              L"Apply", [self, d](bool yes) {
        if (!yes) return;
        const std::vector<std::wstring> phrases = redact::collect(*d);
        const int n = redact::apply(*d);
        if (!n) { [self note:L"The redactions could not be applied."]; return; }
        [self afterChange];
        const redact::Report rep = redact::verify(*d, phrases);
        std::wstring msg = std::to_wstring(n) + L" page(s) redacted.";
        if (!phrases.empty()) {
            if (rep.found.empty()) msg += L" The redacted text was looked for everywhere in the file (" + std::to_wstring(rep.places) + L" places) and is gone.";
            else {
                msg += L"\n\nStill found:";
                for (const auto& f : rep.found) msg += L"\n  - \"" + f.phrase + L"\" in " + f.where;
            }
        }
        [self note:msg + L"\n\nSave the document to make it final."];
    });
}

- (IBAction)clearRedactionMarks:(id)sender {
    if (Document* d = self.pdf) { d->redactions.clear(); [self.pageView setNeedsDisplay:YES]; }
}

- (IBAction)checkRedaction:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(2);
    f[0] = {Field::Note, L"", {}, L"Looks for words or phrases everywhere the file could still hold them: page text, comments, bookmarks, document information and the raw file data."};
    f[1] = {Field::Text, L"Words or phrases (separate them with ;):", {}, L""};
    form::show(self.window, L"Check that the text is gone", f, L"Check", nullptr, [self, d](const Fields& v) {
        std::vector<std::wstring> phrases;
        std::wstring cur;
        for (wchar_t c : v[1].text + L";") {
            if (c == L';') { while (!cur.empty() && cur.back() == L' ') cur.pop_back(); size_t s = cur.find_first_not_of(L' '); if (s != std::wstring::npos) phrases.push_back(cur.substr(s)); cur.clear(); }
            else cur += c;
        }
        if (phrases.empty()) return;
        const redact::Report rep = redact::verify(*d, phrases);
        std::wstring msg = L"Looked in " + std::to_wstring(rep.places) + L" places.";
        if (rep.found.empty()) msg += L" None of the phrases was found.";
        for (const auto& fd : rep.found) msg += L"\n  - \"" + fd.phrase + L"\" in " + fd.where;
        [self note:msg];
    });
}

// ---- the right-click menu ----

- (NSMenu*)contextMenuFor:(int)page at:(annot::PointD)at annot:(int)a {
    NSMenu* m = [[NSMenu alloc] init];
    auto add = [&](NSString* title, SEL action) { [m addItemWithTitle:title action:action keyEquivalent:@""].target = self; };
    if (self.pageView.hasSelection) {
        [m addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@""];
        add(@"Highlight", @selector(highlightSelection:));
        add(@"Underline", @selector(underlineSelection:));
        add(@"Strikethrough", @selector(strikeSelection:));
        add(@"Comment on Selected Text…", @selector(commentOnSelection:));
        add(@"Mark for Redaction", @selector(markSelectionForRedaction:));
        [m addItem:[NSMenuItem separatorItem]];
    }
    if (a >= 0) {
        add(@"Edit Comment…", @selector(editSelectedComment:));
        add(@"Delete", @selector(deleteSelectedAnnotation:));
        [m addItem:[NSMenuItem separatorItem]];
    }
    add(@"Rotate Page Right", @selector(rotateRight:));
    add(@"Rotate Page Left", @selector(rotateLeft:));
    add(@"Delete Pages…", @selector(deletePages:));
    add(@"Properties…", @selector(showProperties:));
    return m;
}

@end
