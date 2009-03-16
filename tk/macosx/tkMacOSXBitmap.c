/*
 * tkMacOSXBitmap.c --
 *
 *	This file handles the implementation of native bitmaps.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
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
 * This structure holds information about native bitmaps.
 */

typedef struct {
    const char *name;		/* Name of icon. */
    OSType iconType;		/* OSType of icon. */
} BuiltInIcon;

/*
 * This array mapps a string name to the supported builtin icons
 * on the Macintosh.
 */

static BuiltInIcon builtInIcons[] = {
    {"document",	kGenericDocumentIcon},
    {"stationery",	kGenericStationeryIcon},
    {"edition",		kGenericEditionFileIcon},
    {"application",	kGenericApplicationIcon},
    {"accessory",	kGenericDeskAccessoryIcon},
    {"folder",		kGenericFolderIcon},
    {"pfolder",		kPrivateFolderIcon},
    {"trash",		kTrashIcon},
    {"floppy",		kGenericFloppyIcon},
    {"ramdisk",		kGenericRAMDiskIcon},
    {"cdrom",		kGenericCDROMIcon},
    {"preferences",	kGenericPreferencesIcon},
    {"querydoc",	kGenericQueryDocumentIcon},
    {"stop",		kAlertStopIcon},
    {"note",		kAlertNoteIcon},
    {"caution",		kAlertCautionIcon},
    {NULL}
};

#define builtInIconSize 32

/*
 *----------------------------------------------------------------------
 *
 * TkpDefineNativeBitmaps --
 *
 *	Add native bitmaps.
 *
 * Results:
 *	A standard Tcl result. If an error occurs then TCL_ERROR is
 *	returned and a message is left in the interp's result.
 *
 * Side effects:
 *	"Name" is entered into the bitmap table and may be used from
 *	here on to refer to the given bitmap.
 *
 *----------------------------------------------------------------------
 */

void
TkpDefineNativeBitmaps(void)
{
    Tcl_HashTable *tablePtr = TkGetBitmapPredefTable();
    BuiltInIcon *builtInPtr;

    for (builtInPtr = builtInIcons; builtInPtr->name != NULL; builtInPtr++) {
	Tcl_HashEntry *predefHashPtr;
	Tk_Uid name;
	int isNew;

	name = Tk_GetUid(builtInPtr->name);
	predefHashPtr = Tcl_CreateHashEntry(tablePtr, name, &isNew);
	if (isNew) {
	    TkPredefBitmap *predefPtr = (TkPredefBitmap *)
		    ckalloc(sizeof(TkPredefBitmap));
	    predefPtr->source = UINT2PTR(builtInPtr->iconType);
	    predefPtr->width = builtInIconSize;
	    predefPtr->height = builtInIconSize;
	    predefPtr->native = 1;
	    Tcl_SetHashValue(predefHashPtr, predefPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetBitmapForIcon --
 *
 * Results:
 *	Bitmap for the given IconRef.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Pixmap
GetBitmapForIcon(
    Display *display,
    IconRef icon)
{
    TkMacOSXDrawingContext dc;
    Pixmap pixmap;

    pixmap = Tk_GetPixmap(display, None, builtInIconSize, builtInIconSize, 0);
    if (TkMacOSXSetupDrawingContext(pixmap, NULL, 1, &dc)) {
	if (dc.context) {
	    const CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1,
		    .tx = 0, .ty = builtInIconSize };
	    const CGRect r = { .origin = { .x = 0, .y = 0 }, .size = {
		    .width = builtInIconSize, .height = builtInIconSize } };

	    CGContextConcatCTM(dc.context, t);
	    PlotIconRefInContext(dc.context, &r, kAlignNone,
		    kTransformNone, NULL, kPlotIconRefNormalFlags, icon);
	}
	TkMacOSXRestoreDrawingContext(&dc);
    }
    return pixmap;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpCreateNativeBitmap --
 *
 *	Create native bitmap.
 *
 * Results:
 *	Native bitmap.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Pixmap
TkpCreateNativeBitmap(
    Display *display,
    const void *source)		/* Info about the icon to build. */
{
    Pixmap pixmap;
    IconRef icon;
    OSErr err;

    err = ChkErr(GetIconRef, kOnSystemDisk, kSystemIconsCreator,
	    PTR2UINT(source), &icon);
    if (err == noErr) {
	pixmap = GetBitmapForIcon(display, icon);
	ReleaseIconRef(icon);
    } else {
	pixmap = Tk_GetPixmap(display, None, builtInIconSize,
		builtInIconSize, 0);
    }
    return pixmap;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetNativeAppBitmap --
 *
 *	Get a named native bitmap.
 *
 *	Attemps to interpret the given name in order as:
 *	    - NSImage named image name
 *	    - NSImage path to image file
 *	    - NSImage url string
 *	    - 4-char OSType of IconServices icon
 *
 * Results:
 *	Native bitmap or None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Pixmap
TkpGetNativeAppBitmap(
    Display *display,		/* The display. */
    const char *name,		/* The name of the bitmap. */
    int *width,			/* The width & height of the bitmap. */
    int *height)
{
    Pixmap pixmap = None;
    NSString *string = [NSString stringWithUTF8String:name];
    NSImage *image = [NSImage imageNamed:string];

    if (!image) {
	image = [[[NSImage alloc] initWithContentsOfFile:string] autorelease];
    }
    if (!image) {
	NSURL *url = [NSURL URLWithString:string];
	if (url) {
	    image = [[[NSImage alloc] initWithContentsOfURL:url] autorelease];
	}
    }
    if (image) {
	TkMacOSXDrawingContext dc;
	NSSize size = [image size];

	pixmap = Tk_GetPixmap(display, None, size.width, size.height, 0);
	*width = size.width;
	*height = size.height;
	if (TkMacOSXSetupDrawingContext(pixmap, NULL, 1, &dc)) {
	    if (dc.context) {
		CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1,
			.tx = 0, .ty = size.height};

		CGContextConcatCTM(dc.context, t);
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext
			graphicsContextWithGraphicsPort:dc.context flipped:NO]];
		[image drawAtPoint:NSZeroPoint fromRect:NSZeroRect
			operation:NSCompositeCopy fraction:1.0];
		[NSGraphicsContext restoreGraphicsState];
	    }
	    TkMacOSXRestoreDrawingContext(&dc);
	}
    } else {
	Tcl_Encoding encoding = Tcl_GetEncoding(NULL, "macRoman");
	Tcl_DString ds;

	Tcl_UtfToExternalDString(encoding, name, -1, &ds);
	if (Tcl_DStringLength(&ds) <= 4) {
	    char string[4] = {};
	    OSType iconType;
	    IconRef icon;
	    OSErr err;

	    memcpy(string, Tcl_DStringValue(&ds),
		    (size_t) Tcl_DStringLength(&ds));
	    iconType = (OSType) string[0] << 24 | (OSType) string[1] << 16 |
		     (OSType) string[2] <<  8 | (OSType) string[3];
	    err = ChkErr(GetIconRef, kOnSystemDisk, kSystemIconsCreator,
		    iconType, &icon);
	    if (err == noErr) {
		pixmap = GetBitmapForIcon(display, icon);
		*width = builtInIconSize;
		*height = builtInIconSize;
		ReleaseIconRef(icon);
	    }
	}
	Tcl_DStringFree(&ds);
	Tcl_FreeEncoding(encoding);
    }
    return pixmap;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
