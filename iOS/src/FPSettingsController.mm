#import "FPSettingsController.h"
#import "FPSettings.h"
#import "FPUpdater.h"
#import "FPEngine.h"

static NSString* const kPrivacy = @"https://github.com/akshsim1991/FeatherPDF-releases/blob/main/PRIVACY.md";

/** A row: its title, the value shown (or nil), and what a tap does; or a switch. */
@interface FPRow : NSObject
@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy, nullable) NSString* (^value)(void);
@property(nonatomic, copy, nullable) void (^tap)(UIView* cell);
@property(nonatomic, copy, nullable) BOOL (^getOn)(void);
@property(nonatomic, copy, nullable) void (^setOn)(BOOL);
@end
@implementation FPRow
@end

@implementation FPSettingsController {
    NSArray<NSString*>* _groups;
    NSArray<NSArray<FPRow*>*>* _rows;
}

- (instancetype)init { return [super initWithStyle:UITableViewStyleInsetGrouped]; }

static FPRow* choice(NSString* title, NSString* (^value)(void), void (^tap)(UIView*)) {
    FPRow* r = [FPRow new]; r.title = title; r.value = value; r.tap = tap; return r;
}
static FPRow* toggle(NSString* title, BOOL (^get)(void), void (^set)(BOOL)) {
    FPRow* r = [FPRow new]; r.title = title; r.getOn = get; r.setOn = set; return r;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"Settings";
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(close)];
    __weak FPSettingsController* me = self;
    NSArray* themes = @[@"Light", @"Dark", @"Like the iPhone"];
    NSArray* colours = @[@"Normal", @"Night (soft)", @"Dimmed (warm)", @"Blackboard (all inverted)", @"Blackboard (pictures as they are)"];
    _groups = @[@"Appearance", @"PDF Viewing", @"Editing", @"Advanced", @""];
    _rows = @[
        @[choice(@"Theme", ^{ return themes[MIN(2, MAX(0, FPSettings.theme))]; },
                 ^(UIView* c) { [me pick:@"Theme" items:themes current:FPSettings.theme from:c set:^(NSInteger i) { FPSettings.theme = i; }]; })],
        @[choice(@"Page colours", ^{ return colours[MIN(4, MAX(0, (int)FPSettings.pageColours))]; },
                 ^(UIView* c) { [me pick:@"Page colours" items:colours current:FPSettings.pageColours from:c set:^(NSInteger i) { FPSettings.pageColours = (FPColours)i; }]; }),
          toggle(@"Open documents where I left them", ^{ return FPSettings.restorePositions; }, ^(BOOL v) { FPSettings.restorePositions = v; }),
          toggle(@"Keep the screen on while reading", ^{ return FPSettings.keepScreenOn; }, ^(BOOL v) { FPSettings.keepScreenOn = v; }),
          toggle(@"Show the sidebar", ^{ return FPSettings.sidebarOpen; }, ^(BOOL v) { FPSettings.sidebarOpen = v; })],
        @[choice(@"Highlight colour", ^{ return FPSettings.colourNames[FPSettings.highlightIndex]; },
                 ^(UIView* c) { [me pick:@"Highlight colour" items:FPSettings.colourNames current:FPSettings.highlightIndex from:c set:^(NSInteger i) { FPSettings.highlightIndex = i; }]; }),
          choice(@"Drawing colour", ^{ return FPSettings.colourNames[FPSettings.drawIndex]; },
                 ^(UIView* c) { [me pick:@"Drawing colour" items:FPSettings.colourNames current:FPSettings.drawIndex from:c set:^(NSInteger i) { FPSettings.drawIndex = i; }]; }),
          choice(@"Line width", ^{ return FPSettings.widthNames[FPSettings.widthIndex]; },
                 ^(UIView* c) { [me pick:@"Line width" items:FPSettings.widthNames current:FPSettings.widthIndex from:c set:^(NSInteger i) { FPSettings.widthIndex = i; }]; })],
        @[toggle(@"Check for new versions", ^{ return FPSettings.checkUpdates; }, ^(BOOL v) { FPSettings.checkUpdates = v; }),
          choice(@"Check now", nil, ^(UIView*) { [FPUpdater checkNow:me]; }),
          choice(@"Forget recent documents", nil, ^(UIView* c) {
              [me confirm:@"Forget the recent documents?" action:@"Forget" from:c then:^{ [FPRecent clear]; }]; }),
          choice(@"Reset every setting", nil, ^(UIView* c) {
              [me confirm:@"Put every setting back to its default? Recent documents are kept." action:@"Reset" from:c then:^{ [FPSettings reset]; [me.tableView reloadData]; }]; })],
        @[choice(@"About Feather PDF", ^{ return FPUpdater.version; }, ^(UIView*) { [me.navigationController pushViewController:[FPAboutController new] animated:YES]; })],
    ];
}

