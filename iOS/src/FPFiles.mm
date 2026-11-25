#import "FPFiles.h"
#import "FPEngine.h"
#import "FPSettings.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@implementation FPFileItem
@end

// ---------------------------------------------------------------------------------------------------------------
// The places

static NSString* const kPlaces = @"searchPlaces";   // [{name, bookmark}]
static NSMutableArray<NSURL*>* g_active;            // the chosen folders whose permission is in force

@implementation FPPlaces

+ (NSURL*)library {
    return [NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
}

+ (void)activate {
    if (g_active) return;
    g_active = [NSMutableArray array];
    NSMutableArray* kept = [NSMutableArray array];
    BOOL changed = NO;
    for (NSDictionary* d in [NSUserDefaults.standardUserDefaults arrayForKey:kPlaces] ?: @[]) {
        BOOL stale = NO;
        NSURL* u = [NSURL URLByResolvingBookmarkData:d[@"bookmark"] options:0 relativeToURL:nil bookmarkDataIsStale:&stale error:nil];
        if (!u) { changed = YES; continue; }   // the folder is gone (or the permission withdrawn)
        [u startAccessingSecurityScopedResource];   // kept in force while the app runs: its files are opened, moved...
        NSData* b = d[@"bookmark"];
        if (stale) { b = [u bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil] ?: b; changed = YES; }
        [g_active addObject:u];
        [kept addObject:@{@"name": d[@"name"] ?: u.lastPathComponent ?: @"", @"bookmark": b}];
    }
    if (changed) [NSUserDefaults.standardUserDefaults setObject:kept forKey:kPlaces];
}

+ (NSArray<NSURL*>*)chosen { [self activate]; return [g_active copy]; }

+ (BOOL)add:(NSURL*)folder {
    [self activate];
    for (NSURL* u in g_active) if ([u.URLByStandardizingPath isEqual:folder.URLByStandardizingPath]) return YES;
    [folder startAccessingSecurityScopedResource];   // (kept, as for the others)
    NSData* b = [folder bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
    if (!b) return NO;
    NSMutableArray* list = [([NSUserDefaults.standardUserDefaults arrayForKey:kPlaces] ?: @[]) mutableCopy];
    [list addObject:@{@"name": [self nameOf:folder], @"bookmark": b}];
    [NSUserDefaults.standardUserDefaults setObject:list forKey:kPlaces];
    [g_active addObject:folder];
    return YES;
}

+ (void)removeAt:(NSUInteger)i {
    [self activate];
    if (i >= g_active.count) return;
    [g_active[i] stopAccessingSecurityScopedResource];
    [g_active removeObjectAtIndex:i];
    NSMutableArray* list = [([NSUserDefaults.standardUserDefaults arrayForKey:kPlaces] ?: @[]) mutableCopy];
    if (i < list.count) [list removeObjectAtIndex:i];
    [NSUserDefaults.standardUserDefaults setObject:list forKey:kPlaces];
}

+ (NSString*)nameOf:(NSURL*)folder {
    if ([folder.URLByStandardizingPath isEqual:self.library.URLByStandardizingPath]) return @"Feather PDF";
    NSString* n = nil;
    [folder getResourceValue:&n forKey:NSURLLocalizedNameKey error:nil];
    return n.length ? n : (folder.lastPathComponent ?: @"Folder");
}
@end

// ---------------------------------------------------------------------------------------------------------------
// The file operations: through the file coordinator (Files, iCloud Drive and other apps may be using the file)

static NSString* stem(NSString* name) { return [name.pathExtension.lowercaseString isEqualToString:@"pdf"] ? name.stringByDeletingPathExtension : name; }

/** `name` in `folder`, or "name 2.pdf", "name 3.pdf"... when that is taken. */
static NSURL* freeName(NSURL* folder, NSString* name) {
    NSString* base = stem(name);
    NSURL* u = [folder URLByAppendingPathComponent:[base stringByAppendingString:@".pdf"]];
    for (int n = 2; [NSFileManager.defaultManager fileExistsAtPath:u.path] && n < 1000; ++n)
        u = [folder URLByAppendingPathComponent:[NSString stringWithFormat:@"%@ %d.pdf", base, n]];
    return u;
}

static NSString* why(NSError* e, NSString* fallback) { return e.localizedDescription.length ? e.localizedDescription : fallback; }

@interface FPFiles () <UITableViewDataSource, UITableViewDelegate, UISearchResultsUpdating, UIDocumentPickerDelegate>
@end

@implementation FPFiles {
    UITableView* _table;
    UISearchController* _search;
    NSArray<FPFileItem*>* _all;
    NSArray<FPFileItem*>* _shown;
    NSString* _text;
    NSInteger _sort;          // 0 name, 1 date (newest first), 2 size (largest first), 3 folder
    UILabel* _status;
    BOOL _scanning, _again;
    NSArray<NSURL*>* _toMove; // the files waiting for the folder picker
    BOOL _copying;            // ... to be copied (not moved)
    UIView* _placesHint;
}

- (instancetype)initWithSearch:(NSString*)text {
    if ((self = [super init])) { _text = text ?: @""; _all = @[]; _shown = @[]; self.title = @"PDF files"; }
    return self;
}

- (NSArray<FPFileItem*>*)found { return _all; }
- (NSArray<FPFileItem*>*)shown { return _shown; }

+ (NSString*)rename:(NSURL*)url to:(NSString*)name result:(NSURL**)result {
    NSString* clean = [[name stringByReplacingOccurrencesOfString:@"/" withString:@"-"] stringByReplacingOccurrencesOfString:@":" withString:@"-"];
    clean = [stem(clean) stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (clean.length == 0) return @"Type a name.";
    NSURL* to = [url.URLByDeletingLastPathComponent URLByAppendingPathComponent:[clean stringByAppendingString:@".pdf"]];
    if ([to.path isEqualToString:url.path]) { if (result) *result = url; return nil; }
    const BOOL caseOnly = [to.path.lowercaseString isEqualToString:url.path.lowercaseString];
    if (!caseOnly && [NSFileManager.defaultManager fileExistsAtPath:to.path]) return [NSString stringWithFormat:@"There is already a file called %@ in that folder.", to.lastPathComponent];
    __block NSError* e = nil;
    NSError* ce = nil;
    [[NSFileCoordinator new] coordinateWritingItemAtURL:url options:NSFileCoordinatorWritingForMoving writingItemAtURL:to options:NSFileCoordinatorWritingForReplacing error:&ce byAccessor:^(NSURL* a, NSURL* b) {
        [NSFileManager.defaultManager moveItemAtURL:a toURL:b error:&e];
    }];
    if (ce || e) return why(e ?: ce, @"It could not be renamed.");
    [FPRecent moved:url to:to];
    if (result) *result = to;
    return nil;
}

+ (NSString*)duplicate:(NSURL*)url result:(NSURL**)result {
    NSURL* to = freeName(url.URLByDeletingLastPathComponent, [stem(url.lastPathComponent) stringByAppendingString:@" copy"]);
    return [self makeCopy:url to:to result:result];
}

+ (NSString*)makeCopy:(NSURL*)url to:(NSURL*)to result:(NSURL**)result {
    __block NSError* e = nil;
    NSError* ce = nil;
    [[NSFileCoordinator new] coordinateReadingItemAtURL:url options:0 writingItemAtURL:to options:NSFileCoordinatorWritingForReplacing error:&ce byAccessor:^(NSURL* a, NSURL* b) {
        [NSFileManager.defaultManager copyItemAtURL:a toURL:b error:&e];
    }];
    if (ce || e) return why(e ?: ce, @"It could not be copied.");
    if (result) *result = to;
    return nil;
}

+ (NSString*)makeCopy:(NSURL*)url into:(NSURL*)folder result:(NSURL**)result {
    return [self makeCopy:url to:freeName(folder, url.lastPathComponent) result:result];
}

+ (NSString*)move:(NSURL*)url into:(NSURL*)folder result:(NSURL**)result {
    if ([url.URLByDeletingLastPathComponent.URLByStandardizingPath isEqual:folder.URLByStandardizingPath]) { if (result) *result = url; return nil; }
    NSURL* to = freeName(folder, url.lastPathComponent);
    __block NSError* e = nil;
    NSError* ce = nil;
    [[NSFileCoordinator new] coordinateWritingItemAtURL:url options:NSFileCoordinatorWritingForMoving writingItemAtURL:to options:NSFileCoordinatorWritingForReplacing error:&ce byAccessor:^(NSURL* a, NSURL* b) {
        [NSFileManager.defaultManager moveItemAtURL:a toURL:b error:&e];
    }];
    if (ce || e) return why(e ?: ce, @"It could not be moved.");
    [FPRecent moved:url to:to];
    if (result) *result = to;
    return nil;
}

+ (NSString*)remove:(NSURL*)url {
    __block NSError* e = nil;
    NSError* ce = nil;
    [[NSFileCoordinator new] coordinateWritingItemAtURL:url options:NSFileCoordinatorWritingForDeleting error:&ce byAccessor:^(NSURL* a) {
        // to the Recently Deleted folder of Files where the place has one; else deleted
        if (![NSFileManager.defaultManager trashItemAtURL:a resultingItemURL:nil error:nil]) [NSFileManager.defaultManager removeItemAtURL:a error:&e];
    }];
    if (ce || e) return why(e ?: ce, @"It could not be deleted.");
    [FPRecent forget:url];
    return nil;
}

// ---------------------------------------------------------------------------------------------------------------
// The screen

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemGroupedBackgroundColor;
    self.navigationItem.largeTitleDisplayMode = UINavigationItemLargeTitleDisplayModeNever;
    _search = [[UISearchController alloc] initWithSearchResultsController:nil];
    _search.searchResultsUpdater = self;
    _search.obscuresBackgroundDuringPresentation = NO;
    _search.searchBar.placeholder = @"Search PDF files by name";
    _search.searchBar.text = _text;
    self.navigationItem.searchController = _search;
    self.navigationItem.hidesSearchBarWhenScrolling = NO;
    self.definesPresentationContext = YES;

    _table = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStyleInsetGrouped];
    _table.dataSource = self; _table.delegate = self;
    _table.allowsMultipleSelectionDuringEditing = YES;
    _table.translatesAutoresizingMaskIntoConstraints = NO;
    _table.refreshControl = [UIRefreshControl new];
    [_table.refreshControl addTarget:self action:@selector(rescan) forControlEvents:UIControlEventValueChanged];
    [self.view addSubview:_table];
    [NSLayoutConstraint activateConstraints:@[
        [_table.topAnchor constraintEqualToAnchor:self.view.topAnchor], [_table.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [_table.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor], [_table.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    ]];
    _status = [UILabel new];
    _status.textColor = UIColor.secondaryLabelColor;
    _status.font = [UIFont systemFontOfSize:13];
    _status.textAlignment = NSTextAlignmentCenter;
    _status.numberOfLines = 0;
    _status.frame = CGRectMake(0, 0, 300, 44);
    _table.tableFooterView = _status;
    [self updateButtons];
    [self rescan];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    self.navigationController.toolbarHidden = !_table.editing;
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    self.navigationController.toolbarHidden = YES;
}

- (void)updateButtons {
    __weak FPFiles* me = self;
    NSArray* sorts = @[@"Name", @"Date modified (newest first)", @"Size (largest first)", @"Folder"];
    NSMutableArray* sortItems = [NSMutableArray array];
    for (NSInteger i = 0; i < (NSInteger)sorts.count; ++i) {
        UIAction* a = [UIAction actionWithTitle:sorts[i] image:nil identifier:nil handler:^(UIAction*) { [me sortBy:i]; }];
        if (i == _sort) a.state = UIMenuElementStateOn;
        [sortItems addObject:a];
    }
    UIMenu* more = [UIMenu menuWithTitle:@"" children:@[
        [UIMenu menuWithTitle:@"Sort by" image:[UIImage systemImageNamed:@"arrow.up.arrow.down"] identifier:nil options:0 children:sortItems],
        [UIAction actionWithTitle:@"Add a place to search…" image:[UIImage systemImageNamed:@"folder.badge.plus"] identifier:nil handler:^(UIAction*) { [me addPlace]; }],
        [UIAction actionWithTitle:@"Places searched" image:[UIImage systemImageNamed:@"folder"] identifier:nil handler:^(UIAction*) { [me showPlaces]; }],
        [UIAction actionWithTitle:@"Search again" image:[UIImage systemImageNamed:@"arrow.clockwise"] identifier:nil handler:^(UIAction*) { [me rescan]; }],
    ]];
    UIBarButtonItem* menu = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"ellipsis.circle"] menu:more];
    menu.accessibilityLabel = @"Sort and places";
    UIBarButtonItem* select = [[UIBarButtonItem alloc] initWithTitle:_table.editing ? @"Done" : @"Select" style:_table.editing ? UIBarButtonItemStyleDone : UIBarButtonItemStylePlain
                                                              target:self action:@selector(toggleSelect)];
    self.navigationItem.rightBarButtonItems = @[menu, select];
    // choosing several: what can be done with all of them
    UIBarButtonItem* flex = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil];
    UIBarButtonItem* share = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"square.and.arrow.up"] style:UIBarButtonItemStylePlain target:self action:@selector(shareChosen:)];
    UIBarButtonItem* move = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"folder"] style:UIBarButtonItemStylePlain target:self action:@selector(moveChosen:)];
    UIBarButtonItem* dup = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"plus.square.on.square"] style:UIBarButtonItemStylePlain target:self action:@selector(duplicateChosen)];
    UIBarButtonItem* del = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"trash"] style:UIBarButtonItemStylePlain target:self action:@selector(deleteChosen)];
    share.accessibilityLabel = @"Share"; move.accessibilityLabel = @"Move"; dup.accessibilityLabel = @"Duplicate"; del.accessibilityLabel = @"Delete";
    del.tintColor = UIColor.systemRedColor;
    self.toolbarItems = @[share, flex, move, flex, dup, flex, del];
}

