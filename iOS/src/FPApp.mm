// Feather PDF for iPhone and iPad: the app, its start screen (open a PDF, the recent documents) and its build check.
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "FPEngine.h"
#import "FPSettings.h"
#import "FPSettingsController.h"
#import "FPUpdater.h"
#import "FPViewer.h"

// ---------------------------------------------------------------------------------------------------------------
// The start screen

@interface FPStart : UIViewController <UITableViewDataSource, UITableViewDelegate, UIDocumentPickerDelegate>
@property(nonatomic, readonly) FPViewer* viewer;
- (void)openURL:(NSURL*)url;
@end

@implementation FPStart {
    FPViewer* _viewer;
    UITableView* _list;
    NSArray<FPRecent*>* _recents;
    UILabel* _empty;
    NSString* _engineProblem;
}

- (FPViewer*)viewer { return _viewer; }

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"Feather PDF";
    self.view.backgroundColor = UIColor.systemGroupedBackgroundColor;
    _viewer = [FPViewer new];
    UIBarButtonItem* gear = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"gear"] style:UIBarButtonItemStylePlain target:self action:@selector(settings)];
    gear.accessibilityLabel = @"Settings";
    UIBarButtonItem* about = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"info.circle"] style:UIBarButtonItemStylePlain target:self action:@selector(about)];
    about.accessibilityLabel = @"About Feather PDF";
    self.navigationItem.rightBarButtonItems = @[gear, about];

    UIButtonConfiguration* c = [UIButtonConfiguration filledButtonConfiguration];
    c.title = @"Open a PDF";
    c.image = [UIImage systemImageNamed:@"doc.badge.plus"];
    c.imagePadding = 10;
    c.baseBackgroundColor = [UIColor colorWithRed:0.78 green:0.16 blue:0.16 alpha:1];
    c.cornerStyle = UIButtonConfigurationCornerStyleLarge;
    c.contentInsets = NSDirectionalEdgeInsetsMake(14, 20, 14, 20);
    UIButton* open = [UIButton buttonWithConfiguration:c primaryAction:[UIAction actionWithHandler:^(UIAction*) { [self pick]; }]];
    open.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:open];

    _list = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStyleInsetGrouped];
    _list.dataSource = self; _list.delegate = self;
    _list.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_list];
    _empty = [UILabel new];
    _empty.text = @"Documents you open appear here.\n\nYou can also open a PDF from Files, Mail or Safari: Share, then Feather PDF.";
    _empty.numberOfLines = 0;
    _empty.textAlignment = NSTextAlignmentCenter;
    _empty.textColor = UIColor.secondaryLabelColor;
    _empty.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_empty];
    [NSLayoutConstraint activateConstraints:@[
        [open.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:16],
        [open.leadingAnchor constraintEqualToAnchor:self.view.readableContentGuide.leadingAnchor],
        [open.trailingAnchor constraintEqualToAnchor:self.view.readableContentGuide.trailingAnchor],
        [_list.topAnchor constraintEqualToAnchor:open.bottomAnchor constant:8],
        [_list.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor], [_list.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [_list.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [_empty.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
        [_empty.leadingAnchor constraintEqualToAnchor:self.view.readableContentGuide.leadingAnchor],
        [_empty.trailingAnchor constraintEqualToAnchor:self.view.readableContentGuide.trailingAnchor],
    ]];
    _engineProblem = [FPEngine start];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    _recents = FPRecent.list;
    _empty.hidden = _recents.count > 0;
    [_list reloadData];
    self.navigationController.navigationBar.prefersLargeTitles = YES;
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if (_engineProblem) {
        UIAlertController* a = [UIAlertController alertControllerWithTitle:@"Feather PDF cannot read documents" message:_engineProblem preferredStyle:UIAlertControllerStyleAlert];
        [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
        [self presentViewController:a animated:YES completion:nil];
        _engineProblem = nil;
        return;
    }
    if (![NSProcessInfo.processInfo.arguments containsObject:@"--selftest"]) [FPUpdater checkOnOpen:self];
}

- (void)settings {
    [self presentViewController:[[UINavigationController alloc] initWithRootViewController:[FPSettingsController new]] animated:YES completion:nil];
}

- (void)about {
    FPAboutController* a = [FPAboutController new];
    a.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(closePresented)];
    [self presentViewController:[[UINavigationController alloc] initWithRootViewController:a] animated:YES completion:nil];
}
- (void)closePresented { [self dismissViewControllerAnimated:YES completion:nil]; }

- (void)pick {
    UIDocumentPickerViewController* p = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypePDF] asCopy:NO];
    p.allowsMultipleSelection = YES;
    p.delegate = self;
    [self presentViewController:p animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController*)c didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    for (NSURL* u in urls) [self openURL:u];
}

