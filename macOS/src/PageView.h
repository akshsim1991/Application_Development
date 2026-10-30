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
@end