- (void)toggleSelect {
    [_table setEditing:!_table.editing animated:YES];
    [self.navigationController setToolbarHidden:!_table.editing animated:YES];
    [self updateButtons];
}

- (void)sortBy:(NSInteger)i { _sort = i; [self filter]; [self updateButtons]; }

- (void)tell:(NSString*)title message:(nullable NSString*)message {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:a animated:YES completion:nil];
}

// ---------------------------------------------------------------------------------------------------------------
// Searching

- (void)rescan {
    if (_scanning) { _again = YES; return; }
    _scanning = YES;
    _status.text = @"Searching…";
    NSMutableArray<NSURL*>* roots = [NSMutableArray arrayWithObject:FPPlaces.library];
    for (NSURL* u in FPPlaces.chosen) {   // (a place inside another is searched once)
        BOOL inside = NO;
        for (NSURL* r in roots) if ([u.URLByStandardizingPath.path hasPrefix:[r.URLByStandardizingPath.path stringByAppendingString:@"/"]] || [u.URLByStandardizingPath isEqual:r.URLByStandardizingPath]) inside = YES;
        if (!inside) [roots addObject:u];
    }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSMutableArray<FPFileItem*>* found = [NSMutableArray array];
        NSMutableSet<NSString*>* seen = [NSMutableSet set];
        NSArray* keys = @[NSURLIsDirectoryKey, NSURLFileSizeKey, NSURLContentModificationDateKey, NSURLIsPackageKey];
        for (NSURL* root in roots) {
            NSString* rootName = [FPPlaces nameOf:root];
            NSString* rootPath = root.URLByStandardizingPath.path;
            NSDirectoryEnumerator* e = [NSFileManager.defaultManager enumeratorAtURL:root includingPropertiesForKeys:keys
                                                                             options:NSDirectoryEnumerationSkipsPackageDescendants errorHandler:^BOOL(NSURL*, NSError*) { return YES; }];
            for (NSURL* u in e) {
                NSString* n = u.lastPathComponent;
                BOOL cloud = NO;
                NSString* real = n;
                if ([n hasPrefix:@"."]) {   // iCloud Drive: a file not downloaded yet is ".Name.pdf.icloud"
                    if (![n.pathExtension isEqualToString:@"icloud"]) continue;
                    real = [[n substringFromIndex:1] stringByDeletingPathExtension];
                    cloud = YES;
                }
                if ([u.path containsString:@"/.Trash"] || [u.path containsString:@"/Inbox/"]) continue;
                if (![real.pathExtension.lowercaseString isEqualToString:@"pdf"]) continue;
                NSNumber* dir = nil; [u getResourceValue:&dir forKey:NSURLIsDirectoryKey error:nil];
                if (dir.boolValue) continue;
                NSURL* fileURL = cloud ? [u.URLByDeletingLastPathComponent URLByAppendingPathComponent:real] : u;
                NSString* key = fileURL.URLByStandardizingPath.path;
                if ([seen containsObject:key]) continue;
                [seen addObject:key];
                FPFileItem* it = [FPFileItem new];
                it.url = fileURL; it.name = real; it.inCloud = cloud;
                NSNumber* size = nil; [u getResourceValue:&size forKey:NSURLFileSizeKey error:nil];
                NSDate* date = nil; [u getResourceValue:&date forKey:NSURLContentModificationDateKey error:nil];
                it.size = size.longLongValue; it.modified = date;
                NSString* folder = fileURL.URLByDeletingLastPathComponent.URLByStandardizingPath.path;
                NSString* rel = folder.length > rootPath.length ? [folder substringFromIndex:rootPath.length + 1] : @"";
                it.where = rel.length ? [NSString stringWithFormat:@"%@ › %@", rootName, [rel stringByReplacingOccurrencesOfString:@"/" withString:@" › "]] : rootName;
                [found addObject:it];
            }
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_scanning = NO;
            self->_all = found;
            [self->_table.refreshControl endRefreshing];
            [self filter];
            if (self->_again) { self->_again = NO; [self rescan]; return; }
            if (self.scanned) self.scanned();
        });
    });
}

