// More of the Pages and File menus: move, crop (by an amount or by drawing a box), merge, split, new PDF from pictures
// or from a text / Markdown file, and copying a page as a picture. As the Windows app does them.
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "PageView.h"
#import "Render.h"
#import "NewPdf.h"
#import "Trace.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include "pageobjects.hpp"
#include "picture.hpp"
#include "platform.hpp"
#include "splitmerge.hpp"
#include "textpdf.hpp"
#include "topdf.hpp"

using form::Field;
using form::Fields;
using form::ns;

@interface FPWindowController (PagesMore)
@end

@implementation FPWindowController (PagesMore)

- (void)say:(const std::wstring&)t { form::message(self.window, t); }

- (std::wstring)pageText { return std::to_wstring([self.pageView currentPage] + 1); }

- (IBAction)movePages:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(2);
    f[0] = {Field::Text, L"Pages to move:", {}, [self pageText]};
    f[1] = {Field::Text, L"Move them before page (" + std::to_wstring(d->pageCount() + 1) + L" for the end):", {}, L"1"};
    form::show(self.window, L"Move pages", f, L"Move", [d](const Fields& v) -> std::wstring {
        if (d->parseRange(v[0].text).empty()) return L"Type page numbers and ranges such as 1-3, 5, 8-.";
        wchar_t* end = nullptr;
        const long n = wcstol(v[1].text.c_str(), &end, 10);
        if (end == v[1].text.c_str() || *end || n < 1 || n > d->pageCount() + 1)
            return L"Type 1 to " + std::to_wstring(d->pageCount()) + L", or " + std::to_wstring(d->pageCount() + 1) + L" to move them to the end.";
        return L"";
    }, [self, d](const Fields& v) {
        if (d->movePages(d->parseRange(v[0].text), static_cast<int>(wcstol(v[1].text.c_str(), nullptr, 10)) - 1)) [self afterChange];
    });
}

- (IBAction)cropPages:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(2);
    f[0] = {Field::Note, L"", {}, L"Cuts the same amount off every edge of every page. To cut a box of your own, choose Pages > Crop by Drawing a Box."};
    f[1] = {Field::Text, L"Millimetres (0 to 100):", {}, L"10"};
    form::show(self.window, L"Crop pages", f, L"Crop", [](const Fields& v) -> std::wstring {
        wchar_t* end = nullptr;
        const double mm = wcstod(v[1].text.c_str(), &end);
        return end == v[1].text.c_str() || *end || mm <= 0 || mm > 100 ? L"Type a number of millimetres greater than 0 and up to 100." : L"";
    }, [self, d](const Fields& v) {
        std::vector<int> all(static_cast<size_t>(d->pageCount()));
        for (int i = 0; i < d->pageCount(); ++i) all[static_cast<size_t>(i)] = i;
        const double pt = wcstod(v[1].text.c_str(), nullptr) * 72.0 / 25.4;
        if (d->cropPages(all, pt, pt, pt, pt)) [self afterChange];
        else [self say:L"That would leave the pages too small. Choose a smaller amount."];
    });
}

- (IBAction)cropByBox:(id)sender {
    if (!self.pdf) return;
    self.pageView.tool = FPToolCrop;
    [self say:L"Draw a box around what should stay of the page. Everything outside it is cut off (Undo brings it back)."];
}

// The box drawn with the crop tool (Annotate.mm passes it on).
- (void)cropDrawn:(const FPToolEvent&)e {
    Document* d = self.pdf;
    if (!d || e.kind != FPToolEvent::Draw || e.pts.size() < 2) return;
    double l, b, r, t;
    if (!d->cropBoxOf(e.page, l, b, r, t)) return;
    const double bl = std::min(e.pts[0].x, e.pts[1].x), br = std::max(e.pts[0].x, e.pts[1].x);
    const double bb = std::min(e.pts[0].y, e.pts[1].y), bt = std::max(e.pts[0].y, e.pts[1].y);
    self.pageView.tool = FPToolNone;
    if (d->cropPages({e.page}, std::max(0.0, bl - l), std::max(0.0, bb - b), std::max(0.0, r - br), std::max(0.0, t - bt))) [self afterChange];
    else [self say:L"That box is too small to keep."];
}

