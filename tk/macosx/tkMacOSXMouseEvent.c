/*
 * tkMacOSXMouseEvent.c --
 *
 *	This file implements functions that decode & handle mouse events on
 *	MacOS X.
 *
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2005-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *	The following terms apply to all files originating from Apple
 *	Computer, Inc. ("Apple") and associated with the software unless
 *	explicitly disclaimed in individual files.
 *
 *	Apple hereby grants permission to use, copy, modify, distribute, and
 *	license this software and its documentation for any purpose, provided
 *	that existing copyright notices are retained in all copies and that
 *	this notice is included verbatim in any distributions. No written
 *	agreement, license, or royalty fee is required for any of the
 *	authorized uses. Modifications to this software may be copyrighted by
 *	their authors and need not follow the licensing terms described here,
 *	provided that the new terms are clearly indicated on the first page of
 *	each file where they apply.
 *
 *	IN NO EVENT SHALL APPLE, THE AUTHORS OR DISTRIBUTORS OF THE SOFTWARE
 *	BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
 *	CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS
 *	DOCUMENTATION, OR ANY DERIVATIVES THEREOF, EVEN IF APPLE OR THE
 *	AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. APPLE,
 *	THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 *	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 *	NON-INFRINGEMENT. THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, AND
 *	APPLE,THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 *	MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 *	GOVERNMENT USE: If you are acquiring this software on behalf of the
 *	U.S. government, the Government shall have only "Restricted Rights" in
 *	the software and related documentation as defined in the Federal
 *	Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2). If you are
 *	acquiring the software on behalf of the Department of Defense, the
 *	software shall be classified as "Commercial Computer Software" and the
 *	Government shall have only "Restricted Rights" as defined in Clause
 *	252.227-7013 (c) (1) of DFARs. Notwithstanding the foregoing, the
 *	authors grant the U.S. Government and others acting in its behalf
 *	permission to use and distribute the software in accordance with the
 *	terms specified in this license.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXWm.h"
#include "tkMacOSXEvent.h"
#include "tkMacOSXDebug.h"

typedef struct {
    unsigned int state;
    long delta;
    Window window;
    Point global;
    Point local;
} MouseEventData;

static int		GenerateButtonEvent(MouseEventData *medPtr);
static unsigned int	ButtonModifiers2State(UInt32 buttonState,
			    UInt32 keyModifiers);

#pragma mark TKApplication(TKEvent)

enum {
    NSWindowWillMoveEventType = 20
};

@implementation TKApplication(TKMouseEvent)
- (NSEvent *)tkProcessMouseEvent:(NSEvent *)theEvent {
#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
#endif
    id		    win;
    NSEventType	    type = [theEvent type];
    NSTrackingArea  *trackingArea = nil;
    NSInteger eventNumber, clickCount, buttonNumber;

    switch (type) {
    case NSMouseEntered:
    case NSMouseExited:
    case NSCursorUpdate:
	trackingArea = [theEvent trackingArea];
	/* fall through */
    case NSLeftMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseDown:
    case NSRightMouseUp:
    case NSOtherMouseDown:
    case NSOtherMouseUp:

    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:

    case NSMouseMoved:
	eventNumber = [theEvent eventNumber];
	if (!trackingArea) {
	    clickCount = [theEvent clickCount];
	    buttonNumber = [theEvent buttonNumber];
	}

    case NSTabletPoint:
    case NSTabletProximity:

    case NSScrollWheel:
	win = [self windowWithWindowNumber:[theEvent windowNumber]];
        break;

    default:
	return theEvent;
	break;
    }

    NSPoint global, local = [theEvent locationInWindow];
    if (win) {
	global = [win convertBaseToScreen:local];
	local.y = [win frame].size.height - local.y;
	global.y = tkMacOSXZeroScreenHeight - global.y;
    } else {
	local.y = tkMacOSXZeroScreenHeight - local.y;
	global = local;
    }

    Window window = TkMacOSXGetXWindow(win);
    TkDisplay *dispPtr = TkGetDisplayList();
    Tk_Window tkwin = Tk_IdToWindow(dispPtr->display, window);
    if (!tkwin) {
	tkwin = TkMacOSXGetCapture();
    }
    if (!tkwin) {
	return theEvent;
    }

    /*
    MacDrawable *macWin = (MacDrawable *) window;
    NSView *view = macWin->toplevel ? macWin->toplevel->view : macWin->view;
    local = [view convertPoint:local fromView:nil];
    local.y = NSHeight([view bounds]) - local.y;
    */
    TkWindow  *winPtr = (TkWindow *) tkwin;
    local.x -= winPtr->wmInfoPtr->xInParent;
    local.y -= winPtr->wmInfoPtr->yInParent;

    int win_x, win_y;
    tkwin = Tk_TopCoordsToWindow(tkwin, local.x, local.y,
		&win_x, &win_y);
 
    unsigned int state = 0;
    NSInteger button = [theEvent buttonNumber];
    EventRef eventRef = (EventRef)[theEvent eventRef];
    UInt32 buttons;
    OSStatus err = GetEventParameter(eventRef, kEventParamMouseChord,
	    typeUInt32, NULL, sizeof(UInt32), NULL, &buttons);
    if (err == noErr) {
	state |= (buttons & ((1<<5) - 1)) << 8;
    } else {
	if (button < 5) {
	    switch (type) {
	    case NSLeftMouseDown:
	    case NSRightMouseDown:
	    case NSLeftMouseDragged:
	    case NSRightMouseDragged:
	    case NSOtherMouseDown:
		state |= 1 << (button + 8);
		break;
	    default:
		break;
	    }
	}
    }
    NSUInteger modifiers = [theEvent modifierFlags];

    if (modifiers & NSAlphaShiftKeyMask) {
	state |= LockMask;
    }
    if (modifiers & NSShiftKeyMask) {
	state |= ShiftMask;
    }
    if (modifiers & NSControlKeyMask) {
	state |= ControlMask;
    }
    if (modifiers & NSCommandKeyMask) {
	state |= Mod1Mask;		/* command key */
    }
    if (modifiers & NSAlternateKeyMask) {
	state |= Mod2Mask;		/* option key */
    }
    if (modifiers & NSNumericPadKeyMask) {
	state |= Mod3Mask;
    }
    if (modifiers & NSFunctionKeyMask) {
	state |= Mod4Mask;
    }

    if (type != NSScrollWheel) {
#ifdef TK_MAC_DEBUG_EVENTS
	TKLog(@"UpdatePointer %p x %f.0 y %f.0 %d", tkwin, global.x, global.y, state);
#endif
	Tk_UpdatePointer(tkwin, global.x, global.y, state);
    } else {
	CGFloat delta;
	int coarseDelta;
	XEvent xEvent;

	xEvent.type = MouseWheelEvent;
	xEvent.xbutton.x = local.x;
	xEvent.xbutton.y = local.y;
	xEvent.xbutton.x_root = global.x;
	xEvent.xbutton.y_root = global.y;
	xEvent.xany.send_event = false;
	xEvent.xany.display = Tk_Display(tkwin);
	xEvent.xany.window = Tk_WindowId(tkwin);

	delta = [theEvent deltaY];
	if (delta != 0.0) {
	    coarseDelta = (delta > -1.0 && delta < 1.0) ? (signbit(delta) ? -1 : 1) : lround(delta);
	    xEvent.xbutton.state = state;
	    xEvent.xkey.keycode = coarseDelta;
	    xEvent.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
	    Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
	}
	delta = [theEvent deltaX];
	if (delta != 0.0) {
	    coarseDelta = (delta > -1.0 && delta < 1.0) ? (signbit(delta) ? -1 : 1) : lround(delta);
	    xEvent.xbutton.state = state | ShiftMask;
	    xEvent.xkey.keycode = coarseDelta;
	    xEvent.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
	    Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
	}
    }

    return theEvent;
}
@end
#pragma mark -


