/*
 * tkMacOSXCarbonEvents.c --
 *
 *	This file implements functions that register for and handle
 *	various Carbon Events and Timers. Most carbon events of interest
 *	to TkAqua are processed in a handler registered on the dispatcher
 *	event target so that we get first crack at them before HIToolbox
 *	dispatchers/processes them further.
 *	As some events are sent directly to the focus or app event target
 *	and not dispatched normally, we also register a handler on the
 *	application event target.
 *
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2005-2008 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *	The following terms apply to all files originating from Apple
 *	Computer, Inc. ("Apple") and associated with the software
 *	unless explicitly disclaimed in individual files.
 *
 *
 *	Apple hereby grants permission to use, copy, modify,
 *	distribute, and license this software and its documentation
 *	for any purpose, provided that existing copyright notices are
 *	retained in all copies and that this notice is included
 *	verbatim in any distributions. No written agreement, license,
 *	or royalty fee is required for any of the authorized
 *	uses. Modifications to this software may be copyrighted by
 *	their authors and need not follow the licensing terms
 *	described here, provided that the new terms are clearly
 *	indicated on the first page of each file where they apply.
 *
 *
 *	IN NO EVENT SHALL APPLE, THE AUTHORS OR DISTRIBUTORS OF THE
 *	SOFTWARE BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 *	INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF
 *	THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
 *	EVEN IF APPLE OR THE AUTHORS HAVE BEEN ADVISED OF THE
 *	POSSIBILITY OF SUCH DAMAGE.  APPLE, THE AUTHORS AND
 *	DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
 *	BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.	 THIS
 *	SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, AND APPLE,THE
 *	AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 *	MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 *	GOVERNMENT USE: If you are acquiring this software on behalf
 *	of the U.S. government, the Government shall have only
 *	"Restricted Rights" in the software and related documentation
 *	as defined in the Federal Acquisition Regulations (FARs) in
 *	Clause 52.227.19 (c) (2).  If you are acquiring the software
 *	on behalf of the Department of Defense, the software shall be
 *	classified as "Commercial Computer Software" and the
 *	Government shall have only "Restricted Rights" as defined in
 *	Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 *	foregoing, the authors grant the U.S. Government and others
 *	acting in its behalf permission to use and distribute the
 *	software in accordance with the terms specified in this
 *	license.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXEvent.h"
#include "tkMacOSXDebug.h"


#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_CARBON_EVENTS
#endif


/*
 * Declarations of functions used only in this file:
 */

static OSStatus CarbonEventHandlerProc(EventHandlerCallRef callRef,
	EventRef event, void *userData);
#ifdef OBSOLETE
static OSStatus InstallStandardApplicationEventHandler(void);
#endif

/*
 * Static data used by several functions in this file:
 */

static EventHandlerUPP carbonEventHandlerUPP = NULL;
static Tcl_Interp *carbonEventInterp = NULL;


/*
 *----------------------------------------------------------------------
 *
 * CarbonEventHandlerProc --
 *
 *	This procedure is the handler for all registered CarbonEvents.
 *
 * Results:
 *	OS status code.
 *
 * Side effects:
 *	Dispatches CarbonEvents.
 *
 *----------------------------------------------------------------------
 */

