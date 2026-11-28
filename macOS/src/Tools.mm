// The tools of the Edit PDF, Pages and Protect menus: the same forms, defaults, checks and messages as the Windows app
// (Windows/src/main.cpp), on the shared engine.
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "PageView.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <algorithm>
#include <cmath>
#include "compress.hpp"
#include "exporter.hpp"
#include "fontlist.hpp"
#include "headfoot.hpp"
#include "pageobjects.hpp"
#include "pdfa.hpp"
#include "platform.hpp"
#include "protect.hpp"
#include "sanitize.hpp"

using form::Field;
using form::Fields;
using form::ns;
using form::ws;

namespace {
constexpr COLORREF kPalette[] = {RGB(0xFF, 0xE0, 0x00), RGB(0x66, 0xDD, 0x66), RGB(0x55, 0xB8, 0xFF), RGB(0xFF, 0x8A, 0xC8),
                                 RGB(0xFF, 0x8C, 0x00), RGB(0xE5, 0x39, 0x35), RGB(0x20, 0x20, 0x20)};
const std::vector<std::wstring> kPaletteNames = {L"Yellow", L"Green", L"Blue", L"Pink", L"Orange", L"Red", L"Black"};

// "Pages (empty = all)": every page, or the list typed ("1-3, 5, 8-").
std::vector<int> pagesFromText(Document& d, const std::wstring& t) {
    if (t.find_first_not_of(L' ') == std::wstring::npos) {
        std::vector<int> all(static_cast<size_t>(d.pageCount()));
        for (int i = 0; i < d.pageCount(); ++i) all[static_cast<size_t>(i)] = i;
        return all;
    }
    return d.parseRange(t);
}

std::wstring badPages(Document& d, const std::wstring& t) {
    return pagesFromText(d, t).empty()
               ? L"The pages are not valid. Leave the box empty for all pages, or type page numbers and ranges such as 1-3, 5, 8-."
               : L"";
}

std::wstring sizeText(size_t b) {
    wchar_t t[32];
    if (b >= (1u << 20)) swprintf(t, 32, L"%.1f MB", b / 1048576.0);
    else swprintf(t, 32, L"%.0f KB", b / 1024.0);
    return t;
}

std::wstring baseName(Document& d) {
    std::wstring base = d.title();
    const size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    return base;
}
} // namespace

@interface FPWindowController (Tools)
@end

@implementation FPWindowController (Tools)

- (void)notice:(const std::wstring&)text { form::message(self.window, text); }
- (void)warn:(const std::wstring&)text { form::message(self.window, text, true); }

// Asks where to save a file of the given extension; `done` gets the path.
- (void)askSaveAs:(const std::wstring&)name extension:(NSString*)ext title:(NSString*)title done:(void (^)(std::wstring path))done {
    NSSavePanel* p = [NSSavePanel savePanel];
    p.nameFieldStringValue = ns(name);
    p.title = title;
    if (UTType* t = [UTType typeWithFilenameExtension:ext]) p.allowedContentTypes = @[ t ];
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r == NSModalResponseOK) done(platform::wide(p.URL.fileSystemRepresentation));
    }];
}

// ---- Edit PDF: watermark, page numbers, headers and footers, background ----

- (IBAction)addWatermark:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(7);
    f[0] = {Field::Text, L"Text:", {}, L"CONFIDENTIAL"};
    f[1] = {Field::Choice, L"Colour:", kPaletteNames, L"", 5};
    f[2] = {Field::Choice, L"Opacity:", {L"10%", L"20%", L"30%", L"40%", L"50%", L"70%", L"100%"}, L"", 2};
    f[3] = {Field::Choice, L"Size:", {L"Fit the page", L"Small", L"Medium", L"Large"}, L"", 0};
    f[4] = {Field::Choice, L"Direction:", {L"Diagonal", L"Horizontal"}, L"", 0};
    f[5] = {Field::Check, L"Put it behind the page content (hidden by pictures and scans)"};
    f[6] = {Field::Text, L"Pages (empty = all):", {}, L""};
    form::show(self.window, L"Watermark", f, L"Add", [d](const Fields& v) -> std::wstring {
        if (v[0].text.empty()) return L"Type the text of the watermark.";
        if (!pageobj::isLatin(v[0].text)) return L"The watermark can use letters, numbers and common symbols of Western languages. Please remove other characters.";
        return badPages(*d, v[6].text);
    }, [self, d](const Fields& v) {
        static const int pct[] = {10, 20, 30, 40, 50, 70, 100};
        pageobj::WatermarkStyle st;
        st.color = kPalette[std::clamp(v[1].choice, 0, 6)];
        st.opacityPercent = pct[std::clamp(v[2].choice, 0, 6)];
        st.sizeMode = v[3].choice;
        st.diagonal = v[4].choice == 0;
        st.behind = v[5].checked;
        if (pageobj::addWatermark(*d, pagesFromText(*d, v[6].text), v[0].text, st)) [self afterChange];
        else [self warn:L"The watermark could not be added."];
    });
}