- (IBAction)mergeFiles:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    // the build check names the files (FEATHER_UI_MERGE="a.pdf:b.pdf") instead of choosing them in the panel
    if (NSString* list = NSProcessInfo.processInfo.environment[@"FEATHER_UI_MERGE"]) {
        NSMutableArray<NSURL*>* urls = [NSMutableArray array];
        for (NSString* f in [list componentsSeparatedByString:@":"]) if (f.length) [urls addObject:[NSURL fileURLWithPath:f]];
        [self askMergeBookmarks:urls];
        return;
    }
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypePDF ];
    p.allowsMultipleSelection = YES;
    p.title = @"Choose the PDFs to add at the end (they are added in the order of their names)";
    __weak FPWindowController* weak = self;
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        NSArray<NSURL*>* urls = [p.URLs sortedArrayUsingComparator:^NSComparisonResult(NSURL* a, NSURL* b) { return [a.path localizedStandardCompare:b.path]; }];
        dispatch_async(dispatch_get_main_queue(), ^{ [weak askMergeBookmarks:urls]; });
    }];
}

// What happens to the bookmarks, then the merge: one new set of pages with every file's bookmarks (one undo step),
// or the pages alone added at the end.
- (void)askMergeBookmarks:(NSArray<NSURL*>*)urls {
    Document* d = self.pdf;
    if (!d || !urls.count) return;
    Fields f(1);
    f[0] = {Field::Choice, L"Bookmarks:", {L"Keep the bookmarks of every file", L"A bookmark for each file, its own bookmarks inside", L"Add the pages only"}, L"", 0};
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Merge PDFs", f, L"Merge", nullptr, [weak, d, urls](const Fields& v) {
        const int choice = v[0].choice;   // read now: the answers go with the form
        dispatch_async(dispatch_get_main_queue(), ^{ [weak mergeURLs:urls into:d bookmarks:choice]; });
    });
}

- (void)mergeURLs:(NSArray<NSURL*>*)urls into:(Document*)d bookmarks:(int)choice {
    std::vector<std::unique_ptr<Document>> docs;
    std::wstring problems;
    for (NSURL* u in urls) {
        auto src = std::make_unique<Document>();
        const std::wstring path = platform::wide(u.fileSystemRepresentation);
        const OpenResult o = src->open(path);
        if (o != OpenResult::Ok) { problems += Document::describe(o, path) + L"\n"; continue; }
        docs.push_back(std::move(src));
    }
    const int before = d->pageCount();
    int added = 0;
    if (choice < 2 && !docs.empty()) {
        auto stem = [](std::wstring t) { const size_t dot = t.find_last_of(L'.'); if (dot != std::wstring::npos) t.resize(dot); return t; };
        std::vector<splitmerge::Source> src{{d, stem(d->title())}};
        for (auto& one : docs) src.push_back({one.get(), stem(one->title())});
        std::vector<unsigned char> bytes;
        if (splitmerge::merge(src, choice == 0 ? splitmerge::Marks::Keep : splitmerge::Marks::UnderFileNames, bytes)) {
            d->beginChange(L"Merge PDFs");
            if (d->reload(std::move(bytes)) == OpenResult::Ok) added = static_cast<int>(docs.size());
            else d->rollbackChange();
        }
        if (!added) problems += L"The files could not be merged.\n";
    } else
        for (auto& src : docs) {
            std::vector<int> all(static_cast<size_t>(src->pageCount()));
            for (int i = 0; i < src->pageCount(); ++i) all[static_cast<size_t>(i)] = i;
            if (d->importPages(*src, all, d->pageCount())) ++added;
            else problems += L"The pages of \"" + src->title() + L"\" could not be added.\n";
        }
    FPTrace(@"merge: %d file(s), %d pages, %d bookmarks", added, d->pageCount(), static_cast<int>(d->bookmarks().size()));
    if (added) { [self afterChange]; [self.pageView showPage:std::min(before, d->pageCount() - 1)]; }
    if (!problems.empty()) [self say:problems];
}