static OSStatus
CarbonEventHandlerProc(
    EventHandlerCallRef callRef,
    EventRef event,
    void *userData)
{
    OSStatus err = eventNotHandledErr;
    TkMacOSXEvent macEvent;
    MacEventStatus eventStatus;

    macEvent.eventRef = event;
    macEvent.eClass = GetEventClass(event);
    macEvent.eKind = GetEventKind(event);
    macEvent.interp = (Tcl_Interp *) userData;
    macEvent.callRef = callRef;
    bzero(&eventStatus, sizeof(eventStatus));

#ifdef TK_MAC_DEBUG_CARBON_EVENTS
    if (!(macEvent.eClass == kEventClassMouse && (
	    macEvent.eKind == kEventMouseMoved ||
	    macEvent.eKind == kEventMouseDragged))) {
/*
	TkMacOSXDbgMsg("Started handling %s",
		TkMacOSXCarbonEventToAscii(event));
*/
	TkMacOSXInitNamedDebugSymbol(HIToolbox, void, _DebugPrintEvent,
		EventRef inEvent);
	if (_DebugPrintEvent) {
	    /*
	     * Carbon-internal event debugging (c.f. Technote 2124)
	     */

	    _DebugPrintEvent(event);
	}
    }
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */

    TkMacOSXProcessEvent(&macEvent,&eventStatus);
    if (eventStatus.stopProcessing) {
	err = noErr;
    }

#ifdef TK_MAC_DEBUG_CARBON_EVENTS
    if (macEvent.eKind != kEventMouseMoved &&
	    macEvent.eKind != kEventMouseDragged) {
/*
	TkMacOSXDbgMsg("Finished handling %s: %s handled",
		TkMacOSXCarbonEventToAscii(event),
		eventStatus.stopProcessing ? "   " : "not");
*/
    }
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
    return err;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInitCarbonEvents --
 *
 *	This procedure initializes all CarbonEvent handlers.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Handlers for Carbon Events are registered.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void
TkMacOSXInitCarbonEvents(
    Tcl_Interp *interp)
{
    const EventTypeSpec dispatcherEventTypes[] = {
	{kEventClassKeyboard,	 kEventRawKeyDown},
	{kEventClassKeyboard,	 kEventRawKeyRepeat},
	{kEventClassKeyboard,	 kEventRawKeyUp},
	{kEventClassKeyboard,	 kEventRawKeyModifiersChanged},
	{kEventClassKeyboard,	 kEventRawKeyRepeat},
    };
    const EventTypeSpec applicationEventTypes[] = {
	{kEventClassMenu,	 kEventMenuBeginTracking},
	{kEventClassMenu,	 kEventMenuEndTracking},
	{kEventClassMenu,	 kEventMenuOpening},
	{kEventClassMenu,	 kEventMenuTargetItem},
	{kEventClassCommand,	 kEventCommandProcess},
	{kEventClassCommand,	 kEventCommandUpdateStatus},
	{kEventClassApplication, kEventAppActivated},
	{kEventClassApplication, kEventAppDeactivated},
	{kEventClassApplication, kEventAppQuit},
	{kEventClassApplication, kEventAppHidden},
	{kEventClassApplication, kEventAppShown},
	{kEventClassApplication, kEventAppAvailableWindowBoundsChanged},
	{kEventClassAppearance,	 kEventAppearanceScrollBarVariantChanged},
	{kEventClassFont,	 kEventFontPanelClosed},
	{kEventClassFont,	 kEventFontSelection},
    };

    carbonEventHandlerUPP = NewEventHandlerUPP(CarbonEventHandlerProc);
    carbonEventInterp = interp;
#ifdef OBSOLETE
    ChkErr(InstallStandardApplicationEventHandler);
#endif
    ChkErr(InstallEventHandler, GetEventDispatcherTarget(),
	    carbonEventHandlerUPP, GetEventTypeCount(dispatcherEventTypes),
	    dispatcherEventTypes, (void *) carbonEventInterp, NULL);
    ChkErr(InstallEventHandler, GetApplicationEventTarget(),
	    carbonEventHandlerUPP, GetEventTypeCount(applicationEventTypes),
	    applicationEventTypes, (void *) carbonEventInterp, NULL);

#ifdef TK_MAC_DEBUG_CARBON_EVENTS
    TkMacOSXInitNamedSymbol(HIToolbox, void, DebugTraceEvent, OSType, UInt32,
	    Boolean);
    if (DebugTraceEvent) {
	unsigned int i;
	const EventTypeSpec *e;

	for (i = 0, e = dispatcherEventTypes;
		i < GetEventTypeCount(dispatcherEventTypes); i++, e++) {
	    DebugTraceEvent(e->eventClass, e->eventKind, 1);
	}
	for (i = 0, e = applicationEventTypes;
		i < GetEventTypeCount(applicationEventTypes); i++, e++) {
	    DebugTraceEvent(e->eventClass, e->eventKind, 1);
	}
	DebugTraceEvent = NULL; /* Only enable tracing once. */
    }
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
}

#ifdef HAVE_QUICKDRAW
/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInstallWindowCarbonEventHandler --
 *
 *	This procedure installs our window CarbonEvent handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Handler for Carbon Events is registered.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void
TkMacOSXInstallWindowCarbonEventHandler(
	Tcl_Interp *interp, WindowRef window)
{
    const EventTypeSpec windowEventTypes[] = {
	{kEventClassMouse,	 kEventMouseDown},
	{kEventClassMouse,	 kEventMouseUp},
	{kEventClassMouse,	 kEventMouseMoved},
	{kEventClassMouse,	 kEventMouseDragged},
	{kEventClassMouse,	 kEventMouseWheelMoved},
	{kEventClassWindow,	 kEventWindowActivated},
	{kEventClassWindow,	 kEventWindowDeactivated},
	{kEventClassWindow,	 kEventWindowUpdate},
	{kEventClassWindow,	 kEventWindowExpanding},
	{kEventClassWindow,	 kEventWindowBoundsChanged},
	{kEventClassWindow,	 kEventWindowDragStarted},
	{kEventClassWindow,	 kEventWindowDragCompleted},
	{kEventClassWindow,	 kEventWindowConstrain},
	{kEventClassWindow,	 kEventWindowGetRegion},
	{kEventClassWindow,	 kEventWindowDrawContent},
    };

    ChkErr(InstallEventHandler, GetWindowEventTarget(window),
	    carbonEventHandlerUPP, GetEventTypeCount(windowEventTypes),
	    windowEventTypes, (void *) (interp ? interp : carbonEventInterp),
	    NULL);

#ifdef TK_MAC_DEBUG_CARBON_EVENTS
    TkMacOSXInitNamedSymbol(HIToolbox, void, DebugTraceEvent, OSType, UInt32,
	    Boolean);
    if (DebugTraceEvent) {
	unsigned int i;
	const EventTypeSpec *e;

	for (i = 0, e = windowEventTypes;
		i < GetEventTypeCount(windowEventTypes); i++, e++) {
	    if (!(e->eventClass == kEventClassMouse && (
		    e->eventKind == kEventMouseMoved ||
		    e->eventKind == kEventMouseDragged))) {
		DebugTraceEvent(e->eventClass, e->eventKind, 1);
	    }
	}
    }
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
}
#endif


#ifdef OBSOLETE
/*
 *----------------------------------------------------------------------
 *
 * InstallStandardApplicationEventHandler --
 *
 *	This procedure installs the carbon standard application event
 *	handler.
 *
 * Results:
 *	OS status code.
 *
 * Side effects:
 *	Standard handlers for application Carbon Events are registered.
 *
 *----------------------------------------------------------------------
 */

static OSStatus
InstallStandardApplicationEventHandler(void)
{
    OSStatus err = memFullErr;

   /*
    * While it is now possible on Leopard to install the standard app
    * handler with InstallStandardEventHandler(), to fully replicate RAEL
    * the standard menubar event handler also needs to be installed.
    * Unfortunately there appears to be no public API to obtain the menubar
    * event target. As a workaround, for now we resort to calling the
    * HIToolbox-internal GetMenuBarEventTarget() directly (symbol acquired
    * via TkMacOSXInitNamedSymbol() from HIToolbox version 343, may not
    * exist in later versions).
    */
    err = ChkErr(InstallStandardEventHandler, GetApplicationEventTarget());
    TkMacOSXInitNamedSymbol(HIToolbox, EventTargetRef,
	    GetMenuBarEventTarget, void);
    if (GetMenuBarEventTarget) {
	ChkErr(InstallStandardEventHandler, GetMenuBarEventTarget());
    } else {
	TkMacOSXDbgMsg("Unable to install standard menubar event handler");
    }
    return err;
}
#endif
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
