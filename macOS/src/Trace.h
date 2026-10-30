#pragma once
// A trace of what happened, for the build check only: written to trace.txt in the FEATHER_UI_CHECK folder (nothing is
// written in normal use).
#import <Foundation/Foundation.h>

void FPTrace(NSString* format, ...) NS_FORMAT_FUNCTION(1, 2);
