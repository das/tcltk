//
//  TKTestView.m
//  TkCocoaTest
//
//  Created by Daniel A. Steffen on 12/09/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "TKTestView.h"

#ifndef __OBJC_GC__
#error OBJC_GC required
#endif

#pragma mark TKTestView

@interface TKTestView()
@property(assign) NSColor *inactiveColor, *activeColor, *color;
@end

/*
 * Very simple custom view to test tracking areas.
 */

@implementation TKTestView

@synthesize inactiveColor, activeColor, color;

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
	[self addTrackingArea:[[NSTrackingArea alloc] initWithRect:[self bounds]
            options:NSTrackingMouseEnteredAndExited|NSTrackingActiveInActiveApp|
		    NSTrackingInVisibleRect|NSTrackingEnabledDuringMouseDrag
            owner:self userInfo:nil]];
	self.inactiveColor = [NSColor blueColor];
	self.activeColor = [NSColor greenColor];
	self.color = self.inactiveColor;
    }
    return self;
}

- (void)drawRect:(NSRect)rect {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    const NSRect *rectsBeingDrawn;
    NSInteger rectsBeingDrawnCount;

    [self getRectsBeingDrawn:&rectsBeingDrawn count:&rectsBeingDrawnCount];
    [self.color setFill];
    NSRectFillListUsingOperation(rectsBeingDrawn, rectsBeingDrawnCount,
	    NSCompositeCopy);
}

- (BOOL)isOpaque {
    return YES;
}

- (void)mouseEntered:(NSEvent *)theEvent {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    self.color = self.activeColor;
    [self setNeedsDisplay:YES];
    //[self displayIfNeeded];
}

- (void)mouseExited:(NSEvent *)theEvent {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    self.color = self.inactiveColor;
    [self setNeedsDisplay:YES];
    //[self displayIfNeeded];
}

@end
