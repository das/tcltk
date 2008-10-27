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
 * Declarations of functions used only in this file:
 */

static void TimerProc(CFRunLoopTimerRef timer, void *info);

/*
 * Static data used by several functions in this file:
 */

static CFRunLoopTimerRef runLoopTimer = NULL;
static int runLoopTimerEnabled = 0, inTrackingLoop = 0;

/*
 * CFTimeInterval to wait forever
 */

#define CF_TIMEINTERVAL_FOREVER 5.05e8

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

    if (!tsdPtr->currentEvent && !inTrackingLoop) {
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
    NSEvent *currentEvent = nil, *event;

    if (!(flags & TCL_WINDOW_EVENTS)) {
	return;
    }

    if (!inTrackingLoop) {
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
		TKLog(@"   event: %@", event);
		objc_clear_stack(0);
		TkMacOSXStartTclEventLoopTimer();
		[NSApp sendEvent:event];
		TkMacOSXStopTclEventLoopTimer();
		[NSApp afterEvent];
	    }
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
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXRunTclEventLoop --
 *
 *	Process a limited number of tcl events.
 *
 * Results:
 *	Returns 1 if events were handled and 0 otherwise.
 *
 * Side effects:
 *	Runs the Tcl event loop.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TkMacOSXRunTclEventLoop(void)
{
    int i = 4, result = 0;

    /* Avoid starving main event loop: process at most 4 events. */
    while(/*--i && */Tcl_ServiceAll()) {
	result = 1;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerProc --
 *
 *	This procedure is the carbon timer handler that runs the tcl
 *	event loop periodically.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Runs the Tcl event loop.
 *
 *----------------------------------------------------------------------
 */

static void
TimerProc(
    CFRunLoopTimerRef timer,
    void *info)
{
    if(runLoopTimerEnabled > 0 && TkMacOSXRunTclEventLoop()) {
//#ifdef TK_MAC_DEBUG_CARBON_EVENTS
	TkMacOSXDbgMsg("Processed tcl events from runloop timer");
//#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXStartTclEventLoopTimer --
 *
 *	This procedure installs (if necessary) and starts a carbon
 *	event timer that runs the tcl event loop periodically.
 *	It should be called whenever a nested carbon event loop might
 *	run by HIToolbox (e.g. during mouse tracking) to ensure that
 *	tcl events continue to be processed.
 *
 * Results:
 *	OS status code.
 *
 * Side effects:
 *	Carbon event timer is installed and started.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE OSStatus
TkMacOSXStartTclEventLoopTimer(void)
{
    OSStatus err = noErr;

    if (++runLoopTimerEnabled > 0) {
	if(!runLoopTimer) {
	    runLoopTimer = CFRunLoopTimerCreate(NULL,
		    CFAbsoluteTimeGetCurrent() + 5 * kEventDurationMillisecond,
		    5 * kEventDurationMillisecond, 0, 0, TimerProc, NULL);
	    if (runLoopTimer) {
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), runLoopTimer,
			kCFRunLoopCommonModes);
	    }
	} else {
	    CFRunLoopTimerSetNextFireDate(runLoopTimer,
		    CFAbsoluteTimeGetCurrent() + CF_TIMEINTERVAL_FOREVER);
	}
    }
    return err;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXStopTclEventLoopTimer --
 *
 *	This procedure stops the carbon event timer started by
 *	TkMacOSXStartTclEventLoopTimer().
 *
 * Results:
 *	OS status code.
 *
 * Side effects:
 *	Carbon event timer is stopped.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE OSStatus
TkMacOSXStopTclEventLoopTimer(void)
{
    OSStatus err = noErr;

    if (--runLoopTimerEnabled == 0) {
	if(runLoopTimer) {
	    CFRunLoopTimerSetNextFireDate(runLoopTimer,
		    CFAbsoluteTimeGetCurrent() + 5 * kEventDurationMillisecond);
	}
    }
    return err;
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
    static int previousServiceMode = TCL_SERVICE_NONE;

    if (tracking) {
	if (!inTrackingLoop++) {
	    previousServiceMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
	    TkMacOSXStartTclEventLoopTimer();
	}
//#ifdef TK_MAC_DEBUG_CARBON_EVENTS
	TkMacOSXDbgMsg("Entering tracking loop");
//#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
    } else {
	if (!--inTrackingLoop) {
	    TkMacOSXStopTclEventLoopTimer();
	    previousServiceMode = Tcl_SetServiceMode(previousServiceMode);
	}
//#ifdef TK_MAC_DEBUG_CARBON_EVENTS
	TkMacOSXDbgMsg("Exiting tracking loop");
//#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXReceiveAndDispatchEvent --
 *
 *	This receives a carbon event and sends it to the carbon event
 *	dispatcher.
 *
 * Results:
 *	Mac OS status
 *
 * Side effects:
 *	This receives and dispatches the next Carbon event.
 *
 *----------------------------------------------------------------------
 */
MODULE_SCOPE OSStatus
TkMacOSXReceiveAndDispatchEvent(void)
{
    static EventTargetRef targetRef = NULL;
    int numEventTypes = 0;
    const EventTypeSpec *eventTypes = NULL;
    EventRef eventRef;
    OSStatus err;
#ifdef HAVE_QUICKDRAW
    const EventTypeSpec trackingEventTypes[] = {
	{'dniw',		 kEventWindowUpdate},
	{kEventClassWindow,	 kEventWindowUpdate},
    };

    if (inTrackingLoop > 0) {
	eventTypes = trackingEventTypes;
	numEventTypes = GetEventTypeCount(trackingEventTypes);
    }
#endif

    /*
     * This is a poll, since we have already counted the events coming
     * into this routine, and are guaranteed to have one waiting.
     */

    err = ReceiveNextEvent(numEventTypes, eventTypes,
	    kEventDurationNoWait, true, &eventRef);
    if (err == noErr) {
#ifdef TK_MAC_DEBUG_CARBON_EVENTS
	UInt32 kind = GetEventKind(eventRef);

	if (kind != kEventMouseMoved && kind != kEventMouseDragged) {
	    TkMacOSXDbgMsg("Dispatching %s", TkMacOSXCarbonEventToAscii(eventRef));
	    TkMacOSXInitNamedDebugSymbol(HIToolbox, void, _DebugPrintEvent,
		    EventRef inEvent);
	    if (_DebugPrintEvent) {
		/* Carbon-internal event debugging (c.f. Technote 2124) */
		_DebugPrintEvent(eventRef);
	    }
	}
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
	if (!targetRef) {
	    targetRef = GetEventDispatcherTarget();
	}
	TkMacOSXStartTclEventLoopTimer();
	err = SendEventToEventTarget(eventRef, targetRef);
	TkMacOSXStopTclEventLoopTimer();
#ifdef TK_MAC_DEBUG_CARBON_EVENTS
	if (err != noErr && err != eventLoopTimedOutErr
		&& err != eventNotHandledErr) {
	    TkMacOSXDbgMsg("SendEventToEventTarget(%s) failed: %d",
		    TkMacOSXCarbonEventToAscii(eventRef), (int)err);
	}
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
	ReleaseEvent(eventRef);
    } else if (err != eventLoopTimedOutErr) {
	TkMacOSXDbgMsg("ReceiveNextEvent failed: %d", (int)err);
    }
    return err;
}