// A stack scanned on one side, turned over and scanned again: fronts 1, 2, 3 then backs 3, 2, 1 -> 1, 1, 2, 2, 3, 3.
- (IBAction)interleaveScan:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(3);
    f[0] = {Field::Note, L"", {}, L"For a two-sided paper scanned on a one-sided scanner: first all the fronts, then all the backs. The pages are put in order: front 1, back 1, front 2, back 2 \u2026"};
    f[1] = {Field::Choice, L"The back pages are:", {L"In the second half of this document", L"In another PDF file (choose it next)"}, L"", 0};
    f[2] = {Field::Check, L"The backs are in reverse order (the stack was turned over)"};
    f[2].checked = true;
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Two-sided scan", f, L"Put in Order", nullptr, [weak, d](const Fields& v) {
        const bool other = v[1].choice == 1, reversed = v[2].checked;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!other) { [weak interleave:d backs:nil reversed:reversed]; return; }
            NSOpenPanel* p = [NSOpenPanel openPanel];
            p.allowedContentTypes = @[ UTTypePDF ];
            p.title = @"Choose the PDF with the back pages";
            [p beginSheetModalForWindow:weak.window completionHandler:^(NSModalResponse r) {
                if (r == NSModalResponseOK) [weak interleave:d backs:p.URL reversed:reversed];
            }];
        });
    });
}

- (void)interleave:(Document*)d backs:(NSURL*)url reversed:(bool)reversed {
    Document backs;
    if (url) {
        const std::wstring path = platform::wide(url.fileSystemRepresentation);
        const OpenResult o = backs.open(path);
        if (o != OpenResult::Ok) { [self say:Document::describe(o, path)]; return; }
    }
    std::vector<unsigned char> bytes;
    if (!splitmerge::interleave(*d, url ? &backs : nullptr, reversed, bytes)) { [self say:L"The pages could not be put in order."]; return; }
    d->beginChange(L"Two-sided scan: pages in order");
    if (d->reload(std::move(bytes)) != OpenResult::Ok) { d->rollbackChange(); return; }
    [self afterChange];
    FPTrace(@"interleave: %d pages, page 2 says %s", d->pageCount(), platform::utf8(d->textRange(1, 0, std::min(6, d->charCount(1)))).c_str());
}