#ifdef MAC_OSX_TK_TODO
/*
 *----------------------------------------------------------------------
 *
 * GetGrabWindowForWindow --
 *
 *	Get the grab window for the given window, if any.
 *
 * Results:
 *	Grab Tk_Window or None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
GetGrabWindowForWindow(
    Tk_Window tkwin) 
{
    Tk_Window grabWin = TkMacOSXGetCapture();

    if (!grabWin) {
	int grabState = TkGrabState((TkWindow *) tkwin);

	if (grabState != TK_GRAB_NONE && grabState != TK_GRAB_IN_TREE) {
	    grabWin = (Tk_Window) (((TkWindow *) tkwin)->dispPtr->grabWinPtr);
	}
    }

    return grabWin;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXKeyModifiers --
 *
 *	Returns the current state of the modifier keys.
 *
 * Results:
 *	An OS Modifier state.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

EventModifiers
TkMacOSXModifierState(void)
{
    UInt32 keyModifiers;
    int isFrontProcess = (GetCurrentEvent() && Tk_MacOSXIsAppInFront());

    keyModifiers = isFrontProcess ? GetCurrentEventKeyModifiers() :
	    GetCurrentKeyModifiers();

    return (EventModifiers) (keyModifiers & USHRT_MAX);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXButtonKeyState --
 *
 *	Returns the current state of the button & modifier keys.
 *
 * Results:
 *	A bitwise inclusive OR of a subset of the following: Button1Mask,
 *	ShiftMask, LockMask, ControlMask, Mod*Mask.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned int
TkMacOSXButtonKeyState(void)
{
    UInt32 buttonState = 0, keyModifiers;
    int isFrontProcess = (GetCurrentEvent() && Tk_MacOSXIsAppInFront());

    buttonState = isFrontProcess ? GetCurrentEventButtonState() :
	    GetCurrentButtonState();
    keyModifiers = isFrontProcess ? GetCurrentEventKeyModifiers() :
	    GetCurrentKeyModifiers();

    return ButtonModifiers2State(buttonState, keyModifiers);
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonModifiers2State --
 *
 *	Converts Carbon mouse button state and modifier values into a Tk
 *	button/modifier state.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static unsigned int
ButtonModifiers2State(
    UInt32 buttonState,
    UInt32 keyModifiers)
{
    unsigned int state;

    /*
     * Tk supports at most 5 buttons.
     */

    state = (buttonState & ((1<<5) - 1)) << 8;

    if (keyModifiers & alphaLock) {
	state |= LockMask;
    }
    if (keyModifiers & shiftKey) {
	state |= ShiftMask;
    }
    if (keyModifiers & controlKey) {
	state |= ControlMask;
    }
    if (keyModifiers & cmdKey) {
	state |= Mod1Mask;		/* command key */
    }
    if (keyModifiers & optionKey) {
	state |= Mod2Mask;		/* option key */
    }
    if (keyModifiers & kEventKeyModifierNumLockMask) {
	state |= Mod3Mask;
    }
    if (keyModifiers & kEventKeyModifierFnMask) {
	state |= Mod4Mask;
    }

    return state;
}

