#pragma once
// The sidebar beside the pages, as on Windows (Ctrl+B there, ⌘B here): the page thumbnails, and the document's
// bookmarks. Clicking a thumbnail or a bookmark goes to its page; the thumbnail of the page shown is selected.
#import <Cocoa/Cocoa.h>
#include "document.hpp"
#include "pagecolours.hpp"

@protocol FPSidebarDelegate
- (void)sidebarGoToPage:(int)page;
@end

@interface FPSidebar : NSView
@property(nonatomic, weak) id<FPSidebarDelegate> delegate;
@property(nonatomic) pagecolours::Mode colours;
- (void)setDocument:(Document*)doc;
- (void)documentChanged;            // pages or bookmarks changed: everything is read again
- (void)setCurrentPage:(int)page;   // selects (and shows) its thumbnail
- (void)showBookmarks:(BOOL)bookmarks;
@end
