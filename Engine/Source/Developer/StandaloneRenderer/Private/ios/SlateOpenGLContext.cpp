// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StandaloneRendererPrivate.h"

#include "OpenGL/SlateOpenGLRenderer.h"

FSlateOpenGLContext::FSlateOpenGLContext()
:	WindowHandle(NULL)
,	Context(NULL)
{
}

FSlateOpenGLContext::~FSlateOpenGLContext()
{
	Destroy();
}

void FSlateOpenGLContext::Initialize( void* InWindow, const FSlateOpenGLContext* SharedContext )
{
}

void FSlateOpenGLContext::Destroy()
{
}

void FSlateOpenGLContext::MakeCurrent()
{
}
