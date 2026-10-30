#import "Form.h"
#include <cmath>
#include <algorithm>
#include <memory>
#include "platform.hpp"

namespace form {

NSString* ns(const std::wstring& s) { return [NSString stringWithUTF8String:platform::utf8(s).c_str()] ?: @""; }
std::wstring ws(NSString* s) { return s ? platform::wide(s.UTF8String) : std::wstring(); }

} // namespace form

// The sheet: a grid of fields, a line for a problem, and Cancel / OK.
@interface FPFormSheet : NSWindowController
@property(nonatomic, copy) void (^finish)(BOOL ok);
@property(nonatomic, copy) NSString* (^check)(void);   // returns a problem, or nil
@property(nonatomic, strong) NSTextField* problem;
@end

@implementation FPFormSheet
- (void)ok:(id)sender {
    NSString* p = self.check ? self.check() : nil;
    if (p.length) {
        self.problem.stringValue = p;
        self.problem.hidden = NO;
        NSBeep();
        return;
    }
    [self.window.sheetParent endSheet:self.window returnCode:NSModalResponseOK];
}
- (void)cancel:(id)sender { [self.window.sheetParent endSheet:self.window returnCode:NSModalResponseCancel]; }
@end

namespace form {

void show(NSWindow* window, const std::wstring& title, Fields given, const std::wstring& okText,
          std::function<std::wstring(const Fields&)> validate, std::function<void(const Fields&)> done) {
    // Information only (notes and one "!Close" button: Properties, Fonts used, signatures): the system's alert sheet,
    // which sizes itself to its text; a long report goes in a scrolling text area under the title.
    if (!okText.empty() && okText[0] == L'!' &&
        std::all_of(given.begin(), given.end(), [](const Field& f) { return f.kind == Field::Note; })) {
        std::wstring text;
        for (const Field& f : given) text += (text.empty() ? L"" : L"\n\n") + f.text;
        NSAlert* a = [[NSAlert alloc] init];
        a.messageText = ns(title);
        [a addButtonWithTitle:ns(okText.substr(1))];
        const size_t lines = static_cast<size_t>(std::count(text.begin(), text.end(), L'\n')) + 1;
        if (text.size() > 600 || lines > 14 || text.find(L'\t') != std::wstring::npos) {   // long, or a table (tab-separated)
            NSScrollView* sv = [NSTextView scrollableTextView];
            sv.frame = NSMakeRect(0, 0, 560, std::min<CGFloat>(320, 24 + 15 * static_cast<CGFloat>(lines)));
            sv.borderType = NSBezelBorder;
            NSTextView* tv = (NSTextView*)sv.documentView;
            tv.editable = NO;
            tv.font = [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular];
            tv.textColor = NSColor.textColor;
            tv.string = ns(text);
            a.accessoryView = sv;
        } else a.informativeText = ns(text);
        auto keep = std::make_shared<Fields>(std::move(given));
        [a beginSheetModalForWindow:window completionHandler:^(NSModalResponse) { if (done) done(*keep); }];
        return;
    }
    auto held = std::make_shared<Fields>(std::move(given));   // lives until the sheet closes
    Fields& fields = *held;
    NSPanel* panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 480, 200) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskDocModalWindow
                                                  backing:NSBackingStoreBuffered defer:NO];
    FPFormSheet* sheet = [[FPFormSheet alloc] initWithWindow:panel];
    NSView* root = panel.contentView;

    NSTextField* heading = [NSTextField labelWithString:ns(title)];
    heading.font = [NSFont systemFontOfSize:15 weight:NSFontWeightSemibold];
    NSGridView* grid = [NSGridView gridViewWithViews:@[]];
    grid.rowSpacing = 10;
    grid.columnSpacing = 10;
    NSMutableArray<NSView*>* controls = [NSMutableArray array];
    NSStackView* notes = [[NSStackView alloc] init];
    notes.orientation = NSUserInterfaceLayoutOrientationVertical;
    notes.alignment = NSLayoutAttributeLeading;
    notes.spacing = 8;
    for (const Field& f : fields) {
        NSView* control = nil;
        NSView* label = [NSGridCell emptyContentView];
        switch (f.kind) {
            case Field::Text:
            case Field::Password: {
                NSTextField* t = f.kind == Field::Password ? [NSSecureTextField textFieldWithString:ns(f.text)] : [NSTextField textFieldWithString:ns(f.text)];
                [t.widthAnchor constraintGreaterThanOrEqualToConstant:280].active = YES;
                control = t;
                label = [NSTextField labelWithString:ns(f.label)];
                break;
            }
            case Field::Choice: {
                NSPopUpButton* p = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
                for (const auto& c : f.choices) [p addItemWithTitle:ns(c)];
                if (f.choice >= 0 && f.choice < static_cast<int>(f.choices.size())) [p selectItemAtIndex:f.choice];
                control = p;
                label = [NSTextField labelWithString:ns(f.label)];
                break;
            }
            case Field::Check: {
                NSButton* b = [NSButton checkboxWithTitle:ns(f.label) target:nil action:nil];
                b.state = f.checked ? NSControlStateValueOn : NSControlStateValueOff;
                control = b;
                break;
            }
            case Field::Note: {
                // A note has its own width: a form of notes only (Properties, Fonts used) has nothing else to give it one.
                const size_t lines = static_cast<size_t>(std::count(f.text.begin(), f.text.end(), L'\n')) + 1;
                if (f.text.size() > 500 || lines > 12) {   // a report: scrollable, and the text can be selected and copied
                    // the system's own scrolling text view: it sizes its text view to the scroll view and follows dark mode
                    NSScrollView* sv = [NSTextView scrollableTextView];
                    sv.borderType = NSBezelBorder;
                    NSTextView* tv = (NSTextView*)sv.documentView;
                    tv.editable = NO;
                    tv.selectable = YES;
                    tv.font = [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular];
                    tv.textColor = NSColor.textColor;
                    tv.string = ns(f.text);
                    tv.textContainerInset = NSMakeSize(6, 6);
                    sv.translatesAutoresizingMaskIntoConstraints = NO;
                    [sv.widthAnchor constraintEqualToConstant:560].active = YES;
                    [sv.heightAnchor constraintEqualToConstant:320].active = YES;
                    control = sv;
                } else {
                    NSTextField* n = [NSTextField wrappingLabelWithString:ns(f.text)];
                    n.textColor = NSColor.secondaryLabelColor;
                    n.selectable = YES;
                    n.preferredMaxLayoutWidth = 440;
                    [n.widthAnchor constraintEqualToConstant:440].active = YES;
                    control = n;
                }
                break;
            }
        }
        [controls addObject:control];
        if (f.kind == Field::Note) {   // notes above the grid, one under the other: in the grid they got the height of one line
            [notes addArrangedSubview:control];
            continue;
        }
        NSGridRow* row = [grid addRowWithViews:@[ label, control ]];
        row.rowAlignment = NSGridRowAlignmentLastBaseline;
    }
    if (grid.numberOfColumns > 0) [grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;

    NSTextField* problem = [NSTextField wrappingLabelWithString:@""];
    problem.textColor = NSColor.systemRedColor;
    problem.hidden = YES;
    sheet.problem = problem;
    NSButton* cancel = [NSButton buttonWithTitle:@"Cancel" target:sheet action:@selector(cancel:)];
    cancel.keyEquivalent = @"\033";
    std::wstring okLabel = okText;
    const bool noCancel = !okLabel.empty() && okLabel[0] == L'!';   // "!Close": an information form with one button
    if (noCancel) okLabel.erase(0, 1);
    NSButton* ok = [NSButton buttonWithTitle:ns(okLabel.empty() ? L"OK" : okLabel) target:sheet action:@selector(ok:)];
    ok.keyEquivalent = @"\r";
    cancel.hidden = noCancel;

    for (NSView* v in @[ heading, notes, grid, problem, cancel, ok ]) {
        v.translatesAutoresizingMaskIntoConstraints = NO;
        [root addSubview:v];
    }
    [NSLayoutConstraint activateConstraints:@[
        [heading.topAnchor constraintEqualToAnchor:root.topAnchor constant:20],
        [heading.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:20],
        [notes.topAnchor constraintEqualToAnchor:heading.bottomAnchor constant:12],
        [notes.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:20],
        [notes.trailingAnchor constraintLessThanOrEqualToAnchor:root.trailingAnchor constant:-20],
        [grid.topAnchor constraintEqualToAnchor:notes.bottomAnchor constant:notes.arrangedSubviews.count ? 14 : 4],
        [grid.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:20],
        [grid.trailingAnchor constraintLessThanOrEqualToAnchor:root.trailingAnchor constant:-20],
        [problem.topAnchor constraintEqualToAnchor:grid.bottomAnchor constant:12],
        [problem.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:20],
        [problem.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-20],
        [ok.topAnchor constraintEqualToAnchor:problem.bottomAnchor constant:14],
        [ok.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-20],
        [ok.bottomAnchor constraintEqualToAnchor:root.bottomAnchor constant:-18],
        [ok.widthAnchor constraintGreaterThanOrEqualToConstant:90],
        [cancel.centerYAnchor constraintEqualToAnchor:ok.centerYAnchor],
        [cancel.trailingAnchor constraintEqualToAnchor:ok.leadingAnchor constant:-10],
        [cancel.widthAnchor constraintGreaterThanOrEqualToConstant:90],
        [root.widthAnchor constraintGreaterThanOrEqualToConstant:440],
    ]];
    // The sheet takes the size its contents need (it was made 480 x 200: a long note would run over the heading).
    [root layoutSubtreeIfNeeded];
    [panel setContentSize:root.fittingSize];

    // Reads the controls back into the fields.
    auto read = [held, controls]() {
        Fields* target = held.get();
        for (size_t i = 0; i < target->size(); ++i) {
            Field& f = (*target)[i];
            NSView* c = controls[i];
            if (f.kind == Field::Text || f.kind == Field::Password) f.text = ws(((NSTextField*)c).stringValue);
            else if (f.kind == Field::Choice) f.choice = static_cast<int>(((NSPopUpButton*)c).indexOfSelectedItem);
            else if (f.kind == Field::Check) f.checked = ((NSButton*)c).state == NSControlStateValueOn;
        }
    };
    sheet.check = ^NSString* {
        read();
        if (!validate) return nil;
        const std::wstring p = validate(*held);
        return p.empty() ? nil : ns(p);
    };
    for (NSView* c in controls)
        if ([c isKindOfClass:[NSTextField class]] && ((NSTextField*)c).isEditable) { panel.initialFirstResponder = c; break; }
    [window beginSheet:panel completionHandler:^(NSModalResponse r) {
        (void)sheet;   // kept alive until the sheet closes
        if (r != NSModalResponseOK) return;
        read();
        if (done) done(*held);
    }];
}

void message(NSWindow* window, const std::wstring& text, bool warning, std::function<void()> done) {
    NSAlert* a = [[NSAlert alloc] init];
    a.messageText = ns(text);
    a.alertStyle = warning ? NSAlertStyleWarning : NSAlertStyleInformational;
    if (window) [a beginSheetModalForWindow:window completionHandler:^(NSModalResponse) { if (done) done(); }];
    else { [a runModal]; if (done) done(); }
}

void ask(NSWindow* window, const std::wstring& text, const std::wstring& yes, std::function<void(bool)> done) {
    NSAlert* a = [[NSAlert alloc] init];
    a.messageText = ns(text);
    [a addButtonWithTitle:ns(yes)];
    [a addButtonWithTitle:@"Cancel"];
    [a beginSheetModalForWindow:window completionHandler:^(NSModalResponse r) { if (done) done(r == NSAlertFirstButtonReturn); }];
}

} // namespace form