/*
 *----------------------------------------------------------------------
 *
 * XQueryPointer --
 *
 *	Check the current state of the mouse. This is not a complete
 *	implementation of this function. It only computes the root coordinates
 *	and the current mask.
 *
 * Results:
 *	Sets root_x_return, root_y_return, and mask_return. Returns true on
 *	success.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
XQueryPointer(
    Display *display,
    Window w,
    Window *root_return,
    Window *child_return,
    int *root_x_return,
    int *root_y_return,
    int *win_x_return,
    int *win_y_return,
    unsigned int *mask_return)
{
    int getGlobal = (root_x_return && root_y_return);
    int getLocal = (win_x_return && win_y_return && w != None);

    if (getGlobal || getLocal) {
	NSPoint global = [NSEvent mouseLocation];
	
	if (getLocal) {
	    MacDrawable *macWin = (MacDrawable *) w;
	    NSWindow *win = TkMacOSXDrawableWindow(w);

	    if (win) {
		NSPoint local;

		local = [win convertScreenToBase:global];
		local.y = [win frame].size.height - local.y;
		if (macWin->winPtr && macWin->winPtr->wmInfoPtr) {
		    local.x -= macWin->winPtr->wmInfoPtr->xInParent;
		    local.y -= macWin->winPtr->wmInfoPtr->yInParent;
		}
		*win_x_return = local.x;
		*win_y_return = local.y;
	    }
	}
	if (getGlobal) {
	    *root_x_return = global.x;
	    *root_y_return = tkMacOSXZeroScreenHeight - global.y;
	}
    }
    if (mask_return) {
	*mask_return = TkMacOSXButtonKeyState();
    }
    return True;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGenerateButtonEventForXPointer --
 *
 *	This procedure generates an X button event for the current pointer
 *	state as reported by XQueryPointer().
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue. Grab state may
 *	also change.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TkGenerateButtonEventForXPointer(
    Window window)		/* X Window containing button event. */
{
    MouseEventData med;
    int global_x, global_y, local_x, local_y;

    bzero(&med, sizeof(MouseEventData));
    XQueryPointer(NULL, window, NULL, NULL, &global_x, &global_y,
	    &local_x, &local_y, &med.state);
    med.global.h = global_x;
    med.global.v = global_y;
    med.local.h = local_x;
    med.local.v = local_y;
    med.window = window;

    return GenerateButtonEvent(&med);
}