/** Shows the viewer (once) and opens the document in a tab there. */
- (void)showViewer {
    if (self.presentedViewController) [self dismissViewControllerAnimated:NO completion:nil];
    UINavigationController* n = self.navigationController;
    if (n.topViewController != _viewer) {
        n.navigationBar.prefersLargeTitles = NO;
        [n popToRootViewControllerAnimated:NO];
        [n pushViewController:_viewer animated:YES];
    }
}

- (void)openURL:(NSURL*)url {
    [self showViewer];
    [_viewer loadViewIfNeeded];
    [_viewer openURL:url];
}

- (void)openPath:(NSString*)path {
    [self showViewer];
    [_viewer loadViewIfNeeded];
    [_viewer openPath:path];
}

- (NSInteger)tableView:(UITableView*)t numberOfRowsInSection:(NSInteger)s { return _recents.count; }
- (NSString*)tableView:(UITableView*)t titleForHeaderInSection:(NSInteger)s { return _recents.count ? @"Recent" : nil; }

- (UITableViewCell*)tableView:(UITableView*)t cellForRowAtIndexPath:(NSIndexPath*)ip {
    UITableViewCell* c = [t dequeueReusableCellWithIdentifier:@"r"] ?: [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"r"];
    FPRecent* r = _recents[ip.row];
    c.textLabel.text = r.name;
    NSString* when = [NSDateFormatter localizedStringFromDate:[NSDate dateWithTimeIntervalSince1970:r.time] dateStyle:NSDateFormatterMediumStyle timeStyle:NSDateFormatterShortStyle];
    c.detailTextLabel.text = r.page > 0 ? [NSString stringWithFormat:@"Page %d · %@", r.page + 1, when] : when;
    c.detailTextLabel.textColor = UIColor.secondaryLabelColor;
    c.imageView.image = [UIImage systemImageNamed:@"doc.richtext"];
    c.imageView.tintColor = [UIColor colorWithRed:0.78 green:0.16 blue:0.16 alpha:1];
    return c;
}

- (void)tableView:(UITableView*)t didSelectRowAtIndexPath:(NSIndexPath*)ip {
    [t deselectRowAtIndexPath:ip animated:YES];
    FPRecent* r = _recents[ip.row];
    NSURL* u = [r resolve];
    if (!u) {
        UIAlertController* a = [UIAlertController alertControllerWithTitle:@"The document is not there any more"
                                                                   message:@"It may have been moved, renamed or deleted. Open it again from Files." preferredStyle:UIAlertControllerStyleAlert];
        [a addAction:[UIAlertAction actionWithTitle:@"Remove from the list" style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) {
            [FPRecent remove:r]; [self viewWillAppear:NO]; }]];
        [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleCancel handler:nil]];
        [self presentViewController:a animated:YES completion:nil];
        return;
    }
    [self openURL:u];
}

- (UISwipeActionsConfiguration*)tableView:(UITableView*)t trailingSwipeActionsConfigurationForRowAtIndexPath:(NSIndexPath*)ip {
    FPRecent* r = _recents[ip.row];
    UIContextualAction* a = [UIContextualAction contextualActionWithStyle:UIContextualActionStyleDestructive title:@"Remove" handler:^(UIContextualAction*, UIView*, void (^done)(BOOL)) {
        [FPRecent remove:r]; [self viewWillAppear:NO]; done(YES); }];
    return [UISwipeActionsConfiguration configurationWithActions:@[a]];
}
@end

