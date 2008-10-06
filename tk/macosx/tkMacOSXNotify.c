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
#include <pthread.h>
#import <objc/objc-auto.h>

/*
 * The following static indicates whether this module has been initialized
 * in the current thread.
 */

typedef struct ThreadSpecificData {
    int initialized;
    NSEvent *currentEvent;
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

static void TkMacOSXNotifyExitHandler(ClientData clientData);
static void TkMacOSXEventsSetupProc(ClientData clientData, int flags);
static void TkMacOSXEventsCheckProc(ClientData clientData, int flags);

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
	/* Install TkAqua event source in main event loop thread. */
	if (GetCurrentEventLoop() == GetMainEventLoop()) {
	    if (!pthread_main_np()) {
		/*
		 * Panic if the Carbon main event loop thread (i.e. the
		 * thread  where HIToolbox was first loaded) is not the
		 * main application thread, as Carbon does not support
		 * this properly.
		 */
		Tcl_Panic("Tk_MacOSXSetupTkNotifier: %s",
		    "first [load] of TkAqua has to occur in the main thread!");
	    }
	    Tcl_CreateEventSource(TkMacOSXEventsSetupProc,
		    TkMacOSXEventsCheckProc, GetMainEventQueue());
	    TkCreateExitHandler(TkMacOSXNotifyExitHandler, NULL);
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
TkMacOSXNotifyExitHandler(clientData)
    ClientData clientData;	/* Not used. */
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
 *	This procedure implements the setup part of the TkAqua Events
 *	event source. It is invoked by Tcl_DoOneEvent before entering
 *	the notifier to check for events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If TkAqua events are queued, then the maximum block time will be
 *	set to 0 to ensure that the notifier returns control to Tcl.
 *
 *----------------------------------------------------------------------
 */

static void
TkMacOSXEventsSetupProc(clientData, flags)
    ClientData clientData;
    int flags;
{
    static const Tcl_Time zeroBlockTime = { 0, 0 };
    ThreadSpecificData *tsdPtr = Tcl_GetThreadData(&dataKey,
	    sizeof(ThreadSpecificData));

    if (!(flags & TCL_WINDOW_EVENTS)) {
	return;
    }

    if (!tsdPtr->currentEvent) {
	NSEvent *currentEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
		untilDate:[NSDate distantPast]
		inMode:NSDefaultRunLoopMode dequeue:YES];
	if (currentEvent) {
	    CFRetain(currentEvent);
	    tsdPtr->currentEvent = currentEvent;
	}
    }

    if (tsdPtr->currentEvent || GetNumEventsInQueue((EventQueueRef)clientData)) {
	Tcl_SetMaxBlockTime(&zeroBlockTime);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXEventsCheckProc --
 *
 *	This procedure processes events sitting in the TkAqua event
 *	queue.
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
TkMacOSXEventsCheckProc(clientData, flags)
    ClientData clientData;
    int flags;
{
    ThreadSpecificData *tsdPtr = Tcl_GetThreadData(&dataKey,
	    sizeof(ThreadSpecificData));
    int numFound;
    OSStatus err = noErr;
    NSEvent *currentEvent = nil;

    if (!(flags & TCL_WINDOW_EVENTS)) {
	return;
    }

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
	if (currentEvent) {
	    TKLog(@"   event: %@", currentEvent);
	    objc_clear_stack(0);
	    [NSApp sendEvent:currentEvent];
	    [NSApp afterEvent];
	}  
	objc_collect_if_needed(OBJC_GENERATIONAL);
    } while (currentEvent);
    
    numFound = GetNumEventsInQueue((EventQueueRef)clientData);

    /* Avoid starving other event sources: */
    if (numFound > 4) {
	numFound = 4;
    }
    while (numFound > 0 && err == noErr) {
	err = TkMacOSXReceiveAndDispatchEvent();
	numFound--;
    }
}