- (void)updateSearchResultsForSearchController:(UISearchController*)c {
    _text = c.searchBar.text ?: @"";
    [self filter];
}

- (void)setSearchText:(NSString*)text { _text = text; _search.searchBar.text = text; [self filter]; }

- (void)filter {
    NSString* t = [_text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
    NSMutableArray* out = [NSMutableArray array];
    for (FPFileItem* it in _all)
        if (t.length == 0 || [it.name rangeOfString:t options:NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch].location != NSNotFound
            || [it.where rangeOfString:t options:NSCaseInsensitiveSearch].location != NSNotFound) [out addObject:it];
    const NSInteger sort = _sort;
    [out sortUsingComparator:^NSComparisonResult(FPFileItem* a, FPFileItem* b) {
        switch (sort) {
            case 1: return [(b.modified ?: NSDate.distantPast) compare:(a.modified ?: NSDate.distantPast)];
            case 2: return a.size == b.size ? NSOrderedSame : (a.size > b.size ? NSOrderedAscending : NSOrderedDescending);
            case 3: { NSComparisonResult r = [a.where localizedStandardCompare:b.where]; if (r != NSOrderedSame) return r; break; }
        }
        return [a.name localizedStandardCompare:b.name];
    }];
    _shown = out;
    [_table reloadData];
    const NSUInteger places = 1 + FPPlaces.chosen.count;
    if (_scanning) _status.text = @"Searching…";
    else if (_all.count == 0) _status.text = [NSString stringWithFormat:@"No PDF files in the %lu place%@ searched.", (unsigned long)places, places == 1 ? @"" : @"s"];
    else _status.text = [NSString stringWithFormat:@"%lu of %lu PDF files · %lu place%@ searched", (unsigned long)_shown.count, (unsigned long)_all.count,
                                                   (unsigned long)places, places == 1 ? @"" : @"s"];
    [_status sizeToFit];
    CGRect f = _status.frame; f.size.width = _table.bounds.size.width; f.size.height = 52; _status.frame = f;
}

// ---------------------------------------------------------------------------------------------------------------
// The places

- (void)addPlace {
    UIDocumentPickerViewController* p = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeFolder]];
    p.delegate = self;
    _toMove = nil;
    [self presentViewController:p animated:YES completion:nil];
}