- (void)close { [self dismissViewControllerAnimated:YES completion:nil]; }

- (void)pick:(NSString*)title items:(NSArray<NSString*>*)items current:(NSInteger)current from:(UIView*)cell set:(void (^)(NSInteger))set {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:title message:nil preferredStyle:UIAlertControllerStyleActionSheet];
    for (NSInteger i = 0; i < (NSInteger)items.count; ++i)
        [a addAction:[UIAlertAction actionWithTitle:i == current ? [items[i] stringByAppendingString:@"  ✓"] : items[i] style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
            set(i); [self.tableView reloadData]; }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    a.popoverPresentationController.sourceView = cell;
    a.popoverPresentationController.sourceRect = cell.bounds;
    [self presentViewController:a animated:YES completion:nil];
}

- (void)confirm:(NSString*)message action:(NSString*)action from:(UIView*)cell then:(void (^)(void))then {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:nil message:message preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:action style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) { then(); }]];
    [self presentViewController:a animated:YES completion:nil];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)t { return _groups.count; }
- (NSString*)tableView:(UITableView*)t titleForHeaderInSection:(NSInteger)s { return _groups[s].length ? _groups[s] : nil; }
- (NSInteger)tableView:(UITableView*)t numberOfRowsInSection:(NSInteger)s { return _rows[s].count; }

- (UITableViewCell*)tableView:(UITableView*)t cellForRowAtIndexPath:(NSIndexPath*)ip {
    FPRow* r = _rows[ip.section][ip.row];
    UITableViewCell* c = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:nil];
    c.textLabel.text = r.title;
    c.textLabel.numberOfLines = 0;
    if (r.getOn) {
        UISwitch* s = [UISwitch new];
        s.on = r.getOn();
        void (^set)(BOOL) = r.setOn;
        [s addAction:[UIAction actionWithHandler:^(UIAction* a) { set(((UISwitch*)a.sender).on); }] forControlEvents:UIControlEventValueChanged];
        c.accessoryView = s;
        c.selectionStyle = UITableViewCellSelectionStyleNone;
    } else {
        c.detailTextLabel.text = r.value ? r.value() : nil;
        c.accessoryType = r.value ? UITableViewCellAccessoryDisclosureIndicator : UITableViewCellAccessoryNone;
        if (!r.value) c.textLabel.textColor = c.tintColor ?: UIColor.systemBlueColor;
    }
    return c;
}

- (void)tableView:(UITableView*)t didSelectRowAtIndexPath:(NSIndexPath*)ip {
    [t deselectRowAtIndexPath:ip animated:YES];
    FPRow* r = _rows[ip.section][ip.row];
    if (r.tap) r.tap([t cellForRowAtIndexPath:ip] ?: t);
}
@end

// ---------------------------------------------------------------------------------------------------------------

