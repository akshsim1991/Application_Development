#import "FPViewer.h"
#import "FPSettings.h"
#import "FPSheets.h"
#import "FPSettingsController.h"
#import <CommonCrypto/CommonDigest.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <cmath>

/** A document open in a tab. */
@interface FPTab : NSObject
@property(nonatomic) FPDoc doc;
@property(nonatomic, strong, nullable) NSURL* url;   // where it came from (security scoped); nil: a file of the app's own
@property(nonatomic, copy) NSString* name;
@property(nonatomic, copy) NSString* localPath;      // the copy the engine reads
@property(nonatomic) int page;
@property(nonatomic) CGPoint offset;
@property(nonatomic) CGFloat zoom;
@end
@implementation FPTab
@end

@interface FPViewer () <FPPagesDelegate, UISearchBarDelegate, UITableViewDataSource, UITableViewDelegate, UIDocumentPickerDelegate>
@end

@implementation FPViewer {
    NSMutableArray<FPTab*>* _tabs;
    NSInteger _current;
    FPPagesView* _pages;
    UIStackView* _column;
    UIScrollView* _tabScroll;
    UIStackView* _tabStack;
    UIView* _searchRow;
    UISearchBar* _search;
    UILabel* _searchCount;
    UIScrollView* _toolScroll;
    NSMutableDictionary<NSNumber*, UIButton*>* _toolButtons;
    UILabel* _pageLabel;
    UIActivityIndicatorView* _busy;
    // sidebar
    UIView* _sidebar;
    UISegmentedControl* _sideTabs;
    UITableView* _thumbList;
    UITableView* _markList;
    NSArray<FPBookmark*>* _marks;
    NSCache<NSNumber*, UIImage*>* _thumbs;
    NSMutableSet<NSNumber*>* _thumbAsked;
    NSInteger _thumbsDrawn;
    dispatch_queue_t _thumbQueue;
    FPDoc _thumbDoc;
    NSArray<FPMatch*>* _matches;
    NSInteger _matchIndex;
    BOOL _fullScreen;
    BOOL _opening;
    NSMutableArray<NSURL*>* _waiting;
}

- (instancetype)init {
    if ((self = [super init])) {
        _tabs = [NSMutableArray array];
        _current = -1;
        _toolButtons = [NSMutableDictionary dictionary];
        _thumbs = [NSCache new];
        _thumbs.countLimit = 80;
        _thumbAsked = [NSMutableSet set];
        _thumbQueue = dispatch_queue_create("thumbnails", DISPATCH_QUEUE_SERIAL);
        _waiting = [NSMutableArray array];
        _matches = @[];
    }
    return self;
}

- (FPDoc)document { return _current >= 0 ? _tabs[_current].doc : nullptr; }
- (NSInteger)tabCount { return _tabs.count; }
- (NSInteger)currentTab { return _current; }
- (FPPagesView*)pagesView { return _pages; }
- (UIView*)tabBarView { return _tabScroll; }
- (UIView*)toolsView { return _toolScroll; }
- (UIView*)sidebarView { return _sidebar; }
- (BOOL)fullScreen { return _fullScreen; }
- (NSInteger)thumbnailsDrawn { return _thumbsDrawn; }

