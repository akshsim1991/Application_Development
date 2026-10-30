#pragma once
// The sidebar beside the pages, as on Windows (Ctrl+B there, ⌘B here): the page thumbnails, and the document's
// bookmarks. Clicking a thumbnail or a bookmark goes to its page; the thumbnail of the page shown is selected.
// Thumbnails, as on Windows: several can be selected (⌘-click, ⇧-click), dragged to another place to move the pages,
// deleted with the Delete key, and right-clicked for the page menu (made by the delegate).
#import <Cocoa/Cocoa.h>
#include "document.hpp"
#include "pagecolours.hpp"

@protocol FPSidebarDelegate
- (void)sidebarGoToPage:(int)page;
@optional
- (void)sidebarFillMenu:(NSMenu*)menu pages:(NSArray<NSNumber*>*)pages;   // the right-click menu of these pages
- (void)sidebarMovePages:(NSArray<NSNumber*>*)pages before:(int)before;   // dragged: before = the page they go before
- (void)sidebarDeletePages:(NSArray<NSNumber*>*)pages;                    // the Delete key
@end

@interface FPSidebar : NSView
@property(nonatomic, weak) id<FPSidebarDelegate> delegate;
@property(nonatomic) pagecolours::Mode colours;
- (void)setDocument:(Document*)doc;
- (void)documentChanged;            // pages or bookmarks changed: everything is read again
- (void)setCurrentPage:(int)page;   // selects (and shows) its thumbnail
- (void)showBookmarks:(BOOL)bookmarks;
- (NSArray<NSNumber*>*)selectedPages;   // in page order
- (void)selectAllPages;
@end
