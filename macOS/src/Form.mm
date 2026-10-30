#import "Form.h"
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
                NSTextField* n = [NSTextField wrappingLabelWithString:ns(f.text)];
                n.textColor = NSColor.secondaryLabelColor;
                [n.widthAnchor constraintLessThanOrEqualToConstant:420].active = YES;
                control = n;
                break;
            }
        }
        [controls addObject:control];
        NSGridRow* row = [grid addRowWithViews:@[ label, control ]];
        row.yPlacement = NSGridRowAlignmentLastBaseline;
        if (f.kind == Field::Note) [grid mergeCellsInHorizontalRange:NSMakeRange(0, 2) verticalRange:NSMakeRange(grid.numberOfRows - 1, 1)];
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

    for (NSView* v in @[ heading, grid, problem, cancel, ok ]) {
        v.translatesAutoresizingMaskIntoConstraints = NO;
        [root addSubview:v];
    }
    [NSLayoutConstraint activateConstraints:@[
        [heading.topAnchor constraintEqualToAnchor:root.topAnchor constant:20],
        [heading.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:20],
        [grid.topAnchor constraintEqualToAnchor:heading.bottomAnchor constant:16],
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
