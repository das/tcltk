/*
 * tkMacOSXEvent.c --
 *
 *	This file contains the basic Mac OS X Event handling routines.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright 2008-2009, Apple Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXEvent.h"
#include "tkMacOSXDebug.h"

#pragma mark TKApplication(TKEvent)

enum {
    NSWindowWillMoveEventType = 20
};

@implementation TKApplication(TKEvent)
/* TODO: replace by +[addLocalMonitorForEventsMatchingMask ? */
- (NSEvent *)tkProcessEvent:(NSEvent *)theEvent {
#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
#endif
    id		    win;
    NSEventType	    type = [theEvent type];
    NSInteger	    subtype;
    NSUInteger	    flags;

    switch ((NSInteger)type) {
    case NSAppKitDefined:
        subtype = [theEvent subtype];

	switch (subtype) {
	case NSApplicationActivatedEventType:
	    break;
	case NSApplicationDeactivatedEventType:
	    break;
	case NSWindowExposedEventType:
	case NSScreenChangedEventType:
	    break;
	case NSWindowMovedEventType:
	    break;
        case NSWindowWillMoveEventType:
            break;

        default:
            break;
	}
	break;
    case NSKeyUp:
    case NSKeyDown:
    case NSFlagsChanged:
	flags = [theEvent modifierFlags];
	theEvent = [self tkProcessKeyEvent:theEvent];
	break;
    case NSLeftMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseDown:
    case NSRightMouseUp:
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSMouseMoved:
    case NSMouseEntered:
    case NSMouseExited:
    case NSScrollWheel:
    case NSOtherMouseDown:
    case NSOtherMouseUp:
    case NSOtherMouseDragged:
    case NSTabletPoint:
    case NSTabletProximity:
	theEvent = [self tkProcessMouseEvent:theEvent];
	break;
    case NSSystemDefined:
        subtype = [theEvent subtype];
	break;
    case NSApplicationDefined:
	win = [theEvent window];
	break;
    case NSCursorUpdate:
        break;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
    case NSEventTypeGesture:
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
    case NSEventTypeSwipe:
    case NSEventTypeBeginGesture:
    case NSEventTypeEndGesture:
        break;
#endif

    default:
	break;
    }
    return theEvent;
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXFlushWindows --
 *
 *	This routine flushes all the windows of the application. It is
 *	called by XSync().
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Flushes all Carbon windows
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void
TkMacOSXFlushWindows(void)
{
    NSInteger windowCount;
    NSInteger *windowNumbers;

    NSCountWindows(&windowCount);
    if(windowCount) {
	windowNumbers = (NSInteger *) ckalloc(windowCount * sizeof(NSInteger));
	NSWindowList(windowCount, windowNumbers);
	for (NSInteger index = 0; index < windowCount; index++) {
	    [[NSApp windowWithWindowNumber:windowNumbers[index]]
		    flushWindowIfNeeded];
	}
	ckfree((char*) windowNumbers);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXProcessEvent --
 *
 *	This dispatches a filtered Carbon event to the appropriate handler
 *
 *	Note on MacEventStatus.stopProcessing: Please be conservative in the
 *	individual handlers and don't assume the event is fully handled unless
 *	you *really* need to ensure that other handlers don't see the event
 *	anymore. Some OS manager or library might be interested in events even
 *	after they are already handled on the Tk level.
 *
 * Results:
 *	0 on success
 *	-1 on failure
 *
 * Side effects:
 *	Converts a Carbon event to a Tk event
 *
 *----------------------------------------------------------------------
 */

#ifdef MAC_OSX_TK_TODO
MODULE_SCOPE int
TkMacOSXProcessEvent(
    TkMacOSXEvent *eventPtr,
    MacEventStatus *statusPtr)
{
    switch (eventPtr->eClass) {
    case kEventClassApplication:
	TkMacOSXProcessApplicationEvent(eventPtr, statusPtr);
	break;
    case kEventClassAppearance:
	TkMacOSXProcessAppearanceEvent(eventPtr, statusPtr);
	break;
    default: {
#ifdef TK_MAC_DEBUG_CARBON_EVENTS
	TkMacOSXDbgMsg("Unrecognised event: %s",
		TkMacOSXCarbonEventToAscii(eventPtr->eventRef));
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
	break;
    }
    }
    return 0;
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
