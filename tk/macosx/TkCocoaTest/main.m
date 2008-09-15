//
//  main.m
//  TkCocoaTest
//
//  Created by Daniel A. Steffen on 11/09/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "TKApplication.h"

#import <objc/objc-auto.h>

#ifndef __OBJC_GC__
#error OBJC_GC required
#endif

/*
 * Replicate the bits we need from NSApplicationMain (in Tk we will typically
 * have no main nib nor plist with NSPrincipalClass).
 * We will require SnowLeopard and -fobjc-gc-only for TkCocoa.
 */

int main(int argc, char *argv[])
{
    if (!objc_collecting_enabled()) {
	TKLog(@"OBJC_GC required");
	exit(1);
    }
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    [TKApplication sharedApplication];
    [NSApp setDelegate:NSApp];
    [pool drain];
    [NSApp run];
    [NSApp release];
    return 0;
}