- (IBAction)addPageNumbers:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(5);
    f[0] = {Field::Choice, L"Format:", {L"1", L"Page 1", L"Page 1 of N", L"1 / N"}, L"", 1};
    f[1] = {Field::Choice, L"Position:", {L"Bottom centre", L"Bottom left", L"Bottom right", L"Top centre", L"Top left", L"Top right"}, L"", 0};
    f[2] = {Field::Text, L"Start at:", {}, L"1"};
    f[3] = {Field::Choice, L"Size:", {L"8 pt", L"10 pt", L"12 pt", L"14 pt"}, L"", 1};
    f[4] = {Field::Text, L"Pages (empty = all):", {}, L""};
    form::show(self.window, L"Page numbers", f, L"Add", [d](const Fields& v) -> std::wstring {
        wchar_t* end = nullptr;
        const long n = wcstol(v[2].text.c_str(), &end, 10);
        if (end == v[2].text.c_str() || *end || n < 0 || n > 1000000) return L"Enter a whole number from 0 to 1000000 for the first page number.";
        return badPages(*d, v[4].text);
    }, [self, d](const Fields& v) {
        static const double sizes[] = {8, 10, 12, 14};
        pageobj::NumberStyle st;
        st.format = v[0].choice;
        st.position = v[1].choice;
        st.startAt = static_cast<int>(wcstol(v[2].text.c_str(), nullptr, 10));
        st.sizePt = sizes[std::clamp(v[3].choice, 0, 3)];
        if (pageobj::addPageNumbers(*d, pagesFromText(*d, v[4].text), st)) [self afterChange];
        else [self warn:L"The page numbers could not be added."];
    });
}

- (IBAction)addHeadersFooters:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(10);
    f[0] = {Field::Note, L"", {}, L"Leave a box empty for nothing there. You can use {page} {pages} {date} {file} {title}."};
    f[1] = {Field::Text, L"Header, left:", {}, L""};
    f[2] = {Field::Text, L"Header, centre:", {}, L"{title}"};
    f[3] = {Field::Text, L"Header, right:", {}, L""};
    f[4] = {Field::Text, L"Footer, left:", {}, L"{date}"};
    f[5] = {Field::Text, L"Footer, centre:", {}, L""};
    f[6] = {Field::Text, L"Footer, right:", {}, L"Page {page} of {pages}"};
    f[7] = {Field::Choice, L"Size:", {L"8 pt", L"9 pt", L"10 pt", L"12 pt"}, L"", 1};
    f[8] = {Field::Text, L"First page number:", {}, L"1"};
    f[9] = {Field::Text, L"Pages (empty = all):", {}, L""};
    form::show(self.window, L"Headers and footers", f, L"Add", [d](const Fields& v) -> std::wstring {
        wchar_t* end = nullptr;
        wcstol(v[8].text.c_str(), &end, 10);
        if (end == v[8].text.c_str() || *end) return L"Enter a whole number for the first page number.";
        return badPages(*d, v[9].text);
    }, [self, d](const Fields& v) {
        static const double sizes[] = {8, 9, 10, 12};
        pageobj::HeaderFooter hf;
        for (int i = 0; i < 6; ++i) hf.text[i] = v[static_cast<size_t>(i) + 1].text;
        hf.sizePt = sizes[std::clamp(v[7].choice, 0, 3)];
        hf.firstNumber = static_cast<int>(wcstol(v[8].text.c_str(), nullptr, 10));
        if (pageobj::addHeaderFooter(*d, pagesFromText(*d, v[9].text), hf)) [self afterChange];
        else [self notice:L"Nothing was added (all boxes are empty?)."];
    });
}

