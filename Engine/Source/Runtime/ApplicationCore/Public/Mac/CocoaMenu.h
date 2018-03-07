// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

@interface FCocoaMenu : NSMenu
{
@private
	bool bHighlightingKeyEquivalent;
}
- (bool)isHighlightingKeyEquivalent;
- (bool)highlightKeyEquivalent:(NSEvent *)Event;
@end
