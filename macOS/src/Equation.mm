// The equation editor, as in the Windows app: the equation written in one line (s^8, x_1, \frac{a}{b}, \sqrt{x} ...),
// buttons for powers, indices, fractions, roots, brackets, Greek letters and signs, and a live picture of the equation.
// Clicking an equation in edit mode opens it; Insert Equation writes a new one, placed where the page is clicked next.
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "PageView.h"
#include <cmath>
#include <functional>
#include <string>
#include <vector>
#include "equation.hpp"
#include "platform.hpp"

using form::ns;
using form::ws;

namespace {
std::wstring g_pending;        // Insert Equation: set where the page is clicked next
double g_pendingSize = 12;
const unichar kSlot = 1;       // in a template: where the selected text (or the caret) goes

struct Template { NSString* label; NSString* insert; NSString* tip; };
NSArray<NSArray*>* structures() {
    return @[
        @[ @"x²", @"^{\x01}", @"Power: s^8, e^{-st}" ], @[ @"x₂", @"_{\x01}", @"Index: x_1, a_{n+1}" ], @[ @"x₂²", @"_{\x01}^{}", @"Index and power" ],
        @[ @"a⁄b", @"\\frac{\x01}{}", @"Fraction: \\frac{top}{bottom}" ], @[ @"√", @"\\sqrt{\x01}", @"Square root" ],
        @[ @"ⁿ√", @"\\sqrt[\x01]{}", @"Root: \\sqrt[3]{x}" ], @[ @"( )", @"\\left(\x01\\right)", @"Brackets that grow with what they hold" ],
        @[ @"[ ]", @"\\left[\x01\\right]", @"Square brackets" ], @[ @"|x|", @"\\left|\x01\\right|", @"Absolute value" ],
        @[ @"∑", @"\\sum_{\x01}^{}", @"Sum with limits" ], @[ @"∏", @"\\prod_{\x01}^{}", @"Product with limits" ],
        @[ @"∫", @"\\int_{\x01}^{}", @"Integral with limits" ], @[ @"lim", @"\\lim_{\x01}", @"Limit: \\lim_{x \\to 0}" ],
        @[ @"sin", @"\\sin \x01", @"Functions (sin, cos, log, ln ...) are set upright" ], @[ @"abc", @"\\text{\x01}", @"Ordinary upright text" ],
    ];
}
NSString* const kSigns = @"+−±×·÷=≠≈≡≤≥<>∞→⇒∂∇′°∈∀∃…";
NSString* const kGreek = @"αβγδεζηθλμνξπρστφχψωΓΔΘΛΠΣΦΨΩ";
} // namespace

bool FPEquationPending() { return !g_pending.empty(); }
void FPEquationClearPending() { g_pending.clear(); }
// Puts the equation written with Insert Equation on the page, starting at (x, y) on its baseline.
bool FPEquationPlace(Document& d, int page, double x, double y, std::wstring& why) {
    const std::wstring src = g_pending;
    g_pending.clear();
    return equation::insert(d, page, x, y, src, g_pendingSize, RGB(0, 0, 0), &why);
}

@interface FPEquationSheet : NSObject <NSTextFieldDelegate, NSComboBoxDelegate>
@property(nonatomic, strong) NSWindow* panel;
@property(nonatomic, strong) NSTextField* field;
@property(nonatomic, strong) NSComboBox* size;
@property(nonatomic, strong) NSImageView* preview;
@property(nonatomic, strong) NSTextField* error;
@property(nonatomic, strong) NSButton* apply;
@property(nonatomic, strong) NSMutableArray<NSString*>* inserts;
@property(nonatomic) BOOL removeChosen;
@end