- (IBAction)addBackground:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(2);
    f[0] = {Field::Choice, L"Colour:", {L"Light yellow (easy reading)", L"Light green", L"Light blue", L"Light grey", L"Cream", L"White"}, L"", 0};
    f[1] = {Field::Text, L"Pages (empty = all):", {}, L""};
    form::show(self.window, L"Background colour", f, L"Add", [d](const Fields& v) { return badPages(*d, v[1].text); },
               [self, d](const Fields& v) {
        static const COLORREF cols[] = {RGB(0xFF, 0xF8, 0xD0), RGB(0xE4, 0xF4, 0xE0), RGB(0xE2, 0xEE, 0xFA), RGB(0xEE, 0xEE, 0xEE), RGB(0xFB, 0xF4, 0xE4), RGB(0xFF, 0xFF, 0xFF)};
        if (pageobj::addBackground(*d, pagesFromText(*d, v[1].text), cols[std::clamp(v[0].choice, 0, 5)])) [self afterChange];
    });
}

// Delete header and footer. The Windows app's window shows the page with what was found outlined; the Mac version asks
// the same questions in a form for now and reports what it found and removed.
- (IBAction)deleteHeaderFooter:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(4);
    f[0] = {Field::Note, L"", {}, L"Finds what repeats at the top and bottom of the pages (running titles, dates, page numbers, rules, logos) and removes it for good."};
    f[1] = {Field::Choice, L"Delete:", {L"Header", L"Footer", L"Header and footer", L"Page numbers only"}, L"", 2};
    f[2] = {Field::Choice, L"Pages:", {L"All pages", L"This page", L"Odd pages", L"Even pages", L"These pages:"}, L"", 0};
    f[3] = {Field::Text, L"Pages (for \"These pages\"):", {}, L""};
    const int current = [self.pageView currentPage];
    form::show(self.window, L"Delete header and footer", f, L"Delete", [d, current](const Fields& v) -> std::wstring {
        std::wstring why;
        if (headfoot::choosePages(*d, v[2].choice, current, v[3].text, &why).empty()) return why.empty() ? L"No pages are chosen." : why;
        return L"";
    }, [self, d, current](const Fields& v) {
        headfoot::Detection det = headfoot::detect(*d);
        headfoot::selectFor(det, v[1].choice);
        std::wstring why;
        const std::vector<int> pages = headfoot::choosePages(*d, v[2].choice, current, v[3].text, &why);
        std::wstring found;
        for (const auto& e : det.elements)
            if (e.selected) found += L"  - " + std::wstring(e.band == headfoot::Band::Header ? L"Header: " : L"Footer: ") + e.label + L"\n";
        const headfoot::Plan plan = headfoot::plan(*d, det, pages);
        if (plan.objects == 0 && plan.areas == 0) {
            [self notice:d->pageCount() < 2 ? L"With one page nothing can be seen to repeat, so no header or footer was found. Nothing was changed."
                                            : L"No repeating header or footer of that kind was found on those pages. Nothing was changed."];
            return;
        }
        if (!headfoot::apply(*d, plan, &why)) { [self warn:L"Nothing was deleted. " + why]; return; }
        [self afterChange];
        [self notice:L"Deleted on " + std::to_wstring(plan.pages.size()) + L" page(s):\n" + found + L"\nUndo (⌘Z) brings them back."];
    });
}

// ---- pages ----

- (std::wstring)currentPageText { return std::to_wstring([self.pageView currentPage] + 1); }

- (IBAction)rotateRight:(id)sender {
    Document* d = self.pdf;
    if (d && d->rotatePages({[self.pageView currentPage]}, 1)) [self afterChange];
}
- (IBAction)rotateLeft:(id)sender {
    Document* d = self.pdf;
    if (d && d->rotatePages({[self.pageView currentPage]}, 3)) [self afterChange];
}

// ⌘⇧R and ⌘⇧L: every page (⌘R and ⌘L the page shown), as on Windows
- (IBAction)rotateAllRight:(id)sender { [self rotateEveryPage:1]; }
- (IBAction)rotateAllLeft:(id)sender { [self rotateEveryPage:3]; }
- (void)rotateEveryPage:(int)turns {
    Document* d = self.pdf;
    if (!d) return;
    std::vector<int> all(static_cast<size_t>(d->pageCount()));
    for (int i = 0; i < d->pageCount(); ++i) all[static_cast<size_t>(i)] = i;
    if (d->rotatePages(all, turns)) [self afterChange];
}

