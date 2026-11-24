#import "FPSheets.h"
#include <cmath>

static NSString* sizeText(double v) {
    return std::fmod(v, 1.0) == 0 ? [NSString stringWithFormat:@"%d", (int)v] : [NSString stringWithFormat:@"%.1f", v];
}

static UILabel* label(NSString* t) {
    UILabel* l = [UILabel new];
    l.text = t;
    [l setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    return l;
}

static UITextField* sizeField(double v) {
    UITextField* f = [UITextField new];
    f.text = sizeText(v);
    f.keyboardType = UIKeyboardTypeDecimalPad;
    f.borderStyle = UITextBorderStyleRoundedRect;
    [f.widthAnchor constraintEqualToConstant:72].active = YES;
    return f;
}

/** A form in a scroll view, filling the sheet. */
static UIStackView* form(UIViewController* vc) {
    vc.view.backgroundColor = UIColor.systemBackgroundColor;
    UIScrollView* s = [UIScrollView new];
    s.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
    s.translatesAutoresizingMaskIntoConstraints = NO;
    [vc.view addSubview:s];
    UIStackView* box = [UIStackView new];
    box.axis = UILayoutConstraintAxisVertical;
    box.spacing = 12;
    box.translatesAutoresizingMaskIntoConstraints = NO;
    [s addSubview:box];
    [NSLayoutConstraint activateConstraints:@[
        [s.topAnchor constraintEqualToAnchor:vc.view.safeAreaLayoutGuide.topAnchor], [s.bottomAnchor constraintEqualToAnchor:vc.view.keyboardLayoutGuide.topAnchor],
        [s.leadingAnchor constraintEqualToAnchor:vc.view.safeAreaLayoutGuide.leadingAnchor], [s.trailingAnchor constraintEqualToAnchor:vc.view.safeAreaLayoutGuide.trailingAnchor],
        [box.topAnchor constraintEqualToAnchor:s.contentLayoutGuide.topAnchor constant:16], [box.bottomAnchor constraintEqualToAnchor:s.contentLayoutGuide.bottomAnchor constant:-16],
        [box.leadingAnchor constraintEqualToAnchor:s.frameLayoutGuide.leadingAnchor constant:16], [box.trailingAnchor constraintEqualToAnchor:s.frameLayoutGuide.trailingAnchor constant:-16],
    ]];
    return box;
}

// ---------------------------------------------------------------------------------------------------------------

@implementation FPEditSheet {
    NSString* _text;
    FPParagraphInfo _info;
    UITextView* _body;
    UITextField* _size;
    UISwitch* _bold;
    UISwitch* _italic;
}

- (instancetype)initWithText:(NSString*)text info:(FPParagraphInfo)info {
    if ((self = [super init])) { _text = text; _info = info; self.title = @"Edit text"; }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemCancel target:self action:@selector(cancel)];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Apply" style:UIBarButtonItemStyleDone target:self action:@selector(done)];
    UIStackView* box = form(self);
    _body = [UITextView new];
    _body.text = _text;
    _body.font = [UIFont systemFontOfSize:17];
    _body.layer.borderColor = UIColor.separatorColor.CGColor;
    _body.layer.borderWidth = 1;
    _body.layer.cornerRadius = 8;
    [_body.heightAnchor constraintEqualToConstant:180].active = YES;
    [box addArrangedSubview:_body];
    _size = sizeField(_info.size);
    _bold = [UISwitch new]; _bold.on = _info.bold;
    _italic = [UISwitch new]; _italic.on = _info.italic;
    UIStackView* row = [[UIStackView alloc] initWithArrangedSubviews:@[label(@"Size"), _size, label(@"Bold"), _bold, label(@"Italic"), _italic]];
    row.spacing = 8;
    row.alignment = UIStackViewAlignmentCenter;
    [box addArrangedSubview:row];
    UIButton* up = [UIButton systemButtonWithPrimaryAction:[UIAction actionWithTitle:@"x²" image:nil identifier:nil handler:^(UIAction*) { [self script:1]; }]];
    UIButton* down = [UIButton systemButtonWithPrimaryAction:[UIAction actionWithTitle:@"x₂" image:nil identifier:nil handler:^(UIAction*) { [self script:-1]; }]];
    UIButton* eq = [UIButton systemButtonWithPrimaryAction:[UIAction actionWithTitle:@"∑ Equation" image:nil identifier:nil handler:^(UIAction*) {
        void (^e)(void) = self.equation;
        [self dismissViewControllerAnimated:YES completion:^{ if (e) e(); }];
    }]];
    UIStackView* tools = [[UIStackView alloc] initWithArrangedSubviews:@[up, down, eq, [UIView new]]];
    tools.spacing = 18;
    [box addArrangedSubview:tools];
    UILabel* hint = label(@"Select text and tap x² or x₂ to raise or lower it.");
    hint.font = [UIFont systemFontOfSize:13];
    hint.textColor = UIColor.secondaryLabelColor;
    hint.numberOfLines = 0;
    [box addArrangedSubview:hint];
}