- (IBAction)splitDocument:(id)sender {
    Document* d = self.pdf;
    if (!d || d->pageCount() < 2) { [self say:L"A document needs at least two pages to be split."]; return; }
    Fields f(4);
    f[0] = {Field::Choice, L"Split:", {L"Into single pages", L"Every N pages", L"By page ranges", L"At the bookmarks (chapters)", L"Into files no larger than"}, L"", 1};
    f[1] = {Field::Text, L"N pages per file:", {}, L"10"};
    f[2] = {Field::Text, L"Ranges (use ; between):", {}, L"1-5; 6-"};
    f[3] = {Field::Text, L"Largest file (MB):", {}, L"10"};
    if (const char* how = getenv("FEATHER_UI_SPLIT")) f[0].choice = atoi(how);   // the build check
    auto parts = [d](const std::wstring& t) {
        std::vector<std::wstring> out;
        size_t pos = 0;
        while (pos <= t.size()) {
            const size_t e = t.find(L';', pos);
            std::wstring part = t.substr(pos, e == std::wstring::npos ? std::wstring::npos : e - pos);
            if (part.find_first_not_of(L' ') != std::wstring::npos) out.push_back(part);
            if (e == std::wstring::npos) break;
            pos = e + 1;
        }
        (void)d;
        return out;
    };
    form::show(self.window, L"Split into several files", f, L"Name…", [d, parts](const Fields& v) -> std::wstring {
        if (v[0].choice == 1) { wchar_t* e = nullptr; const long n = wcstol(v[1].text.c_str(), &e, 10); if (e == v[1].text.c_str() || *e || n < 1) return L"Type how many pages each file should have."; }
        if (v[0].choice == 2) {
            const auto ps = parts(v[2].text);
            if (ps.empty()) return L"Type at least one range.";
            for (const auto& p : ps) if (d->parseRange(p).empty()) return L"\"" + p + L"\" is not a valid page range.";
        }
        if (v[0].choice == 3 && d->bookmarks().empty()) return L"This document has no bookmarks. Choose another way to split it.";
        if (v[0].choice == 4) { wchar_t* e = nullptr; const double mb = wcstod(v[3].text.c_str(), &e); if (e == v[3].text.c_str() || *e || mb <= 0.01) return L"Type the largest size in megabytes, for example 10 or 2.5."; }
        return L"";
    }, [self, d, parts](const Fields& v) {
        std::vector<std::vector<int>> groups;
        std::vector<std::wstring> names;   // the bookmarks' titles, for the file names
        const int n = d->pageCount();
        if (v[0].choice >= 3) {
            const auto ps = v[0].choice == 3 ? splitmerge::byBookmarks(*d, 1) : splitmerge::bySize(*d, wcstod(v[3].text.c_str(), nullptr) * 1024 * 1024);
            for (const auto& p : ps) { groups.push_back(p.pages); names.push_back(splitmerge::fileSafe(p.name)); }
        } else if (v[0].choice == 0) for (int i = 0; i < n; ++i) groups.push_back({i});
        else if (v[0].choice == 1) {
            const int per = static_cast<int>(wcstol(v[1].text.c_str(), nullptr, 10));
            for (int i = 0; i < n; i += per) { std::vector<int> g; for (int k = i; k < std::min(n, i + per); ++k) g.push_back(k); groups.push_back(g); }
        } else for (const auto& p : parts(v[2].text)) groups.push_back(d->parseRange(p));
        auto write = [self, d, groups, names](std::wstring stem) {
            if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, L".pdf") == 0) stem.resize(stem.size() - 4);
            int saved = 0;
            for (size_t i = 0; i < groups.size(); ++i) {
                if (groups[i].empty()) continue;
                std::wstring file = stem + L"-" + std::to_wstring(i + 1);
                if (i < names.size() && !names[i].empty()) file += L" " + names[i];
                std::vector<unsigned char> bytes;   // with the bookmarks that point into the part
                if (splitmerge::partBytes(*d, groups[i], bytes) && Document::writeFile(file + L".pdf", bytes) == SaveResult::Ok) ++saved;
            }
            FPTrace(@"split: %d file(s)", saved);
            [self say:std::to_wstring(saved) + L" file(s) were saved, numbered from \"" + stem.substr(stem.find_last_of(L'/') + 1) + L"-1\"."];
        };
        if (const char* to = getenv("FEATHER_UI_SPLIT_TO")) { write(platform::wide(to)); return; }   // the build check
        NSSavePanel* p = [NSSavePanel savePanel];
        std::wstring base = d->title();
        if (base.find_last_of(L'.') != std::wstring::npos) base.resize(base.find_last_of(L'.'));
        p.nameFieldStringValue = ns(base + L" (part).pdf");
        p.title = @"Name for the files (a number is added)";
        [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
            if (r != NSModalResponseOK) return;
            write(platform::wide(p.URL.fileSystemRepresentation));
        }];
    });
}

- (IBAction)copyPageAsPicture:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    const int page = [self.pageView currentPage];
    const int w = static_cast<int>(std::lround(d->pageSize(page).w * 150 / 72.0)), h = static_cast<int>(std::lround(d->pageSize(page).h * 150 / 72.0));
    CGImageRef img = FPRenderPage(*d, page, w, h, 0, 0, w, h, pagecolours::Mode::Normal);
    if (!img) return;
    NSImage* ns = [[NSImage alloc] initWithCGImage:img size:NSMakeSize(w * 72.0 / 150, h * 72.0 / 150)];
    CGImageRelease(img);
    [NSPasteboard.generalPasteboard clearContents];
    [NSPasteboard.generalPasteboard writeObjects:@[ ns ]];
    [self say:L"Page " + std::to_wstring(page + 1) + L" was copied as a picture (150 dpi). Paste it into another app."];
}