- (IBAction)rotatePages:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(2);
    f[0] = {Field::Choice, L"Turn:", {L"Right (clockwise)", L"Left (anticlockwise)", L"Upside down"}, L"", 0};
    f[1] = {Field::Text, L"Pages (empty = all):", {}, L""};
    form::show(self.window, L"Rotate pages", f, L"Rotate", [d](const Fields& v) { return badPages(*d, v[1].text); }, [self, d](const Fields& v) {
        static const int turns[] = {1, 3, 2};
        if (d->rotatePages(pagesFromText(*d, v[1].text), turns[std::clamp(v[0].choice, 0, 2)])) [self afterChange];
    });
}

- (IBAction)deletePages:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(1);
    f[0] = {Field::Text, L"Pages to delete:", {}, [self currentPageText]};
    form::show(self.window, L"Delete pages", f, L"Delete", [d](const Fields& v) -> std::wstring {
        const auto p = d->parseRange(v[0].text);
        if (p.empty()) return L"Type page numbers and ranges such as 1-3, 5, 8-.";
        if (static_cast<int>(p.size()) >= d->pageCount()) return L"A document needs at least one page. Leave at least one page.";
        return L"";
    }, [self, d](const Fields& v) {
        if (d->deletePages(d->parseRange(v[0].text))) [self afterChange];
    });
}

- (IBAction)insertBlankPage:(id)sender {
    Document* d = self.pdf;
    const int cur = [self.pageView currentPage];
    if (d && d->insertBlankPage(cur, cur)) {
        [self afterChange];
        [self.pageView showPage:cur + 1];
    }
}

- (IBAction)extractPages:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(1);
    f[0] = {Field::Text, L"Pages to save as a new file:", {}, [self currentPageText]};
    form::show(self.window, L"Extract pages", f, L"Save as…", [d](const Fields& v) {
        return d->parseRange(v[0].text).empty() ? std::wstring(L"Type page numbers and ranges such as 1-3, 5, 8-.") : std::wstring();
    }, [self, d](const Fields& v) {
        const std::vector<int> pages = d->parseRange(v[0].text);
        [self askSaveAs:baseName(*d) + L" (pages).pdf" extension:@"pdf" title:@"Save the pages as" done:^(std::wstring path) {
            if (d->extractPages(pages, path) == SaveResult::Ok) [self notice:std::to_wstring(pages.size()) + L" page(s) saved."];
            else [self warn:L"The pages could not be saved. The folder may be read-only, or the disk may be full."];
        }];
    });
}

- (IBAction)insertPagesFromFile:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypePDF ];
    p.title = @"Insert the pages of";
    const int at = [self.pageView currentPage] + 1;
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        Document src;
        const std::wstring path = platform::wide(p.URL.fileSystemRepresentation);
        const OpenResult o = src.open(path);
        if (o != OpenResult::Ok) { [self warn:Document::describe(o, path)]; return; }
        std::vector<int> all(static_cast<size_t>(src.pageCount()));
        for (int i = 0; i < src.pageCount(); ++i) all[static_cast<size_t>(i)] = i;
        if (d->importPages(src, all, at)) {
            [self afterChange];
            [self.pageView showPage:at];
        } else [self warn:L"The pages could not be inserted."];
    }];
}

- (IBAction)reversePages:(id)sender {
    Document* d = self.pdf;
    if (d && d->reversePages()) [self afterChange];
}

- (IBAction)removeBlankPages:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    const std::vector<int> blank = pageobj::blankPages(*d);
    if (blank.empty()) { [self notice:L"There are no blank pages."]; return; }
    if (static_cast<int>(blank.size()) >= d->pageCount()) { [self notice:L"Every page is blank; a document needs at least one page."]; return; }
    form::ask(self.window, L"Remove " + std::to_wstring(blank.size()) + L" blank page(s) (" + fontlist::pageList(blank) + L")?", L"Remove", [self, d, blank](bool yes) {
        if (yes && d->deletePages(blank)) [self afterChange];
    });
}

