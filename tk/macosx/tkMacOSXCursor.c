/*
 * tkMacOSXCursor.c --
 *
 *	This file contains Macintosh specific cursor related routines.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright 2008-2009, Apple Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXPrivate.h"

/*
 * There are three different ways to set the cursor on the Mac.
 */

#define NONE		-1	/* Hidden cursor */
#define SELECTOR	1	/* NSCursor class method */
#define IMAGENAMED	2	/* Named NSImage */
#define IMAGEPATH	3	/* Path to NSImage */

/*
 * The following data structure contains the system specific data necessary to
 * control Windows cursors.
 */

typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    NSCursor *macCursor;	/* Macintosh cursor */
    int type;			/* Type of Mac cursor */
} TkMacOSXCursor;

/*
 * The table below is used to map from the name of a predefined cursor
 * to a NSCursor.
 */

struct CursorName {
    const char *name;
    const int kind;
    NSString *id;
    NSPoint hotspot;
};

static const struct CursorName cursorNames[] = {
    {"none",			NONE,	    nil},
    {"arrow",			SELECTOR,   @"arrowCursor"},
    {"top_left_arrow",		SELECTOR,   @"arrowCursor"},
    {"copyarrow",		SELECTOR,   @"dragCopyCursor"},
    {"aliasarrow",		SELECTOR,   @"dragLinkCursor"},
    {"contextualmenuarrow",	SELECTOR,   @"contextualMenuCursor"},
    {"ibeam",			SELECTOR,   @"IBeamCursor"},
    {"text",			SELECTOR,   @"IBeamCursor"},
    {"xterm",			SELECTOR,   @"IBeamCursor"},
    {"cross",			SELECTOR,   @"crosshairCursor"},
    {"crosshair",		SELECTOR,   @"crosshairCursor"},
    {"cross-hair",		SELECTOR,   @"crosshairCursor"},
//    {"plus",			kThemePlusCursor},
    {"closedhand",		SELECTOR,   @"closedHandCursor"},
    {"fist",			SELECTOR,   @"closedHandCursor"},
    {"openhand",		SELECTOR,   @"openHandCursor"},
    {"hand",			SELECTOR,   @"openHandCursor"},
    {"pointinghand",		SELECTOR,   @"pointingHandCursor"},
    {"resizeleft",		SELECTOR,   @"resizeLeftCursor"},
    {"resizeright",		SELECTOR,   @"resizeRightCursor"},
    {"resizeleftright",		SELECTOR,   @"resizeLeftRightCursor"},
    {"resizeup",		SELECTOR,   @"resizeUpCursor"},
    {"resizedown",		SELECTOR,   @"resizeDownCursor"},
    {"resizeupdown",		SELECTOR,   @"resizeUpDownCursor"},
    {"notallowed",		SELECTOR,   @"operationNotAllowedCursor"},
    {"poof",			SELECTOR,   @"disappearingItemCursor"},
//    {"watch",			kThemeWatchCursor},
//    {"countinguphand",	kThemeCountingUpHandCursor},
//    {"countingdownhand",	kThemeCountingDownHandCursor},
//    {"countingupanddownhand",	kThemeCountingUpAndDownHandCursor},*/
    {"spinning",		IMAGENAMED, @"NSWaitCursor", {8, -8}},
    {"wait",			IMAGENAMED, @"NSWaitCursor", {8, -8}},
    {"resizebottomleft",	IMAGENAMED, @"NSTruthBottomLeftResizeCursor", {4, -12}},
    {"resizetopleft",		IMAGENAMED, @"NSTruthTopLeftResizeCursor", {4, -4}},
    {"resizebottomright",	IMAGENAMED, @"NSTruthBottomRightResizeCursor", {12, -12}},
    {"resizetopright",		IMAGENAMED, @"NSTruthTopRightResizeCursor", {12, -4}},
    {"bucket",			IMAGEPATH, @"bucket.cur", {13, 12}},
    {"cancel",			IMAGEPATH, @"cancel.cur", {8, 5}},
    {"resize",			IMAGEPATH, @"resize.cur", {8, 8}},
    {"eyedrop",			IMAGEPATH, @"eyedrop.cur", {15, 0}},
    {"eyedrop-full",		IMAGEPATH, @"eyedrop-full.cur", {15, 0}},
    {"zoom-in",			IMAGEPATH, @"zoom-in.cur", {7, 7}},
    {"zoom-out",		IMAGEPATH, @"zoom-out.cur", {7, 7}},
    {"X_cursor",		IMAGEPATH, @"cursor00.cur"},
//    {"arrow",			IMAGEPATH, @"cursor02.cur"},
    {"based_arrow_down",	IMAGEPATH, @"cursor04.cur"},
    {"based_arrow_up",		IMAGEPATH, @"cursor06.cur"},
    {"boat",			IMAGEPATH, @"cursor08.cur"},
    {"bogosity",		IMAGEPATH, @"cursor0a.cur"},
    {"bottom_left_corner",	IMAGEPATH, @"cursor0c.cur"},
    {"bottom_right_corner",	IMAGEPATH, @"cursor0e.cur"},
    {"bottom_side",		IMAGEPATH, @"cursor10.cur"},
    {"bottom_tee",		IMAGEPATH, @"cursor12.cur"},
    {"box_spiral",		IMAGEPATH, @"cursor14.cur"},
    {"center_ptr",		IMAGEPATH, @"cursor16.cur"},
    {"circle",			IMAGEPATH, @"cursor18.cur"},
    {"clock",			IMAGEPATH, @"cursor1a.cur"},
    {"coffee_mug",		IMAGEPATH, @"cursor1c.cur"},
    {"cross",			IMAGEPATH, @"cursor1e.cur"},
    {"cross_reverse",		IMAGEPATH, @"cursor20.cur"},
    {"crosshair",		IMAGEPATH, @"cursor22.cur"},
    {"diamond_cross",		IMAGEPATH, @"cursor24.cur"},
    {"dot",			IMAGEPATH, @"cursor26.cur"},
    {"dotbox",			IMAGEPATH, @"cursor28.cur"},
    {"double_arrow",		IMAGEPATH, @"cursor2a.cur"},
    {"draft_large",		IMAGEPATH, @"cursor2c.cur"},
    {"draft_small",		IMAGEPATH, @"cursor2e.cur"},
    {"draped_box",		IMAGEPATH, @"cursor30.cur"},
    {"exchange",		IMAGEPATH, @"cursor32.cur"},
    {"fleur",			IMAGEPATH, @"cursor34.cur"},
    {"gobbler",			IMAGEPATH, @"cursor36.cur"},
    {"gumby",			IMAGEPATH, @"cursor38.cur"},
    {"hand1",			IMAGEPATH, @"cursor3a.cur"},
    {"hand2",			IMAGEPATH, @"cursor3c.cur"},
    {"heart",			IMAGEPATH, @"cursor3e.cur"},
    {"icon",			IMAGEPATH, @"cursor40.cur"},
    {"iron_cross",		IMAGEPATH, @"cursor42.cur"},
    {"left_ptr",		IMAGEPATH, @"cursor44.cur"},
    {"left_side",		IMAGEPATH, @"cursor46.cur"},
    {"left_tee",		IMAGEPATH, @"cursor48.cur"},
    {"leftbutton",		IMAGEPATH, @"cursor4a.cur"},
    {"ll_angle",		IMAGEPATH, @"cursor4c.cur"},
    {"lr_angle",		IMAGEPATH, @"cursor4e.cur"},
    {"man",			IMAGEPATH, @"cursor50.cur"},
    {"middlebutton",		IMAGEPATH, @"cursor52.cur"},
    {"mouse",			IMAGEPATH, @"cursor54.cur"},
    {"pencil",			IMAGEPATH, @"cursor56.cur"},
    {"pirate",			IMAGEPATH, @"cursor58.cur"},
    {"plus",			IMAGEPATH, @"cursor5a.cur"},
    {"question_arrow",		IMAGEPATH, @"cursor5c.cur"},
    {"right_ptr",		IMAGEPATH, @"cursor5e.cur"},
    {"right_side",		IMAGEPATH, @"cursor60.cur"},
    {"right_tee",		IMAGEPATH, @"cursor62.cur"},
    {"rightbutton",		IMAGEPATH, @"cursor64.cur"},
    {"rtl_logo",		IMAGEPATH, @"cursor66.cur"},
    {"sailboat",		IMAGEPATH, @"cursor68.cur"},
    {"sb_down_arrow",		IMAGEPATH, @"cursor6a.cur"},
    {"sb_h_double_arrow",	IMAGEPATH, @"cursor6c.cur"},
    {"sb_left_arrow",		IMAGEPATH, @"cursor6e.cur"},
    {"sb_right_arrow",		IMAGEPATH, @"cursor70.cur"},
    {"sb_up_arrow",		IMAGEPATH, @"cursor72.cur"},
    {"sb_v_double_arrow",	IMAGEPATH, @"cursor74.cur"},
    {"shuttle",			IMAGEPATH, @"cursor76.cur"},
    {"sizing",			IMAGEPATH, @"cursor78.cur"},
    {"spider",			IMAGEPATH, @"cursor7a.cur"},
    {"spraycan",		IMAGEPATH, @"cursor7c.cur"},
    {"star",			IMAGEPATH, @"cursor7e.cur"},
    {"target",			IMAGEPATH, @"cursor80.cur"},
    {"tcross",			IMAGEPATH, @"cursor82.cur"},
//    {"top_left_arrow",	IMAGEPATH, @"cursor84.cur"},
    {"top_left_corner",		IMAGEPATH, @"cursor86.cur"},
    {"top_right_corner",	IMAGEPATH, @"cursor88.cur"},
    {"top_side",		IMAGEPATH, @"cursor8a.cur"},
    {"top_tee",			IMAGEPATH, @"cursor8c.cur"},
    {"trek",			IMAGEPATH, @"cursor8e.cur"},
    {"ul_angle",		IMAGEPATH, @"cursor90.cur"},
    {"umbrella",		IMAGEPATH, @"cursor92.cur"},
    {"ur_angle",		IMAGEPATH, @"cursor94.cur"},
    {"watch",			IMAGEPATH, @"cursor96.cur"},
//    {"xterm",			IMAGEPATH, @"cursor98.cur"},
//    {"none",			IMAGEPATH, @"cursor9a.cur"},
    {NULL}
};

