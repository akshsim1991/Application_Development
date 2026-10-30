// Measuring (Tools > Measure), as on Windows: a ruler (drag from one point to another), a protractor (click the corner,
// then a point on each side), rulers along the page edges, units (mm, cm, in, pt) and a drawing scale (a plan at 1:50
// measures in real sizes). Measurements stay over the page until cleared, or are kept as comments on the page.
#import <Cocoa/Cocoa.h>
#import "WindowController.h"
#import "PageView.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include "annot.hpp"
#include "platform.hpp"

namespace {
int units() { return static_cast<int>(std::clamp<NSInteger>([NSUserDefaults.standardUserDefaults integerForKey:@"measureUnits"], 0, 3)); }
int scaleIndex() { return static_cast<int>(std::clamp<NSInteger>([NSUserDefaults.standardUserDefaults integerForKey:@"measureScale"], 0, 9)); }
double scaleFactor() { static const double v[] = {1, 2, 5, 10, 20, 50, 100, 0.5, 0.2, 0.1}; return v[scaleIndex()]; }
double unitsPerPoint() { static const double v[] = {25.4 / 72, 2.54 / 72, 1.0 / 72, 1}; return v[units()]; }
const wchar_t* unitName() { static const wchar_t* v[] = {L"mm", L"cm", L"in", L"pt"}; return v[units()]; }
} // namespace

@interface FPWindowController (Measure)
@end

@implementation FPWindowController (Measure)

- (IBAction)chooseRuler:(id)sender { self.pageView.tool = self.pageView.tool == FPToolRuler ? FPToolNone : FPToolRuler; }
- (IBAction)chooseProtractor:(id)sender { self.pageView.tool = self.pageView.tool == FPToolProtractor ? FPToolNone : FPToolProtractor; }

- (IBAction)toggleRulers:(id)sender {
    self.pageView.rulerUnits = units();
    self.pageView.showRulers = !self.pageView.showRulers;
}

- (IBAction)setMeasureUnits:(NSMenuItem*)sender {
    [NSUserDefaults.standardUserDefaults setInteger:sender.tag forKey:@"measureUnits"];
    self.pageView.rulerUnits = units();
}

- (IBAction)setMeasureScale:(NSMenuItem*)sender { [NSUserDefaults.standardUserDefaults setInteger:sender.tag forKey:@"measureScale"]; }

- (IBAction)clearMeasurements:(id)sender { [self.pageView clearMeasures]; }

// Each measurement becomes a green drawing with a note holding its value.
- (IBAction)keepMeasurements:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    for (const FPMeasure& m : [self.pageView measures]) {
        std::vector<std::vector<annot::PointD>> strokes{{m.a, m.b}};
        if (m.angle) strokes.push_back({m.a, m.c});
        if (annot::addInk(*d, m.page, strokes, RGB(0x00, 0x90, 0x60), 1.5))
            annot::addNote(*d, m.page, {(m.a.x + m.b.x) / 2, (m.a.y + m.b.y) / 2}, m.text, RGB(0x66, 0xDD, 0x66));
    }
    [self.pageView clearMeasures];
    [self afterChange];
}

- (void)measureDrawn:(const FPToolEvent&)e {
    if (e.kind != FPToolEvent::Draw || e.pts.size() < 2) return;
    wchar_t t[64];
    FPMeasure m;
    m.page = e.page;
    if (self.pageView.tool == FPToolRuler) {
        const double dx = e.pts.back().x - e.pts.front().x, dy = e.pts.back().y - e.pts.front().y;
        const double v = std::hypot(dx, dy) * unitsPerPoint() * scaleFactor();
        std::swprintf(t, 64, units() == 1 || units() == 2 ? L"%.2f %ls" : L"%.1f %ls", v, unitName());
        m.a = e.pts.front();
        m.b = e.pts.back();
    } else {
        if (e.pts.size() < 3) return;
        const double ax = e.pts[1].x - e.pts[0].x, ay = e.pts[1].y - e.pts[0].y;
        const double bx = e.pts[2].x - e.pts[0].x, by = e.pts[2].y - e.pts[0].y;
        const double la = std::hypot(ax, ay), lb = std::hypot(bx, by);
        if (la < 0.5 || lb < 0.5) return;
        const double c = std::clamp((ax * bx + ay * by) / (la * lb), -1.0, 1.0);
        std::swprintf(t, 64, L"%.1f°", std::acos(c) * 180.0 / M_PI);
        m.a = e.pts[0];
        m.b = e.pts[1];
        m.c = e.pts[2];
        m.angle = true;
    }
    m.text = t;
    [self.pageView addMeasure:m];
}

// Called by validateMenuItem: (WindowController.mm) for the Measure items.
- (BOOL)validateMeasure:(NSMenuItem*)item {
    if (item.action == @selector(chooseRuler:)) item.state = self.pageView.tool == FPToolRuler;
    if (item.action == @selector(chooseProtractor:)) item.state = self.pageView.tool == FPToolProtractor;
    if (item.action == @selector(toggleRulers:)) item.state = self.pageView.showRulers;
    if (item.action == @selector(setMeasureUnits:)) item.state = item.tag == units();
    if (item.action == @selector(setMeasureScale:)) item.state = item.tag == scaleIndex();
    if (item.action == @selector(keepMeasurements:) || item.action == @selector(clearMeasurements:)) return ![self.pageView measures].empty();
    return YES;
}

@end