- (void)showPlaces {
    NSMutableString* m = [NSMutableString stringWithString:@"• Feather PDF (On My iPhone › Feather PDF)\n"];
    NSArray<NSURL*>* chosen = FPPlaces.chosen;
    for (NSURL* u in chosen) [m appendFormat:@"• %@\n", [FPPlaces nameOf:u]];
    [m appendString:@"\niOS lets Feather PDF into a folder only when you choose it. To search all your PDFs, add “On My iPhone” and “iCloud Drive” (in Browse, choose the place itself, then Open)."];
    UIAlertController* a = [UIAlertController alertControllerWithTitle:@"Places searched" message:m preferredStyle:UIAlertControllerStyleAlert];
    __weak FPFiles* me = self;
    [a addAction:[UIAlertAction actionWithTitle:@"Add a place…" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) { [me addPlace]; }]];
    for (NSUInteger i = 0; i < chosen.count; ++i)
        [a addAction:[UIAlertAction actionWithTitle:[@"Stop searching " stringByAppendingString:[FPPlaces nameOf:chosen[i]]] style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) {
            [FPPlaces removeAt:i]; [me rescan]; }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Close" style:UIAlertActionStyleCancel handler:nil]];
    [self presentViewController:a animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController*)c didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    NSURL* folder = urls.firstObject;
    if (!folder) return;
    if (_toMove.count) {   // the destination of a move (or copy)
        NSArray<NSURL*>* files = _toMove;
        _toMove = nil;
        const BOOL scoped = [folder startAccessingSecurityScopedResource];
        [self moveFiles:files into:folder copy:_copying];
        if (scoped) [folder stopAccessingSecurityScopedResource];
        return;
    }
    if (![FPPlaces add:folder]) { [self tell:@"That folder cannot be searched" message:@"iOS did not let Feather PDF keep it."]; return; }
    [self rescan];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)c { _toMove = nil; }

