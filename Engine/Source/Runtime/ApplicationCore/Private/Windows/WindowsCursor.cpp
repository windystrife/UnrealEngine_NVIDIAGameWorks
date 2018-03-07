// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsCursor.h"
#include "Misc/AssertionMacros.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "Misc/CoreMisc.h"
#include "Math/Vector2D.h"
#include "Windows/WindowsWindow.h"
#include "Windows/WindowsHWrapper.h"
#include "HAL/PlatformProcess.h"

#include "Windows/AllowWindowsPlatformTypes.h"
	#include <Ole2.h>
	#include <oleidl.h>
#include "Windows/HideWindowsPlatformTypes.h"

FWindowsCursor::FWindowsCursor()
{
	// Load up cursors that we'll be using
	for( int32 CurCursorIndex = 0; CurCursorIndex < EMouseCursor::TotalCursorCount; ++CurCursorIndex )
	{
		CursorHandles[ CurCursorIndex ] = NULL;
		CursorOverrideHandles[ CurCursorIndex ] = NULL;

		HCURSOR CursorHandle = NULL;
		switch( CurCursorIndex )
		{
		case EMouseCursor::None:
		case EMouseCursor::Custom:
			// The mouse cursor will not be visible when None is used
			break;

		case EMouseCursor::Default:
			CursorHandle = ::LoadCursor( NULL, IDC_ARROW );
			break;

		case EMouseCursor::TextEditBeam:
			CursorHandle = ::LoadCursor( NULL, IDC_IBEAM );
			break;

		case EMouseCursor::ResizeLeftRight:
			CursorHandle = ::LoadCursor( NULL, IDC_SIZEWE );
			break;

		case EMouseCursor::ResizeUpDown:
			CursorHandle = ::LoadCursor( NULL, IDC_SIZENS );
			break;

		case EMouseCursor::ResizeSouthEast:
			CursorHandle = ::LoadCursor( NULL, IDC_SIZENWSE );
			break;

		case EMouseCursor::ResizeSouthWest:
			CursorHandle = ::LoadCursor( NULL, IDC_SIZENESW );
			break;

		case EMouseCursor::CardinalCross:
			CursorHandle = ::LoadCursor(NULL, IDC_SIZEALL);
			break;

		case EMouseCursor::Crosshairs:
			CursorHandle = ::LoadCursor( NULL, IDC_CROSS );
			break;

		case EMouseCursor::Hand:
			CursorHandle = ::LoadCursor( NULL, IDC_HAND );
			break;

		case EMouseCursor::GrabHand:
			CursorHandle = LoadCursorFromFile((LPCTSTR)*(FString( FPlatformProcess::BaseDir() ) / FString::Printf( TEXT("%sEditor/Slate/Cursor/grabhand.cur"), *FPaths::EngineContentDir() )));
			if (CursorHandle == NULL)
			{
				// Failed to load file, fall back
				CursorHandle = ::LoadCursor( NULL, IDC_HAND );
			}
			break;

		case EMouseCursor::GrabHandClosed:
			CursorHandle = LoadCursorFromFile((LPCTSTR)*(FString( FPlatformProcess::BaseDir() ) / FString::Printf( TEXT("%sEditor/Slate/Cursor/grabhand_closed.cur"), *FPaths::EngineContentDir() )));
			if (CursorHandle == NULL)
			{
				// Failed to load file, fall back
				CursorHandle = ::LoadCursor( NULL, IDC_HAND );
			}
			break;

		case EMouseCursor::SlashedCircle:
			CursorHandle = ::LoadCursor(NULL, IDC_NO);
			break;

		case EMouseCursor::EyeDropper:
			CursorHandle = LoadCursorFromFile((LPCTSTR)*(FString( FPlatformProcess::BaseDir() ) / FString::Printf( TEXT("%sEditor/Slate/Icons/eyedropper.cur"), *FPaths::EngineContentDir() )));
			break;

			// NOTE: For custom app cursors, use:
			//		CursorHandle = ::LoadCursor( InstanceHandle, (LPCWSTR)MY_RESOURCE_ID );

		default:
			// Unrecognized cursor type!
			check( 0 );
			break;
		}

		CursorHandles[ CurCursorIndex ] = CursorHandle;
	}

	// Set the default cursor
	SetType( EMouseCursor::Default );
}