@implementation FPEquationSheet
- (double)sizeValue {
    const double v = self.size.stringValue.doubleValue;
    return v >= 4 && v <= 144 ? v : 12;
}
- (void)render {
    const std::wstring src = ws(self.field.stringValue);
    std::wstring err;
    std::vector<unsigned char> px;
    int w = 0, h = 0;
    NSImage* img = nil;
    const double backing = self.panel.backingScaleFactor > 0 ? self.panel.backingScaleFactor : 2;
    if (src.find_first_not_of(L" \t") != std::wstring::npos && equation::preview(src, [self sizeValue], 2.0 * backing, px, w, h, &err)) {
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr pixelsWide:w pixelsHigh:h bitsPerSample:8 samplesPerPixel:4
                                                                        hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:w * 4 bitsPerPixel:32];
        unsigned char* out = rep.bitmapData;
        for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {   // BGRA to RGBA
            out[i * 4] = px[i * 4 + 2]; out[i * 4 + 1] = px[i * 4 + 1]; out[i * 4 + 2] = px[i * 4]; out[i * 4 + 3] = 255;
        }
        img = [[NSImage alloc] initWithSize:NSMakeSize(w / backing, h / backing)];
        [img addRepresentation:rep];
    }
    self.preview.image = img;
    self.error.stringValue = ns(err);
    self.apply.enabled = err.empty();
}
- (void)controlTextDidChange:(NSNotification*)n { [self render]; }
- (void)comboBoxSelectionDidChange:(NSNotification*)n {
    dispatch_async(dispatch_get_main_queue(), ^{ [self render]; });   // the box shows the choice only afterwards
}
// A button's text at the caret: a template wraps the selected text (the caret goes inside it otherwise).
- (void)insertClicked:(NSButton*)b {
    NSString* what = self.inserts[static_cast<NSUInteger>(b.tag)];
    NSText* ed = self.field.currentEditor;
    if (!ed) {
        [self.panel makeFirstResponder:self.field];
        ed = self.field.currentEditor;
        [ed setSelectedRange:NSMakeRange(ed.string.length, 0)];
    }
    const NSRange sel = ed.selectedRange;
    NSString* selected = [ed.string substringWithRange:sel];
    const NSRange slot = [what rangeOfString:[NSString stringWithCharacters:&kSlot length:1]];
    NSString* put = what;
    NSUInteger caret = what.length;
    if (slot.location != NSNotFound) {
        put = [what stringByReplacingCharactersInRange:slot withString:selected];
        if (selected.length > 0) {
            const NSRange next = [put rangeOfString:@"{}" options:0 range:NSMakeRange(slot.location + selected.length, put.length - slot.location - selected.length)];
            caret = next.location != NSNotFound ? next.location + 1 : put.length;
        } else caret = slot.location;
    }
    [ed replaceCharactersInRange:sel withString:put];
    [ed setSelectedRange:NSMakeRange(sel.location + caret, 0)];
    [self render];
}
- (void)applyClicked:(id)sender {
    [self render];
    if (!self.apply.enabled) { NSBeep(); return; }
    [self.panel.sheetParent endSheet:self.panel returnCode:NSModalResponseOK];
}
- (void)cancelClicked:(id)sender { [self.panel.sheetParent endSheet:self.panel returnCode:NSModalResponseCancel]; }
- (void)removeClicked:(id)sender {
    self.removeChosen = YES;
    [self.panel.sheetParent endSheet:self.panel returnCode:NSModalResponseOK];
}
@end

static FPEquationSheet* g_sheet = nil;

@interface FPWindowController (Equation)
- (void)editEquation:(const equation::Found&)found;
- (IBAction)insertEquation:(id)sender;
@end

@implementation FPWindowController (Equation)