// ---------------------------------------------------------------------------------------------------------------
// The screen

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* t) {
        return t.userInterfaceStyle == UIUserInterfaceStyleDark ? [UIColor colorWithWhite:0.05 alpha:1] : [UIColor colorWithRed:0.81 green:0.85 blue:0.86 alpha:1];
    }];
    self.navigationItem.hidesBackButton = YES;
    self.navigationItem.largeTitleDisplayMode = UINavigationItemLargeTitleDisplayModeNever;   // (the start screen's large title stays there)
    UIBarButtonItem* home = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"house"] style:UIBarButtonItemStylePlain target:self action:@selector(leave)];
    home.accessibilityLabel = @"Recent documents";
    UIBarButtonItem* side = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"sidebar.left"] style:UIBarButtonItemStylePlain target:self action:@selector(toggleSidebar)];
    side.accessibilityLabel = @"Pages and bookmarks";
    self.navigationItem.leftBarButtonItems = @[home, side];

    _column = [[UIStackView alloc] init];
    _column.axis = UILayoutConstraintAxisVertical;
    _column.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_column];
    [NSLayoutConstraint activateConstraints:@[
        [_column.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [_column.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [_column.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [_column.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    ]];

    // the tabs: one for each open document
    _tabScroll = [[UIScrollView alloc] init];
    _tabScroll.showsHorizontalScrollIndicator = NO;
    _tabScroll.backgroundColor = FPSettings.barColour;
    _tabStack = [[UIStackView alloc] init];
    _tabStack.axis = UILayoutConstraintAxisHorizontal;
    _tabStack.spacing = 4;
    _tabStack.translatesAutoresizingMaskIntoConstraints = NO;
    [_tabScroll addSubview:_tabStack];
    [NSLayoutConstraint activateConstraints:@[
        [_tabStack.leadingAnchor constraintEqualToAnchor:_tabScroll.contentLayoutGuide.leadingAnchor constant:6],
        [_tabStack.trailingAnchor constraintEqualToAnchor:_tabScroll.contentLayoutGuide.trailingAnchor constant:-6],
        [_tabStack.topAnchor constraintEqualToAnchor:_tabScroll.contentLayoutGuide.topAnchor constant:4],
        [_tabStack.bottomAnchor constraintEqualToAnchor:_tabScroll.contentLayoutGuide.bottomAnchor],
        [_tabStack.heightAnchor constraintEqualToAnchor:_tabScroll.frameLayoutGuide.heightAnchor constant:-4],
        [_tabScroll.heightAnchor constraintEqualToConstant:42],
    ]];
    [_column addArrangedSubview:_tabScroll];

    // finding
    _searchRow = [[UIView alloc] init];
    _search = [[UISearchBar alloc] init];
    _search.placeholder = @"Find in document";
    _search.delegate = self;
    _search.searchBarStyle = UISearchBarStyleMinimal;
    _searchCount = [[UILabel alloc] init];
    _searchCount.font = [UIFont monospacedDigitSystemFontOfSize:14 weight:UIFontWeightRegular];
    UIButton* prev = [UIButton systemButtonWithImage:[UIImage systemImageNamed:@"chevron.up"] target:self action:@selector(previousMatch)];
    UIButton* next = [UIButton systemButtonWithImage:[UIImage systemImageNamed:@"chevron.down"] target:self action:@selector(nextMatch)];
    UIButton* done = [UIButton systemButtonWithImage:[UIImage systemImageNamed:@"xmark"] target:self action:@selector(closeSearch)];
    UIStackView* sr = [[UIStackView alloc] initWithArrangedSubviews:@[_search, _searchCount, prev, next, done]];
    sr.spacing = 6;
    sr.alignment = UIStackViewAlignmentCenter;
    sr.translatesAutoresizingMaskIntoConstraints = NO;
    [_searchRow addSubview:sr];
    [NSLayoutConstraint activateConstraints:@[
        [sr.leadingAnchor constraintEqualToAnchor:_searchRow.leadingAnchor constant:4], [sr.trailingAnchor constraintEqualToAnchor:_searchRow.trailingAnchor constant:-8],
        [sr.topAnchor constraintEqualToAnchor:_searchRow.topAnchor], [sr.bottomAnchor constraintEqualToAnchor:_searchRow.bottomAnchor],
        [_searchRow.heightAnchor constraintEqualToConstant:52],
    ]];
    _searchRow.backgroundColor = UIColor.secondarySystemBackgroundColor;
    _searchRow.hidden = YES;
    [_column addArrangedSubview:_searchRow];

    // the pages, with the page number, the sidebar and a spinner over them
    UIView* middle = [[UIView alloc] init];
    _pages = [[FPPagesView alloc] initWithFrame:CGRectZero];
    _pages.delegate = self;
    _pages.translatesAutoresizingMaskIntoConstraints = NO;
    [middle addSubview:_pages];
    _pageLabel = [[UILabel alloc] init];
    _pageLabel.textColor = UIColor.whiteColor;
    _pageLabel.font = [UIFont monospacedDigitSystemFontOfSize:13 weight:UIFontWeightMedium];
    _pageLabel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.6];
    _pageLabel.layer.cornerRadius = 11;
    _pageLabel.clipsToBounds = YES;
    _pageLabel.textAlignment = NSTextAlignmentCenter;
    _pageLabel.userInteractionEnabled = YES;
    [_pageLabel addGestureRecognizer:[[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(askPage)]];
    _pageLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [middle addSubview:_pageLabel];
    _busy = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
    _busy.hidesWhenStopped = YES;
    _busy.translatesAutoresizingMaskIntoConstraints = NO;
    [middle addSubview:_busy];
    [self buildSidebarIn:middle];
    [NSLayoutConstraint activateConstraints:@[
        [_pages.topAnchor constraintEqualToAnchor:middle.topAnchor], [_pages.bottomAnchor constraintEqualToAnchor:middle.bottomAnchor],
        [_pages.leadingAnchor constraintEqualToAnchor:middle.leadingAnchor], [_pages.trailingAnchor constraintEqualToAnchor:middle.trailingAnchor],
        [_pageLabel.trailingAnchor constraintEqualToAnchor:middle.safeAreaLayoutGuide.trailingAnchor constant:-12],
        [_pageLabel.bottomAnchor constraintEqualToAnchor:middle.bottomAnchor constant:-12],
        [_pageLabel.heightAnchor constraintEqualToConstant:24], [_pageLabel.widthAnchor constraintGreaterThanOrEqualToConstant:64],
        [_busy.centerXAnchor constraintEqualToAnchor:middle.centerXAnchor], [_busy.centerYAnchor constraintEqualToAnchor:middle.centerYAnchor],
    ]];
    [_column addArrangedSubview:middle];

    // the tools
    _toolScroll = [[UIScrollView alloc] init];
    _toolScroll.showsHorizontalScrollIndicator = NO;
    _toolScroll.backgroundColor = UIColor.systemBackgroundColor;
    UIStackView* tools = [[UIStackView alloc] init];
    tools.spacing = 4;
    tools.translatesAutoresizingMaskIntoConstraints = NO;
    [_toolScroll addSubview:tools];
    NSArray* names = @[@"Read", @"Edit text", @"Add text", @"Highlight", @"Draw", @"Note", @"Erase"];
    for (NSInteger i = 0; i < (NSInteger)names.count; ++i) {
        UIButton* b = [UIButton buttonWithType:UIButtonTypeSystem];
        [b setTitle:names[i] forState:UIControlStateNormal];
        b.titleLabel.font = [UIFont systemFontOfSize:16];
        b.contentEdgeInsets = UIEdgeInsetsMake(8, 12, 8, 12);
        b.tag = i;
        [b addTarget:self action:@selector(toolPressed:) forControlEvents:UIControlEventTouchUpInside];
        if (i == FPToolDraw) [b addGestureRecognizer:[[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(choosePen:)]];
        _toolButtons[@(i)] = b;
        [tools addArrangedSubview:b];
    }
    [NSLayoutConstraint activateConstraints:@[
        [tools.leadingAnchor constraintEqualToAnchor:_toolScroll.contentLayoutGuide.leadingAnchor constant:6],
        [tools.trailingAnchor constraintEqualToAnchor:_toolScroll.contentLayoutGuide.trailingAnchor constant:-6],
        [tools.topAnchor constraintEqualToAnchor:_toolScroll.contentLayoutGuide.topAnchor],
        [tools.heightAnchor constraintEqualToConstant:48],
        [_toolScroll.heightAnchor constraintEqualToAnchor:tools.heightAnchor],
    ]];
    UIView* toolBox = [[UIView alloc] init];   // the tools, clear of the home indicator
    toolBox.backgroundColor = UIColor.systemBackgroundColor;
    _toolScroll.translatesAutoresizingMaskIntoConstraints = NO;
    [toolBox addSubview:_toolScroll];
    [NSLayoutConstraint activateConstraints:@[
        [_toolScroll.topAnchor constraintEqualToAnchor:toolBox.topAnchor],
        [_toolScroll.leadingAnchor constraintEqualToAnchor:toolBox.safeAreaLayoutGuide.leadingAnchor],
        [_toolScroll.trailingAnchor constraintEqualToAnchor:toolBox.safeAreaLayoutGuide.trailingAnchor],
        [_toolScroll.bottomAnchor constraintEqualToAnchor:toolBox.safeAreaLayoutGuide.bottomAnchor],
    ]];
    [_column addArrangedSubview:toolBox];
    [self chooseTool:FPToolRead];
    [self applySettings];
    [self updateMenus];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    [self applySettings];
    self.navigationController.interactivePopGestureRecognizer.enabled = NO;   // (a swipe on the page moves the page)
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    self.navigationController.interactivePopGestureRecognizer.enabled = YES;
    UIApplication.sharedApplication.idleTimerDisabled = NO;
    [self keepPages];
}

- (void)applySettings {
    _pages.colours = FPSettings.pageColours;
    _pages.penColor = FPSettings.drawColour;
    _pages.penWidth = FPSettings.lineWidth;
    UIApplication.sharedApplication.idleTimerDisabled = FPSettings.keepScreenOn;
}

- (BOOL)prefersStatusBarHidden { return _fullScreen; }
- (BOOL)prefersHomeIndicatorAutoHidden { return _fullScreen; }

- (void)toolPressed:(UIButton*)b { [self chooseTool:(FPTool)b.tag]; }

- (void)chooseTool:(FPTool)t {
    if (t == FPToolDraw && _pages.tool == FPToolDraw) { [self choosePen:nil]; return; }
    _pages.tool = t;
    for (NSNumber* k in _toolButtons) {
        UIButton* b = _toolButtons[k];
        const BOOL on = k.integerValue == t;
        [b setTitleColor:on ? [UIColor colorWithRed:0.78 green:0.16 blue:0.16 alpha:1] : UIColor.labelColor forState:UIControlStateNormal];
        b.titleLabel.font = on ? [UIFont boldSystemFontOfSize:16] : [UIFont systemFontOfSize:16];
    }
}

- (void)choosePen:(UILongPressGestureRecognizer*)g {
    if (g && g.state != UIGestureRecognizerStateBegan) return;
    UIAlertController* a = [UIAlertController alertControllerWithTitle:@"Pen" message:nil preferredStyle:UIAlertControllerStyleActionSheet];
    NSArray* colours = FPSettings.colourNames;
    for (NSInteger i = 0; i < (NSInteger)colours.count; ++i)
        [a addAction:[UIAlertAction actionWithTitle:[@"Colour: " stringByAppendingString:colours[i]] style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
            FPSettings.drawIndex = i; [self applySettings]; [self chooseTool:FPToolDraw]; }]];
    NSArray* widths = FPSettings.widthNames;
    for (NSInteger i = 0; i < (NSInteger)widths.count; ++i)
        [a addAction:[UIAlertAction actionWithTitle:[@"Width: " stringByAppendingString:widths[i]] style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
            FPSettings.widthIndex = i; [self applySettings]; [self chooseTool:FPToolDraw]; }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [self present:a from:_toolButtons[@(FPToolDraw)]];
}

/** An alert or sheet, anchored on an iPad (where action sheets are popovers). */
- (void)present:(UIViewController*)vc from:(nullable UIView*)anchor {
    if (UIPopoverPresentationController* p = vc.popoverPresentationController) {
        p.sourceView = anchor ?: self.view;
        p.sourceRect = anchor ? anchor.bounds : CGRectMake(self.view.bounds.size.width / 2, self.view.bounds.size.height / 2, 1, 1);
    }
    [self presentViewController:vc animated:YES completion:nil];
}

- (void)tell:(NSString*)title message:(nullable NSString*)message {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:a animated:YES completion:nil];
}

// ---------------------------------------------------------------------------------------------------------------
// Menus

- (void)updateMenus {
    FPDoc d = self.document;
    FPTab* t = _current >= 0 ? _tabs[_current] : nil;
    self.title = t ? ([FPEngine isModified:d] ? [@"• " stringByAppendingString:t.name] : t.name) : @"Feather PDF";
    __weak FPViewer* me = self;
    UIAction* (^act)(NSString*, NSString*, void (^)(void)) = ^UIAction*(NSString* title, NSString* icon, void (^run)(void)) {
        UIAction* a = [UIAction actionWithTitle:title image:icon ? [UIImage systemImageNamed:icon] : nil identifier:nil handler:^(UIAction*) { run(); }];
        if (!d && ![title isEqualToString:@"Open another PDF…"] && ![title isEqualToString:@"Settings"]) a.attributes = UIMenuElementAttributesDisabled;
        return a;
    };
    NSMutableArray* colours = [NSMutableArray array];
    NSArray* names = @[@"Normal", @"Night (soft)", @"Dimmed (warm)", @"Blackboard (all inverted)", @"Blackboard (pictures as they are)"];
    for (NSInteger i = 0; i < (NSInteger)names.count; ++i) {
        UIAction* a = [UIAction actionWithTitle:names[i] image:nil identifier:nil handler:^(UIAction*) {
            FPSettings.pageColours = (FPColours)i; [me applySettings]; [me updateMenus]; }];
        if (FPSettings.pageColours == i) a.state = UIMenuElementStateOn;
        [colours addObject:a];
    }
    UIMenu* menu = [UIMenu menuWithTitle:@"" children:@[
        [UIMenu menuWithTitle:@"" image:nil identifier:nil options:UIMenuOptionsDisplayInline children:@[
            act(@"Save", @"square.and.arrow.down", ^{ [me save:nil]; }),
            act(@"Save a copy…", @"doc.on.doc", ^{ [me saveCopy]; }),
            act(@"Share…", @"square.and.arrow.up", ^{ [me share]; }),
        ]],
        [UIMenu menuWithTitle:@"" image:nil identifier:nil options:UIMenuOptionsDisplayInline children:@[
            act(@"Redo", @"arrow.uturn.forward", ^{ [me redo]; }),
            act(@"Go to page…", @"arrow.right.doc.on.clipboard", ^{ [me askPage]; }),
            act(@"Pages", @"square.grid.2x2", ^{ [me setSidebar:YES]; [me showSidebarPage:0]; }),
            act(@"Bookmarks", @"bookmark", ^{ [me setSidebar:YES]; [me showSidebarPage:1]; }),
            [UIMenu menuWithTitle:@"Page colours" image:[UIImage systemImageNamed:@"circle.lefthalf.filled"] identifier:nil options:0 children:colours],
        ]],
        [UIMenu menuWithTitle:@"" image:nil identifier:nil options:UIMenuOptionsDisplayInline children:@[
            act(@"Turn this page", @"rotate.right", ^{ [me turnPage]; }),
            act(@"Delete this page…", @"trash", ^{ [me deletePage]; }),
        ]],
        [UIMenu menuWithTitle:@"" image:nil identifier:nil options:UIMenuOptionsDisplayInline children:@[
            act(@"Open another PDF…", @"plus.rectangle.on.rectangle", ^{ [me pickDocument]; }),
            act(@"Close this tab", @"xmark.rectangle", ^{ [me closeTab:me.currentTab]; }),
            act(@"Settings", @"gear", ^{ [me showSettings]; }),
        ]],
    ]];
    UIBarButtonItem* more = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"ellipsis.circle"] menu:menu];
    more.accessibilityLabel = @"More";
    UIBarButtonItem* undo = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"arrow.uturn.backward"] style:UIBarButtonItemStylePlain target:self action:@selector(undo)];
    undo.enabled = d && [FPEngine canUndo:d];
    undo.accessibilityLabel = @"Undo";
    UIBarButtonItem* find = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"magnifyingglass"] style:UIBarButtonItemStylePlain target:self action:@selector(openSearch)];
    find.enabled = d != nullptr;
    find.accessibilityLabel = @"Find";
    self.navigationItem.rightBarButtonItems = @[more, undo, find];
    [self refreshTabs];
}

