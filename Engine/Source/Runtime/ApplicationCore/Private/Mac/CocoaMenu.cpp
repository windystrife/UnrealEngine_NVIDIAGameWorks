// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CocoaMenu.h"
#include "MacApplication.h"

@implementation FCocoaMenu

- (id)initWithTitle:(NSString *)Title
{
	id Self = [super initWithTitle:Title];
	if (Self)
	{
		bHighlightingKeyEquivalent = false;
	}
	return Self;
}

- (bool)isHighlightingKeyEquivalent
{
	SCOPED_AUTORELEASE_POOL;
	FCocoaMenu* SuperMenu = [[self supermenu] isKindOfClass:[FCocoaMenu class]] ? (FCocoaMenu*)[self supermenu] : nil;
	if ( SuperMenu )
	{
		return [SuperMenu isHighlightingKeyEquivalent];
	}
	else
	{
		return bHighlightingKeyEquivalent;
	}
}

- (bool)highlightKeyEquivalent:(NSEvent *)Event
{
	SCOPED_AUTORELEASE_POOL;
	bHighlightingKeyEquivalent = true;
	bool bHighlighted = [super performKeyEquivalent:Event];
	bHighlightingKeyEquivalent = false;
	return bHighlighted;
}

@end
