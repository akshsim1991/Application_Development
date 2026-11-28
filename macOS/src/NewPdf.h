#pragma once
// File > New PDF: from pictures, or from a text or Markdown file (PagesMore.mm). The new document opens unsaved.
#import <Cocoa/Cocoa.h>

@interface FPNewPdf : NSObject
+ (void)fromPictures;
+ (void)fromTextFile;
// File > Combine Open PDFs: the open documents (those ticked; the key window's tabs first, in their order) as one new,
// unsaved PDF, with their unsaved changes.
+ (void)combineOpen;
// Convert to PDF (the Finder's Services / Quick Actions menu): each file made into a PDF next to it and opened.
+ (void)convertFiles:(NSArray<NSURL*>*)urls;
/** File > Convert to PDF: pictures, text and Markdown, Word, Excel and PowerPoint files chosen, each made a PDF next to it. */
+ (void)chooseAndConvert;
@end