- (void)showSettings {
    UINavigationController* n = [[UINavigationController alloc] initWithRootViewController:[FPSettingsController new]];
    [self presentViewController:n animated:YES completion:nil];
}

// ---------------------------------------------------------------------------------------------------------------
// Opening

- (void)openURL:(NSURL*)url {
    if (_opening) { [_waiting addObject:url]; return; }   // one at a time (several chosen at once)
    for (NSUInteger i = 0; i < _tabs.count; ++i)
        if (_tabs[i].url && [_tabs[i].url.URLByStandardizingPath isEqual:url.URLByStandardizingPath]) { [self switchToTab:i]; [self openNext]; return; }
    _opening = YES;
    [_busy startAnimating];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSString* name = url.lastPathComponent ?: @"Document.pdf";
        NSString* copy = nil;
        NSString* failure = nil;
        const BOOL scoped = [url startAccessingSecurityScopedResource];
        // the bookmark that opens it again from Recent: made while the permission to read it is in force (one made
        // after stopAccessing... cannot reach the file later: "not there any more")
        NSData* mark = [url bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
        __block NSData* data = nil;
        NSError* err = nil;
        [[NSFileCoordinator new] coordinateReadingItemAtURL:url options:NSFileCoordinatorReadingWithoutChanges error:&err byAccessor:^(NSURL* u) {
            data = [NSData dataWithContentsOfURL:u];
        }];
        if (scoped) [url stopAccessingSecurityScopedResource];
        if (data.length) {
            copy = [self cachePathFor:url.absoluteString];
            if (![data writeToFile:copy atomically:YES]) failure = @"The copy could not be written.";
        } else failure = err.localizedDescription ?: @"Feather PDF may not read it.";
        dispatch_async(dispatch_get_main_queue(), ^{
            [self->_busy stopAnimating];
            if (failure) {
                [self tell:@"The document could not be read" message:[failure stringByAppendingString:@"\n\nOpen it again from Files."]];
                [self openNext];
                return;
            }
            [self openCopy:copy url:url name:name password:@"" mark:mark];
        });
    });
}

- (void)openPath:(NSString*)path {
    [self openCopy:path url:nil name:path.lastPathComponent password:@"" mark:nil];
}

- (void)openKeptCopy:(NSString*)path name:(NSString*)name page:(int)page {
    [self openCopy:path url:nil name:name password:@"" mark:nil];
    if (page > 0 && _current >= 0) [_pages goToPage:page];
}