// ---------------------------------------------------------------------------------------------------------------
// The list

- (NSInteger)numberOfSectionsInTableView:(UITableView*)t { return 2; }

- (NSInteger)tableView:(UITableView*)t numberOfRowsInSection:(NSInteger)s {
    if (s == 0) return FPPlaces.chosen.count == 0 ? 1 : 0;   // until a place is added: how to search the whole iPhone
    return _shown.count;
}

- (NSString*)tableView:(UITableView*)t titleForHeaderInSection:(NSInteger)s { return s == 1 && _shown.count ? @"PDF files" : nil; }

- (UITableViewCell*)tableView:(UITableView*)t cellForRowAtIndexPath:(NSIndexPath*)ip {
    if (ip.section == 0) {
        UITableViewCell* c = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
        c.textLabel.text = @"Search all of your iPhone";
        c.textLabel.textColor = t.tintColor;
        c.detailTextLabel.text = @"Add “On My iPhone” and “iCloud Drive” once: Feather PDF then finds every PDF there.";
        c.detailTextLabel.numberOfLines = 0;
        c.imageView.image = [UIImage systemImageNamed:@"folder.badge.plus"];
        return c;
    }
    UITableViewCell* c = [t dequeueReusableCellWithIdentifier:@"f"] ?: [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"f"];
    FPFileItem* it = _shown[ip.row];
    c.textLabel.text = stem(it.name);
    c.textLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
    NSString* size = [NSByteCountFormatter stringFromByteCount:it.size countStyle:NSByteCountFormatterCountStyleFile];
    NSString* date = it.modified ? [NSDateFormatter localizedStringFromDate:it.modified dateStyle:NSDateFormatterMediumStyle timeStyle:NSDateFormatterNoStyle] : @"";
    c.detailTextLabel.text = [NSString stringWithFormat:@"%@ · %@%@%@", it.where, it.inCloud ? @"in iCloud" : size, date.length ? @" · " : @"", date];
    c.detailTextLabel.textColor = UIColor.secondaryLabelColor;
    c.imageView.image = [UIImage systemImageNamed:it.inCloud ? @"icloud.and.arrow.down" : @"doc.richtext"];
    c.imageView.tintColor = [UIColor colorWithRed:0.78 green:0.16 blue:0.16 alpha:1];
    return c;
}

