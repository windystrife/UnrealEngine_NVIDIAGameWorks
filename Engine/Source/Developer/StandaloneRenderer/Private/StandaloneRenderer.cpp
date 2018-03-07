// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StandaloneRenderer.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Styling/CoreStyle.h"
#include "StandaloneRendererLog.h"

#if PLATFORM_WINDOWS
#include "Windows/D3D/SlateD3DRenderer.h"
#endif

#include "Rendering/SlateRenderer.h"
#include "OpenGL/SlateOpenGLRenderer.h"

DEFINE_LOG_CATEGORY(LogStandaloneRenderer);

/**
 * Single function to create the standalone renderer for the running platform
 */
TSharedRef<FSlateRenderer> GetStandardStandaloneRenderer()
{
	// create a standalone renderer object
	TSharedPtr<FSlateRenderer> Renderer = NULL;
#if PLATFORM_WINDOWS && ( WINVER > 0x502 )	// D3D11 not supported on Windows XP
	bool bUseOpenGL = FParse::Param( FCommandLine::Get(), TEXT("opengl") );
	if( bUseOpenGL )
	{
#endif
		Renderer = TSharedPtr<FSlateRenderer>( new FSlateOpenGLRenderer( FCoreStyle::Get() ) );
#if PLATFORM_WINDOWS && ( WINVER > 0x502 )	// D3D11 not supported on Windows XP
	}
	else
	{
		Renderer = TSharedPtr<FSlateRenderer>( new FSlateD3DRenderer( FCoreStyle::Get() ) );
	}
#endif

	// enforce non-NULL pointer
	return Renderer.ToSharedRef();
}

class FStandaloneRenderer : public IModuleInterface
{
};

IMPLEMENT_MODULE( FStandaloneRenderer, BlankPlugin )