// ---------------------------------------------------------------------------------------------------------------
// The build check on a simulator (.github/workflows/ios.yml): opens Documents/in.pdf and goes through the screen as a
// person would (edit mode, a paragraph changed, a highlight, a note, a pen stroke, finding, night colours, undo and redo,
// tabs, the sidebar, full screen, saving), writing report.txt, pictures of the screen and saved.pdf into Documents/selftest.

@interface FPSelfTest : NSObject
- (instancetype)initWithStart:(FPStart*)start window:(UIWindow*)window;
- (void)run;
@end

@implementation FPSelfTest {
    FPStart* _start;
    UIWindow* _window;
    NSString* _out;
    NSString* _in;
    NSMutableArray<NSString*>* _lines;
    NSMutableArray* _steps;   // pairs: name, block
    int _fails;
}

- (instancetype)initWithStart:(FPStart*)start window:(UIWindow*)window {
    if ((self = [super init])) {
        _start = start; _window = window;
        NSString* docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        _in = [docs stringByAppendingPathComponent:@"in.pdf"];
        _out = [docs stringByAppendingPathComponent:@"selftest"];
        _lines = [NSMutableArray array];
        _steps = [NSMutableArray array];
    }
    return self;
}

- (void)check:(NSString*)what ok:(BOOL)ok {
    [_lines addObject:[(ok ? @"ok   " : @"FAIL ") stringByAppendingString:what]];
    if (!ok) _fails++;
    [self write];
}

- (void)note:(NSString*)what { [_lines addObject:[@"note " stringByAppendingString:what]]; }

- (void)write {
    [[_lines componentsJoinedByString:@"\n"] writeToFile:[_out stringByAppendingPathComponent:@"report.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
}

- (void)shot:(NSString*)name {
    UIGraphicsImageRendererFormat* f = [UIGraphicsImageRendererFormat defaultFormat];
    UIGraphicsImageRenderer* r = [[UIGraphicsImageRenderer alloc] initWithBounds:_window.bounds format:f];
    UIImage* img = [r imageWithActions:^(UIGraphicsImageRendererContext*) { [self->_window drawViewHierarchyInRect:self->_window.bounds afterScreenUpdates:YES]; }];
    [UIImagePNGRepresentation(img) writeToFile:[_out stringByAppendingPathComponent:[name stringByAppendingString:@".png"]] atomically:YES];
}

- (void)step:(NSString*)name do:(void (^)(void))block { [_steps addObject:@[name, [block copy]]]; }

/** The steps one after another, each once the pages on screen are drawn (or after a few seconds). */
- (void)next:(NSUInteger)i waited:(double)waited {
    if (i >= _steps.count) { [self finish]; return; }
    FPViewer* v = _start.viewer;
    if (v.document && !v.pagesView.settled && waited < 8) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ [self next:i waited:waited + 0.2]; });
        return;
    }
    NSString* name = _steps[i][0];
    void (^block)(void) = _steps[i][1];
    @try { block(); } @catch (NSException* e) { [self check:[NSString stringWithFormat:@"%@ (crashed: %@)", name, e.reason] ok:NO]; }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.9 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ [self next:i + 1 waited:0]; });
}

- (void)finish {
    [_lines addObject:_fails ? [NSString stringWithFormat:@"%d FAILED", _fails] : @"ALL PASSED"];
    [_lines addObject:@"DONE"];
    [self write];
}

