// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxCursor.h"
#include "Misc/App.h"
#include "Linux/LinuxApplication.h"
#include "Linux/LinuxPlatformApplicationMisc.h"

FLinuxCursor::FLinuxCursor()
	: 	bHidden(false)
	,	CachedGlobalXPosition(0)
	,	CachedGlobalYPosition(0)
	,	bPositionCacheIsValid(false)
{
	if (!FApp::CanEverRender())
	{
		// assume that non-rendering application will be fine with token cursor
		UE_LOG(LogInit, Log, TEXT("Not creating cursor resources due to headless application."));
		return;
	}

	if (!FLinuxPlatformApplicationMisc::InitSDL()) //	will not initialize more than once
	{
		UE_LOG(LogInit, Fatal, TEXT("FLinuxCursor::FLinuxCursor() : InitSDL() failed, cannot construct cursor."));
		// unreachable
		return;
	}

#if DO_CHECK
	uint32 InitializedSubsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	check(InitializedSubsystems & SDL_INIT_VIDEO);
#endif // DO_CHECK

	// Load up cursors that we'll be using
	for( int32 CurCursorIndex = 0; CurCursorIndex < EMouseCursor::TotalCursorCount; ++CurCursorIndex )
	{
		CursorHandles[ CurCursorIndex ] = NULL;
		CursorOverrideHandles[ CurCursorIndex ] = NULL;

		SDL_HCursor CursorHandle = NULL;
		switch( CurCursorIndex )
		{
		case EMouseCursor::None:
		case EMouseCursor::Custom:
			// The mouse cursor will not be visible when None is used
			break;

		case EMouseCursor::Default:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_ARROW );
			break;

		case EMouseCursor::TextEditBeam:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_IBEAM );
			break;

		case EMouseCursor::ResizeLeftRight:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEWE );
			break;

		case EMouseCursor::ResizeUpDown:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENS );
			break;

		case EMouseCursor::ResizeSouthEast:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENWSE );
			break;

		case EMouseCursor::ResizeSouthWest:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENESW );
			break;

		case EMouseCursor::CardinalCross:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEALL );
			break;

		case EMouseCursor::Crosshairs:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_CROSSHAIR );
			break;

		case EMouseCursor::Hand:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_HAND );
			break;

		case EMouseCursor::GrabHand:
			//CursorHandle = LoadCursorFromFile((LPCTSTR)*(FString( FPlatformProcess::BaseDir() ) / FString::Printf( TEXT("%sEditor/Slate/Old/grabhand.cur"), *FPaths::EngineContentDir() )));
			//if (CursorHandle == NULL)
			//{
			//	// Failed to load file, fall back
				CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_HAND );
			//}
			break;

		case EMouseCursor::GrabHandClosed:
			//CursorHandle = LoadCursorFromFile((LPCTSTR)*(FString( FPlatformProcess::BaseDir() ) / FString::Printf( TEXT("%sEditor/Slate/Old/grabhand_closed.cur"), *FPaths::EngineContentDir() )));
			//if (CursorHandle == NULL)
			//{
			//	// Failed to load file, fall back
				CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_HAND );
			//}
			break;

		case EMouseCursor::SlashedCircle:
			CursorHandle = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_NO );
			break;

		case EMouseCursor::EyeDropper:
			{
				Uint8 mask[32]={0x00, 0x07, 0x00, 0x0f, 0x00, 0x5f, 0x00, 0xfe,
								0x01, 0xfc, 0x00, 0xf8, 0x01, 0xfc, 0x02, 0xf8,
								0x07, 0xd0, 0x0f, 0x80, 0x1f, 0x00, 0x3e, 0x00,
								0x7c, 0x00, 0x78, 0x00, 0xf0, 0x00, 0x40, 0x00};

				Uint8 data[32]={0x00, 0x07, 0x00, 0x0b, 0x00, 0x53, 0x00, 0xa6,
								0x01, 0x0c, 0x00, 0xf8, 0x01, 0x7c, 0x02, 0x38,
								0x04, 0x50, 0x08, 0x80, 0x11, 0x00, 0x22, 0x00,
								0x44, 0x00, 0x48, 0x00, 0xb0, 0x00, 0x40, 0x00};

				CursorHandle = SDL_CreateCursor(data, mask, 16, 16, 0, 15);
			}
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

FLinuxCursor::~FLinuxCursor()
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
			case EMouseCursor::Custom:
				// Standard shared cursors don't need to be destroyed
				break;
			case EMouseCursor::EyeDropper:
				SDL_FreeCursor(CursorHandles[CurCursorIndex]);
				break;

			default:
				// Unrecognized cursor type!
				check( 0 );
				break;
		}
	}
}

void FLinuxCursor::SetCustomShape( SDL_HCursor CursorHandle )
{
	CursorHandles[EMouseCursor::Custom] = CursorHandle;
}

FVector2D FLinuxCursor::GetPosition() const
{
	if (!bPositionCacheIsValid)
	{
		SDL_GetGlobalMouseState(&CachedGlobalXPosition, &CachedGlobalYPosition);
		bPositionCacheIsValid = true;
	}

	int CursorX = CachedGlobalXPosition;
	int CursorY = CachedGlobalYPosition;

	return FVector2D( CursorX, CursorY );
}