- (BOOL)tableView:(UITableView*)t canEditRowAtIndexPath:(NSIndexPath*)ip { return ip.section == 1; }

- (void)tableView:(UITableView*)t didSelectRowAtIndexPath:(NSIndexPath*)ip {
    if (ip.section == 0) { [t deselectRowAtIndexPath:ip animated:YES]; [self addPlace]; return; }
    if (t.editing) return;   // choosing several
    [t deselectRowAtIndexPath:ip animated:YES];
    if (self.openFile) self.openFile(_shown[ip.row].url);
}

- (NSArray<NSURL*>*)chosenURLs {
    NSMutableArray* out = [NSMutableArray array];
    for (NSIndexPath* ip in _table.indexPathsForSelectedRows ?: @[]) if (ip.section == 1 && ip.row < (NSInteger)_shown.count) [out addObject:_shown[ip.row].url];
    return out;
}

- (UISwipeActionsConfiguration*)tableView:(UITableView*)t trailingSwipeActionsConfigurationForRowAtIndexPath:(NSIndexPath*)ip {
    if (ip.section != 1) return nil;
    NSURL* u = _shown[ip.row].url;
    __weak FPFiles* me = self;
    UIContextualAction* del = [UIContextualAction contextualActionWithStyle:UIContextualActionStyleDestructive title:@"Delete" handler:^(UIContextualAction*, UIView*, void (^done)(BOOL)) {
        [me confirmDelete:@[u]]; done(YES); }];
    del.image = [UIImage systemImageNamed:@"trash"];
    UIContextualAction* ren = [UIContextualAction contextualActionWithStyle:UIContextualActionStyleNormal title:@"Rename" handler:^(UIContextualAction*, UIView*, void (^done)(BOOL)) {
        [me askRename:u]; done(YES); }];
    ren.backgroundColor = UIColor.systemBlueColor;
    return [UISwipeActionsConfiguration configurationWithActions:@[del, ren]];
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)t contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)ip point:(CGPoint)p {
    if (ip.section != 1 || t.editing) return nil;
    FPFileItem* it = _shown[ip.row];
    NSURL* u = it.url;
    __weak FPFiles* me = self;
    return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:^UIMenu*(NSArray<UIMenuElement*>*) {
        NSMutableArray* items = [NSMutableArray arrayWithArray:@[
            [UIAction actionWithTitle:@"Open" image:[UIImage systemImageNamed:@"doc.text"] identifier:nil handler:^(UIAction*) { if (me.openFile) me.openFile(u); }],
            [UIAction actionWithTitle:@"Rename" image:[UIImage systemImageNamed:@"pencil"] identifier:nil handler:^(UIAction*) { [me askRename:u]; }],
            [UIAction actionWithTitle:@"Duplicate" image:[UIImage systemImageNamed:@"plus.square.on.square"] identifier:nil handler:^(UIAction*) { [me duplicateFiles:@[u]]; }],
            [UIAction actionWithTitle:@"Move…" image:[UIImage systemImageNamed:@"folder"] identifier:nil handler:^(UIAction*) { [me chooseDestinationFor:@[u] copy:NO from:nil]; }],
            [UIAction actionWithTitle:@"Copy to…" image:[UIImage systemImageNamed:@"doc.on.doc"] identifier:nil handler:^(UIAction*) { [me chooseDestinationFor:@[u] copy:YES from:nil]; }],
            [UIAction actionWithTitle:@"Share…" image:[UIImage systemImageNamed:@"square.and.arrow.up"] identifier:nil handler:^(UIAction*) { [me share:@[u] from:nil]; }],
            [UIAction actionWithTitle:@"Information" image:[UIImage systemImageNamed:@"info.circle"] identifier:nil handler:^(UIAction*) { [me showInfo:it]; }],
        ]];
        UIAction* del = [UIAction actionWithTitle:@"Delete" image:[UIImage systemImageNamed:@"trash"] identifier:nil handler:^(UIAction*) { [me confirmDelete:@[u]]; }];
        del.attributes = UIMenuElementAttributesDestructive;
        [items addObject:[UIMenu menuWithTitle:@"" image:nil identifier:nil options:UIMenuOptionsDisplayInline children:@[del]]];
        return [UIMenu menuWithTitle:it.name children:items];
    }];
}