- (NSString*)cachePathFor:(NSString*)key {
    // in Caches (kept between launches, unless iOS needs the room): Recent opens it when the original is gone
    NSString* dir = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES).firstObject stringByAppendingPathComponent:@"open"];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    unsigned char h[CC_SHA256_DIGEST_LENGTH];
    NSData* k = [key dataUsingEncoding:NSUTF8StringEncoding];
    CC_SHA256(k.bytes, (CC_LONG)k.length, h);
    NSMutableString* s = [NSMutableString string];
    for (int i = 0; i < 8; ++i) [s appendFormat:@"%02x", h[i]];
    return [dir stringByAppendingPathComponent:[s stringByAppendingString:@".pdf"]];
}

- (void)openCopy:(NSString*)path url:(nullable NSURL*)url name:(NSString*)name password:(NSString*)password mark:(nullable NSData*)mark {
    int result = 0;
    FPDoc d = [FPEngine open:path password:password result:&result];
    if (!d) {
        if (result == 4 || result == 5) { [self askPasswordFor:path url:url name:name wrong:result == 5 mark:mark]; return; }
        [self tell:@"Feather PDF" message:[FPEngine describe:result name:name]];
        [self openNext];
        return;
    }
    [self keepPages];
    FPTab* t = [FPTab new];
    t.doc = d; t.url = url; t.name = name; t.localPath = path; t.zoom = 1;
    t.page = (url && FPSettings.restorePositions) ? [FPRecent pageForURL:url] : 0;
    if (url) [FPRecent add:url name:name bookmark:mark copy:path.lastPathComponent];
    [_tabs addObject:t];
    _current = _tabs.count - 1;
    [self closeSearch];
    [_pages setDocument:d page:t.page];
    [self sidebarDocumentChanged];
    [self updateMenus];
    if (self.opened) self.opened();
    [self openNext];
}

- (void)openNext {
    _opening = NO;
    if (_waiting.count) { NSURL* u = _waiting.firstObject; [_waiting removeObjectAtIndex:0]; [self openURL:u]; }
}

- (void)askPasswordFor:(NSString*)path url:(nullable NSURL*)url name:(NSString*)name wrong:(BOOL)wrong mark:(nullable NSData*)mark {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:wrong ? @"Wrong password" : @"This document is protected"
                                                               message:wrong ? @"Try again." : @"Type its password to open it." preferredStyle:UIAlertControllerStyleAlert];
    [a addTextFieldWithConfigurationHandler:^(UITextField* f) { f.secureTextEntry = YES; f.placeholder = @"Password"; }];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:^(UIAlertAction*) {
        [self openNext];
        if (self->_tabs.count == 0) [self.navigationController popViewControllerAnimated:YES];
    }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Open" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        [self openCopy:path url:url name:name password:a.textFields.firstObject.text ?: @"" mark:mark];
    }]];
    [self presentViewController:a animated:YES completion:nil];
}

- (void)pickDocument {
    UIDocumentPickerViewController* p = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypePDF] asCopy:NO];
    p.allowsMultipleSelection = YES;
    p.delegate = self;
    [self presentViewController:p animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController*)c didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    if (c.documentPickerMode == UIDocumentPickerModeExportToService) { [self tell:@"Saved" message:nil]; return; }
    for (NSURL* u in urls) [self openURL:u];
}

// ---------------------------------------------------------------------------------------------------------------
// Tabs

/** The shown tab's place kept in it (before another is shown). */
- (void)keepPages {
    if (_current < 0 || _current >= (NSInteger)_tabs.count) return;
    FPTab* t = _tabs[_current];
    t.page = _pages.currentPage;
    t.offset = _pages.viewState;
    t.zoom = _pages.zoom;
    if (t.url) [FPRecent setPage:t.page forURL:t.url];
}

- (void)switchToTab:(NSInteger)i {
    if (i < 0 || i >= (NSInteger)_tabs.count || i == _current) return;
    [self keepPages];
    _current = i;
    FPTab* t = _tabs[i];
    [self closeSearch];
    [_pages setDocument:t.doc page:t.page];
    if (t.zoom > 1.001) {
        const CGPoint off = t.offset; const CGFloat z = t.zoom;
        dispatch_async(dispatch_get_main_queue(), ^{ [self->_pages restoreViewState:off zoom:z]; });
    }
    [self sidebarDocumentChanged];
    [self updateMenus];
}

- (void)closeTab:(NSInteger)i {
    if (i < 0 || i >= (NSInteger)_tabs.count) return;
    FPTab* t = _tabs[i];
    if ([FPEngine isModified:t.doc] && !self.opened) {
        if (i != _current) [self switchToTab:i];
        UIAlertController* a = [UIAlertController alertControllerWithTitle:[NSString stringWithFormat:@"Save the changes to %@?", t.name] message:nil preferredStyle:UIAlertControllerStyleAlert];
        [a addAction:[UIAlertAction actionWithTitle:@"Save" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
            [self save:^{ [self dropTab:[self->_tabs indexOfObject:t]]; }]; }]];
        [a addAction:[UIAlertAction actionWithTitle:@"Don’t save" style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) {
            [self dropTab:[self->_tabs indexOfObject:t]]; }]];
        [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
        [self presentViewController:a animated:YES completion:nil];
        return;
    }
    [self dropTab:i];
}

- (void)dropTab:(NSInteger)i {
    if (i < 0 || i >= (NSInteger)_tabs.count) return;
    if (i == _current) [self keepPages];
    FPTab* t = _tabs[i];
    [_tabs removeObjectAtIndex:i];
    [FPEngine close:t.doc];
    if (_tabs.count == 0) {
        _current = -1;
        [_pages setDocument:nullptr page:0];
        [self sidebarDocumentChanged];
        [self updateMenus];
        [self.navigationController popViewControllerAnimated:YES];
        return;
    }
    const NSInteger next = i < _current ? _current - 1 : MIN(i, (NSInteger)_tabs.count - 1);
    _current = -1;   // (the closed tab's place is not kept)
    FPTab* n = _tabs[next];
    _current = next;
    [self closeSearch];
    [_pages setDocument:n.doc page:n.page];
    [self sidebarDocumentChanged];
    [self updateMenus];
}

