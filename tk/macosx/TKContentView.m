//
//  TKContentView.m
//  TkCocoaTest
//
//  Created by Daniel A. Steffen on 11/09/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#include "tkMacOSXPrivate.h"
#import <ApplicationServices/ApplicationServices.h>

#ifndef __OBJC_GC__
#error OBJC_GC required
#endif

#pragma mark TKContentView

@interface TKContentView(TKContentViewPrivate)
- (void)removeSubviews;
- (void)restoreSubviews;
@end

/*
 * Custom content view for Tk NSWindows, containing standard NSView subviews.
 * The goal is to emulate X11-style drawing in response to Expose events:
 * during the normal AppKit drawing cycle, we supress drawing of all subviews
 * and instead send Expose events about the subviews that would be redrawn.
 * Our Expose event handling then draws the subviews manually via their
 * -displayRectIgnoringOpacity:. Window flushing is suspended until all Expose
 * events for a given draw have been handled.
 */

@implementation TKContentView

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        emtpySubviews = [NSArray array];
	savedSubviews = nil;
	lockFocusLevel = 0;
    }
    return self;
}

/*
 * The only reliable way I have found to supress subview drawing without
 * affecting -getRectsBeingDrawn:count: or needing to override internal NSView
 * methods is to remove subviews after the content view's -lockFocus and
 * restore them before its -unlockFocus.
 */
- (BOOL)lockFocusIfCanDraw  {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    BOOL result = [super lockFocusIfCanDraw];
    if (result && !lockFocusLevel++) {
	[self removeSubviews];
    }
    return result;
}

- (void)unlockFocus {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    if (!--lockFocusLevel) {
	[self restoreSubviews];
    }
    return [super unlockFocus];
}

/*
 * For testing, draw content view background via NSCompositePlusDarker.
 * No content view drawing will likely be needed in Tk as we will always be
 * fully covered by an opaque Tk frame (painted with the toplevel bgcolor).
 */
- (void)drawRect:(NSRect)rect {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    /*const NSRect *rectsBeingDrawn;
    NSInteger rectsBeingDrawnCount;

    [self getRectsBeingDrawn:&rectsBeingDrawn count:&rectsBeingDrawnCount];
    [[NSColor redColor] setFill];
    NSRectFillListUsingOperation(rectsBeingDrawn, rectsBeingDrawnCount,
	    NSCompositePlusDarker);*/
}

- (BOOL)isOpaque {
    return YES;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)wantsDefaultClipping {
    return NO;
}


/*
 * Compute dirty region from -getRectsBeingDrawn:count: and send Expose events
 * for all subviews whose frame intersects that region.
 */
- (void)viewWillDraw {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    const NSRect *rectsBeingDrawn;
    NSInteger rectsBeingDrawnCount;
    HIMutableShapeRef drawShape = HIShapeCreateMutable();

    [self getRectsBeingDrawn:&rectsBeingDrawn count:&rectsBeingDrawnCount];
    while (rectsBeingDrawnCount--) {
	CGRect r = NSRectToCGRect(*rectsBeingDrawn++);
	HIShapeUnionWithRect(drawShape, &r);
    }
    if (TkMacOSXGenerateExposeEvents([self window], drawShape)) {
	 /* If any Expose events have been sent, disable window flushing until they
	 * have all been handled. */
	/*[[self window] disableFlushWindow];
	[[NSRunLoop mainRunLoop] performSelector:@selector(reenableFlush)
		target:self argument:nil order:1000
		modes:[NSArray arrayWithObjects:NSRunLoopCommonModes,
			NSEventTrackingRunLoopMode, nil]];*/
    }
    CFRelease(drawShape);
    [super viewWillDraw];
}

- (void)reenableFlush {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    NSWindow *window = [self window];
    if ([window isFlushWindowDisabled]) {
	[window enableFlushWindow];
	[window flushWindowIfNeeded];
    }
}

@end

