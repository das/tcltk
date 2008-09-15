//
//  TKApplication.m
//  TkCocoaTest
//
//  Created by Daniel A. Steffen on 11/09/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "TKApplication.h"
#import "TKContentView.h"
#import "TKTestView.h"

#import <objc/objc-auto.h>

#ifndef __OBJC_GC__
#error OBJC_GC required
#endif

#pragma mark TKApplication

@interface TKApplication(TKApplicationPrivate)
- (void)setupEventLoop;
- (void)afterEvent;
- (void)eventLoopException:(NSException *)theException;
@end

@interface TKApplication()
@property NSWindow *mainWindow;
@property BOOL useModal;
@end

@implementation TKApplication

@synthesize mainWindow, useModal;

/*
 * Setup test window, views, menus & bindings programmatically.
 */
- (void)applicationWillFinishLaunching:(NSNotification *)notification {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);

    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"TkCocoaTest"];
    [[mainMenu addItemWithTitle:@"TkCocoaTest" action:nil keyEquivalent:@""]
	    setSubmenu:appMenu];
    [appMenu addItemWithTitle:@"About TkCocoaTest"
	    action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit TkCocoaTest"
	    action:@selector(terminate:) keyEquivalent:@"q"];
    [self setMainMenu:mainMenu];

    NSRect frame = NSInsetRect([[NSScreen mainScreen] visibleFrame], 100, 100);
    frame.origin.y += frame.size.height;
    frame.size = NSMakeSize(260, 140);
    frame.origin.y -= frame.size.height;

    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
	    styleMask:NSTitledWindowMask|NSClosableWindowMask|
		    NSMiniaturizableWindowMask|NSResizableWindowMask
	    backing:NSBackingStoreBuffered defer:YES];
    [window setTitle:@"TkCocoaTest"];
    self.mainWindow = window;

    /* Custom content view that enables X11-style drawing */
    frame.origin = NSZeroPoint;
    TKContentView *contentView = [[TKContentView alloc]
	    initWithFrame:frame];
    [window setContentView:contentView];
    [window setContentMinSize:frame.size];

    NSButton *button = [[NSButton alloc]
	    initWithFrame:NSMakeRect(14, 12, 102, 32)];
    [button setButtonType:NSMomentaryLightButton];
    [button setBezelStyle:NSRoundedBezelStyle];
    [button setTitle:@"Alert"];
    [button setAction:@selector(showAlert:)];
    [contentView addSubview:button];
    [button setAutoresizingMask:NSViewMaxXMargin|NSViewMaxYMargin];

    NSButton *checkbox = [[NSButton alloc]
	    initWithFrame:NSMakeRect(34, 58, 61, 18)];
    [checkbox setButtonType:NSSwitchButton];
    [checkbox setTitle:@"Modal"];
    [checkbox bind:@"value" toObject:self withKeyPath:@"useModal" options:nil];
    [contentView addSubview:checkbox];
    [checkbox setAutoresizingMask:NSViewMaxXMargin|NSViewMaxYMargin];

    NSButton *xButton = [[NSButton alloc]
	    initWithFrame:NSMakeRect(14, 92, 102, 32)];
    [xButton setButtonType:NSMomentaryLightButton];
    [xButton setBezelStyle:NSRoundedBezelStyle];
    [xButton setTitle:@"Exception"];
    [xButton setAction:@selector(throwTestException:)];
    [contentView addSubview:xButton];
    [xButton setAutoresizingMask:NSViewMaxXMargin|NSViewMinYMargin];

    TKTestView *demoView =  [[TKTestView alloc]
	    initWithFrame:NSMakeRect(140, 20, 100, 100)];
    [contentView addSubview:demoView];
    [demoView setAutoresizingMask:NSViewMinXMargin|NSViewMaxYMargin];

    [window makeKeyAndOrderFront:self];
    [window makeMainWindow];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}

/*
 * Need to override -[NSApplication run] (resp. replicate its functionality at
 * different locations in Tk) as we have to integrate with the Tcl event loop
 * (which is CFRunLoop based, and may already be running when we are loaded).
 */
- (void)run {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    /* Replicate -[NSApplication run] event loop setup */
    [self setupEventLoop];
    while ([self isRunning]) {
	NS_DURING {
	    SInt32 runLoopStatus = 0;

	    [self setWindowsNeedUpdate:YES];
	    objc_collect_if_needed(OBJC_GENERATIONAL);
	    while ([self isRunning] && runLoopStatus != kCFRunLoopRunFinished) {
		/* Poll for events via AppKit */
		NSEvent *event = [self nextEventMatchingMask:NSAnyEventMask
			untilDate:[NSDate distantPast]
			inMode:NSDefaultRunLoopMode dequeue:YES];
		if (event) {
		    /* Dispatch events via AppKit */
		    TKLog(@"   event: %@", event);
		    objc_clear_stack(0);
		    [self sendEvent:event];
		    [self afterEvent];
		} else {
		    /* Wait for events via CFRunLoopRun */
		    runLoopStatus = CFRunLoopRunInMode(
			    kCFRunLoopDefaultMode, 1.0e10, TRUE);
		    TKLog(@"    rl status: %d", runLoopStatus);
		}
		objc_collect_if_needed(OBJC_GENERATIONAL);
	    }
	} NS_HANDLER {
	    /* App level exception handling will most likely not be needed
	     * in Tk, we cannot safely recover from most exceptions caught only
	     * at this level (e.g. when thrown from inside a Tcl evaluation) */
	    [self eventLoopException:localException];
	    [NSCursor unhide];
	    objc_collect_if_needed(OBJC_GENERATIONAL);
	} NS_ENDHANDLER
    }
}

/*
 * Test modal & modeless alerts (i.e. nested event loop)
 */
- (void)showAlert:(id)sender {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    NSAlert *alert = [[NSAlert alloc] init];
    [alert addButtonWithTitle:@"OK"];
    [alert setMessageText:@"Test Alert"];
    [alert setAlertStyle:NSInformationalAlertStyle];
    if (self.useModal) {
	[self alertDidEnd:alert returnCode:[alert runModal] contextInfo:nil];
    } else {
	[alert beginSheetModalForWindow:self.mainWindow modalDelegate:self
		didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
		contextInfo:nil];
    }
}

- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode
	contextInfo:(void *)contextInfo {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    if (returnCode != NSAlertFirstButtonReturn) {
	TKLog(@"    Alert return code: %d", returnCode);
    }
}

/*
 * Test exception handling
 */
- (void)throwTestException:(id)sender {
    TKLog(@"-[%@(%p) %s]", [self class], self, _cmd);
    @throw [NSException exceptionWithName:@"TestException"
	    reason:@"Test Exception" userInfo:nil];
}

@end
