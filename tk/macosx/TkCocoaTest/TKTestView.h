//
//  TKTestView.h
//  TkCocoaTest
//
//  Created by Daniel A. Steffen on 12/09/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface TKTestView : NSView {
@private
#ifndef __LP64__
    NSColor *inactiveColor, *activeColor, *color;
#endif
}

- (id)initWithFrame:(NSRect)frame;
- (void)drawRect:(NSRect)rect;
- (BOOL)isOpaque;
- (void)mouseEntered:(NSEvent *)theEvent; 
- (void)mouseExited:(NSEvent *)theEvent;

@end
