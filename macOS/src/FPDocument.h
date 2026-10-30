#pragma once
// One open PDF: the shared engine's Document inside macOS's document system, which gives Open, Open Recent, one window
// (or tab) per file and reopening the windows of the last session.
#import <Cocoa/Cocoa.h>
#include <memory>
#include "document.hpp"

extern NSNotificationName const FPDocumentReloaded;   // the pages were read again (Revert): views take the new Document

@interface FPDocument : NSDocument
@property(nonatomic, readonly) Document* pdf;
- (void)changed;   // the PDF was changed (the window's "edited" dot, and the questions on closing)
@end