- (void)viewDidAppear:(BOOL)animated { [super viewDidAppear:animated]; [_body becomeFirstResponder]; }

/** x² and x₂: the selected text made superscript (subscript), or plain again. */
- (void)script:(int)kind {
    NSRange r = _body.selectedRange;
    if (r.length == 0) {
        UIAlertController* a = [UIAlertController alertControllerWithTitle:@"Select the text to raise or lower first." message:nil preferredStyle:UIAlertControllerStyleAlert];
        [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
        [self presentViewController:a animated:YES completion:nil];
        return;
    }
    NSString* made = [FPEngine scriptText:[_body.text substringWithRange:r] kind:kind];
    _body.text = [_body.text stringByReplacingCharactersInRange:r withString:made];
    _body.selectedRange = NSMakeRange(r.location, made.length);
}

- (void)cancel { [self dismissViewControllerAnimated:YES completion:nil]; }

- (void)done {
    const double v = _size.text.doubleValue;
    const double size = v >= 2 && v <= 400 ? v : _info.size;
    NSString* t = _body.text;
    const BOOL b = _bold.on, i = _italic.on;
    auto apply = self.apply;
    [self dismissViewControllerAnimated:YES completion:^{ if (apply) apply(t, size, b, i); }];
}
@end

// ---------------------------------------------------------------------------------------------------------------

@implementation FPEquationSheet {
    NSString* _source;
    double _size0;
    BOOL _existing;
    UITextField* _field;
    UITextField* _size;
    UIImageView* _preview;
    UILabel* _error;
}

- (instancetype)initWithSource:(NSString*)source size:(double)size existing:(BOOL)existing {
    if ((self = [super init])) { _source = source; _size0 = size; _existing = existing; self.title = @"Equation editor"; }
    return self;
}

- (UIScrollView*)strip:(NSArray<NSArray<NSString*>*>*)items {
    UIScrollView* s = [UIScrollView new];
    s.showsHorizontalScrollIndicator = NO;
    UIStackView* row = [UIStackView new];
    row.spacing = 6;
    row.translatesAutoresizingMaskIntoConstraints = NO;
    [s addSubview:row];
    for (NSArray<NSString*>* it in items) {
        NSString* insert = it[1];
        UIButton* b = [UIButton buttonWithType:UIButtonTypeSystem];
        [b setTitle:it[0] forState:UIControlStateNormal];
        b.titleLabel.font = [UIFont systemFontOfSize:18];
        b.backgroundColor = UIColor.secondarySystemBackgroundColor;
        b.layer.cornerRadius = 6;
        [b addAction:[UIAction actionWithHandler:^(UIAction*) { [self put:insert]; }] forControlEvents:UIControlEventTouchUpInside];
        [b.widthAnchor constraintGreaterThanOrEqualToConstant:44].active = YES;
        [row addArrangedSubview:b];
    }
    [NSLayoutConstraint activateConstraints:@[
        [row.leadingAnchor constraintEqualToAnchor:s.contentLayoutGuide.leadingAnchor], [row.trailingAnchor constraintEqualToAnchor:s.contentLayoutGuide.trailingAnchor],
        [row.topAnchor constraintEqualToAnchor:s.contentLayoutGuide.topAnchor], [row.bottomAnchor constraintEqualToAnchor:s.contentLayoutGuide.bottomAnchor],
        [row.heightAnchor constraintEqualToAnchor:s.frameLayoutGuide.heightAnchor], [s.heightAnchor constraintEqualToConstant:40],
    ]];
    return s;
}

static NSArray<NSArray<NSString*>*>* chars(NSString* s) {
    NSMutableArray* out = [NSMutableArray array];
    [s enumerateSubstringsInRange:NSMakeRange(0, s.length) options:NSStringEnumerationByComposedCharacterSequences usingBlock:^(NSString* c, NSRange, NSRange, BOOL*) {
        [out addObject:@[c, c]]; }];
    return out;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemCancel target:self action:@selector(cancel)];
    UIBarButtonItem* ok = [[UIBarButtonItem alloc] initWithTitle:_existing ? @"Apply" : @"Insert" style:UIBarButtonItemStyleDone target:self action:@selector(finish)];
    if (_existing) {
        UIBarButtonItem* rm = [[UIBarButtonItem alloc] initWithTitle:@"Remove" style:UIBarButtonItemStylePlain target:self action:@selector(removeIt)];
        rm.tintColor = UIColor.systemRedColor;
        self.navigationItem.rightBarButtonItems = @[ok, rm];
    } else self.navigationItem.rightBarButtonItem = ok;
    UIStackView* box = form(self);
    _field = [UITextField new];
    _field.text = _source;
    _field.font = [UIFont monospacedSystemFontOfSize:18 weight:UIFontWeightRegular];
    _field.borderStyle = UITextBorderStyleRoundedRect;
    _field.autocorrectionType = UITextAutocorrectionTypeNo;
    _field.autocapitalizationType = UITextAutocapitalizationTypeNone;
    _field.smartQuotesType = UITextSmartQuotesTypeNo;
    _field.smartDashesType = UITextSmartDashesTypeNo;
    _field.spellCheckingType = UITextSpellCheckingTypeNo;
    _field.placeholder = @"s^8 + 9s^6 = 0   G(s) = \\frac{2K}{s(s+2)}";
    [_field addTarget:self action:@selector(render) forControlEvents:UIControlEventEditingChanged];
    [box addArrangedSubview:_field];
    UILabel* hint = label(@"s^8 is a power, x_1 an index (use { } for more: e^{-st}). \\frac{top}{bottom} is a fraction, \\sqrt{x} a root.");
    hint.font = [UIFont systemFontOfSize:13];
    hint.textColor = UIColor.secondaryLabelColor;
    hint.numberOfLines = 0;
    [box addArrangedSubview:hint];
    NSArray* structures = @[@[@"x²", @"^{\1}"], @[@"x₂", @"_{\1}"], @[@"a⁄b", @"\\frac{\1}{}"], @[@"√", @"\\sqrt{\1}"], @[@"ⁿ√", @"\\sqrt[\1]{}"],
                            @[@"( )", @"\\left(\1\\right)"], @[@"|x|", @"\\left|\1\\right|"], @[@"∑", @"\\sum_{\1}^{}"], @[@"∫", @"\\int_{\1}^{}"],
                            @[@"lim", @"\\lim_{\1}"], @[@"sin", @"\\sin \1"], @[@"abc", @"\\text{\1}"]];
    [box addArrangedSubview:[self strip:structures]];
    [box addArrangedSubview:[self strip:chars(@"+−±×·÷=≠≈≤≥<>∞→∂′°∈")]];
    [box addArrangedSubview:[self strip:chars(@"αβγδεθλμπρστφωΔΣΩ")]];
    _size = sizeField(_size0);
    [_size addTarget:self action:@selector(render) forControlEvents:UIControlEventEditingChanged];
    UIStackView* row = [[UIStackView alloc] initWithArrangedSubviews:@[label(@"Size"), _size, [UIView new]]];
    row.spacing = 8;
    row.alignment = UIStackViewAlignmentCenter;
    [box addArrangedSubview:row];
    _preview = [UIImageView new];
    _preview.backgroundColor = UIColor.whiteColor;
    _preview.contentMode = UIViewContentModeCenter;
    _preview.clipsToBounds = YES;
    _preview.layer.cornerRadius = 6;
    [_preview.heightAnchor constraintGreaterThanOrEqualToConstant:100].active = YES;
    [box addArrangedSubview:_preview];
    _error = label(@"");
    _error.textColor = UIColor.systemRedColor;
    _error.numberOfLines = 0;
    [box addArrangedSubview:_error];
    [self render];
}