/** The tab strip: each document's name (a dot while it has unsaved changes), × to close it, and + to open another. */
- (void)refreshTabs {
    for (UIView* v in _tabStack.arrangedSubviews) { [_tabStack removeArrangedSubview:v]; [v removeFromSuperview]; }
    for (NSUInteger i = 0; i < _tabs.count; ++i) {
        FPTab* t = _tabs[i];
        const BOOL on = (NSInteger)i == _current;
        UIView* cell = [[UIView alloc] init];
        cell.backgroundColor = on ? UIColor.systemBackgroundColor : UIColor.clearColor;
        cell.layer.cornerRadius = 8;
        cell.layer.maskedCorners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
        UIButton* title = [UIButton buttonWithType:UIButtonTypeSystem];
        [title setTitle:[([FPEngine isModified:t.doc] ? @"• " : @"") stringByAppendingString:t.name] forState:UIControlStateNormal];
        title.titleLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
        title.titleLabel.font = on ? [UIFont boldSystemFontOfSize:14] : [UIFont systemFontOfSize:14];
        [title setTitleColor:on ? UIColor.labelColor : UIColor.whiteColor forState:UIControlStateNormal];
        title.tag = (NSInteger)i;
        [title addTarget:self action:@selector(tabPressed:) forControlEvents:UIControlEventTouchUpInside];
        title.accessibilityLabel = [NSString stringWithFormat:@"Tab %lu: %@", (unsigned long)i + 1, t.name];
        UIButton* close = [UIButton buttonWithType:UIButtonTypeSystem];
        [close setImage:[UIImage systemImageNamed:@"xmark" withConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:11 weight:UIImageSymbolWeightBold]] forState:UIControlStateNormal];
        close.tintColor = on ? UIColor.labelColor : UIColor.whiteColor;
        close.tag = (NSInteger)i;
        close.accessibilityLabel = [@"Close " stringByAppendingString:t.name];
        [close addTarget:self action:@selector(tabClosePressed:) forControlEvents:UIControlEventTouchUpInside];
        UIStackView* row = [[UIStackView alloc] initWithArrangedSubviews:@[title, close]];
        row.spacing = 4;
        row.translatesAutoresizingMaskIntoConstraints = NO;
        [cell addSubview:row];
        [NSLayoutConstraint activateConstraints:@[
            [row.leadingAnchor constraintEqualToAnchor:cell.leadingAnchor constant:12], [row.trailingAnchor constraintEqualToAnchor:cell.trailingAnchor constant:-6],
            [row.topAnchor constraintEqualToAnchor:cell.topAnchor], [row.bottomAnchor constraintEqualToAnchor:cell.bottomAnchor],
            [title.widthAnchor constraintLessThanOrEqualToConstant:190], [close.widthAnchor constraintEqualToConstant:28],
        ]];
        [_tabStack addArrangedSubview:cell];
    }
    UIButton* plus = [UIButton buttonWithType:UIButtonTypeSystem];
    [plus setImage:[UIImage systemImageNamed:@"plus"] forState:UIControlStateNormal];
    plus.tintColor = UIColor.whiteColor;
    plus.accessibilityLabel = @"Open another PDF";
    [plus addTarget:self action:@selector(pickDocument) forControlEvents:UIControlEventTouchUpInside];
    [plus.widthAnchor constraintEqualToConstant:40].active = YES;
    [_tabStack addArrangedSubview:plus];
}

- (void)tabPressed:(UIButton*)b { [self switchToTab:b.tag]; }
- (void)tabClosePressed:(UIButton*)b { [self closeTab:b.tag]; }

/** The house: back to the recent documents; each document with unsaved changes is asked about first. */
- (void)leave {
    [self keepPages];
    for (NSUInteger i = 0; i < _tabs.count; ++i)
        if ([FPEngine isModified:_tabs[i].doc] && !self.opened) {
            [self switchToTab:i];
            FPTab* t = _tabs[i];
            UIAlertController* a = [UIAlertController alertControllerWithTitle:[NSString stringWithFormat:@"Save the changes to %@?", t.name] message:nil preferredStyle:UIAlertControllerStyleAlert];
            [a addAction:[UIAlertAction actionWithTitle:@"Save" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
                [self save:^{ [self dropTab:[self->_tabs indexOfObject:t]]; if (self->_tabs.count) [self leave]; }]; }]];
            [a addAction:[UIAlertAction actionWithTitle:@"Don’t save" style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) {
                [self dropTab:[self->_tabs indexOfObject:t]]; if (self->_tabs.count) [self leave]; }]];
            [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
            [self presentViewController:a animated:YES completion:nil];
            return;
        }
    while (_tabs.count) [self dropTab:_tabs.count - 1];
}

// ---------------------------------------------------------------------------------------------------------------
// The sidebar: the pages as thumbnails, and the bookmarks

- (void)buildSidebarIn:(UIView*)host {
    _sidebar = [[UIView alloc] init];
    _sidebar.backgroundColor = UIColor.systemBackgroundColor;
    _sidebar.layer.shadowColor = UIColor.blackColor.CGColor;
    _sidebar.layer.shadowOpacity = 0.25;
    _sidebar.layer.shadowRadius = 6;
    _sidebar.hidden = YES;
    _sidebar.translatesAutoresizingMaskIntoConstraints = NO;
    [host addSubview:_sidebar];
    _sideTabs = [[UISegmentedControl alloc] initWithItems:@[@"Pages", @"Bookmarks"]];
    _sideTabs.selectedSegmentIndex = 0;
    [_sideTabs addTarget:self action:@selector(sideTabChanged) forControlEvents:UIControlEventValueChanged];
    UIButton* close = [UIButton systemButtonWithImage:[UIImage systemImageNamed:@"xmark"] target:self action:@selector(closeSidebar)];
    close.accessibilityLabel = @"Close the sidebar";
    UIStackView* head = [[UIStackView alloc] initWithArrangedSubviews:@[_sideTabs, close]];
    head.spacing = 8;
    head.translatesAutoresizingMaskIntoConstraints = NO;
    [_sidebar addSubview:head];
    _thumbList = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStylePlain];
    _markList = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStylePlain];
    for (UITableView* t in @[_thumbList, _markList]) {
        t.dataSource = self; t.delegate = self;
        t.translatesAutoresizingMaskIntoConstraints = NO;
        [_sidebar addSubview:t];
        [NSLayoutConstraint activateConstraints:@[
            [t.topAnchor constraintEqualToAnchor:head.bottomAnchor constant:8], [t.bottomAnchor constraintEqualToAnchor:_sidebar.bottomAnchor],
            [t.leadingAnchor constraintEqualToAnchor:_sidebar.leadingAnchor], [t.trailingAnchor constraintEqualToAnchor:_sidebar.trailingAnchor],
        ]];
    }
    _thumbList.separatorStyle = UITableViewCellSeparatorStyleNone;
    _markList.hidden = YES;
    NSLayoutConstraint* w = [_sidebar.widthAnchor constraintEqualToAnchor:host.widthAnchor multiplier:0.78];
    w.priority = UILayoutPriorityDefaultHigh;
    [NSLayoutConstraint activateConstraints:@[
        [_sidebar.leadingAnchor constraintEqualToAnchor:host.leadingAnchor], [_sidebar.topAnchor constraintEqualToAnchor:host.topAnchor],
        [_sidebar.bottomAnchor constraintEqualToAnchor:host.bottomAnchor], w, [_sidebar.widthAnchor constraintLessThanOrEqualToConstant:300],
        [head.topAnchor constraintEqualToAnchor:_sidebar.topAnchor constant:8], [head.leadingAnchor constraintEqualToAnchor:_sidebar.safeAreaLayoutGuide.leadingAnchor constant:8],
        [head.trailingAnchor constraintEqualToAnchor:_sidebar.trailingAnchor constant:-8],
    ]];
}

- (BOOL)wide { return self.view.bounds.size.width >= 700; }
- (void)toggleSidebar { [self setSidebar:_sidebar.hidden]; }
- (void)closeSidebar { [self setSidebar:NO]; }
- (void)sideTabChanged { [self showSidebarPage:_sideTabs.selectedSegmentIndex]; }

- (void)showSidebarPage:(NSInteger)which {
    _sideTabs.selectedSegmentIndex = which;
    _thumbList.hidden = which != 0;
    _markList.hidden = which != 1;
}

- (void)setSidebar:(BOOL)open {
    if (open && _fullScreen) [self setFullScreen:NO];
    _sidebar.hidden = !open;
    FPSettings.sidebarOpen = open;
    if (open) {
        [_thumbList reloadData];
        if (_pages.pageCount > 0) [_thumbList scrollToRowAtIndexPath:[NSIndexPath indexPathForRow:MAX(0, _pages.currentPage) inSection:0] atScrollPosition:UITableViewScrollPositionMiddle animated:NO];
    }
}