@end

// ---- new documents (File > New PDF): need no open document ----

@implementation FPNewPdf

+ (void)fromPictures {
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypeImage ];
    p.allowsMultipleSelection = YES;
    p.title = @"Choose the pictures (they become pages in the order of their names)";
    if ([p runModal] != NSModalResponseOK) return;
    NSArray<NSURL*>* urls = [p.URLs sortedArrayUsingComparator:^NSComparisonResult(NSURL* a, NSURL* b) { return [a.path localizedStandardCompare:b.path]; }];
    std::vector<std::pair<double, double>> sizes;
    std::vector<std::wstring> ok;
    for (NSURL* u in urls) {
        const std::wstring f = platform::wide(u.fileSystemRepresentation);
        int w, h;
        if (!pageobj::imageSize(f, w, h)) continue;
        double pw = w * 0.75, ph = h * 0.75;
        const double big = std::max(pw, ph);
        if (big > 842) { pw *= 842 / big; ph *= 842 / big; }   // no larger than A4's long side
        sizes.push_back({pw, ph});
        ok.push_back(f);
    }
    if (ok.empty()) { form::message(nil, L"None of those files is a picture Feather PDF can read.", true); return; }
    std::vector<unsigned char> bytes;
    Document doc;
    if (!Document::blankBytes(sizes, bytes) || doc.load(std::move(bytes), {}) != OpenResult::Ok) { form::message(nil, L"The new document could not be made.", true); return; }
    int bad = 0;
    for (size_t i = 0; i < ok.size(); ++i) {
        std::wstring err;
        if (!pageobj::addPicture(doc, static_cast<int>(i), 0, sizes[i].second, ok[i], err, sizes[i].first)) ++bad;
    }
    std::vector<unsigned char> out;
    if (!doc.serialize(out) || ![FPDocument openNewWithBytes:std::move(out) name:@"New PDF.pdf"]) { form::message(nil, L"The new document could not be made.", true); return; }
    if (bad) form::message(nil, L"Some pictures could not be placed. Their pages are empty.", true);
}