- (IBAction)removeDuplicatePages:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    std::vector<int> extra;
    for (const auto& g : pageobj::duplicatePages(*d)) for (size_t i = 1; i < g.size(); ++i) extra.push_back(g[i]);
    std::sort(extra.begin(), extra.end());
    if (extra.empty()) { [self notice:L"There are no duplicate pages."]; return; }
    form::ask(self.window, L"Remove " + std::to_wstring(extra.size()) + L" page(s) that repeat an earlier page (" + fontlist::pageList(extra) + L")?", L"Remove", [self, d, extra](bool yes) {
        if (yes && d->deletePages(extra)) [self afterChange];
    });
}

// ---- file: export, PDF/A, compress, protection, hidden data, fonts, properties ----

- (IBAction)exportDocument:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(3);
    f[0] = {Field::Choice, L"Export as:", {L"PNG pictures (best for text and drawings)", L"JPEG pictures (smaller, for photos)", L"Plain text (.txt)",
                                         L"Markdown (.md)", L"Word document (.docx)", L"Excel workbook, tables only (.xlsx)", L"CSV, tables only (.csv)"}, L"", 4};
    f[1] = {Field::Choice, L"Picture resolution:", {L"72 dpi (screen)", L"150 dpi (good quality)", L"220 dpi (high quality)", L"300 dpi (print)"}, L"", 1};
    f[2] = {Field::Text, L"Pages (empty = all):", {}, L""};
    form::show(self.window, L"Export", f, L"Export…", [d](const Fields& v) { return badPages(*d, v[2].text); }, [self, d](const Fields& v) {
        static const exporter::Format fmts[] = {exporter::Format::Png, exporter::Format::Jpeg, exporter::Format::Text, exporter::Format::Markdown,
                                                exporter::Format::Word, exporter::Format::ExcelXlsx, exporter::Format::Csv};
        static const int dpis[] = {72, 150, 220, 300};
        const exporter::Format fmt = fmts[std::clamp(v[0].choice, 0, 6)];
        const int dpi = dpis[std::clamp(v[1].choice, 0, 3)];
        const std::vector<int> pages = pagesFromText(*d, v[2].text);
        const bool pics = fmt == exporter::Format::Png || fmt == exporter::Format::Jpeg;
        NSString* ext = ns(exporter::extension(fmt));
        [self askSaveAs:baseName(*d) + L"." + exporter::extension(fmt) extension:ext
                  title:pics ? @"Export as (each page gets its number added)" : @"Export as" done:^(std::wstring path) {
            const exporter::Result r = exporter::run(*d, pages, fmt, path, dpi);
            if (!r.ok) { [self warn:r.error.empty() ? L"The export did not succeed." : r.error]; return; }
            if (pics) [self notice:std::to_wstring(r.items) + L" picture(s) saved."];
            else [self notice:L"Exported."];
        }];
    });
}

- (IBAction)savePdfA:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(2);
    f[0] = {Field::Note, L"", {}, L"PDF/A-2b, for archiving: fonts are embedded; scripts, attachments and passwords are left out. Comments and fields become part of the pages."};
    f[1] = {Field::Choice, L"Pages:", {L"Keep text and drawings (recommended)", L"Pictures with searchable text (larger)"}, L"", 0};
    form::show(self.window, L"Save a PDF/A copy", f, L"Choose file…", nullptr, [self, d](const Fields& v) {
        const bool pictures = v[1].choice == 1;
        [self askSaveAs:baseName(*d) + L" (PDF-A).pdf" extension:@"pdf" title:@"Save the PDF/A copy as" done:^(std::wstring path) {
            std::vector<unsigned char> out;
            pdfa::Result r;
            std::wstring err;
            if (!pdfa::convert(*d, pictures, out, r, err) || Document::writeFile(path, out) != SaveResult::Ok) {
                [self warn:L"The PDF/A copy could not be made. " + err];
                return;
            }
            std::wstring msg = L"The PDF/A-2b copy is saved (" + std::to_wstring(r.pages) + L" pages).";
            for (const auto& n : r.notes) msg += L"\n" + n;
            [self notice:msg];
        }];
    });
}