- (void)sidebarDocumentChanged {
    [_thumbs removeAllObjects];
    [_thumbAsked removeAllObjects];
    _thumbDoc = self.document;
    _marks = _thumbDoc ? [FPEngine bookmarks:_thumbDoc] : @[];
    [_thumbList reloadData];
    [_markList reloadData];
    if (FPSettings.sidebarOpen && _thumbDoc && !_fullScreen) _sidebar.hidden = NO;
}

- (NSInteger)tableView:(UITableView*)t numberOfRowsInSection:(NSInteger)s {
    if (t == _thumbList) return _thumbDoc ? _pages.pageCount : 0;
    return MAX(1, (NSInteger)_marks.count);
}

- (CGFloat)thumbWidth { return MAX(80, _sidebar.bounds.size.width - 56); }

- (CGFloat)tableView:(UITableView*)t heightForRowAtIndexPath:(NSIndexPath*)ip {
    if (t != _thumbList) return 44;
    const CGSize s = [FPEngine pageSize:_thumbDoc page:(int)ip.row];
    return self.thumbWidth * s.height / MAX(1.0, s.width) + 34;
}

- (UITableViewCell*)tableView:(UITableView*)t cellForRowAtIndexPath:(NSIndexPath*)ip {
    if (t == _markList) {
        UITableViewCell* c = [t dequeueReusableCellWithIdentifier:@"mark"] ?: [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"mark"];
        if (_marks.count == 0) { c.textLabel.text = @"This document has no bookmarks."; c.textLabel.textColor = UIColor.secondaryLabelColor; c.indentationLevel = 0; return c; }
        FPBookmark* m = _marks[ip.row];
        c.textLabel.text = m.title;
        c.textLabel.textColor = UIColor.labelColor;
        c.textLabel.numberOfLines = 2;
        c.indentationLevel = MIN(6, m.depth);
        return c;
    }
    UITableViewCell* c = [t dequeueReusableCellWithIdentifier:@"thumb"];
    UIImageView* img; UILabel* num;
    if (!c) {
        c = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"thumb"];
        img = [[UIImageView alloc] init]; img.tag = 1; img.backgroundColor = UIColor.whiteColor; img.layer.borderWidth = 1;
        num = [[UILabel alloc] init]; num.tag = 2; num.font = [UIFont systemFontOfSize:12]; num.textAlignment = NSTextAlignmentCenter;
        [c.contentView addSubview:img]; [c.contentView addSubview:num];
        c.selectionStyle = UITableViewCellSelectionStyleNone;
    }
    img = (UIImageView*)[c.contentView viewWithTag:1];
    num = (UILabel*)[c.contentView viewWithTag:2];
    const CGFloat w = self.thumbWidth;
    const CGSize s = [FPEngine pageSize:_thumbDoc page:(int)ip.row];
    const CGFloat h = w * s.height / MAX(1.0, s.width);
    img.frame = CGRectMake((t.bounds.size.width - w) / 2, 8, w, h);
    num.frame = CGRectMake(0, 10 + h, t.bounds.size.width, 18);
    num.text = [NSString stringWithFormat:@"%ld", (long)ip.row + 1];
    const BOOL here = ip.row == _pages.currentPage;
    img.layer.borderColor = (here ? [UIColor colorWithRed:0.78 green:0.16 blue:0.16 alpha:1] : [UIColor colorWithWhite:0.5 alpha:0.3]).CGColor;
    img.layer.borderWidth = here ? 3 : 1;
    UIImage* th = [_thumbs objectForKey:@(ip.row)];
    img.image = th;
    if (!th) [self askThumb:(int)ip.row size:CGSizeMake(w, h)];
    return c;
}

- (void)askThumb:(int)page size:(CGSize)size {
    if ([_thumbAsked containsObject:@(page)]) return;
    [_thumbAsked addObject:@(page)];
    FPDoc doc = _thumbDoc;
    const FPColours colours = FPSettings.pageColours;
    const CGFloat scale = UIScreen.mainScreen.scale;
    dispatch_async(_thumbQueue, ^{
        const CGSize px = CGSizeMake(std::round(size.width * scale), std::round(size.height * scale));
        CGImageRef img = [FPEngine render:doc page:page part:CGRectMake(0, 0, px.width, px.height) full:px colours:colours];
        UIImage* ui = img ? [UIImage imageWithCGImage:img scale:scale orientation:UIImageOrientationUp] : nil;
        if (img) CGImageRelease(img);
        dispatch_async(dispatch_get_main_queue(), ^{
            if (doc != self->_thumbDoc || !ui) return;
            [self->_thumbs setObject:ui forKey:@(page)];
            self->_thumbsDrawn++;
            NSIndexPath* ip = [NSIndexPath indexPathForRow:page inSection:0];
            if ([self->_thumbList.indexPathsForVisibleRows containsObject:ip]) [self->_thumbList reloadRowsAtIndexPaths:@[ip] withRowAnimation:UITableViewRowAnimationNone];
        });
    });
}

- (void)tableView:(UITableView*)t didSelectRowAtIndexPath:(NSIndexPath*)ip {
    [t deselectRowAtIndexPath:ip animated:NO];
    int page = -1;
    if (t == _thumbList) page = (int)ip.row;
    else if (ip.row < (NSInteger)_marks.count) page = _marks[ip.row].page;
    if (page < 0) return;
    [_pages goToPage:page];
    if (!self.wide) [self setSidebar:NO];
}

// ---------------------------------------------------------------------------------------------------------------
// Full screen: a tap on the page (reading) hides the bars, the tabs, the tools and the sidebar; another brings them back

- (void)setFullScreen:(BOOL)on {
    _fullScreen = on;
    [self.navigationController setNavigationBarHidden:on animated:YES];
    _tabScroll.hidden = on;
    _toolScroll.superview.hidden = on;
    if (on) _sidebar.hidden = YES;
    else if (FPSettings.sidebarOpen && self.document) _sidebar.hidden = NO;
    if (on) _searchRow.hidden = YES;
    [self setNeedsStatusBarAppearanceUpdate];
    [self setNeedsUpdateOfHomeIndicatorAutoHidden];
}

// ---------------------------------------------------------------------------------------------------------------
// What the pages report

- (void)pagesShowPage:(int)page of:(int)count {
    _pageLabel.text = count > 0 ? [NSString stringWithFormat:@"  %d / %d  ", page + 1, count] : @"";
    if (!_sidebar.hidden && count > 0) {
        [_thumbList reloadRowsAtIndexPaths:_thumbList.indexPathsForVisibleRows ?: @[] withRowAnimation:UITableViewRowAnimationNone];
        NSIndexPath* ip = [NSIndexPath indexPathForRow:page inSection:0];
        if (![_thumbList.indexPathsForVisibleRows containsObject:ip] && page < [_thumbList numberOfRowsInSection:0])
            [_thumbList scrollToRowAtIndexPath:ip atScrollPosition:UITableViewScrollPositionMiddle animated:NO];
    }
}

- (void)pagesTappedOutside { if (_pages.tool == FPToolRead) [self setFullScreen:!_fullScreen]; }

- (void)pagesTapped:(int)page at:(CGPoint)p {
    FPDoc d = self.document;
    if (!d) return;
    switch (_pages.tool) {
        case FPToolEditText: [self editAt:page point:p]; break;
        case FPToolAddText: [self addTextAt:page point:p]; break;
        case FPToolNote: [self addNoteAt:page point:p]; break;
        case FPToolErase: [self eraseAt:page point:p]; break;
        default: if (![self showAnnotationAt:page point:p] && _pages.tool == FPToolRead) [self setFullScreen:!_fullScreen]; break;
    }
}