// ---------------------------------------------------------------------------------------------------------------
// What can be done with a file, or with several

- (void)askRename:(NSURL*)u {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:@"Rename" message:nil preferredStyle:UIAlertControllerStyleAlert];
    [a addTextFieldWithConfigurationHandler:^(UITextField* f) { f.text = stem(u.lastPathComponent); f.clearButtonMode = UITextFieldViewModeWhileEditing; }];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Rename" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        NSString* w = [FPFiles rename:u to:a.textFields.firstObject.text ?: @"" result:nil];
        if (w) [self tell:@"Not renamed" message:w];
        [self rescan];
    }]];
    [self presentViewController:a animated:YES completion:nil];
}

- (void)duplicateFiles:(NSArray<NSURL*>*)urls {
    NSMutableArray* failed = [NSMutableArray array];
    for (NSURL* u in urls) if (NSString* w = [FPFiles duplicate:u result:nil]) [failed addObject:[NSString stringWithFormat:@"%@: %@", u.lastPathComponent, w]];
    if (failed.count) [self tell:@"Not every file was duplicated" message:[failed componentsJoinedByString:@"\n"]];
    [self leaveSelect];
    [self rescan];
}

- (void)duplicateChosen { NSArray* c = self.chosenURLs; if (c.count) [self duplicateFiles:c]; }

/** Where to: Feather PDF's own folder, one of the places, or any folder (the folder picker). */
- (void)chooseDestinationFor:(NSArray<NSURL*>*)urls copy:(BOOL)copy from:(nullable UIBarButtonItem*)item {
    if (urls.count == 0) return;
    UIAlertController* a = [UIAlertController alertControllerWithTitle:copy ? @"Copy to" : @"Move to"
                                                               message:urls.count == 1 ? urls.firstObject.lastPathComponent : [NSString stringWithFormat:@"%lu files", (unsigned long)urls.count]
                                                        preferredStyle:UIAlertControllerStyleActionSheet];
    NSMutableArray<NSURL*>* places = [NSMutableArray arrayWithObject:FPPlaces.library];
    [places addObjectsFromArray:FPPlaces.chosen];
    for (NSURL* p in places)
        [a addAction:[UIAlertAction actionWithTitle:[FPPlaces nameOf:p] style:UIAlertActionStyleDefault handler:^(UIAlertAction*) { [self moveFiles:urls into:p copy:copy]; }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Choose a folder…" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        self->_toMove = urls; self->_copying = copy;
        UIDocumentPickerViewController* p = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeFolder]];
        p.delegate = self;
        [self presentViewController:p animated:YES completion:nil];
    }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    if (item) a.popoverPresentationController.barButtonItem = item;
    else { a.popoverPresentationController.sourceView = self.view; a.popoverPresentationController.sourceRect = CGRectMake(self.view.bounds.size.width / 2, self.view.bounds.size.height / 2, 1, 1); }
    [self presentViewController:a animated:YES completion:nil];
}