- (IBAction)compressDocument:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(4);
    f[0] = {Field::Note, L"", {}, L"Makes the file smaller by storing large pictures at a lower resolution as JPEG. Text and drawings stay sharp."};
    f[1] = {Field::Choice, L"Quality:", {L"Balanced (150 dpi) - recommended", L"High quality (220 dpi)", L"Smallest file (96 dpi)", L"Make it smaller than..."}, L"", 0};
    f[2] = {Field::Text, L"Target size (MB):", {}, L"2"};
    f[3] = {Field::Check, L"Black and white / greyscale (smallest for scans)"};
    form::show(self.window, L"Compress", f, L"Compress", [](const Fields& v) -> std::wstring {
        if (v[1].choice != 3) return L"";
        wchar_t* end = nullptr;
        const double mb = wcstod(v[2].text.c_str(), &end);
        return (end == v[2].text.c_str() || *end || mb < 0.05 || mb > 10000) ? L"Type the size in megabytes, for example 2 or 0.5." : L"";
    }, [self, d](const Fields& v) {
        compress::Options o;
        static const int dpis[] = {150, 220, 96};
        static const int qual[] = {80, 88, 70};
        if (v[1].choice == 3) o.targetMB = wcstod(v[2].text.c_str(), nullptr);
        else { o.dpi = dpis[v[1].choice]; o.jpegQuality = qual[v[1].choice]; }
        o.greyscale = v[3].checked;
        const compress::Result r = compress::run(*d, o);
        if (!r.changed) {
            [self notice:L"No pictures could be made smaller: they are already at or below this resolution, have transparent parts, or the file has no pictures. The document is unchanged."];
            return;
        }
        [self afterChange];
        std::wstring msg = L"The file is now about " + sizeText(r.after) + L" (it was " + sizeText(r.before) + L"). Pictures made smaller: " +
                           std::to_wstring(r.pictures) + L".\n\nUndo (⌘Z) takes the change back; Save keeps it.";
        if (o.targetMB > 0 && !r.reachedTarget)
            msg = L"The file could not get under " + sizeText(static_cast<size_t>(o.targetMB * 1048576.0)) + L"; the smallest result is " +
                  sizeText(r.after) + L" (it was " + sizeText(r.before) + L").\n\nUndo (⌘Z) takes the change back; Save keeps it.";
        [self notice:msg];
    });
}

- (IBAction)protectDocument:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields g(9);
    g[0] = {Field::Note, L"", {}, L"Protect the document with a password (AES-256). Keep the passwords safe: they cannot be recovered.\nThe protection is applied when you save the document."};
    g[1] = {Field::Password, L"Password to open it:", {}, L""};
    g[2] = {Field::Password, L"Type it again:", {}, L""};
    g[3] = {Field::Check, L"Limit what others can do with it"};
    g[4] = {Field::Password, L"Owner password:", {}, L""};
    g[5] = {Field::Password, L"Type it again:", {}, L""};
    g[6] = {Field::Check, L"Allow printing"}; g[6].checked = true;
    g[7] = {Field::Check, L"Allow copying text and pictures"}; g[7].checked = true;
    g[8] = {Field::Check, L"Allow changes (editing, comments, filling in forms)"};
    form::show(self.window, L"Password protection", g, L"Protect", [](const Fields& v) -> std::wstring {
        if (v[1].text != v[2].text) return L"The two passwords to open the document are not the same. Type them again.";
        if (v[3].checked) {
            if (v[4].text.empty()) return L"Type an owner password: it is needed to lift the limits later.";
            if (v[4].text != v[5].text) return L"The two owner passwords are not the same. Type them again.";
            if (v[4].text == v[1].text) return L"The owner password must be different from the password to open the document.";
        } else if (v[1].text.empty()) return L"Type a password to open the document, choose limits, or both.";
        return L"";
    }, [self, d](const Fields& v) {
        protect::Options o;
        o.userPassword = v[1].text;
        o.ownerPassword = v[3].checked ? v[4].text : L"";
        o.allowPrint = !v[3].checked || v[6].checked;
        o.allowCopy = !v[3].checked || v[7].checked;
        o.allowModify = !v[3].checked || v[8].checked;
        if (d->setProtection(o)) {
            [(FPDocument*)self.document changed];
            [self notice:L"New protection is set. It is applied when you save the document (⌘S)."];
        }
    });
}