- (void)pagesSwept:(int)page from:(CGPoint)a to:(CGPoint)b {
    FPDoc d = self.document;
    NSArray* rects = [FPEngine selectionRects:d page:page from:a to:b];
    if (rects.count == 0) { [_pages clearSelection]; return; }
    if (_pages.tool == FPToolHighlight) {
        if ([FPEngine addMarkup:d page:page kind:FPMarkupHighlight rects:rects color:FPSettings.highlightColour]) [self changedDocument];
        return;
    }
    NSString* text = [FPEngine selectionText:d page:page from:a to:b];
    UIAlertController* s = [UIAlertController alertControllerWithTitle:nil message:text.length > 80 ? [[text substringToIndex:80] stringByAppendingString:@"…"] : text
                                                        preferredStyle:UIAlertControllerStyleActionSheet];
    [s addAction:[UIAlertAction actionWithTitle:@"Copy" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) { UIPasteboard.generalPasteboard.string = text; [self->_pages clearSelection]; }]];
    NSArray* names = @[@"Highlight", @"Underline", @"Strike out"];
    const uint32_t colours[] = {FPSettings.highlightColour, 0x1E88E5, 0xE53935};
    for (int k = 0; k < 3; ++k) {
        const uint32_t c = colours[k];
        [s addAction:[UIAlertAction actionWithTitle:names[k] style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
            if ([FPEngine addMarkup:d page:page kind:(FPMarkup)k rects:rects color:c]) [self changedDocument];
            [self->_pages clearSelection]; }]];
    }
    [s addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:^(UIAlertAction*) { [self->_pages clearSelection]; }]];
    [self present:s from:_pages];
}

- (void)pagesStroke:(int)page points:(NSArray<NSValue*>*)points {
    if ([FPEngine addInk:self.document page:page points:points color:FPSettings.drawColour width:FPSettings.lineWidth]) [self changedDocument];
}

- (void)changedDocument {
    [_pages documentChanged];
    _matches = @[]; _pages.matches = @[]; _matchIndex = -1; _searchCount.text = @"";
    [self sidebarDocumentChanged];
    [self updateMenus];
}

// ---------------------------------------------------------------------------------------------------------------
// The tools

- (void)editAt:(int)page point:(CGPoint)p {
    FPDoc d = self.document;
    // an equation (Cambria Math, LaTeX's, or one made here) opens in the equation editor
    if ([self editEquationAt:page point:p any:NO]) return;
    FPParagraphInfo info{};
    NSString* text = [FPEngine paragraphAt:d page:page point:p info:&info];
    if (!text) { [self tell:@"There is no text here to change." message:nil]; return; }
    FPEditSheet* sheet = [[FPEditSheet alloc] initWithText:text info:info];
    __weak FPViewer* me = self;
    sheet.equation = ^{ if (![me editEquationAt:page point:p any:YES]) [me tell:@"That text could not be read as an equation." message:nil]; };
    sheet.apply = ^(NSString* t, double size, BOOL bold, BOOL italic) {
        NSString* why = [FPEngine replaceParagraph:d text:t family:info.family bold:bold italic:italic size:size color:info.color];
        if (!why) [me changedDocument]; else [me tell:@"Not changed" message:why];
    };
    [self presentViewController:[[UINavigationController alloc] initWithRootViewController:sheet] animated:YES completion:nil];
}

- (NSString*)editParagraphAt:(int)page point:(CGPoint)p appending:(NSString*)more {
    FPDoc d = self.document;
    FPParagraphInfo info{};
    NSString* text = [FPEngine paragraphAt:d page:page point:p info:&info];
    if (!text) return @"no paragraph";
    NSString* why = [FPEngine replaceParagraph:d text:[NSString stringWithFormat:@"%@ %@", text, more] family:info.family bold:info.bold italic:info.italic size:info.size color:info.color];
    if (!why) [self changedDocument];
    return why;
}

- (BOOL)editEquationAt:(int)page point:(CGPoint)p any:(BOOL)any {
    FPDoc d = self.document;
    double size = 12; BOOL uncertain = NO;
    NSString* source = [FPEngine equationAt:d page:page point:p any:any size:&size uncertain:&uncertain];
    if (!source) return NO;
    FPEquationSheet* sheet = [[FPEquationSheet alloc] initWithSource:source size:size existing:YES];
    sheet.title = uncertain ? @"Equation (check it)" : @"Equation editor";
    __weak FPViewer* me = self;
    sheet.done = ^(NSString* src, double sz, BOOL remove) {
        if (!remove && [src isEqualToString:source] && std::fabs(sz - size) < 0.05) return;
        NSString* why = [FPEngine replaceEquation:d source:remove ? @"" : src size:sz];
        if (!why) [me changedDocument]; else [me tell:@"Not changed" message:why];
    };
    [self presentViewController:[[UINavigationController alloc] initWithRootViewController:sheet] animated:YES completion:nil];
    return YES;
}

- (void)addTextAt:(int)page point:(CGPoint)p {
    FPDoc d = self.document;
    UIAlertController* a = [UIAlertController alertControllerWithTitle:@"Add text" message:nil preferredStyle:UIAlertControllerStyleAlert];
    [a addTextFieldWithConfigurationHandler:^(UITextField* f) { f.placeholder = @"Text"; }];
    [a addAction:[UIAlertAction actionWithTitle:@"Equation…" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        FPEquationSheet* sheet = [[FPEquationSheet alloc] initWithSource:@"" size:12 existing:NO];
        sheet.title = @"Insert equation";
        sheet.done = ^(NSString* src, double sz, BOOL) {
            if (src.length == 0) return;
            NSString* why = [FPEngine insertEquation:d page:page point:p source:src size:sz];
            if (!why) [self changedDocument]; else [self tell:@"Not added" message:why];
        };
        [self presentViewController:[[UINavigationController alloc] initWithRootViewController:sheet] animated:YES completion:nil];
    }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Add" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        NSString* t = a.textFields.firstObject.text ?: @"";
        if (t.length == 0) return;
        // in the drawing colour, as on Windows; yellow would not be read on white paper, so it is dark grey then
        const uint32_t colour = FPSettings.drawIndex == 0 ? 0x202020 : FPSettings.drawColour;
        if ([FPEngine addText:d page:page point:p text:t size:12 color:colour]) [self changedDocument];
        else [self tell:@"The text could not be added" message:@"Letters of Western languages only."];
    }]];
    [self presentViewController:a animated:YES completion:nil];
}

- (void)addNoteAt:(int)page point:(CGPoint)p {
    FPDoc d = self.document;
    UIAlertController* a = [UIAlertController alertControllerWithTitle:@"Add a note" message:nil preferredStyle:UIAlertControllerStyleAlert];
    [a addTextFieldWithConfigurationHandler:^(UITextField* f) { f.placeholder = @"Note"; }];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Add" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        if ([FPEngine addNote:d page:page point:p text:a.textFields.firstObject.text ?: @""]) [self changedDocument];
    }]];
    [self presentViewController:a animated:YES completion:nil];
}