- (void)moveChosen:(UIBarButtonItem*)item { [self chooseDestinationFor:self.chosenURLs copy:NO from:item]; }

- (void)moveFiles:(NSArray<NSURL*>*)urls into:(NSURL*)folder copy:(BOOL)copy {
    NSMutableArray* failed = [NSMutableArray array];
    for (NSURL* u in urls) {
        NSString* w = copy ? [FPFiles makeCopy:u into:folder result:nil] : [FPFiles move:u into:folder result:nil];
        if (w) [failed addObject:[NSString stringWithFormat:@"%@: %@", u.lastPathComponent, w]];
    }
    if (failed.count) [self tell:copy ? @"Not every file was copied" : @"Not every file was moved" message:[failed componentsJoinedByString:@"\n"]];
    [self leaveSelect];
    [self rescan];
}

- (void)share:(NSArray<NSURL*>*)urls from:(nullable UIBarButtonItem*)item {
    if (urls.count == 0) return;
    UIActivityViewController* a = [[UIActivityViewController alloc] initWithActivityItems:urls applicationActivities:nil];
    if (item) a.popoverPresentationController.barButtonItem = item;
    else { a.popoverPresentationController.sourceView = self.view; a.popoverPresentationController.sourceRect = CGRectMake(self.view.bounds.size.width / 2, 80, 1, 1); }
    [self presentViewController:a animated:YES completion:nil];
}

- (void)shareChosen:(UIBarButtonItem*)item { [self share:self.chosenURLs from:item]; }

- (void)confirmDelete:(NSArray<NSURL*>*)urls {
    if (urls.count == 0) return;
    NSString* title = urls.count == 1 ? [NSString stringWithFormat:@"Delete %@?", urls.firstObject.lastPathComponent]
                                      : [NSString stringWithFormat:@"Delete %lu PDF files?", (unsigned long)urls.count];
    UIAlertController* a = [UIAlertController alertControllerWithTitle:title message:@"Where Files keeps a Recently Deleted folder (iCloud Drive), they can be brought back from there."
                                                        preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Delete" style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) {
        NSMutableArray* failed = [NSMutableArray array];
        for (NSURL* u in urls) if (NSString* w = [FPFiles remove:u]) [failed addObject:[NSString stringWithFormat:@"%@: %@", u.lastPathComponent, w]];
        if (failed.count) [self tell:@"Not every file was deleted" message:[failed componentsJoinedByString:@"\n"]];
        [self leaveSelect];
        [self rescan];
    }]];
    [self presentViewController:a animated:YES completion:nil];
}

- (void)deleteChosen { [self confirmDelete:self.chosenURLs]; }

- (void)leaveSelect { if (_table.editing) [self toggleSelect]; }

- (void)showInfo:(FPFileItem*)it {
    NSMutableString* m = [NSMutableString string];
    [m appendFormat:@"Where: %@\n", it.where];
    if (it.inCloud) [m appendString:@"In iCloud Drive, not on this iPhone yet (opening it downloads it)\n"];
    else [m appendFormat:@"Size: %@\n", [NSByteCountFormatter stringFromByteCount:it.size countStyle:NSByteCountFormatterCountStyleFile]];
    if (it.modified) [m appendFormat:@"Changed: %@\n", [NSDateFormatter localizedStringFromDate:it.modified dateStyle:NSDateFormatterLongStyle timeStyle:NSDateFormatterShortStyle]];
    NSDate* created = nil;
    [it.url getResourceValue:&created forKey:NSURLCreationDateKey error:nil];
    if (created) [m appendFormat:@"Made: %@\n", [NSDateFormatter localizedStringFromDate:created dateStyle:NSDateFormatterLongStyle timeStyle:NSDateFormatterShortStyle]];
    if (!it.inCloud) {
        int result = 0;
        FPDoc d = [FPEngine open:it.url.path password:@"" result:&result];
        if (d) { [m appendFormat:@"Pages: %d\n", [FPEngine pageCount:d]]; [FPEngine close:d]; }
        else if (result == 4) [m appendString:@"Protected with a password\n"];
    }
    __weak FPFiles* me = self;
    NSURL* u = it.url;
    UIAlertController* a = [UIAlertController alertControllerWithTitle:it.name message:m preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"Open" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) { if (me.openFile) me.openFile(u); }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Close" style:UIAlertActionStyleCancel handler:nil]];
    [self presentViewController:a animated:YES completion:nil];
}

@end