- (void)viewDidAppear:(BOOL)animated { [super viewDidAppear:animated]; [_field becomeFirstResponder]; }

- (double)sizeValue {
    const double v = _size.text.doubleValue;
    return v >= 4 && v <= 144 ? v : 12;
}

- (void)render {
    NSString* src = _field.text ?: @"";
    if ([src stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet].length == 0) { _preview.image = nil; _error.text = @""; return; }
    NSString* err = nil;
    UIImage* img = [FPEngine equationPreview:src size:self.sizeValue scale:2 * UIScreen.mainScreen.scale error:&err];
    if (img) img = [UIImage imageWithCGImage:img.CGImage scale:UIScreen.mainScreen.scale orientation:UIImageOrientationUp];
    _preview.image = img;
    _preview.contentMode = img && (img.size.width > _preview.bounds.size.width - 8 || img.size.height > _preview.bounds.size.height - 8)
        ? UIViewContentModeScaleAspectFit : UIViewContentModeCenter;
    _error.text = err ?: @"";
}

/** A button's text at the cursor; a template wraps the selected text (the cursor goes inside it otherwise). */
- (void)put:(NSString*)what {
    UITextRange* r = _field.selectedTextRange ?: [_field textRangeFromPosition:_field.endOfDocument toPosition:_field.endOfDocument];
    const NSInteger a = [_field offsetFromPosition:_field.beginningOfDocument toPosition:r.start];
    const NSInteger b = [_field offsetFromPosition:_field.beginningOfDocument toPosition:r.end];
    NSString* sel = [_field textInRange:r] ?: @"";
    NSString* text = what;
    NSInteger caret = what.length;
    const NSRange slot = [what rangeOfString:@"\1"];
    if (slot.location != NSNotFound) {
        text = [what stringByReplacingCharactersInRange:slot withString:sel];
        if (sel.length) {
            const NSRange braces = [text rangeOfString:@"{}" options:0 range:NSMakeRange(slot.location + sel.length, text.length - slot.location - sel.length)];
            caret = braces.location != NSNotFound ? (NSInteger)braces.location + 1 : (NSInteger)text.length;
        } else caret = slot.location;
    }
    [_field replaceRange:r withText:text];
    UITextPosition* p = [_field positionFromPosition:_field.beginningOfDocument offset:MIN(a + caret, (NSInteger)_field.text.length)];
    if (p) _field.selectedTextRange = [_field textRangeFromPosition:p toPosition:p];
    (void)b;
    [self render];
}

- (void)cancel { [self dismissViewControllerAnimated:YES completion:nil]; }
- (void)finish { [self close:NO]; }
- (void)removeIt { [self close:YES]; }
- (void)close:(BOOL)remove {
    NSString* src = _field.text ?: @"";
    const double size = self.sizeValue;
    auto done = self.done;
    [self dismissViewControllerAnimated:YES completion:^{ if (done) done(src, size, remove); }];
}
@end
