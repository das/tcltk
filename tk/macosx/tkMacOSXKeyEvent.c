/*
 * tkMacOSXKeyEvent.c --
 *
 *	This file implements functions that decode & handle keyboard events on
 *	MacOS X.
 *
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2006-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *	The following terms apply to all files originating from Apple Computer,
 *	Inc. ("Apple") and associated with the software unless explicitly
 *	disclaimed in individual files.
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
 *	IN NO EVENT SHALL APPLE, THE AUTHORS OR DISTRIBUTORS OF THE SOFTWARE BE
 *	LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
 *	CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS
 *	DOCUMENTATION, OR ANY DERIVATIVES THEREOF, EVEN IF APPLE OR THE AUTHORS
 *	HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. APPLE, THE AUTHORS
 *	AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT
 *	NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 *	A PARTICULAR PURPOSE, AND NON-INFRINGEMENT. THIS SOFTWARE IS PROVIDED
 *	ON AN "AS IS" BASIS, AND APPLE,THE AUTHORS AND DISTRIBUTORS HAVE NO
 *	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 *	MODIFICATIONS.
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
#include "tkMacOSXEvent.h"

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_KEYBOARD
#endif
*/

static Tk_Window grabWinPtr = NULL;
				/* Current grab window, NULL if no grab. */
static Tk_Window keyboardGrabWinPtr = NULL;
				/* Current keyboard grab window. */


@implementation TKApplication(TKKeyEvent)
- (NSEvent *)tkProcessKeyEvent:(NSEvent *)theEvent {
#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
#endif
    NSWindow*	    w;
    NSEventType	    type = [theEvent type];
    NSUInteger	    modifiers, len;
    BOOL	    repeat = NO;
    unsigned short  keyCode;
    NSString	    *characters = nil, *charactersIgnoringModifiers = nil;
    static NSUInteger savedModifiers = 0;


    switch (type) {
    case NSKeyUp:
    case NSKeyDown:
	repeat = [theEvent isARepeat];
	characters = [theEvent characters];
	charactersIgnoringModifiers = [theEvent charactersIgnoringModifiers];
    case NSFlagsChanged:
	modifiers = [theEvent modifierFlags];
	keyCode = [theEvent keyCode];
	w = [self windowWithWindowNumber:[theEvent windowNumber]];
#ifdef TK_MAC_DEBUG_EVENTS
	TKLog(@"-[%@(%p) %s] %d %u %@ %@ %u %@", [self class], self, _cmd, repeat, modifiers, characters, charactersIgnoringModifiers, keyCode, w);
#endif
	break;

    default:
	return theEvent;
	break;
    }

    unsigned int state = 0;

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

    /*
     * The focus must be in the FrontWindow on the Macintosh. We then query Tk
     * to determine the exact Tk window that owns the focus.
     */

    TkWindow *winPtr = TkMacOSXGetTkWindow(w);
    Tk_Window tkwin = (Tk_Window) winPtr;

    if (!tkwin) {
	TkMacOSXDbgMsg("tkwin == NULL");
	return theEvent;
    }
    tkwin = (Tk_Window) winPtr->dispPtr->focusPtr;
    if (!tkwin) {
	TkMacOSXDbgMsg("tkwin == NULL");
	return theEvent;
    }

    XEvent xEvent;
    xEvent.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
    xEvent.xany.send_event = false;
    xEvent.xany.display = Tk_Display(tkwin);
    xEvent.xany.window = Tk_WindowId(tkwin);

    xEvent.xkey.root = XRootWindow(Tk_Display(tkwin), 0);
    xEvent.xkey.subwindow = None;
    xEvent.xkey.time = TkpGetMS();
    xEvent.xkey.state = state;
    xEvent.xkey.same_screen = true;
    xEvent.xkey.trans_chars[0] = 0;
    xEvent.xkey.nbytes = 0;

    if (type == NSFlagsChanged) {
	if (savedModifiers > modifiers) {
	    xEvent.xany.type = KeyRelease;
	} else {
	    xEvent.xany.type = KeyPress;
	}

	/*
	 * Use special '-1' to signify a special keycode to our platform
	 * specific code in tkMacOSXKeyboard.c. This is rather like what
	 * happens on Windows.
	 */

	xEvent.xany.send_event = -1;

	/*
	 * Set keycode (which was zero) to the changed modifier
	 */

	xEvent.xkey.keycode = (modifiers ^ savedModifiers);
    } else {
	if (type == NSKeyUp || repeat) {
	    xEvent.xany.type = KeyRelease;
	} else {
	    xEvent.xany.type = KeyPress;
	}
	xEvent.xkey.keycode = (keyCode << 16) | (UInt16)
		[characters characterAtIndex:0];
	if (![characters getCString:xEvent.xkey.trans_chars
		maxLength:XMaxTransChars encoding:NSUTF8StringEncoding]) {
	    TkMacOSXDbgMsg("characters too long");
	    return theEvent;
	}
	len = [charactersIgnoringModifiers length];
	if (len) {
	    xEvent.xkey.nbytes = [charactersIgnoringModifiers characterAtIndex:0];
	    if (len > 1) {
		TkMacOSXDbgMsg("more than one charactersIgnoringModifiers");
	    }
	}
	if (repeat) {
	    Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
	    xEvent.xany.type = KeyPress;
	    xEvent.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
	}
    }
    Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
    savedModifiers = modifiers;

    return theEvent;
}
@end
#pragma mark -