- (IBAction)removeProtection:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    form::ask(self.window, L"Remove the password protection? The saved file will open without a password and without limits.", L"Remove", [self, d](bool yes) {
        if (yes && d->removeProtection()) {
            [(FPDocument*)self.document changed];
            [self notice:L"The protection will be removed when you save the document (⌘S)."];
        }
    });
}

- (IBAction)removeHiddenData:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(7);
    f[0] = {Field::Note, L"", {}, L"Removes data people do not see but that travels with the file. The pages look the same."};
    f[1] = {Field::Check, L"Document information and metadata (author, title, software, dates)", {}, L"", 0, true};
    f[2] = {Field::Check, L"Attached files", {}, L"", 0, true};
    f[3] = {Field::Check, L"Scripts and automatic actions", {}, L"", 0, true};
    f[4] = {Field::Check, L"Hidden annotations, page thumbnails, application data", {}, L"", 0, true};
    f[5] = {Field::Check, L"All comments, highlights and drawings", {}, L"", 0, false};
    f[6] = {Field::Check, L"Bookmarks", {}, L"", 0, false};
    form::show(self.window, L"Remove hidden data", f, L"Remove", nullptr, [self, d](const Fields& v) {
        sanitize::Options o;
        o.metadata = v[1].checked; o.attachments = v[2].checked; o.scripts = v[3].checked; o.hidden = v[4].checked;
        o.comments = v[5].checked; o.bookmarks = v[6].checked;
        std::vector<unsigned char> bytes, out;
        sanitize::Report rep;
        std::wstring err;
        if (!d->serialize(bytes) || !sanitize::run(bytes, o, out, rep, err)) { [self warn:L"The document could not be cleaned. " + err]; return; }
        d->beginChange(L"Remove hidden data");
        if (d->reload(std::move(out)) != OpenResult::Ok) { d->undo(); [self warn:L"The cleaned document could not be opened, so nothing was changed."]; return; }
        [self afterChange];
        std::wstring msg = rep.removed.empty() ? L"Nothing of the chosen kinds was found. The document was already clean." : L"Removed:\n";
        for (const auto& r : rep.removed) msg += L"  - " + r + L"\n";
        if (!rep.removed.empty()) {
            wchar_t sz[160];
            swprintf(sz, 160, L"\nSize: %.0f KB before, %.0f KB after.\n\nSave the document to keep the change; Undo (⌘Z) takes it back.", rep.before / 1024.0, rep.after / 1024.0);
            msg += sz;
        }
        [self notice:msg];
    });
}

- (IBAction)showFonts:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(1);
    f[0] = {Field::Note, L"", {}, fontlist::asText(fontlist::build(*d))};
    form::show(self.window, L"Fonts used in " + d->title(), f, L"!Close", nullptr, nullptr);
}

- (IBAction)showProperties:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    const auto& api = pdfium::api();
    auto meta = [&](const char* key) {
        const unsigned long n = api.FPDF_GetMetaText(d->handle(), key, nullptr, 0);
        if (n <= 2) return std::wstring();
        std::vector<unsigned short> buf(n / 2 + 1);
        api.FPDF_GetMetaText(d->handle(), key, buf.data(), n);
        return std::wstring(buf.begin(), buf.begin() + static_cast<long>(n / 2 - 1));
    };
    int version = 0;
    api.FPDF_GetFileVersion(d->handle(), &version);
    const PageSize& s = d->pageSize(0);
    wchar_t size[64];
    swprintf(size, 64, L"%.0f × %.0f mm", s.w / 72.0 * 25.4, s.h / 72.0 * 25.4);
    std::wstring t = L"File: " + d->path() + L"\nTitle: " + meta("Title") + L"\nAuthor: " + meta("Author") + L"\nSubject: " + meta("Subject") +
                     L"\nCreated with: " + meta("Creator") + L"\nPDF producer: " + meta("Producer") + L"\nPages: " + std::to_wstring(d->pageCount()) +
                     L"\nPage size (first page): " + size + L"\nPDF version: " + std::to_wstring(version / 10) + L"." + std::to_wstring(version % 10) +
                     L"\nPassword protected: " + (d->isProtected() ? L"yes" : L"no");
    Fields f(1);
    f[0] = {Field::Note, L"", {}, t};
    form::show(self.window, L"Document properties", f, L"!Close", nullptr, nullptr);
}

@end