- (void)run {
    [NSFileManager.defaultManager removeItemAtPath:_out error:nil];
    [NSFileManager.defaultManager createDirectoryAtPath:_out withIntermediateDirectories:YES attributes:nil error:nil];
    [self write];
    [self shot:@"00-start"];
    FPViewer* v = _start.viewer;
    NSString* append = @"Edited on iPhone.";
    __block FPDoc doc = nullptr;
    __weak FPSelfTest* me = self;
    [self step:@"open" do:^{ [self->_start openPath:self->_in]; }];
    [self step:@"opened" do:^{
        doc = v.document;
        [me check:[NSString stringWithFormat:@"the document opened with %d pages", doc ? [FPEngine pageCount:doc] : 0] ok:doc && [FPEngine pageCount:doc] > 0];
        [me check:@"its pages are drawn" ok:v.pagesView.settled];
        [me check:[NSString stringWithFormat:@"the title shows the file's name (%@)", v.title] ok:[v.title hasSuffix:@"in.pdf"]];
        CGRect tools = [v.toolsView convertRect:v.toolsView.bounds toView:nil];
        CGRect pages = [v.pagesView convertRect:v.pagesView.bounds toView:nil];
        UIEdgeInsets safe = self->_window.safeAreaInsets;
        [me note:[NSString stringWithFormat:@"window %@, safe area top %.0f bottom %.0f, pages %@, tools %@", NSStringFromCGSize(self->_window.bounds.size),
                  safe.top, safe.bottom, NSStringFromCGRect(pages), NSStringFromCGRect(tools)]];
        [me check:@"the pages start below the status bar and the title bar" ok:CGRectGetMinY(pages) >= safe.top];
        [me check:@"the tools are below the pages, clear of the home indicator" ok:CGRectGetMinY(tools) >= CGRectGetMaxY(pages) - 1 && CGRectGetMaxY(tools) <= self->_window.bounds.size.height - safe.bottom + 1];
        [me shot:@"01-open"];
    }];
    [self step:@"edit mode" do:^{
        [v chooseTool:FPToolEditText];
        [me check:@"edit mode outlines the text" ok:[FPEngine editableBlocks:doc page:0].count > 0];
    }];
    [self step:@"edit mode picture" do:^{ [me shot:@"02-edit-mode"]; }];
    [self step:@"edit a paragraph" do:^{
        NSString* why = [v editParagraphAt:0 point:CGPointMake(100, 700) appending:append];
        [me check:[NSString stringWithFormat:@"a paragraph is changed (%@)", why ?: @"done"] ok:why == nil];
        [me check:@"the new words are in the document" ok:[FPEngine find:doc text:append].count > 0];
    }];
    [self step:@"edited picture" do:^{ [me shot:@"03-edited"]; }];
    [self step:@"highlight" do:^{
        NSArray* r = [FPEngine selectionRects:doc page:0 from:CGPointMake(72, 765) to:CGPointMake(250, 765)];
        [me check:@"text is selected for a highlight" ok:r.count > 0];
        [me check:@"the highlight is added" ok:[FPEngine addMarkup:doc page:0 kind:FPMarkupHighlight rects:r color:0xFFEB3B]];
        [me check:@"a note is added" ok:[FPEngine addNote:doc page:0 point:CGPointMake(400, 300) text:@"From the build check"]];
        [v changedDocument];
    }];
    [self step:@"draw" do:^{
        [v chooseTool:FPToolDraw];
        NSMutableArray* pts = [NSMutableArray array];
        for (int k = 0; k <= 12; ++k) [pts addObject:[NSValue valueWithCGPoint:CGPointMake(180 + k * 20, 420 + (k % 2 ? -20 : 20))]];
        [(id<FPPagesDelegate>)v pagesStroke:0 points:pts];
        NSString* kind = nil;
        const int i = [FPEngine annotAt:doc page:0 point:CGPointMake(300, 420) kind:&kind comment:nil];
        [me check:[NSString stringWithFormat:@"a pen stroke becomes a drawing on the page (%@)", kind ?: @"none"] ok:i >= 0 && [kind isEqualToString:@"drawing"]];
        [v chooseTool:FPToolRead];
    }];
    [self step:@"annotated picture" do:^{ [me shot:@"04-annotated"]; }];
    [self step:@"find" do:^{
        [v runSearch:@"Edited"];
        [me check:[NSString stringWithFormat:@"finding shows the matches (%lu)", (unsigned long)v.pagesView.matches.count] ok:v.pagesView.matches.count > 0];
    }];
    [self step:@"found picture" do:^{ [me shot:@"05-found"]; }];
    [self step:@"night colours" do:^{ v.pagesView.colours = FPColoursNight; }];
    [self step:@"night picture" do:^{ [me shot:@"06-night"]; v.pagesView.colours = FPColoursNormal; }];
    [self step:@"undo and redo" do:^{
        [me check:@"undo" ok:[FPEngine undo:doc]];
        [me check:@"redo" ok:[FPEngine redo:doc]];
        [v changedDocument];
    }];
    [self step:@"tabs" do:^{
        [me check:@"one tab, shown in the tab strip" ok:v.tabCount == 1 && !v.tabBarView.hidden && v.tabBarView.bounds.size.height > 0];
        NSString* second = [self->_out stringByAppendingPathComponent:@"second.pdf"];
        [NSFileManager.defaultManager copyItemAtPath:self->_in toPath:second error:nil];
        [v openPath:second];
    }];
    [self step:@"a second tab" do:^{
        [me check:@"a second document opens in a second tab" ok:v.tabCount == 2 && v.currentTab == 1 && v.document != doc];
        [me shot:@"10-two-tabs"];
        [v switchToTab:0];
        [me check:@"the first tab shows its own document again" ok:v.currentTab == 0 && v.document == doc];
        [v closeTab:1];
        [me check:@"closing the second tab leaves the first" ok:v.tabCount == 1 && v.document == doc];
        [v setSidebar:YES];
    }];
    [self step:@"sidebar" do:^{ [me check:@"the sidebar is shown" ok:!v.sidebarView.hidden && v.sidebarView.bounds.size.width > 50]; }];
    [self step:@"sidebar thumbnails" do:^{
        [me check:[NSString stringWithFormat:@"page thumbnails are drawn (%ld)", (long)v.thumbnailsDrawn] ok:v.thumbnailsDrawn > 0];
        [me shot:@"11-sidebar"];
        [v setSidebar:NO];
        [v setFullScreen:YES];
    }];
    [self step:@"full screen" do:^{
        [me check:@"full screen: the title bar, tabs and tools are hidden" ok:v.fullScreen && v.navigationController.navigationBarHidden && v.tabBarView.hidden && v.toolsView.superview.hidden];
        [me shot:@"12-full-screen"];
        [v setFullScreen:NO];
        [me check:@"and shown again" ok:!v.fullScreen && !v.navigationController.navigationBarHidden && !v.tabBarView.hidden];
    }];
    [self step:@"save" do:^{
        NSData* bytes = [FPEngine bytesToSave:doc];
        [me check:@"the document is written" ok:bytes.length > 500];
        [bytes writeToFile:[self->_out stringByAppendingPathComponent:@"saved.pdf"] atomically:YES];
        [FPEngine markSaved:doc];
        [me check:@"then it is not modified" ok:![FPEngine isModified:doc]];
    }];
    [self step:@"settings" do:^{
        UINavigationController* n = [[UINavigationController alloc] initWithRootViewController:[FPSettingsController new]];
        [v presentViewController:n animated:NO completion:nil];
    }];
    [self step:@"settings picture" do:^{
        [me shot:@"08-settings"];
        [v dismissViewControllerAnimated:NO completion:nil];
    }];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.6 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ [self next:0 waited:0]; });
}
@end