/*
 *----------------------------------------------------------------------
 *
 * XGrabKeyboard --
 *
 *	Simulates a keyboard grab by setting the focus.
 *
 * Results:
 *	Always returns GrabSuccess.
 *
 * Side effects:
 *	Sets the keyboard focus to the specified window.
 *
 *----------------------------------------------------------------------
 */

int
XGrabKeyboard(
    Display* display,
    Window grab_window,
    Bool owner_events,
    int pointer_mode,
    int keyboard_mode,
    Time time)
{
    keyboardGrabWinPtr = Tk_IdToWindow(display, grab_window);

    return GrabSuccess;
}

/*
 *----------------------------------------------------------------------
 *
 * XUngrabKeyboard --
 *
 *	Releases the simulated keyboard grab.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the keyboard focus back to the value before the grab.
 *
 *----------------------------------------------------------------------
 */

void
XUngrabKeyboard(
    Display* display,
    Time time)
{
    keyboardGrabWinPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetCapture --
 *
 * Results:
 *	Returns the current grab window
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
TkMacOSXGetCapture(void)
{
    return grabWinPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpSetCapture --
 *
 *	This function captures the mouse so that all future events will be
 *	reported to this window, even if the mouse is outside the window. If
 *	the specified window is NULL, then the mouse is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the capture flag and captures the mouse.
 *
 *----------------------------------------------------------------------
 */

void
TkpSetCapture(
    TkWindow *winPtr)		/* Capture window, or NULL. */
{
    while (winPtr && !Tk_IsTopLevel(winPtr)) {
	winPtr = winPtr->parentPtr;
    }
#if 0
    {
	TkWindow *w = NULL;
	WindowModality m;

	if (winPtr) {
	    w = winPtr;
	    m = kWindowModalityAppModal;
	} else if (grabWinPtr) {
	    w = (TkWindow *) grabWinPtr;
	    m = kWindowModalityNone;
	}

	if (w && w->window != None && TkMacOSXHostToplevelExists(w)) {
	    ChkErr(SetWindowModality, TkMacOSXDrawableWindow(w->window), m,
		    NULL);
	}
    }
#endif
    grabWinPtr = (Tk_Window) winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetCaretPos --
 *
 *	This enables correct placement of the XIM caret. This is called by
 *	widgets to indicate their cursor placement, and the caret location is
 *	used by TkpGetString to place the XIM caret.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetCaretPos(
    Tk_Window tkwin,
    int x,
    int y,
    int height)
{
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