+ (void)combineOpen {
    // the key window's tabs in their order, then every other open document
    NSMutableArray<FPDocument*>* docs = [NSMutableArray array];
    auto take = [&](NSWindow* w) {
        FPDocument* d = [w.windowController.document isKindOfClass:[FPDocument class]] ? (FPDocument*)w.windowController.document : nil;
        if (d && d.pdf && ![docs containsObject:d]) [docs addObject:d];
    };
    NSWindow* key = NSApp.keyWindow ?: NSApp.mainWindow;
    for (NSWindow* w in (key.tabbedWindows ?: (key ? @[ key ] : @[]))) take(w);
    for (NSDocument* d in [NSDocumentController sharedDocumentController].documents)
        if ([d isKindOfClass:[FPDocument class]] && ((FPDocument*)d).pdf && ![docs containsObject:(FPDocument*)d]) [docs addObject:(FPDocument*)d];
    if (docs.count > 24) [docs removeObjectsInRange:NSMakeRange(24, docs.count - 24)];
    if (docs.count < 2) {
        form::message(key, L"Open the PDFs to combine first (two or more), then choose Combine Open PDFs again.");
        return;
    }
    form::Fields f;
    f.push_back({form::Field::Note, L"", {}, L"Tick the documents to combine. They follow one another in the order shown (this window's tabs first)."});
    for (FPDocument* d in docs) {
        const int n = d.pdf->pageCount();
        form::Field c{form::Field::Check, d.pdf->title() + L"  (" + std::to_wstring(n) + (n == 1 ? L" page)" : L" pages)")};
        c.checked = true;
        f.push_back(c);
    }
    f.push_back({form::Field::Choice, L"Bookmarks:", {L"A bookmark for each document, its own bookmarks inside", L"Keep the bookmarks of every document", L"No bookmarks"}, L"", 0});
    NSArray<FPDocument*>* list = [docs copy];
    form::show(key, L"Combine PDFs", f, L"Combine", [](const form::Fields& v) -> std::wstring {
        int ticked = 0;
        for (const auto& x : v) if (x.kind == form::Field::Check && x.checked) ++ticked;
        return ticked < 2 ? L"Tick two or more documents." : L"";
    }, [list](const form::Fields& v) {
        std::vector<bool> use;
        for (size_t i = 0; i < list.count; ++i) use.push_back(v[i + 1].checked);
        const int marks = v.back().choice;
        dispatch_async(dispatch_get_main_queue(), ^{   // after the sheet has gone
            std::vector<splitmerge::Source> src;
            for (size_t i = 0; i < list.count; ++i) {
                if (!use[i] || !list[i].pdf) continue;
                std::wstring name = list[i].pdf->title();
                const size_t dot = name.find_last_of(L'.');
                if (dot != std::wstring::npos && (name.compare(dot, std::wstring::npos, L".pdf") == 0 || name.compare(dot, std::wstring::npos, L".PDF") == 0)) name.resize(dot);
                src.push_back({list[i].pdf, name});
            }
            std::vector<unsigned char> bytes;
            const bool ok = src.size() > 1 && splitmerge::merge(src, marks == 0 ? splitmerge::Marks::UnderFileNames : marks == 1 ? splitmerge::Marks::Keep : splitmerge::Marks::None, bytes);
            FPDocument* made = ok ? [FPDocument openNewWithBytes:std::move(bytes) name:@"Combined.pdf"] : nil;
            if (!made) { form::message(nil, L"The documents could not be combined. A document protected against changes (a permissions password) cannot be copied from; remove its protection first.", true); return; }
            FPTrace(@"combine: %d documents, %d pages, %d bookmarks", static_cast<int>(src.size()), made.pdf->pageCount(), static_cast<int>(made.pdf->bookmarks().size()));
        });
    });
}

+ (void)convertFiles:(NSArray<NSURL*>*)urls {
    std::wstring problems;
    for (NSURL* u in urls) {
        std::wstring written;
        const topdf::Result r = topdf::convert(platform::wide(u.fileSystemRepresentation), written);
        FPTrace(@"convert %@: %d %s", u.lastPathComponent, r.ok, platform::utf8(written).c_str());
        if (!r.ok) { problems += L"\u201c" + form::ws(u.lastPathComponent) + L"\u201d could not be converted to PDF. " + r.error + L"\n\n"; continue; }
        NSURL* pdf = [NSURL fileURLWithPath:[NSString stringWithUTF8String:platform::utf8(written).c_str()]];
        [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:pdf display:YES
                                                                     completionHandler:^(NSDocument*, BOOL, NSError* e) { if (e) [NSApp presentError:e]; }];
    }
    if (!problems.empty()) form::message(nil, problems, true);
}

+ (void)fromTextFile {
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypePlainText, [UTType typeWithFilenameExtension:@"md"] ?: UTTypePlainText ];
    p.title = @"Choose a text (.txt) or Markdown (.md) file";
    if ([p runModal] != NSModalResponseOK) return;
    const std::wstring path = platform::wide(p.URL.fileSystemRepresentation);
    const bool markdown = ![p.URL.pathExtension.lowercaseString isEqualToString:@"txt"];
    std::vector<unsigned char> bytes;
    const textpdf::Result r = textpdf::build(platform::readTextFile(path), markdown, bytes);
    NSString* name = [p.URL.lastPathComponent.stringByDeletingPathExtension stringByAppendingPathExtension:@"pdf"];
    if (!r.ok || ![FPDocument openNewWithBytes:std::move(bytes) name:name]) { form::message(nil, L"The file could not be made into a PDF.", true); return; }
    if (r.replaced)
        form::message(nil, std::to_wstring(r.replaced) + L" character(s) are not in the standard PDF fonts (for example Indian or Asian scripts) and were written as \"?\".");
}

@end
