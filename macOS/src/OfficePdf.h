// Word, Excel and PowerPoint files (and OpenDocument, RTF and Apple's Pages, Numbers and Keynote files) made into PDFs on
// the Mac, as Feather PDF for Windows does with the Office on the PC (Windows/src/officepdf.cpp). A Mac converts them
// only with a program that reads them; the first one found is used:
//   Microsoft Word, Excel or PowerPoint      (the closest result: the document as Office prints it)
//   LibreOffice                               (free; run without a window)
//   Pages, Numbers or Keynote                 (free from Apple; they open Word, Excel and PowerPoint files too)
//   the Mac's own text system                 (Word, RTF and OpenDocument text only: the text, its styles and pictures)
// Microsoft's and Apple's programs are asked through AppleScript: macOS asks once whether Feather PDF may control them.
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, FPOfficeKind) { FPOfficeNone, FPOfficeText, FPOfficeSheet, FPOfficeSlides };

@interface FPOfficePdf : NSObject
/** By the extension: a word processor's document, a spreadsheet, a presentation, or none of them. */
+ (FPOfficeKind)kindOf:(NSURL*)url;
/** Writes the PDF of `in` to `out`. YES when done (`program` says which made it); NO with the reason in `error`. */
+ (BOOL)convert:(NSURL*)in to:(NSURL*)out program:(NSString* _Nullable * _Nullable)program error:(NSString* _Nullable * _Nullable)error;
@end

NS_ASSUME_NONNULL_END
