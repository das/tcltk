//
//  TKContentView.m
//  TkCocoaTest
//
//  Created by Daniel A. Steffen on 11/09/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "TKContentView.h"
#import <ApplicationServices/ApplicationServices.h>

#ifndef __OBJC_GC__
#error OBJC_GC required
#endif

#pragma mark TKExposeEvent

/*
 * Minimal fake X11 Expose event that can be "sent" via
 * -performSelector:target:argument:order:modes:
 */

@interface TKExposeEvent : NSObject {
@private
#ifndef __LP64__
    NSView *view;
    NSRect rect;
#endif
}
@property NSView *view;
@property NSRect rect;
@end

@implementation TKExposeEvent
@synthesize view, rect;
@end

#pragma mark -
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
    const NSRect *rectsBeingDrawn;
    NSInteger rectsBeingDrawnCount;

    [self getRectsBeingDrawn:&rectsBeingDrawn count:&rectsBeingDrawnCount];
    [[NSColor redColor] setFill];
    NSRectFillListUsingOperation(rectsBeingDrawn, rectsBeingDrawnCount,
	    NSCompositePlusDarker);
}

- (BOOL)isOpaque {
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
    NSInteger order = 0;

    [self getRectsBeingDrawn:&rectsBeingDrawn count:&rectsBeingDrawnCount];
    if (rectsBeingDrawnCount > 1) {
	/* Use HIShapes for region computation, that's what the existing
	 * codebase in Tk uses already. */
	HIMutableShapeRef drawShape = HIShapeCreateMutable();

	while (rectsBeingDrawnCount--) {
	    CGRect r = NSRectToCGRect(*rectsBeingDrawn++);
	    HIShapeUnionWithRect(drawShape, &r);
	}
	for (NSView *subview in [self subviews]) {
	    CGRect frame = NSRectToCGRect([subview frame]);

	    if (HIShapeIntersectsRect(drawShape, &frame)) {
		HIShapeRef frameShape = HIShapeCreateWithRect(&frame);
		HIShapeRef drawFrameShape = HIShapeCreateIntersection(
			drawShape, frameShape);

		HIShapeGetBounds(drawFrameShape, &frame);
		[self generateExposeEvent:subview rect:NSRectFromCGRect(frame)
			order:order++];
		CFRelease(drawFrameShape);
		CFRelease(frameShape);
	    }
	}
	CFRelease(drawShape);
    } else if (rectsBeingDrawnCount == 1) {
	for (NSView *subview in [self subviews]) {
	    NSRect frame = [subview frame];

	    if (NSIntersectsRect(frame, *rectsBeingDrawn)) {
		[self generateExposeEvent:subview
			rect:NSIntersectionRect(frame, *rectsBeingDrawn)
			order:order++];
	    }
	}
    }
    /* If any Expose events have been sent, disable window flushing until they
     * have all been handled. */
    if (order) {
	[[self window] disableFlushWindow];
	[[NSRunLoop mainRunLoop] performSelector:@selector(reenableFlush)
		target:self argument:nil order:order
		modes:[NSArray arrayWithObjects:NSRunLoopCommonModes,
			NSEventTrackingRunLoopMode, nil]];
    }
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

/*
 * "Send" fake Expose event via -performSelector:target:argument:order:modes:
 * In Tk this will be handled by posting a Tcl event, with similar end effect
 * given that the Tcl event loop is CFRunLoop based.
 */
- (void)generateExposeEvent:(NSView*)view rect:(NSRect)rect
	order:(NSInteger)order {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    TKLog(@"    view:%@(%p) rect:%@", [view class], view,
	    NSStringFromRect(rect));
    TKExposeEvent *expose = [[TKExposeEvent alloc] init];
    expose.view = view;
    expose.rect = [self convertRect:rect toView:view];
    [[NSRunLoop mainRunLoop] performSelector:@selector(handleExposeEvent:)
	    target:self argument:expose order:order
	    modes:[NSArray arrayWithObjects:NSRunLoopCommonModes,
		    NSEventTrackingRunLoopMode, nil]];
}

/*
 * Handle fake Expose event by redrawing view via -displayRectIgnoringOpacity:
 */
- (void)handleExposeEvent:(id)expose {
    NSView *view = ((TKExposeEvent*)expose).view;
    NSRect rect = ((TKExposeEvent*)expose).rect;
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    TKLog(@"    view:%@(%p) rect:%@", [view class], view,
	    NSStringFromRect(rect));
    [view displayRectIgnoringOpacity:rect];
}

@end

#pragma mark -
#pragma mark Disabled

/*
 * Abandoned attemps at bypassing normal subview drawing by overriding
 * various NSView methods.
 */

#if 0

@implementation TKContentView(disabled)

- (void)viewWillDraw {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    for (NSView *subview in [self subviews]) {
	[subview setNeedsDisplay:NO];
    }
    [[self window] setViewsNeedDisplay:NO];
    [super viewWillDraw];
}

- (BOOL)needsDisplay {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    for (NSView *v in [self subviews]) {
	[v setNeedsDisplay:NO];
    }
    return [super needsDisplay];
}

- (BOOL)canDraw {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    //return NO;
    return [super canDraw];
}

- (NSRect)visibleRect {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    //return NSZeroRect;
    return [super visibleRect];
}

- (void)displayIfNeeded {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    const NSRect *rectsBeingDrawn;
    NSInteger rectsBeingDrawnCount;
    [self getRectsBeingDrawn:&rectsBeingDrawn count:&rectsBeingDrawnCount];
    [super displayIfNeeded];
}

- (void)displayIfNeededInRect:(NSRect)rect {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    const NSRect *rectsBeingDrawn;
    NSInteger rectsBeingDrawnCount;
    [self getRectsBeingDrawn:&rectsBeingDrawn count:&rectsBeingDrawnCount];
    [super displayIfNeededInRect:rect];
}

- (void)displayIfNeededIgnoringOpacity {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    const NSRect *rectsBeingDrawn;
    NSInteger rectsBeingDrawnCount;
    [self getRectsBeingDrawn:&rectsBeingDrawn count:&rectsBeingDrawnCount];
    [super displayIfNeededIgnoringOpacity];
}

- (void)displayIfNeededInRectIgnoringOpacity:(NSRect)rect {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    const NSRect *rectsBeingDrawn;
    NSInteger rectsBeingDrawnCount;
    [self getRectsBeingDrawn:&rectsBeingDrawn count:&rectsBeingDrawnCount];
    [super displayIfNeededInRectIgnoringOpacity:rect];
}

@end

#endif
