//
//  TKContentView.h
//  TkCocoaTest
//
//  Created by Daniel A. Steffen on 11/09/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface TKContentView : NSView {
@private
    NSArray *emtpySubviews, *savedSubviews;
    NSInteger lockFocusLevel;
}

- (id)initWithFrame:(NSRect)frame;
- (BOOL)lockFocusIfCanDraw;
- (void)unlockFocus;
- (void)drawRect:(NSRect)rect;
- (BOOL)isOpaque;
- (void)viewWillDraw;
- (void)reenableFlush;
- (void)generateExposeEvent:(NSView*)subview rect:(NSRect)exposedRect
	order:(NSInteger)order;
- (void)handleExposeEvent:(id)expose;

@end
