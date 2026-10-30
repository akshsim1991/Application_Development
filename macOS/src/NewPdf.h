#pragma once
// File > New PDF: from pictures, or from a text or Markdown file (PagesMore.mm). The new document opens unsaved.
#import <Cocoa/Cocoa.h>

@interface FPNewPdf : NSObject
+ (void)fromPictures;
+ (void)fromTextFile;
@end