void FLinuxCursor::InvalidateCaches()
{
	bPositionCacheIsValid = false;
}

void FLinuxCursor::SetCachedPosition( const int32 X, const int32 Y )
{
	CachedGlobalXPosition = X;
	CachedGlobalYPosition = Y;
	bPositionCacheIsValid = true;
}

void FLinuxCursor::SetPosition( const int32 X, const int32 Y )
{
	// according to reports on IRC SDL_WarpMouseGlobal() doesn't work on some WMs so use InWindow unless we don't have a window.
	SDL_HWindow WndFocus = SDL_GetMouseFocus();

	if (WndFocus)
	{
		int WndX, WndY;

		// Get top-left.
		if(LinuxApplication)
		{
			LinuxApplication->GetWindowPositionInEventLoop(WndFocus, &WndX, &WndY);
		}
		else
		{
			SDL_GetWindowPosition(WndFocus, &WndX, &WndY);
		}

		SDL_WarpMouseInWindow(WndFocus, X - WndX, Y - WndY);
	}
	else
	{
		SDL_WarpMouseGlobal(X, Y);
	}

	SetCachedPosition(X, Y);
}

void FLinuxCursor::SetType(const EMouseCursor::Type InNewCursor)
{
	checkf(InNewCursor < EMouseCursor::TotalCursorCount, TEXT("Invalid cursor(%d) supplied"), InNewCursor);
	CurrentType = InNewCursor;

	SDL_HCursor CurrentCursor = CursorOverrideHandles[InNewCursor] ? CursorOverrideHandles[InNewCursor] : CursorHandles[InNewCursor];

	if (CurrentCursor == nullptr)
	{
		if (InNewCursor != EMouseCursor::Custom)
		{
			bHidden = true;
		}
		SDL_ShowCursor(SDL_DISABLE);
		SDL_SetCursor(CursorHandles[0]);
	}
	else
	{
		bHidden = false;
		SDL_ShowCursor(SDL_ENABLE);
		SDL_SetCursor(CurrentCursor);
	}
}

void FLinuxCursor::GetSize( int32& Width, int32& Height ) const
{
	Width = 16;
	Height = 16;
}

void FLinuxCursor::Show( bool bShow )
{
	if( bShow )
	{
		// Show mouse cursor.
		bHidden = false;
		SDL_ShowCursor(SDL_ENABLE);
	}
	else
	{
		// Disable the cursor.
		bHidden = true;
		SDL_ShowCursor(SDL_DISABLE);
	}
}

void FLinuxCursor::Lock( const RECT* const Bounds )
{
	LinuxApplication->OnMouseCursorLock( Bounds != NULL );

	// Lock/Unlock the cursor
	if ( Bounds == NULL )
	{
		CursorClipRect = FIntRect();
	}
	else
	{
		CursorClipRect.Min.X = FMath::TruncToInt(Bounds->left);
		CursorClipRect.Min.Y = FMath::TruncToInt(Bounds->top);
		CursorClipRect.Max.X = FMath::TruncToInt(Bounds->right) - 1;
		CursorClipRect.Max.Y = FMath::TruncToInt(Bounds->bottom) - 1;
	}

	FVector2D CurrentPosition = GetPosition();
	if( UpdateCursorClipping( CurrentPosition ) )
	{
		SetPosition( CurrentPosition.X, CurrentPosition.Y );
	}
}

bool FLinuxCursor::UpdateCursorClipping( FVector2D& CursorPosition )
{
	bool bAdjusted = false;

	if (CursorClipRect.Area() > 0)
	{
		if (CursorPosition.X < CursorClipRect.Min.X)
		{
			CursorPosition.X = CursorClipRect.Min.X;
			bAdjusted = true;
		}
		else if (CursorPosition.X > CursorClipRect.Max.X)
		{
			CursorPosition.X = CursorClipRect.Max.X;
			bAdjusted = true;
		}

		if (CursorPosition.Y < CursorClipRect.Min.Y)
		{
			CursorPosition.Y = CursorClipRect.Min.Y;
			bAdjusted = true;
		}
		else if (CursorPosition.Y > CursorClipRect.Max.Y)
		{
			CursorPosition.Y = CursorClipRect.Max.Y;
			bAdjusted = true;
		}
	}

	return bAdjusted;
}

bool FLinuxCursor::IsHidden()
{
	return bHidden;
}

void FLinuxCursor::SetTypeShape(EMouseCursor::Type InCursorType, void* InCursorHandle)
{
	checkf(InCursorType < EMouseCursor::TotalCursorCount, TEXT("Invalid cursor(%d) supplied"), (int)InCursorType);

	SDL_Cursor* CursorHandle = (SDL_Cursor*)InCursorHandle;
	CursorOverrideHandles[InCursorType] = CursorHandle;

	if (CurrentType == InCursorType)
	{
		SetType(CurrentType);
	}
}