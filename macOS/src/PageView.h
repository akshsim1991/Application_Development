#pragma once
// The pages of one document, one under the other, in a scroll view: drawn by PDFium at the screen's sharpness, zoomed
// with the trackpad (pinch), Command + / - / 0 or the toolbar, and shown in the reading colours.
#import <Cocoa/Cocoa.h>
#include "document.hpp"
#include "pagecolours.hpp"

extern NSNotificationName const FPPageViewChanged;   // the page shown or the zoom changed (the toolbar follows)

@interface FPPageView : NSView
@property(nonatomic) Document* document;              // not owned
@property(nonatomic, readonly) double zoom;           // 1.0 = 100 %: one PDF point is one screen point
@property(nonatomic) BOOL fitWidth;                    // the zoom follows the window's width
@property(nonatomic) pagecolours::Mode colours;

- (void)documentChanged;                               // pages added, removed or changed: lay out and draw again
- (int)currentPage;                                    // 0-based: the page at the top third of the window
- (void)showPage:(int)page;
- (void)setZoom:(double)zoom;                          // keeps the middle of the window on the same spot
- (void)zoomIn;
- (void)zoomOut;
- (void)fitPage;                                       // the whole page in the window

// Page layout, as on Windows: 0 one page at a time, 1 continuous, 2 two pages side by side (cover: the first alone).
@property(nonatomic) int layout;
@property(nonatomic) BOOL cover;

// Search: every match is tinted, the current one more strongly and scrolled into view.
- (void)setMatches:(std::vector<TextHit>)hits current:(int)current;

// The selected text (dragging over the text of a page), and Select All of the page shown.
- (NSString*)selectedText;
- (BOOL)hasSelection;

// Called when a link (or anything else) jumps to another page, with the page left (for Back).
@property(nonatomic, copy) void (^jumped)(int fromPage);
@end