/*
 *----------------------------------------------------------------------
 *
 * TkGenerateButtonEvent --
 *
 *	Given a global x & y position and the button key status this procedure
 *	generates the appropiate X button event. It also handles the state
 *	changes needed to implement implicit grabs.
 *
 * Results:
 *	True if event(s) are generated, false otherwise.
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue. Grab state may
 *	also change.
 *
 *----------------------------------------------------------------------
 */

int
TkGenerateButtonEvent(
    int x,			/* X location of mouse, */
    int y,			/* Y location of mouse. */
    Window window,		/* X Window containing button event. */
    unsigned int state)		/* Button Key state suitable for X event. */
{
    MacDrawable *macWin = (MacDrawable *) window;
    NSWindow *win = TkMacOSXDrawableWindow(window);
    MouseEventData med;

    bzero(&med, sizeof(MouseEventData));
    med.state = state;
    med.window = window;
    med.global.h = x;
    med.global.v = y;
    med.local = med.global;

    if (win) {
	NSPoint local = NSMakePoint(x, tkMacOSXZeroScreenHeight - y);

	local = [win convertScreenToBase:local];
	local.y = [win frame].size.height - local.y;
	if (macWin->winPtr && macWin->winPtr->wmInfoPtr) {
	    local.x -= macWin->winPtr->wmInfoPtr->xInParent;
	    local.y -= macWin->winPtr->wmInfoPtr->yInParent;
	}
	med.local.h = local.x;
	med.local.v = tkMacOSXZeroScreenHeight - local.y;
    }

    return GenerateButtonEvent(&med);
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateButtonEvent --
 *
 *	Generate an X button event from a MouseEventData structure. Handles
 *	the state changes needed to implement implicit grabs.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue. Grab state may
 *	also change.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateButtonEvent(
    MouseEventData *medPtr)
{
    Tk_Window tkwin;
    int dummy;
    TkDisplay *dispPtr;

#if UNUSED
    /*
     * ButtonDown events will always occur in the front window. ButtonUp
     * events, however, may occur anywhere on the screen. ButtonUp events
     * should only be sent to Tk if in the front window or during an implicit
     * grab.
     */

    if ((medPtr->activeNonFloating == NULL)
	    || ((!(TkpIsWindowFloating(medPtr->whichWin))
	    && (medPtr->activeNonFloating != medPtr->whichWin))
	    && TkMacOSXGetCapture() == NULL)) {
	return false;
    }
#endif

    dispPtr = TkGetDisplayList();
    tkwin = Tk_IdToWindow(dispPtr->display, medPtr->window);

    if (tkwin != NULL) {
	tkwin = Tk_TopCoordsToWindow(tkwin, medPtr->local.h, medPtr->local.v,
		&dummy, &dummy);
    }

    Tk_UpdatePointer(tkwin, medPtr->global.h, medPtr->global.v, medPtr->state);
    return true;
}

#ifdef MAC_OSX_TK_TODO
/*
 *----------------------------------------------------------------------
 *
 * GenerateToolbarButtonEvent --
 *
 *	Generates a "ToolbarButton" virtual event. This can be used to manage
 *	disappearing toolbars.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Places a virtual event on the event queue.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateToolbarButtonEvent(
    MouseEventData *medPtr)
{
    Tk_Window rootwin, tkwin = NULL;
    TkDisplay *dispPtr;
    TkWindow *winPtr;
    XVirtualEvent event;

    dispPtr = TkGetDisplayList();
    rootwin = Tk_IdToWindow(dispPtr->display, medPtr->window);
    if (rootwin) {
	tkwin = Tk_TopCoordsToWindow(rootwin,
		medPtr->local.h, medPtr->local.v, &event.x, &event.y);
    }
    if (!tkwin) {
	return true;
    }
    winPtr = (TkWindow *) tkwin;

    bzero(&event, sizeof(XVirtualEvent));
    event.type = VirtualEvent;
    event.serial = LastKnownRequestProcessed(winPtr->display);
    event.send_event = false;
    event.display = winPtr->display;
    event.event = winPtr->window;
    event.root = XRootWindow(winPtr->display, 0);
    event.subwindow = None;
    event.time = TkpGetMS();
    event.x_root = medPtr->global.h;
    event.y_root = medPtr->global.v;
    event.state = medPtr->state;
    event.same_screen = true;
    event.name = Tk_GetUid("ToolbarButton");

    Tk_QueueWindowEvent((XEvent *) &event, TCL_QUEUE_TAIL);
    return true;
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