/*
 * Declarations of static variables used in this file.
 */

static TkMacOSXCursor * gCurrentCursor = NULL;
				/* A pointer to the current cursor. */
static int gResizeOverride = false;
				/* A boolean indicating whether we should use
				 * the resize cursor during installations. */
static int gTkOwnsCursor = true;/* A boolean indicating whether Tk owns the
				 * cursor. If not (for instance, in the case
				 * where a Tk window is embedded in another
				 * app's window, and the cursor is out of the
				 * tk window, we will not attempt to adjust
				 * the cursor. */

/*
 * Declarations of procedures local to this file
 */

static void FindCursorByName(TkMacOSXCursor *macCursorPtr, const char *string);

/*
 *----------------------------------------------------------------------
 *
 * FindCursorByName --
 *
 *	Retrieve a system cursor by name, and fill the macCursorPtr
 *	structure. If the cursor cannot be found, the macCursor field
 *	will be nil.
 *
 * Results:
 *	Fills the macCursorPtr record.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
FindCursorByName(
    TkMacOSXCursor *macCursorPtr,
    const char *name)
{
    Tcl_Obj *strPtr = Tcl_NewStringObj(name, -1);
    int idx, result;
    NSImage *image = nil;

    macCursorPtr->macCursor = nil;
    macCursorPtr->type = 0;

    result = Tcl_GetIndexFromObjStruct(NULL, strPtr, cursorNames,
		sizeof(struct CursorName), NULL, TCL_EXACT, &idx);
    Tcl_DecrRefCount(strPtr);
    if (result == TCL_OK) {
	NSCursor *macCursor = nil;

	macCursorPtr->type = cursorNames[idx].kind;
	switch (cursorNames[idx].kind) {
	case SELECTOR:
	    macCursor = [[NSCursor performSelector:
		    NSSelectorFromString(cursorNames[idx].id)] retain];
	    break;
	case IMAGENAMED:
	    image = [NSImage imageNamed:cursorNames[idx].id];
	    break;
	case IMAGEPATH: {
	    NSString *path = [[NSApp tkFrameworkBundle]
		    pathForImageResource:cursorNames[idx].id];
#ifdef TK_MAC_DEBUG
	    if (!path && getenv("TK_SRCROOT")) {
	    	// FIXME: fallback to TK_SRCROOT
		path = [NSString stringWithFormat:@"%s/win/rc/%@",
			getenv("TK_SRCROOT"), cursorNames[idx].id];
	    }
#endif
	    if (path) {
		image = [[[NSImage alloc] initWithContentsOfFile:path]
			autorelease];
	    }
	    break;
	}
	}
	if (image) {
	    // FIXME: read hotspot info from .cur files
	    macCursor = [[NSCursor alloc] initWithImage:image
		    hotSpot:cursorNames[idx].hotspot];
	}
	macCursorPtr->macCursor = TkMacOSXMakeUncollectable(macCursor);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetCursorByName --
 *
 *	Retrieve a system cursor by name.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkGetCursorByName(
    Tcl_Interp *interp,		/* Interpreter to use for error reporting. */
    Tk_Window tkwin,		/* Window in which cursor will be used. */
    Tk_Uid string)		/* Description of cursor. See manual entry
				 * for details on legal syntax. */
{
    TkMacOSXCursor *macCursorPtr;

    macCursorPtr = (TkMacOSXCursor *) ckalloc(sizeof(TkMacOSXCursor));
    macCursorPtr->info.cursor = (Tk_Cursor) macCursorPtr;

    FindCursorByName(macCursorPtr, string);

    if (macCursorPtr->macCursor == nil && macCursorPtr->type != NONE) {
	const char **argv;
	int argc;

	/*
	 * The user may be trying to specify an XCursor with fore & back
	 * colors. We don't want this to be an error, so pick off the
	 * first word, and try again.
	 */

	if (Tcl_SplitList(interp, string, &argc, &argv) == TCL_OK ) {
	    if (argc > 1) {
		FindCursorByName(macCursorPtr, argv[0]);
	    }
	    ckfree((char *) argv);
	}
    }

    if (macCursorPtr->macCursor == nil && macCursorPtr->type != NONE) {
	ckfree((char *)macCursorPtr);
	Tcl_AppendResult(interp, "bad cursor spec \"", string, "\"", NULL);
	return NULL;
    }
    return (TkCursor *) macCursorPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateCursorFromData --
 *
 *	Creates a cursor from the source and mask bits.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkCreateCursorFromData(
    Tk_Window tkwin,		/* Window in which cursor will be used. */
    const char *source,		/* Bitmap data for cursor shape. */
    const char *mask,		/* Bitmap data for cursor mask. */
    int width, int height,	/* Dimensions of cursor. */
    int xHot, int yHot,		/* Location of hot-spot in cursor. */
    XColor fgColor,		/* Foreground color for cursor. */
    XColor bgColor)		/* Background color for cursor. */
{
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpFreeCursor --
 *
 *	This procedure is called to release a cursor allocated by
 *	TkGetCursorByName.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor data structure is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TkpFreeCursor(
    TkCursor *cursorPtr)
{
    TkMacOSXCursor *macCursorPtr = (TkMacOSXCursor *) cursorPtr;

    TkMacOSXMakeCollectableAndRelease(macCursorPtr->macCursor);
    if (macCursorPtr == gCurrentCursor) {
	gCurrentCursor = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInstallCursor --
 *
 *	Installs either the current cursor as defined by TkpSetCursor or a
 *	resize cursor as the cursor the Macintosh should currently display.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the Macintosh mouse cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXInstallCursor(
    int resizeOverride)
{
    TkMacOSXCursor *macCursorPtr = gCurrentCursor;
    static int cursorHidden = 0;
    int cursorNone = 0;

    gResizeOverride = resizeOverride;

    if (resizeOverride || !macCursorPtr) {
	[[NSCursor arrowCursor] set];
    } else {
	switch (macCursorPtr->type) {
	case NONE:
	    if (!cursorHidden) {
		cursorHidden = 1;
		[NSCursor hide];
	    }
	    cursorNone = 1;
	    break;
	case SELECTOR:
	case IMAGENAMED:
	case IMAGEPATH:
	    [macCursorPtr->macCursor set];
	    break;
	}
    }
    if (cursorHidden && !cursorNone) {
	cursorHidden = 0;
	[NSCursor unhide];
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpSetCursor --
 *
 *	Set the current cursor and install it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the current cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkpSetCursor(
    TkpCursor cursor)
{
    int cursorChanged = 1;

    if (!gTkOwnsCursor) {
	return;
    }

    if (cursor == None) {
	/*
	 * This is a little tricky. We can't really tell whether
	 * gCurrentCursor is NULL because it was NULL last time around or
	 * because we just freed the current cursor. So if the input cursor is
	 * NULL, we always need to reset it, we can't trust the cursorChanged
	 * logic.
	 */

	gCurrentCursor = NULL;
    } else {
	if (gCurrentCursor == (TkMacOSXCursor *) cursor) {
	    cursorChanged = 0;
	}
	gCurrentCursor = (TkMacOSXCursor *) cursor;
    }

    if (Tk_MacOSXIsAppInFront() && cursorChanged) {
	TkMacOSXInstallCursor(gResizeOverride);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MacOSXTkOwnsCursor --
 *
 *	Sets whether Tk has the right to adjust the cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May keep Tk from changing the cursor.
 *
 *----------------------------------------------------------------------
 */

void
Tk_MacOSXTkOwnsCursor(
    int tkOwnsIt)
{
    gTkOwnsCursor = tkOwnsIt;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
