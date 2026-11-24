// The pages of a document, one under another: scrolled with a finger, zoomed with two (or a double tap). Each page is
// drawn in tiles at the zoom shown (sharp at any zoom); over it, an overlay shows search matches, the text being
// selected, edit mode's outlines and the stroke being drawn. The tools (selecting, highlighting, drawing) take one
// finger; two fingers still move and zoom. As Android/.../DocumentView.kt.
#import <UIKit/UIKit.h>
#import "FPEngine.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, FPTool) { FPToolRead, FPToolEditText, FPToolAddText, FPToolHighlight, FPToolDraw, FPToolNote, FPToolErase };

@protocol FPPagesDelegate <NSObject>
- (void)pagesTapped:(int)page at:(CGPoint)point;              // page point
- (void)pagesTappedOutside;                                   // beside the pages
- (void)pagesSwept:(int)page from:(CGPoint)a to:(CGPoint)b;   // text selected (Read: a long press) or swept (Highlight)
- (void)pagesStroke:(int)page points:(NSArray<NSValue*>*)points;   // a pen stroke, page points
- (void)pagesShowPage:(int)page of:(int)count;
@end

@interface FPPagesView : UIView
@property(nonatomic, weak) id<FPPagesDelegate> delegate;
@property(nonatomic) FPTool tool;
@property(nonatomic) FPColours colours;
@property(nonatomic) uint32_t penColor;      // 0xRRGGBB
@property(nonatomic) double penWidth;        // points
@property(nonatomic, readonly) FPDoc doc;
@property(nonatomic, readonly) int pageCount;
@property(nonatomic, readonly) int currentPage;
@property(nonatomic, readonly) BOOL settled;   // every page on screen has been drawn
@property(nonatomic, copy) NSArray<FPMatch*>* matches;
@property(nonatomic) NSInteger currentMatch;

- (void)setDocument:(FPDoc _Nullable)doc page:(int)page;
- (void)documentChanged;                 // an edit, undo, a page turned or deleted
- (void)goToPage:(int)page;
- (void)showMatch:(NSInteger)i;
- (void)clearSelection;
- (CGPoint)viewState;                     // zoom and scroll position, kept for a tab
- (void)restoreViewState:(CGPoint)state zoom:(CGFloat)zoom;
@property(nonatomic, readonly) CGFloat zoom;
@end

NS_ASSUME_NONNULL_END