@implementation FPAboutController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"About";
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    UITextView* t = [UITextView new];
    t.editable = NO;
    t.dataDetectorTypes = UIDataDetectorTypeLink;
    t.font = [UIFont systemFontOfSize:16];
    t.textContainerInset = UIEdgeInsetsMake(16, 16, 16, 16);
    NSString* name = FPUpdater.codename;
    NSString* v = name.length ? [NSString stringWithFormat:@"%@ “%@”", FPUpdater.version, name] : FPUpdater.version;
    t.text = [NSString stringWithFormat:@"Feather PDF\nVersion %@ for iPhone and iPad\n\n"
        "A fast, lightweight PDF reader and editor. Read and search documents; change their text and equations; add text, "
        "highlights, drawings and notes; turn and delete pages; save and share. Everything happens on your iPhone, with "
        "no account, cloud service or tracking.\n\n%@\n\n"
        "PRIVACY\nFeather PDF does not collect, store or send any personal data. It uses the internet only to see whether a new "
        "version is out (a small version file; Settings > Advanced turns it off); your documents are opened only when you "
        "choose them, and stay on your iPhone. The privacy policy: %@", v, [FPEngine aboutCredits], kPrivacy];
    t.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:t];
    UIButton* lic = [UIButton systemButtonWithPrimaryAction:[UIAction actionWithTitle:@"Open-source licences" image:nil identifier:nil handler:^(UIAction*) { [self licences]; }]];
    UIButton* write = [UIButton systemButtonWithPrimaryAction:[UIAction actionWithTitle:@"Write to the developer" image:nil identifier:nil handler:^(UIAction*) {
        NSString* subject = [[NSString stringWithFormat:@"Feather PDF %@ (iPhone)", FPUpdater.version] stringByAddingPercentEncodingWithAllowedCharacters:NSCharacterSet.URLQueryAllowedCharacterSet];
        NSURL* u = [NSURL URLWithString:[NSString stringWithFormat:@"mailto:%@?subject=%@", [FPEngine feedbackAddress], subject]];
        [UIApplication.sharedApplication openURL:u options:@{} completionHandler:nil];
    }]];
    UIStackView* buttons = [[UIStackView alloc] initWithArrangedSubviews:@[lic, write]];
    buttons.distribution = UIStackViewDistributionFillEqually;
    buttons.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:buttons];
    [NSLayoutConstraint activateConstraints:@[
        [t.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor], [t.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [t.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor], [t.bottomAnchor constraintEqualToAnchor:buttons.topAnchor],
        [buttons.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor], [buttons.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [buttons.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-8], [buttons.heightAnchor constraintEqualToConstant:44],
    ]];
}

/** The licences of the open-source parts inside the app (the bundle's licenses folder). */
- (void)licences {
    NSString* dir = [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"licenses"];
    NSArray* files = [[NSFileManager.defaultManager contentsOfDirectoryAtPath:dir error:nil] sortedArrayUsingSelector:@selector(compare:)] ?: @[];
    NSMutableString* all = [NSMutableString stringWithString:@"Feather PDF uses these open-source parts, under the licences below: PDFium (BSD-3-Clause, Apache-2.0) "
        "with FreeType, libjpeg-turbo, libpng, zlib, OpenJPEG, Little CMS, Anti-Grain Geometry, ICU, Abseil, fast_float, simdutf; "
        "and fonts made from Latin Modern Math for equations in TeX documents (GUST Font License).\n"];
    for (NSString* f in files) {
        NSString* body = [NSString stringWithContentsOfFile:[dir stringByAppendingPathComponent:f] encoding:NSUTF8StringEncoding error:nil] ?: @"(not readable)";
        [all appendFormat:@"\n\n==== %@ ====\n\n%@", f, body];
    }
    UIViewController* vc = [UIViewController new];
    vc.title = @"Open-source licences";
    UITextView* t = [UITextView new];
    t.editable = NO;
    t.font = [UIFont monospacedSystemFontOfSize:11 weight:UIFontWeightRegular];
    t.text = all;
    vc.view = t;
    [self.navigationController pushViewController:vc animated:YES];
}
@end