// ---------------------------------------------------------------------------------------------------------------
// The app

@interface FPAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow* window;
@end

@implementation FPAppDelegate {
    FPStart* _start;
    NSMutableArray<NSURL*>* _pending;
    FPSelfTest* _test;
}

- (BOOL)application:(UIApplication*)app didFinishLaunchingWithOptions:(NSDictionary*)options {
    _pending = [NSMutableArray array];
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    _start = [FPStart new];
    UINavigationController* n = [[UINavigationController alloc] initWithRootViewController:_start];
    n.navigationBar.prefersLargeTitles = YES;
    self.window.rootViewController = n;
    self.window.tintColor = [UIColor colorWithRed:0.78 green:0.16 blue:0.16 alpha:1];
    [self.window makeKeyAndVisible];
    [FPSettings applyTheme];
    if ([NSProcessInfo.processInfo.arguments containsObject:@"--selftest"]) {
        _test = [[FPSelfTest alloc] initWithStart:_start window:self.window];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ [self->_test run]; });
    }
    return YES;
}

/** A PDF from Files, Mail, Safari (Share, then Feather PDF) or "Open in". */
- (BOOL)application:(UIApplication*)app openURL:(NSURL*)url options:(NSDictionary<UIApplicationOpenURLOptionsKey, id>*)options {
    if (!url.isFileURL) return NO;
    [_start loadViewIfNeeded];
    [_start openURL:url];
    return YES;
}

@end

int main(int argc, char* argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(FPAppDelegate.class));
    }
}