// The sheet; `done` gets the notation and size, or `remove` true for Remove Equation.
- (void)equationSheet:(NSString*)title source:(const std::wstring&)source size:(double)size existing:(BOOL)existing
                 done:(std::function<void(const std::wstring&, double, bool)>)done {
    FPEquationSheet* s = [[FPEquationSheet alloc] init];
    s.inserts = [NSMutableArray array];
    NSWindow* p = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 700, 540) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered defer:NO];
    p.title = title;
    s.panel = p;
    NSTextField* heading = [NSTextField labelWithString:@"Type the equation, or build it with the buttons:"];
    s.field = [NSTextField textFieldWithString:ns(source)];
    s.field.font = [NSFont systemFontOfSize:18];
    s.field.delegate = s;
    s.field.placeholderString = @"s^8 + 9s^6 = 0     G(s) = \\frac{2K}{s(s+2)}";
    s.size = [[NSComboBox alloc] initWithFrame:NSMakeRect(0, 0, 70, 26)];
    [s.size addItemsWithObjectValues:@[ @"8", @"9", @"10", @"11", @"12", @"14", @"16", @"18", @"20", @"24", @"28", @"36" ]];
    s.size.stringValue = [NSString stringWithFormat:@"%g", std::round(size * 10) / 10];
    s.size.delegate = s;
    NSStackView* top = [NSStackView stackViewWithViews:@[ s.field, [NSTextField labelWithString:@"Size"], s.size ]];
    top.spacing = 8;
    [s.size.widthAnchor constraintEqualToConstant:70].active = YES;
    NSTextField* hint = [NSTextField wrappingLabelWithString:
        @"s^8 is a power, x_1 an index (use { } for more: e^{-st}, x_{n+1}). \\frac{top}{bottom} is a fraction, \\sqrt{x} a root. "
        @"Spaces only separate; signs get their own space. Select text and press a button to wrap it."];
    hint.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
    hint.textColor = NSColor.secondaryLabelColor;
    // the buttons, in rows
    NSMutableArray<NSView*>* rows = [NSMutableArray array];
    NSMutableArray<NSView*>* row = [NSMutableArray array];
    auto flush = [&] { if (row.count) { NSStackView* r = [NSStackView stackViewWithViews:row]; r.spacing = 4; [rows addObject:r]; row = [NSMutableArray array]; } };
    auto add = [&](NSString* label, NSString* insert, NSString* tip) {
        NSButton* b = [NSButton buttonWithTitle:label target:s action:@selector(insertClicked:)];
        b.font = [NSFont systemFontOfSize:15];
        b.bezelStyle = NSBezelStyleRegularSquare;
        b.tag = static_cast<NSInteger>(s.inserts.count);
        if (tip) b.toolTip = tip;
        [b.widthAnchor constraintGreaterThanOrEqualToConstant:34].active = YES;
        [s.inserts addObject:insert];
        [row addObject:b];
        if (row.count >= 16) flush();
    };
    for (NSArray* t in structures()) add(t[0], [t[1] stringByReplacingOccurrencesOfString:@"\x01" withString:[NSString stringWithCharacters:&kSlot length:1]], t[2]);
    flush();
    for (NSUInteger i = 0; i < kSigns.length; ++i) add([kSigns substringWithRange:NSMakeRange(i, 1)], [kSigns substringWithRange:NSMakeRange(i, 1)], nil);
    flush();
    for (NSUInteger i = 0; i < kGreek.length; ++i) add([kGreek substringWithRange:NSMakeRange(i, 1)], [kGreek substringWithRange:NSMakeRange(i, 1)], nil);
    flush();
    NSStackView* buttons = [NSStackView stackViewWithViews:rows];
    buttons.orientation = NSUserInterfaceLayoutOrientationVertical;
    buttons.alignment = NSLayoutAttributeLeading;
    buttons.spacing = 4;
    s.preview = [[NSImageView alloc] initWithFrame:NSMakeRect(0, 0, 600, 140)];
    s.preview.imageScaling = NSImageScaleProportionallyDown;
    s.preview.wantsLayer = YES;
    s.preview.layer.backgroundColor = NSColor.whiteColor.CGColor;
    s.preview.layer.borderColor = NSColor.separatorColor.CGColor;
    s.preview.layer.borderWidth = 1;
    [s.preview.heightAnchor constraintGreaterThanOrEqualToConstant:120].active = YES;
    s.error = [NSTextField wrappingLabelWithString:@""];
    s.error.textColor = NSColor.systemRedColor;
    NSButton* remove = [NSButton buttonWithTitle:@"Remove Equation" target:s action:@selector(removeClicked:)];
    remove.hidden = !existing;
    NSButton* cancel = [NSButton buttonWithTitle:@"Cancel" target:s action:@selector(cancelClicked:)];
    cancel.keyEquivalent = @"\x1b";
    s.apply = [NSButton buttonWithTitle:existing ? @"Apply" : @"Insert" target:s action:@selector(applyClicked:)];
    s.apply.keyEquivalent = @"\r";
    NSView* spacer = [[NSView alloc] init];
    [spacer setContentHuggingPriority:1 forOrientation:NSLayoutConstraintOrientationHorizontal];
    NSStackView* foot = [NSStackView stackViewWithViews:@[ remove, spacer, cancel, s.apply ]];
    NSStackView* all = [NSStackView stackViewWithViews:@[ heading, top, hint, buttons, [NSTextField labelWithString:@"How it will look:"], s.preview, s.error, foot ]];
    all.orientation = NSUserInterfaceLayoutOrientationVertical;
    all.alignment = NSLayoutAttributeLeading;
    all.spacing = 8;
    all.edgeInsets = NSEdgeInsetsMake(16, 16, 16, 16);
    all.translatesAutoresizingMaskIntoConstraints = NO;
    [p.contentView addSubview:all];
    [NSLayoutConstraint activateConstraints:@[
        [all.leadingAnchor constraintEqualToAnchor:p.contentView.leadingAnchor], [all.trailingAnchor constraintEqualToAnchor:p.contentView.trailingAnchor],
        [all.topAnchor constraintEqualToAnchor:p.contentView.topAnchor], [all.bottomAnchor constraintEqualToAnchor:p.contentView.bottomAnchor],
        [top.widthAnchor constraintEqualToAnchor:all.widthAnchor constant:-32], [hint.widthAnchor constraintEqualToAnchor:all.widthAnchor constant:-32],
        [s.preview.widthAnchor constraintEqualToAnchor:all.widthAnchor constant:-32], [s.error.widthAnchor constraintEqualToAnchor:all.widthAnchor constant:-32],
        [foot.widthAnchor constraintEqualToAnchor:all.widthAnchor constant:-32],
    ]];
    g_sheet = s;
    [s render];
    [self.window beginSheet:p completionHandler:^(NSModalResponse r) {
        FPEquationSheet* sheet = g_sheet;
        g_sheet = nil;
        if (r != NSModalResponseOK || !sheet) return;
        done(ws(sheet.field.stringValue), [sheet sizeValue], sheet.removeChosen);
    }];
    [p makeFirstResponder:s.field];
}

