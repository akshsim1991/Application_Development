#pragma once
// The command palette (⌘K): every menu command, found by typing (Palette.mm).
#import <Cocoa/Cocoa.h>

void FPShowCommandPalette(NSWindow* forWindow);
// Feather PDF as the app that opens PDF files (Reading.mm).
void FPSetAsDefaultPdfApp(NSWindow* window);
