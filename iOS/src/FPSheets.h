// The editors shown over a document: changing a paragraph (with x², x₂ and the equation editor), and the equation
// editor (the equation in one line, buttons for the structures, signs and Greek letters, and a live picture of it).
#import <UIKit/UIKit.h>
#import "FPEngine.h"

NS_ASSUME_NONNULL_BEGIN

@interface FPEditSheet : UIViewController
- (instancetype)initWithText:(NSString*)text info:(FPParagraphInfo)info;
@property(nonatomic, copy, nullable) void (^apply)(NSString* text, double size, BOOL bold, BOOL italic);
@property(nonatomic, copy, nullable) void (^equation)(void);   // "Equation": the paragraph opened in the equation editor
@end

@interface FPEquationSheet : UIViewController
- (instancetype)initWithSource:(NSString*)source size:(double)size existing:(BOOL)existing;
@property(nonatomic, copy, nullable) void (^done)(NSString* source, double size, BOOL remove);
@end

NS_ASSUME_NONNULL_END
