// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Rendering/RenderingPolicy.h"
#include "Rendering/SlateRenderer.h"

/* FSlateRenderingPolicy interface
 *****************************************************************************/

TSharedRef<class FSlateFontCache> FSlateRenderingPolicy::GetFontCache() const
{
	return FontServices->GetFontCache();
}

TSharedRef<class FSlateFontServices> FSlateRenderingPolicy::GetFontServices() const
{
	return FontServices;
}
