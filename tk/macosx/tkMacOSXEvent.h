/*
 * tkMacOSXEvent.h --
 *
 *	Declarations of Macintosh specific functions for implementing the
 *	Mac OS X Notifier.
 *
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright 2008-2009, Apple Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TKMACEVENT
#define _TKMACEVENT

#ifndef _TKMACINT
#include "tkMacOSXInt.h"
#endif

MODULE_SCOPE void TkMacOSXFlushWindows(void);
#ifdef MAC_OSX_TK_TODO
MODULE_SCOPE int TkMacOSXProcessEvent(TkMacOSXEvent *eventPtr,
	MacEventStatus *statusPtr);
MODULE_SCOPE int TkMacOSXProcessApplicationEvent(TkMacOSXEvent *e,
	MacEventStatus *statusPtr);
MODULE_SCOPE int TkMacOSXProcessAppearanceEvent(TkMacOSXEvent *e,
	MacEventStatus *statusPtr);
MODULE_SCOPE int TkMacOSXProcessFontEvent(TkMacOSXEvent *e,
	MacEventStatus *statusPtr);
#endif

#endif