FWindowsCursor::~FWindowsCursor()
{
	// Release cursors
	// NOTE: Shared cursors will automatically be destroyed when the application is destroyed.
	//       For dynamically created cursors, use DestroyCursor
	for( int32 CurCursorIndex = 0; CurCursorIndex < EMouseCursor::TotalCursorCount; ++CurCursorIndex )
	{
		switch( CurCursorIndex )
		{
		case EMouseCursor::None:
		case EMouseCursor::Default:
		case EMouseCursor::TextEditBeam:
		case EMouseCursor::ResizeLeftRight:
		case EMouseCursor::ResizeUpDown:
		case EMouseCursor::ResizeSouthEast:
		case EMouseCursor::ResizeSouthWest:
		case EMouseCursor::CardinalCross:
		case EMouseCursor::Crosshairs:
		case EMouseCursor::Hand:
		case EMouseCursor::GrabHand:
		case EMouseCursor::GrabHandClosed:
		case EMouseCursor::SlashedCircle:
		case EMouseCursor::EyeDropper:
		case EMouseCursor::Custom:
			// Standard shared cursors don't need to be destroyed
			break;

		default:
			// Unrecognized cursor type!
			check( 0 );
			break;
		}
	}
}

FVector2D FWindowsCursor::GetPosition() const
{
	POINT CursorPos;
	::GetCursorPos(&CursorPos);

	return FVector2D( CursorPos.x, CursorPos.y );
}

void FWindowsCursor::SetPosition( const int32 X, const int32 Y )
{
	::SetCursorPos( X, Y );
}

void FWindowsCursor::SetType( const EMouseCursor::Type InNewCursor )
{
	// NOTE: Watch out for contention with FWindowsViewport::UpdateMouseCursor
	checkf( InNewCursor < EMouseCursor::TotalCursorCount, TEXT("Invalid cursor(%d) supplied"), (int)InNewCursor );
	CurrentType = InNewCursor;

	if (CursorOverrideHandles[InNewCursor])
	{
		::SetCursor(CursorOverrideHandles[InNewCursor]);
	}
	else
	{
		::SetCursor(CursorHandles[InNewCursor]);
	}
}

void FWindowsCursor::GetSize( int32& Width, int32& Height ) const
{
	//TODO this is wrong, this should query the size of the cursor on the platform.

	Width = 16;
	Height = 16;
}

void FWindowsCursor::Show( bool bShow )
{
	if( bShow )
	{
		// Show mouse cursor. Each time ShowCursor(true) is called an internal value is incremented so we 
		// call ShowCursor until the cursor is actually shown (>= 0 value returned by showcursor)
		while ( ::ShowCursor(true)<0 );
	}
	else
	{		// Disable the cursor.  Wait until its actually disabled.
		while ( ::ShowCursor(false)>=0 );
	}
}

void FWindowsCursor::Lock( const RECT* const Bounds )
{
	// Lock/Unlock the cursor
	::ClipCursor(Bounds);
	// If the cursor is not visible and we're running game, assume we're in a mode where the mouse is controlling the camera and lock it to the center of the widget.
}

void FWindowsCursor::SetTypeShape(EMouseCursor::Type InCursorType, void* InCursorHandle)
{
	checkf(InCursorType < EMouseCursor::TotalCursorCount, TEXT("Invalid cursor(%d) supplied"), (int)InCursorType);

	HCURSOR CursorHandle = (HCURSOR)InCursorHandle;
	CursorOverrideHandles[InCursorType] = CursorHandle;

	if (CurrentType == InCursorType)
	{
		SetType(CurrentType);
	}
}
