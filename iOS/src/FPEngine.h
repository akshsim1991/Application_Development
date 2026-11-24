// The iPhone and iPad app's way into the shared engine (shared/), as com.featherpdf.engine.Engine is the Android app's
// (Android/app/src/main/cpp/bridge.cpp): the same calls, in Objective-C. Every call takes one lock: the engine and
// PDFium are used by one thread at a time (pages are drawn on background threads while the screen may edit).
// Page coordinates are PDF points with the origin at the bottom left, as the engine has them.
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef void* FPDoc;   // an open document (the engine's), or NULL

typedef NS_ENUM(int, FPColours) { FPColoursNormal = 0, FPColoursNight = 1, FPColoursDimmed = 2, FPColoursBlackboard = 3, FPColoursBlackboardOriginal = 4 };
typedef NS_ENUM(int, FPMarkup) { FPMarkupHighlight = 0, FPMarkupUnderline = 1, FPMarkupStrikeOut = 2 };

/** The paragraph under a point (paragraphAt): its style, as the editor starts from it. */
typedef struct {
    double size;
    BOOL bold, italic;
    int family;          // 0 sans, 1 serif, 2 mono
    uint32_t color;      // 0xRRGGBB
    int lines, align;
} FPParagraphInfo;

/** A match of Find: its page and box (page points). */
@interface FPMatch : NSObject
@property(nonatomic) int page;
@property(nonatomic) CGRect box;
@end

/** A bookmark: how deep it is, its page (-1: none) and title. */
@interface FPBookmark : NSObject
@property(nonatomic) int depth, page;
@property(nonatomic, copy) NSString* title;
@end

@interface FPEngine : NSObject
/** Loads PDFium; nil when it is ready, else why not. */
+ (nullable NSString*)start;

+ (FPDoc _Nullable)open:(NSString*)path password:(NSString*)password result:(int*)result;   // result: 0 opened, 4 needs a password, 5 wrong
+ (NSString*)describe:(int)result name:(NSString*)name;
+ (void)close:(FPDoc)doc;
+ (int)pageCount:(FPDoc)doc;
+ (CGSize)pageSize:(FPDoc)doc page:(int)page;        // points, as shown (turned pages included)

/** Part `part` (pixels) of the page drawn `full` pixels large; nil when it could not be drawn. */
+ (nullable CGImageRef)render:(FPDoc)doc page:(int)page part:(CGRect)part full:(CGSize)full colours:(FPColours)colours CF_RETURNS_RETAINED;

/** A page point to a point of the page drawn `size` large (and back). */
+ (CGPoint)pageToView:(FPDoc)doc page:(int)page size:(CGSize)size point:(CGPoint)p;
+ (CGPoint)viewToPage:(FPDoc)doc page:(int)page size:(CGSize)size point:(CGPoint)p;

// ---- text
+ (NSArray<NSValue*>*)selectionRects:(FPDoc)doc page:(int)page from:(CGPoint)a to:(CGPoint)b;   // CGRect: x left, y bottom, width, height
+ (NSString*)selectionText:(FPDoc)doc page:(int)page from:(CGPoint)a to:(CGPoint)b;
+ (NSArray<NSValue*>*)editableBlocks:(FPDoc)doc page:(int)page;   // CGRect boxes of what edit mode outlines
+ (nullable NSString*)paragraphAt:(FPDoc)doc page:(int)page point:(CGPoint)p info:(FPParagraphInfo*)info;
+ (nullable NSString*)replaceParagraph:(FPDoc)doc text:(NSString*)text family:(int)family bold:(BOOL)bold italic:(BOOL)italic size:(double)size color:(uint32_t)color;
+ (BOOL)addText:(FPDoc)doc page:(int)page point:(CGPoint)p text:(NSString*)text size:(double)size color:(uint32_t)color;

// ---- equations
+ (nullable NSString*)equationAt:(FPDoc)doc page:(int)page point:(CGPoint)p any:(BOOL)any size:(double*)size uncertain:(BOOL*)uncertain;
+ (nullable NSString*)replaceEquation:(FPDoc)doc source:(NSString*)source size:(double)size;   // nil when done, else why not
+ (nullable NSString*)insertEquation:(FPDoc)doc page:(int)page point:(CGPoint)p source:(NSString*)source size:(double)size;
+ (nullable UIImage*)equationPreview:(NSString*)source size:(double)size scale:(double)scale error:(NSString* _Nullable * _Nullable)error;
+ (NSString*)scriptText:(NSString*)text kind:(int)kind;   // x² (1) and x₂ (-1), or plain again

// ---- annotations
+ (BOOL)addMarkup:(FPDoc)doc page:(int)page kind:(FPMarkup)kind rects:(NSArray<NSValue*>*)rects color:(uint32_t)color;
+ (BOOL)addInk:(FPDoc)doc page:(int)page points:(NSArray<NSValue*>*)points color:(uint32_t)color width:(double)width;
+ (BOOL)addNote:(FPDoc)doc page:(int)page point:(CGPoint)p text:(NSString*)text;
+ (int)annotAt:(FPDoc)doc page:(int)page point:(CGPoint)p kind:(NSString* _Nullable * _Nullable)kind comment:(NSString* _Nullable * _Nullable)comment;
+ (BOOL)removeAnnot:(FPDoc)doc page:(int)page index:(int)index;
+ (BOOL)setAnnotText:(FPDoc)doc page:(int)page index:(int)index text:(NSString*)text;

// ---- finding, bookmarks, pages, history, saving
+ (NSArray<FPMatch*>*)find:(FPDoc)doc text:(NSString*)text;
+ (NSArray<FPBookmark*>*)bookmarks:(FPDoc)doc;
+ (BOOL)rotatePage:(FPDoc)doc page:(int)page;
+ (BOOL)deletePage:(FPDoc)doc page:(int)page;
+ (BOOL)undo:(FPDoc)doc;
+ (BOOL)redo:(FPDoc)doc;
+ (BOOL)canUndo:(FPDoc)doc;
+ (BOOL)canRedo:(FPDoc)doc;
+ (BOOL)isModified:(FPDoc)doc;
+ (nullable NSData*)bytesToSave:(FPDoc)doc;
+ (void)markSaved:(FPDoc)doc;

+ (NSString*)aboutCredits;
+ (NSString*)feedbackAddress;
@end

NS_ASSUME_NONNULL_END
