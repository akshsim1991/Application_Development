// The open documents, each in a tab (as in Feather PDF for Windows, macOS and Android): reading, finding, editing text,
// equations, annotating, page tools, saving. A sidebar shows the pages as thumbnails and the bookmarks; a tap on the
// page shows it alone, full screen (another tap brings the bars back).
#import <UIKit/UIKit.h>
#import "FPPagesView.h"

NS_ASSUME_NONNULL_BEGIN

@interface FPViewer : UIViewController
/** Opens a document in a new tab (or shows its tab when it is open): a file from Files or another app. */
- (void)openURL:(NSURL*)url;
/** The build check: a file of the app's own, opened as it is (no security scope). */
- (void)openPath:(NSString*)path;
@property(nonatomic, readonly) NSInteger tabCount;
@property(nonatomic, readonly) NSInteger currentTab;
@property(nonatomic, readonly) FPDoc document;
@property(nonatomic, readonly) FPPagesView* pagesView;
@property(nonatomic, readonly) UIView* tabBarView;
@property(nonatomic, readonly) UIView* toolsView;
@property(nonatomic, readonly) UIView* sidebarView;
@property(nonatomic, readonly) BOOL fullScreen;
@property(nonatomic, readonly) NSInteger thumbnailsDrawn;
@property(nonatomic, copy, nullable) void (^opened)(void);   // the build check: called when a document has opened
- (void)switchToTab:(NSInteger)i;
- (void)closeTab:(NSInteger)i;
- (void)setSidebar:(BOOL)open;
- (void)setFullScreen:(BOOL)on;
- (void)chooseTool:(FPTool)t;
- (void)runSearch:(NSString*)text;
- (void)changedDocument;
- (nullable NSString*)editParagraphAt:(int)page point:(CGPoint)p appending:(NSString*)more;   // the build check's edit
@end

NS_ASSUME_NONNULL_END
