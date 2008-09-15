//
//  TKAppDelegate.h
//  TkCocoaTest
//
//  Created by Daniel A. Steffen on 11/09/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface TKApplication : NSApplication {
@private
#ifndef __LP64__
    NSWindow *mainWindow;
    BOOL useModal;
#endif
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification;
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender;
- (void)run;
- (void)showAlert:(id)sender;
- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode
	contextInfo:(void *)contextInfo;
- (void)throwTestException:(id)sender;

@end