- (void)eraseAt:(int)page point:(CGPoint)p {
    FPDoc d = self.document;
    NSString* kind = nil;
    const int i = [FPEngine annotAt:d page:page point:p kind:&kind comment:nil];
    if (i < 0) { [self tell:@"Tap a highlight, drawing or note." message:nil]; return; }
    UIAlertController* a = [UIAlertController alertControllerWithTitle:[NSString stringWithFormat:@"Remove this %@?", kind ?: @"annotation"] message:nil preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Remove" style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) {
        if ([FPEngine removeAnnot:d page:page index:i]) [self changedDocument]; }]];
    [self presentViewController:a animated:YES completion:nil];
}

/** Reading: a tap on a note or a commented highlight shows its comment (YES); elsewhere nothing is shown (NO). */
- (BOOL)showAnnotationAt:(int)page point:(CGPoint)p {
    FPDoc d = self.document;
    NSString* kind = nil; NSString* comment = nil;
    const int i = [FPEngine annotAt:d page:page point:p kind:&kind comment:&comment];
    if (i < 0 || comment.length == 0) return NO;
    UIAlertController* a = [UIAlertController alertControllerWithTitle:kind.capitalizedString ?: @"Note" message:nil preferredStyle:UIAlertControllerStyleAlert];
    [a addTextFieldWithConfigurationHandler:^(UITextField* f) { f.text = comment; }];
    [a addAction:[UIAlertAction actionWithTitle:@"Close" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Remove" style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) {
        if ([FPEngine removeAnnot:d page:page index:i]) [self changedDocument]; }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Save" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        NSString* t = a.textFields.firstObject.text ?: @"";
        if (![t isEqualToString:comment] && [FPEngine setAnnotText:d page:page index:i text:t]) [self changedDocument]; }]];
    [self presentViewController:a animated:YES completion:nil];
    return YES;
}

// ---------------------------------------------------------------------------------------------------------------
// Finding

- (void)openSearch { _searchRow.hidden = NO; [_search becomeFirstResponder]; }
- (void)closeSearch {
    _searchRow.hidden = YES;
    [_search resignFirstResponder];
    _matches = @[]; _pages.matches = @[]; _matchIndex = -1;
}
- (void)searchBarSearchButtonClicked:(UISearchBar*)b { [self runSearch:b.text ?: @""]; }

- (void)runSearch:(NSString*)text {
    FPDoc d = self.document;
    if (!d || text.length == 0) return;
    [_search resignFirstResponder];
    _matches = [FPEngine find:d text:text];
    _pages.matches = _matches;
    _matchIndex = -1;
    if (_matches.count == 0) { _searchCount.text = @"0"; return; }
    const int here = _pages.currentPage;
    NSInteger first = 0;
    for (NSUInteger k = 0; k < _matches.count; ++k) if (_matches[k].page >= here) { first = (NSInteger)k; break; }
    _matchIndex = first - 1;
    [self nextMatch];
}
- (void)nextMatch { [self stepMatch:1]; }
- (void)previousMatch { [self stepMatch:-1]; }
- (void)stepMatch:(NSInteger)d {
    const NSInteger n = _matches.count;
    if (n == 0) return;
    _matchIndex = ((_matchIndex + d) % n + n) % n;
    _searchCount.text = [NSString stringWithFormat:@"%ld / %ld", (long)_matchIndex + 1, (long)n];
    [_pages showMatch:_matchIndex];
}

// ---------------------------------------------------------------------------------------------------------------
// Pages, history

- (void)undo { if ([FPEngine undo:self.document]) [self changedDocument]; }
- (void)redo { if ([FPEngine redo:self.document]) [self changedDocument]; }
- (void)turnPage { if ([FPEngine rotatePage:self.document page:_pages.currentPage]) [self changedDocument]; }

- (void)deletePage {
    if (_pages.pageCount <= 1) { [self tell:@"A document needs at least one page." message:nil]; return; }
    const int p = _pages.currentPage;
    UIAlertController* a = [UIAlertController alertControllerWithTitle:[NSString stringWithFormat:@"Delete page %d?", p + 1] message:@"Undo brings it back." preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Delete" style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) {
        if ([FPEngine deletePage:self.document page:p]) [self changedDocument]; }]];
    [self presentViewController:a animated:YES completion:nil];
}

- (void)askPage {
    if (!self.document) return;
    UIAlertController* a = [UIAlertController alertControllerWithTitle:@"Go to page" message:nil preferredStyle:UIAlertControllerStyleAlert];
    [a addTextFieldWithConfigurationHandler:^(UITextField* f) { f.keyboardType = UIKeyboardTypeNumberPad; f.placeholder = [NSString stringWithFormat:@"1 – %d", self->_pages.pageCount]; }];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Go" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        [self->_pages goToPage:a.textFields.firstObject.text.intValue - 1]; }]];
    [self presentViewController:a animated:YES completion:nil];
}

// ---------------------------------------------------------------------------------------------------------------
// Saving

/** Writes the document back where it came from (asks for a place when it cannot be written there). */
- (void)save:(nullable void (^)(void))then {
    if (_current < 0) return;
    FPTab* t = _tabs[_current];
    NSData* bytes = [FPEngine bytesToSave:t.doc];
    if (!bytes) { [self tell:@"The document could not be written." message:nil]; return; }
    if (!t.url) { [self saveCopy]; return; }
    const BOOL scoped = [t.url startAccessingSecurityScopedResource];
    __block BOOL ok = NO;
    NSError* err = nil;
    [[NSFileCoordinator new] coordinateWritingItemAtURL:t.url options:NSFileCoordinatorWritingForReplacing error:&err byAccessor:^(NSURL* u) {
        ok = [bytes writeToURL:u options:NSDataWritingAtomic error:nil];
    }];
    if (scoped) [t.url stopAccessingSecurityScopedResource];
    if (!ok) {
        UIAlertController* a = [UIAlertController alertControllerWithTitle:@"Not saved here" message:@"This place does not let Feather PDF change the file. Save a copy instead?" preferredStyle:UIAlertControllerStyleAlert];
        [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
        [a addAction:[UIAlertAction actionWithTitle:@"Save a copy…" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) { [self saveCopy]; }]];
        [self presentViewController:a animated:YES completion:nil];
        return;
    }
    [FPEngine markSaved:t.doc];
    [self updateMenus];
    if (then) then();
}

- (nullable NSURL*)exportFile {
    if (_current < 0) return nil;
    FPTab* t = _tabs[_current];
    NSData* bytes = [FPEngine bytesToSave:t.doc];
    if (!bytes) return nil;
    NSString* dir = [NSTemporaryDirectory() stringByAppendingPathComponent:@"share"];
    [NSFileManager.defaultManager removeItemAtPath:dir error:nil];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    NSString* name = [t.name.lowercaseString hasSuffix:@".pdf"] ? t.name : [t.name stringByAppendingString:@".pdf"];
    NSString* path = [dir stringByAppendingPathComponent:name];
    return [bytes writeToFile:path atomically:YES] ? [NSURL fileURLWithPath:path] : nil;
}

- (void)saveCopy {
    NSURL* f = [self exportFile];
    if (!f) { [self tell:@"The document could not be written." message:nil]; return; }
    UIDocumentPickerViewController* p = [[UIDocumentPickerViewController alloc] initForExportingURLs:@[f] asCopy:YES];
    p.delegate = self;
    [self presentViewController:p animated:YES completion:nil];
}

- (void)share {
    NSURL* f = [self exportFile];
    if (!f) return;
    UIActivityViewController* a = [[UIActivityViewController alloc] initWithActivityItems:@[f] applicationActivities:nil];
    a.popoverPresentationController.barButtonItem = self.navigationItem.rightBarButtonItems.firstObject;
    [self presentViewController:a animated:YES completion:nil];
}

@end
