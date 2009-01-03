/*
 * tkMacOSXEvent.c --
 *
 *	This file contains the basic Mac OS X Event handling routines.
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
#include "tkMacOSXDebug.h"

#pragma mark TKApplication(TKEvent)

enum {
    NSWindowWillMoveEventType = 20
};

@implementation TKApplication(TKEvent)
/* replace by +[addLocalMonitorForEventsMatchingMask ? */
- (NSEvent *)tkProcessEvent:(NSEvent *)theEvent {
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
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
 *	This routine flushes all the Carbon windows of the application. It is
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
    WindowRef wRef = GetWindowList();

    while (wRef) {
	ChkErr(HIWindowFlush, wRef);
	wRef = GetNextWindow(wRef);
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

MODULE_SCOPE int
TkMacOSXProcessEvent(
    TkMacOSXEvent *eventPtr,
    MacEventStatus *statusPtr)
{
    switch (eventPtr->eClass) {
    case kEventClassMouse:
	TkMacOSXProcessMouseEvent(eventPtr, statusPtr);
	break;
    case kEventClassWindow:
	TkMacOSXProcessWindowEvent(eventPtr, statusPtr);
	break;
    case kEventClassKeyboard:
	TkMacOSXProcessKeyboardEvent(eventPtr, statusPtr);
	break;
    case kEventClassApplication:
	TkMacOSXProcessApplicationEvent(eventPtr, statusPtr);
	break;
    case kEventClassAppearance:
	TkMacOSXProcessAppearanceEvent(eventPtr, statusPtr);
	break;
    case kEventClassMenu:
	TkMacOSXProcessMenuEvent(eventPtr, statusPtr);
	break;
    case kEventClassCommand:
	TkMacOSXProcessCommandEvent(eventPtr, statusPtr);
	break;
    case kEventClassFont:
	TkMacOSXProcessFontEvent(eventPtr, statusPtr);
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

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXProcessMenuEvent --
 *
 *	This routine processes the event in eventPtr, and generates the
 *	appropriate Tk events from it.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TkMacOSXProcessMenuEvent(
    TkMacOSXEvent *eventPtr,
    MacEventStatus *statusPtr)
{
    int menuContext;
    OSStatus err;
    MenuRef menu;
    MenuItemIndex index;

    switch (eventPtr->eKind) {
    case kEventMenuBeginTracking:
    case kEventMenuEndTracking:
    case kEventMenuOpening:
    case kEventMenuTargetItem:
	break;
    default:
	return 0;
    }
    err = ChkErr(GetEventParameter, eventPtr->eventRef,
	    kEventParamMenuContext, typeUInt32, NULL, sizeof(menuContext),
	    NULL, &menuContext);
    if (err != noErr) {
	return 0;
    }

    if ((menuContext & kMenuContextMenuBarTracking) ||
	    (menuContext & kMenuContextPopUpTracking)) {
	switch (eventPtr->eKind) {
	case kEventMenuBeginTracking:
	    TkMacOSXClearMenubarActive();

	    /*
	     * Handle -postcommand
	     */

	    TkMacOSXPreprocessMenu();
	    TkMacOSXTrackingLoop(1);
	    break;
	case kEventMenuEndTracking:
	    TkMacOSXTrackingLoop(0);
	    break;
	case kEventMenuOpening:
	    err = ChkErr(GetEventParameter, eventPtr->eventRef,
		    kEventParamDirectObject, typeMenuRef, NULL, sizeof(menu),
		    NULL, &menu);
	    if (err != noErr) {
		return 0;
	    }
	    TkMacOSXClearActiveMenu(menu);
	    return TkMacOSXGenerateParentMenuSelectEvent(menu);
	case kEventMenuTargetItem:
	    err = ChkErr(GetEventParameter, eventPtr->eventRef,
		    kEventParamDirectObject, typeMenuRef, NULL, sizeof(menu),
		    NULL, &menu);
	    if (err != noErr) {
		return 0;
	    }

	    err = ChkErr(GetEventParameter, eventPtr->eventRef,
		    kEventParamMenuItemIndex, typeMenuItemIndex, NULL,
		    sizeof(index), NULL, &index);
	    if (err != noErr) {
		return 0;
	    }
	    return TkMacOSXGenerateMenuSelectEvent(menu, index);
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXProcessCommandEvent --
 *
 *	This routine processes the event in eventPtr, and generates the
 *	appropriate Tk events from it.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TkMacOSXProcessCommandEvent(
    TkMacOSXEvent *eventPtr,
    MacEventStatus *statusPtr)
{
    HICommand command;
    int menuContext;
    OSStatus err;

    switch (eventPtr->eKind) {
    case kEventCommandProcess:
    case kEventCommandUpdateStatus:
	break;
    default:
	return 0;
    }

    err = ChkErr(GetEventParameter, eventPtr->eventRef,
	    kEventParamDirectObject, typeHICommand, NULL, sizeof(command),
	    NULL, &command);
    if (err != noErr) {
	return 0;
    }

    if (command.attributes & kHICommandFromMenu) {
	if (eventPtr->eKind == kEventCommandProcess) {
	    err = ChkErr(GetEventParameter, eventPtr->eventRef,
		    kEventParamMenuContext, typeUInt32, NULL,
		    sizeof(menuContext), NULL, &menuContext);
	    if (err != noErr) {
		return 0;
	    }

	    if ((menuContext & kMenuContextMenuBar) &&
		    (menuContext & kMenuContextMenuBarTracking)) {
		TkMacOSXHandleMenuSelect(GetMenuID(command.menu.menuRef),
			command.menu.menuItemIndex,
			(GetCurrentEventKeyModifiers() & optionKey) != 0);
		return 1;
	    }
	} else {
	    Tcl_CmdInfo dummy;

	    if (command.commandID == kHICommandPreferences
		    && eventPtr->interp != NULL) {
		if (Tcl_GetCommandInfo(eventPtr->interp,
			"::tk::mac::ShowPreferences", &dummy)) {
		    if (!IsMenuItemEnabled(command.menu.menuRef,
			    command.menu.menuItemIndex)) {
			EnableMenuItem(command.menu.menuRef,
				command.menu.menuItemIndex);
		    }
		} else {
		    if (IsMenuItemEnabled(command.menu.menuRef,
			    command.menu.menuItemIndex)) {
			DisableMenuItem(command.menu.menuRef,
				command.menu.menuItemIndex);
		    }
		}
		statusPtr->stopProcessing = 1;
		return 1;
	    }
	}
    }
    return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
