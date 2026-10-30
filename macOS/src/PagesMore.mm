// More of the Pages and File menus: move, crop (by an amount or by drawing a box), merge, split, new PDF from pictures
// or from a text / Markdown file, and copying a page as a picture. As the Windows app does them.
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "PageView.h"
#import "Render.h"
#import "NewPdf.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <algorithm>
#include <cmath>
#include "pageobjects.hpp"
#include "picture.hpp"
#include "platform.hpp"
#include "textpdf.hpp"

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
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypePDF ];
    p.allowsMultipleSelection = YES;
    p.title = @"Choose the PDFs to add at the end (they are added in the order of their names)";
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        NSArray<NSURL*>* urls = [p.URLs sortedArrayUsingComparator:^NSComparisonResult(NSURL* a, NSURL* b) { return [a.path localizedStandardCompare:b.path]; }];
        int added = 0;
        std::wstring problems;
        for (NSURL* u in urls) {
            Document src;
            const std::wstring path = platform::wide(u.fileSystemRepresentation);
            const OpenResult o = src.open(path);
            if (o != OpenResult::Ok) { problems += Document::describe(o, path) + L"\n"; continue; }
            std::vector<int> all(static_cast<size_t>(src.pageCount()));
            for (int i = 0; i < src.pageCount(); ++i) all[static_cast<size_t>(i)] = i;
            if (d->importPages(src, all, d->pageCount())) ++added;
            else problems += L"The pages of \"" + src.title() + L"\" could not be added.\n";
        }
        if (added) { [self afterChange]; [self.pageView showPage:d->pageCount() - 1]; }
        if (!problems.empty()) [self say:problems];
    }];
}

- (IBAction)splitDocument:(id)sender {
    Document* d = self.pdf;
    if (!d || d->pageCount() < 2) { [self say:L"A document needs at least two pages to be split."]; return; }
    Fields f(3);
    f[0] = {Field::Choice, L"Split:", {L"Into single pages", L"Every N pages", L"By page ranges"}, L"", 1};
    f[1] = {Field::Text, L"N pages per file:", {}, L"10"};
    f[2] = {Field::Text, L"Ranges (use ; between):", {}, L"1-5; 6-"};
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
        return L"";
    }, [self, d, parts](const Fields& v) {
        std::vector<std::vector<int>> groups;
        const int n = d->pageCount();
        if (v[0].choice == 0) for (int i = 0; i < n; ++i) groups.push_back({i});
        else if (v[0].choice == 1) {
            const int per = static_cast<int>(wcstol(v[1].text.c_str(), nullptr, 10));
            for (int i = 0; i < n; i += per) { std::vector<int> g; for (int k = i; k < std::min(n, i + per); ++k) g.push_back(k); groups.push_back(g); }
        } else for (const auto& p : parts(v[2].text)) groups.push_back(d->parseRange(p));
        NSSavePanel* p = [NSSavePanel savePanel];
        std::wstring base = d->title();
        if (base.find_last_of(L'.') != std::wstring::npos) base.resize(base.find_last_of(L'.'));
        p.nameFieldStringValue = ns(base + L" (part).pdf");
        p.title = @"Name for the files (a number is added)";
        [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
            if (r != NSModalResponseOK) return;
            std::wstring stem = platform::wide(p.URL.fileSystemRepresentation);
            if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, L".pdf") == 0) stem.resize(stem.size() - 4);
            int saved = 0;
            for (size_t i = 0; i < groups.size(); ++i)
                if (!groups[i].empty() && d->extractPages(groups[i], stem + L"-" + std::to_wstring(i + 1) + L".pdf") == SaveResult::Ok) ++saved;
            [self say:std::to_wstring(saved) + L" file(s) were saved next to \"" + stem.substr(stem.find_last_of(L'/') + 1) + L"-1.pdf\"."];
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
