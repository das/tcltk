/*
 * tkMacOSXNotify.c --
 *
 *	This file contains the implementation of a tcl event source
 *	for the AppKit & Carbon event loops.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2005-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXEvent.h"
#include <tclInt.h>
#include <pthread.h>
#import <objc/objc-auto.h>

typedef struct ThreadSpecificData {
    int initialized, inTrackingLoop, previousServiceMode;
    NSEvent *currentEvent;
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

static void TkMacOSXNotifyExitHandler(ClientData clientData);
static void TkMacOSXEventsSetupProc(ClientData clientData, int flags);
static void TkMacOSXEventsCheckProc(ClientData clientData, int flags);

#pragma mark TKApplication(TKNotify)

@interface TKApplication(TKNotify)
- (NSEvent *)nextEventMatchingMask:(NSUInteger)mask untilDate:(NSDate *)expiration inMode:(NSString *)mode dequeue:(BOOL)deqFlag;
@end

@implementation TKApplication(TKNotify)
- (NSEvent *)nextEventMatchingMask:(NSUInteger)mask untilDate:(NSDate *)expiration inMode:(NSString *)mode dequeue:(BOOL)deqFlag {
    NSEvent *event = [super nextEventMatchingMask:mask untilDate:expiration
	    inMode:mode dequeue:deqFlag];

    if (event) {
	ThreadSpecificData *tsdPtr = Tcl_GetThreadData(&dataKey,
		sizeof(ThreadSpecificData));

	if (tsdPtr->inTrackingLoop) {
	    event = [NSApp tkProcessEvent:event];
	}
    }
    return event;
}
@end
#pragma mark -


/*
 *----------------------------------------------------------------------
 *
 * Tk_MacOSXSetupTkNotifier --
 *
 *	This procedure is called during Tk initialization to create
 *	the event source for TkAqua events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new event source is created.
 *
 *----------------------------------------------------------------------
 */

void
Tk_MacOSXSetupTkNotifier(void)
{
    ThreadSpecificData *tsdPtr = Tcl_GetThreadData(&dataKey,
	    sizeof(ThreadSpecificData));

    if (!tsdPtr->initialized) {
	tsdPtr->initialized = 1;

	/*
	 * Install TkAqua event source in main event loop thread.
	 */

	if (CFRunLoopGetMain() == CFRunLoopGetCurrent()) {
	    if (!pthread_main_np()) {
		/*
		 * Panic if main runloop is not on the main application thread.
		 */

		Tcl_Panic("Tk_MacOSXSetupTkNotifier: %s",
		    "first [load] of TkAqua has to occur in the main thread!");
	    }
	    Tcl_CreateEventSource(TkMacOSXEventsSetupProc,
		    TkMacOSXEventsCheckProc, GetMainEventQueue());
	    TkCreateExitHandler(TkMacOSXNotifyExitHandler, NULL);
	    Tcl_SetServiceMode(TCL_SERVICE_ALL);
	    TclMacOSXNotifierAddRunLoopMode(NSEventTrackingRunLoopMode);
	    TclMacOSXNotifierAddRunLoopMode(NSModalPanelRunLoopMode);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXNotifyExitHandler --
 *
 *	This function is called during finalization to clean up the
 *	TkMacOSXNotify module.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
TkMacOSXNotifyExitHandler(
    ClientData clientData)	/* Not used. */
{
    ThreadSpecificData *tsdPtr = Tcl_GetThreadData(&dataKey,
	    sizeof(ThreadSpecificData));

    Tcl_DeleteEventSource(TkMacOSXEventsSetupProc,
	    TkMacOSXEventsCheckProc, GetMainEventQueue());
    tsdPtr->initialized = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXEventsSetupProc --
 *
 *	This procedure implements the setup part of the TkAqua Events event
 *	source. It is invoked by Tcl_DoOneEvent before entering the notifier
 *	to check for events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If TkAqua events are queued, then the maximum block time will be set
 *	to 0 to ensure that the notifier returns control to Tcl.
 *
 *----------------------------------------------------------------------
 */

static void
TkMacOSXEventsSetupProc(
    ClientData clientData,
    int flags)
{
    ThreadSpecificData *tsdPtr = Tcl_GetThreadData(&dataKey,
	    sizeof(ThreadSpecificData));

    if (!(flags & TCL_WINDOW_EVENTS) || tsdPtr->inTrackingLoop) {
	return;
    } else {
	static const Tcl_Time zeroBlockTime = { 0, 0 };

	if (!tsdPtr->currentEvent) {
	    NSEvent *currentEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
		    untilDate:[NSDate distantPast]
		    inMode:NSDefaultRunLoopMode dequeue:YES];
	    if (currentEvent) {
		CFRetain(currentEvent);
		tsdPtr->currentEvent = currentEvent;
	    }
	}
	if (tsdPtr->currentEvent) {
	    Tcl_SetMaxBlockTime(&zeroBlockTime);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXEventsCheckProc --
 *
 *	This procedure processes events sitting in the TkAqua event queue.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Moves applicable queued TkAqua events onto the Tcl event queue.
 *
 *----------------------------------------------------------------------
 */

static void
TkMacOSXEventsCheckProc(
    ClientData clientData,
    int flags)
{
    ThreadSpecificData *tsdPtr = Tcl_GetThreadData(&dataKey,
	    sizeof(ThreadSpecificData));

    if (!(flags & TCL_WINDOW_EVENTS) ||  tsdPtr->inTrackingLoop) {
	return;
    } else {
	NSEvent *currentEvent = nil, *event;

 	do {
	    if (tsdPtr->currentEvent) {
		currentEvent = tsdPtr->currentEvent;
		CFRelease(currentEvent);
		tsdPtr->currentEvent = nil;
	    } else {
		currentEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
			untilDate:[NSDate distantPast]
			inMode:NSDefaultRunLoopMode dequeue:YES];
	    }
	    event = currentEvent ? [NSApp tkProcessEvent:currentEvent] : nil;
	    if (event) {
#ifdef TK_MAC_DEBUG_EVENTS
		TKLog(@"   event: %@", event);
#endif
		objc_clear_stack(0);
		TkMacOSXTrackingLoop(1);
		[NSApp sendEvent:event];
		TkMacOSXTrackingLoop(0);
		objc_collect(OBJC_COLLECT_IF_NEEDED);
	    }
	} while (currentEvent);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXTrackingLoop --
 *
 *	Call with 1 before entering a mouse tracking loop (e.g. window
 *	resizing or menu tracking) to enable tcl event processing but
 *	disable  carbon event processing (except for update events)
 *	during the loop, and with 0 after exiting the loop to reset.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void
TkMacOSXTrackingLoop(int tracking)
{
    ThreadSpecificData *tsdPtr = Tcl_GetThreadData(&dataKey,
	    sizeof(ThreadSpecificData));

    if (tracking) {
	if (!tsdPtr->inTrackingLoop++) {
	    tsdPtr->previousServiceMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
	}
#ifdef TK_MAC_DEBUG_CARBON_EVENTS
	TkMacOSXDbgMsg("Entering tracking loop");
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
    } else {
	if (!--tsdPtr->inTrackingLoop) {
	    tsdPtr->previousServiceMode =
		    Tcl_SetServiceMode(tsdPtr->previousServiceMode);
	}
#ifdef TK_MAC_DEBUG_CARBON_EVENTS
	TkMacOSXDbgMsg("Exiting tracking loop");
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