- (void)editEquation:(const equation::Found&)found {
    Document* d = self.pdf;
    if (!d) return;
    const equation::Found f = found;
    __weak FPWindowController* weak = self;
    [self equationSheet:@"Equation Editor" source:f.source size:f.size existing:YES done:[weak, d, f](const std::wstring& src, double size, bool remove) {
        FPWindowController* me = weak;
        if (!me || me.pdf != d) return;
        if (!remove && src == f.source && std::fabs(size - f.size) < 0.05) return;
        std::wstring why;
        if (equation::replace(*d, f, remove ? std::wstring() : src, size, &why)) [me afterChange];
        else form::message(me.window, why.empty() ? L"The equation could not be changed." : why);
    }];
}

- (IBAction)insertEquation:(id)sender {
    if (!self.pdf) return;
    __weak FPWindowController* weak = self;
    [self equationSheet:@"Insert Equation" source:L"" size:12 existing:NO done:[weak](const std::wstring& src, double size, bool) {
        FPWindowController* me = weak;
        if (!me || src.find_first_not_of(L" ") == std::wstring::npos) return;
        g_pending = src;
        g_pendingSize = size;
        me.pageView.tool = FPToolPlaceTable;
        form::message(me.window, L"Click on the page where the equation should start (on its baseline).");
    }];
}

@end
