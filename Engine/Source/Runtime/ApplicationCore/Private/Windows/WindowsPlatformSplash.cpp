// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformSplash.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "CoreGlobals.h"
#include "Misc/Parse.h"
#include "Misc/EngineVersionBase.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Stats/Stats.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Windows/WindowsPlatformApplicationMisc.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Misc/EngineBuildSettings.h"

THIRD_PARTY_INCLUDES_START
#pragma warning(push)
#pragma warning(disable:4005)
#include <d2d1.h>
#include <strsafe.h>
#include <wincodec.h>
#pragma warning(pop)
THIRD_PARTY_INCLUDES_END

#pragma comment( lib, "windowscodecs.lib" )

/**
 * Splash screen functions and static globals
 */

static HANDLE GSplashScreenThread = NULL;
static HBITMAP GSplashScreenBitmap = NULL;
static HWND GSplashScreenWnd = NULL; 
static HWND GSplashScreenGuard = NULL; 
static FString GSplashScreenFileName;
static FText GSplashScreenAppName;
static FText GSplashScreenText[ SplashTextType::NumTextTypes ];
static RECT GSplashScreenTextRects[ SplashTextType::NumTextTypes ];
static HFONT GSplashScreenSmallTextFontHandle = NULL;
static HFONT GSplashScreenNormalTextFontHandle = NULL;
static HFONT GSplashScreenTitleTextFontHandle = NULL;
static FCriticalSection GSplashScreenSynchronizationObject;



/**
 * Window's proc for splash screen
 */
LRESULT CALLBACK SplashScreenWindowProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;

	switch( message )
	{
		case WM_PAINT:
			{
				hdc = BeginPaint(hWnd, &ps);

				// Draw splash bitmap
				DrawState(hdc, DSS_NORMAL, NULL, (LPARAM)GSplashScreenBitmap, 0, 0, 0, 0, 0, DST_BITMAP);

				{
					// Take a critical section since another thread may be trying to set the splash text
					FScopeLock ScopeLock( &GSplashScreenSynchronizationObject );

					// Draw splash text
					for( int32 CurTypeIndex = 0; CurTypeIndex < SplashTextType::NumTextTypes; ++CurTypeIndex )
					{
						const FText& SplashText = GSplashScreenText[ CurTypeIndex ];
						const RECT& TextRect = GSplashScreenTextRects[ CurTypeIndex ];

						if( !SplashText.IsEmpty() )
						{
							if ( CurTypeIndex == SplashTextType::VersionInfo1 || CurTypeIndex == SplashTextType::StartupProgress )
							{
								SelectObject( hdc, GSplashScreenNormalTextFontHandle );
							}
							else if ( CurTypeIndex == SplashTextType::GameName )
							{
								SelectObject( hdc, GSplashScreenTitleTextFontHandle );
							}
							else
							{
								SelectObject( hdc, GSplashScreenSmallTextFontHandle );
							}

							// Alignment
							if ( CurTypeIndex == SplashTextType::GameName )
							{
								SetTextAlign( hdc, TA_RIGHT | TA_TOP | TA_NOUPDATECP );
							}
							else
							{
								SetTextAlign( hdc, TA_LEFT | TA_TOP | TA_NOUPDATECP );
							}

							SetBkColor( hdc, 0x00000000 );
							SetBkMode( hdc, TRANSPARENT );

							RECT ClientRect;
							GetClientRect( hWnd, &ClientRect );

							// Draw background text passes
							const int32 NumBGPasses = 8;
							for( int32 CurBGPass = 0; CurBGPass < NumBGPasses; ++CurBGPass )
							{
								int32 BGXOffset, BGYOffset;
								switch( CurBGPass )
								{
									default:
									case 0:	BGXOffset = -1; BGYOffset =  0; break;
									case 2:	BGXOffset = -1; BGYOffset = -1; break;
									case 3:	BGXOffset =  0; BGYOffset = -1; break;
									case 4:	BGXOffset =  1; BGYOffset = -1; break;
									case 5:	BGXOffset =  1; BGYOffset =  0; break;
									case 6:	BGXOffset =  1; BGYOffset =  1; break;
									case 7:	BGXOffset =  0; BGYOffset =  1; break;
									case 8:	BGXOffset = -1; BGYOffset =  1; break;
								}

								SetTextColor( hdc, 0x00000000 );
								TextOut(
									hdc,
									TextRect.left + BGXOffset,
									TextRect.top + BGYOffset,
									*SplashText.ToString(),
									SplashText.ToString().Len() );
							}
							
							// Draw foreground text pass
							if( CurTypeIndex == SplashTextType::StartupProgress )
							{
								SetTextColor( hdc, RGB( 200, 200, 200 ) );
							}
							else if( CurTypeIndex == SplashTextType::VersionInfo1 )
							{
								SetTextColor( hdc, RGB( 240, 240, 240 ) );
							}
							else if ( CurTypeIndex == SplashTextType::GameName )
							{
								SetTextColor(hdc, RGB(240, 240, 240));
							}
							else
							{
								SetTextColor( hdc, RGB( 160, 160, 160 ) );
							}

							TextOut(
								hdc,
								TextRect.left,
								TextRect.top,
								*SplashText.ToString(),
								SplashText.ToString().Len() );
						}
					}
				}

				EndPaint(hWnd, &ps);
			}
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

/**
 * Helper function to load the splash screen bitmap
 * This replaces the old win32 api call to ::LoadBitmap which couldn't handle more modern BMP formats containing
 *  - colour space information or newer format extensions.
 * This code is largely taken from the WicViewerGDI sample provided by Microsoft on MSDN.
 */
HBITMAP LoadSplashBitmap()
{
	HRESULT hr = CoInitialize(NULL);

	// The factory pointer
	IWICImagingFactory *Factory = NULL;

	// Create the COM imaging factory
	hr = CoCreateInstance(
		CLSID_WICImagingFactory,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&Factory)
		);

	// Decode the source image to IWICBitmapSource
	IWICBitmapDecoder *Decoder = NULL;

	// Create a decoder
	hr = Factory->CreateDecoderFromFilename(
		(LPCTSTR)*GSplashScreenFileName, // Image to be decoded
		NULL,                            // Do not prefer a particular vendor
		GENERIC_READ,                    // Desired read access to the file
		WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
		&Decoder                        // Pointer to the decoder
		);

	IWICBitmapFrameDecode *Frame = NULL;

	// Retrieve the first frame of the image from the decoder
	if (SUCCEEDED(hr))
	{
		hr = Decoder->GetFrame(0, &Frame);
	}

	// Retrieve IWICBitmapSource from the frame
	IWICBitmapSource *OriginalBitmapSource = NULL;
	if (SUCCEEDED(hr))
	{
		hr = Frame->QueryInterface( IID_IWICBitmapSource, reinterpret_cast<void **>(&OriginalBitmapSource));
	}

	IWICBitmapSource *ToRenderBitmapSource = NULL;

	// convert the pixel format
	if (SUCCEEDED(hr))
	{
		IWICFormatConverter *Converter = NULL;

		hr = Factory->CreateFormatConverter(&Converter);

		// Format convert to 32bppBGR
		if (SUCCEEDED(hr))
		{
			hr = Converter->Initialize(
				Frame,                          // Input bitmap to convert
				GUID_WICPixelFormat32bppBGR,     // Destination pixel format
				WICBitmapDitherTypeNone,         // Specified dither patterm
				NULL,                            // Specify a particular palette 
				0.f,                             // Alpha threshold
				WICBitmapPaletteTypeCustom       // Palette translation type
				);

			// Store the converted bitmap if successful
			if (SUCCEEDED(hr))
			{
				hr = Converter->QueryInterface(IID_PPV_ARGS(&ToRenderBitmapSource));
			}
		}

		Converter->Release();
	}

	// Create a DIB from the converted IWICBitmapSource
	HBITMAP hDIBBitmap = 0;
	if (SUCCEEDED(hr))
	{
		// Get image attributes and check for valid image
		UINT width = 0;
		UINT height = 0;

		void *ImageBits = NULL;
	
		// Check BitmapSource format
		WICPixelFormatGUID pixelFormat;
		hr = ToRenderBitmapSource->GetPixelFormat(&pixelFormat);

		if (SUCCEEDED(hr))
		{
			hr = (pixelFormat == GUID_WICPixelFormat32bppBGR) ? S_OK : E_FAIL;
		}

		if (SUCCEEDED(hr))
		{
			hr = ToRenderBitmapSource->GetSize(&width, &height);
		}

		// Create a DIB section based on Bitmap Info
		// BITMAPINFO Struct must first be setup before a DIB can be created.
		// Note that the height is negative for top-down bitmaps
		if (SUCCEEDED(hr))
		{
			BITMAPINFO bminfo;
			ZeroMemory(&bminfo, sizeof(bminfo));
			bminfo.bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
			bminfo.bmiHeader.biWidth        = width;
			bminfo.bmiHeader.biHeight       = -(LONG)height;
			bminfo.bmiHeader.biPlanes       = 1;
			bminfo.bmiHeader.biBitCount     = 32;
			bminfo.bmiHeader.biCompression  = BI_RGB;

			// Get a DC for the full screen
			HDC hdcScreen = GetDC(NULL);

			hr = hdcScreen ? S_OK : E_FAIL;

			// Release the previously allocated bitmap 
			if (SUCCEEDED(hr))
			{
				if (hDIBBitmap)
				{
					ensure(DeleteObject(hDIBBitmap));
				}

				hDIBBitmap = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &ImageBits, NULL, 0);

				ReleaseDC(NULL, hdcScreen);

				hr = hDIBBitmap ? S_OK : E_FAIL;
			}
		}

		UINT cbStride = 0;
		if (SUCCEEDED(hr))
		{
			// Size of a scan line represented in bytes: 4 bytes each pixel
			hr = UIntMult(width, sizeof(DWORD), &cbStride);
		}
	
		UINT cbImage = 0;
		if (SUCCEEDED(hr))
		{
			// Size of the image, represented in bytes
			hr = UIntMult(cbStride, height, &cbImage);
		}

		// Extract the image into the HBITMAP    
		if (SUCCEEDED(hr) && ToRenderBitmapSource)
		{
			hr = ToRenderBitmapSource->CopyPixels(
				NULL,
				cbStride,
				cbImage, 
				reinterpret_cast<BYTE *> (ImageBits));
		}

		// Image Extraction failed, clear allocated memory
		if (FAILED(hr) && hDIBBitmap)
		{
			ensure(DeleteObject(hDIBBitmap));
			hDIBBitmap = NULL;
		}
	}

	if ( OriginalBitmapSource )
	{
		OriginalBitmapSource->Release();
	}

	if ( ToRenderBitmapSource )
	{
		ToRenderBitmapSource->Release();
	}

	if ( Decoder )
	{
		Decoder->Release();
	}

	if ( Frame )
	{
		Frame->Release();
	}

	if ( Factory )
	{
		Factory->Release();
	}

	return hDIBBitmap;
}

/**
 * Splash screen thread entry function
 */
uint32 WINAPI StartSplashScreenThread( LPVOID unused )
{
	WNDCLASS wc;
	wc.style       = CS_HREDRAW | CS_VREDRAW; 
	wc.lpfnWndProc = (WNDPROC) SplashScreenWindowProc; 
	wc.cbClsExtra  = 0; 
	wc.cbWndExtra  = 0; 
	wc.hInstance   = hInstance; 

	wc.hIcon       = LoadIcon(hInstance, MAKEINTRESOURCE(FWindowsPlatformApplicationMisc::GetAppIcon()));
	if(wc.hIcon == NULL)
	{
		wc.hIcon   = LoadIcon((HINSTANCE) NULL, IDI_APPLICATION); 
	}

	wc.hCursor     = LoadCursor((HINSTANCE) NULL, IDC_ARROW); 
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = TEXT("SplashScreenClass"); 
 
	if(!RegisterClass(&wc)) 
	{
		return 0; 
	} 

	// Load splash screen image, display it and handle all window's messages
	GSplashScreenBitmap = LoadSplashBitmap();
	if(GSplashScreenBitmap)
	{
		BITMAP bm;
		GetObjectW(GSplashScreenBitmap, sizeof(bm), &bm);

		const int32 BorderWidth = GetSystemMetrics(SM_CXBORDER);
		const int32 BorderHeight = GetSystemMetrics(SM_CYBORDER);
		const int32 WindowWidth = bm.bmWidth + BorderWidth;
		const int32 WindowHeight = bm.bmHeight + BorderHeight;
		int32 ScreenPosX = (GetSystemMetrics(SM_CXSCREEN) - WindowWidth) / 2;
		int32 ScreenPosY = (GetSystemMetrics(SM_CYSCREEN) - WindowHeight) / 2;

		const bool bAllowFading = true;

		// Force the editor splash screen to show up in the taskbar and alt-tab lists
		uint32 dwWindowStyle = (GIsEditor ? WS_EX_APPWINDOW : 0) | WS_EX_TOOLWINDOW;
		if( bAllowFading )
		{
			dwWindowStyle |= WS_EX_LAYERED;
		}

		GSplashScreenWnd = CreateWindowEx(
			dwWindowStyle,
			wc.lpszClassName, 
			TEXT("SplashScreen"),
			WS_BORDER|WS_POPUP,
			ScreenPosX,
			ScreenPosY,
			WindowWidth,
			WindowHeight,
			(HWND) NULL,
			(HMENU) NULL,
			hInstance,
			(LPVOID) NULL); 

		if( bAllowFading )
		{
			// Set window to fully transparent to start out
			SetLayeredWindowAttributes( GSplashScreenWnd, 0, 0, LWA_ALPHA );
		}


		// Setup font
		{
			HFONT SystemFontHandle = ( HFONT )GetStockObject( DEFAULT_GUI_FONT );

			// Create small font
			{
				LOGFONT MyFont;
				FMemory::Memzero( &MyFont, sizeof( MyFont ) );
				GetObjectW( SystemFontHandle, sizeof( MyFont ), &MyFont );
				MyFont.lfHeight = 10;
				// MyFont.lfQuality = ANTIALIASED_QUALITY;
				GSplashScreenSmallTextFontHandle = CreateFontIndirect( &MyFont );
				if( GSplashScreenSmallTextFontHandle == NULL )
				{
					// Couldn't create font, so just use a system font
					GSplashScreenSmallTextFontHandle = SystemFontHandle;
				}
			}

			// Create normal font
			{
				LOGFONT MyFont;
				FMemory::Memzero( &MyFont, sizeof( MyFont ) );
				GetObjectW( SystemFontHandle, sizeof( MyFont ), &MyFont );
				MyFont.lfHeight = 12;
				// MyFont.lfQuality = ANTIALIASED_QUALITY;
				GSplashScreenNormalTextFontHandle = CreateFontIndirect( &MyFont );
				if( GSplashScreenNormalTextFontHandle == NULL )
				{
					// Couldn't create font, so just use a system font
					GSplashScreenNormalTextFontHandle = SystemFontHandle;
				}
			}

			// Create title font
			{
				LOGFONT MyFont;
				FMemory::Memzero(&MyFont, sizeof( MyFont ));
				GetObjectW(SystemFontHandle, sizeof( MyFont ), &MyFont);
				MyFont.lfHeight = 40;
				MyFont.lfWeight = FW_BOLD;
				MyFont.lfQuality = ANTIALIASED_QUALITY;
				StringCchCopy(MyFont.lfFaceName, LF_FACESIZE, TEXT("Verdana"));
				GSplashScreenTitleTextFontHandle = CreateFontIndirect(&MyFont);
				if ( GSplashScreenTitleTextFontHandle == NULL )
				{
					// Couldn't create font, so just use a system font
					GSplashScreenTitleTextFontHandle = SystemFontHandle;
				}
			}
		}

		// Setup bounds for game name
		GSplashScreenTextRects[ SplashTextType::GameName ].top = 10;
		GSplashScreenTextRects[ SplashTextType::GameName ].bottom = 60;
		GSplashScreenTextRects[ SplashTextType::GameName ].left = bm.bmWidth - 12;
		GSplashScreenTextRects[ SplashTextType::GameName ].right = 12;
		
		// Setup bounds for version info text 1
		GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].top = bm.bmHeight - 60;
		GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].bottom = bm.bmHeight - 40;
		GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].left = 10;
		GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].right = bm.bmWidth - 20;

		// Setup bounds for copyright info text
		if( GIsEditor )
		{
			GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].top = bm.bmHeight - 44;
			GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].bottom = bm.bmHeight - 34;
		}
		else
		{
			GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].top = bm.bmHeight - 16;
			GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].bottom = bm.bmHeight - 6;
		}
		GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].left = 10;
		GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].right = bm.bmWidth - 20;

		// Setup bounds for startup progress text
		GSplashScreenTextRects[ SplashTextType::StartupProgress ].top = bm.bmHeight - 20;
		GSplashScreenTextRects[ SplashTextType::StartupProgress ].bottom = bm.bmHeight;
		GSplashScreenTextRects[ SplashTextType::StartupProgress ].left = 10;
		GSplashScreenTextRects[ SplashTextType::StartupProgress ].right = bm.bmWidth - 20;

		if (GSplashScreenWnd)
		{
			SetWindowText(GSplashScreenWnd, *GSplashScreenAppName.ToString());
			ShowWindow(GSplashScreenWnd, SW_SHOW); 
			UpdateWindow(GSplashScreenWnd); 
		 
			const double FadeStartTime = FPlatformTime::Seconds();
			const float FadeDuration = 0.2f;
			int32 CurrentOpacityByte = 0;

			MSG message;
			bool bIsSplashFinished = false;
			while( !bIsSplashFinished )
			{
				if( PeekMessage(&message, NULL, 0, 0, PM_REMOVE) )
				{
					TranslateMessage(&message);
					DispatchMessage(&message);

					if( message.message == WM_QUIT )
					{
						bIsSplashFinished = true;
					}
				}

				// Update window opacity
				CA_SUPPRESS(6239)
				if( bAllowFading && CurrentOpacityByte < 255 )
				{
					// Set window to fully transparent to start out
					const float TimeSinceFadeStart = (float)( FPlatformTime::Seconds() - FadeStartTime );
					const float FadeAmount = FMath::Clamp( TimeSinceFadeStart / FadeDuration, 0.0f, 1.0f );
					const int32 NewOpacityByte = 255 * FadeAmount;
					if( NewOpacityByte != CurrentOpacityByte )
					{
						CurrentOpacityByte = NewOpacityByte;
						SetLayeredWindowAttributes( GSplashScreenWnd, 0, CurrentOpacityByte, LWA_ALPHA );
					}

					// We're still fading, but still yield a timeslice
					FPlatformProcess::Sleep( 0.0f );
				}
				else
				{
					// Give up some time
					FPlatformProcess::Sleep( 1.0f / 60.0f );
				}
			}
		}

		ensure(DeleteObject(GSplashScreenBitmap));
		GSplashScreenBitmap = NULL;
	}

	UnregisterClass(wc.lpszClassName, hInstance);
	return 0; 
}

/**
 * Sets the text displayed on the splash screen (for startup/loading progress)
 *
 * @param	InType		Type of text to change
 * @param	InText		Text to display
 */
static void StartSetSplashText( const SplashTextType::Type InType, const TCHAR* InText )
{
	// Only allow copyright text displayed while loading the game.  Editor displays all.
	GSplashScreenText[InType] = FText::FromString(InText);
}

void FWindowsPlatformSplash::Show()
{
	if( !GSplashScreenThread && FParse::Param(FCommandLine::Get(),TEXT("NOSPLASH")) != true )
	{
		const FText GameName = FText::FromString( FApp::GetProjectName() );

		const TCHAR* SplashImage = GIsEditor ? ( GameName.IsEmpty() ? TEXT("EdSplashDefault") : TEXT("EdSplash") ) : ( GameName.IsEmpty() ? TEXT("SplashDefault") : TEXT("Splash") );

		// make sure a splash was found
		FString SplashPath;
		bool IsCustom;
		if ( GetSplashPath(SplashImage, SplashPath, IsCustom ) == true )
		{
			// Don't set the game name if the splash screen is custom.
			if ( !IsCustom )
			{
				StartSetSplashText(SplashTextType::GameName, *GameName.ToString());
			}

			// In the editor, we'll display loading info
			if( GIsEditor )
			{
				// Set initial startup progress info
				{
					StartSetSplashText( SplashTextType::StartupProgress,
						*NSLOCTEXT("UnrealEd", "SplashScreen_InitialStartupProgress", "Loading..." ).ToString() );
				}

				// Set version info
				{
					const FText Version = FText::FromString( FEngineVersion::Current().ToString( FEngineBuildSettings::IsPerforceBuild() ? EVersionComponent::Branch : EVersionComponent::Patch ) );

					FText VersionInfo;
					FText AppName;
					if( GameName.IsEmpty() )
					{
						VersionInfo = FText::Format( NSLOCTEXT( "UnrealEd", "UnrealEdTitleWithVersionNoGameName_F", "Unreal Editor {0}" ), Version );
						AppName = NSLOCTEXT( "UnrealEd", "UnrealEdTitleNoGameName_F", "Unreal Editor" );
					}
					else
					{
						VersionInfo = FText::Format( NSLOCTEXT( "UnrealEd", "UnrealEdTitleWithVersion_F", "Unreal Editor {0}  -  {1}" ), Version, GameName );
						AppName = FText::Format( NSLOCTEXT( "UnrealEd", "UnrealEdTitle_F", "Unreal Editor - {0}" ), GameName );
					}

					StartSetSplashText( SplashTextType::VersionInfo1, *VersionInfo.ToString() );

					// Change the window text (which will be displayed in the taskbar)
					GSplashScreenAppName = AppName;
				}

				// Display copyright information in editor splash screen
				{
					const FString CopyrightInfo = NSLOCTEXT( "UnrealEd", "SplashScreen_CopyrightInfo", "Copyright \x00a9 1998-2017   Epic Games, Inc.   All rights reserved." ).ToString();
					StartSetSplashText( SplashTextType::CopyrightInfo, *CopyrightInfo );
				}
			}

			// Spawn a window to receive the Z-order swap when the splashscreen is destroyed.
			// This will prevent the main window from being sent to the background when the splash window closes.
			GSplashScreenGuard = CreateWindow(
				TEXT("STATIC"), 
				TEXT("SplashScreenGuard"),
				0,
				0,
				0,
				0,
				0,
				HWND_MESSAGE,
				(HMENU) NULL,
				hInstance,
				(LPVOID) NULL); 

			if (GSplashScreenGuard)
			{
				ShowWindow(GSplashScreenGuard, SW_SHOW); 
			}
			
			GSplashScreenFileName = SplashPath;
			DWORD ThreadID = 0;
			GSplashScreenThread = CreateThread(NULL, 128 * 1024, (LPTHREAD_START_ROUTINE)StartSplashScreenThread, (LPVOID)NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, &ThreadID);
#if	STATS
			FStartupMessages::Get().AddThreadMetadata( FName( "SplashScreenThread" ), ThreadID );
#endif // STATS
		}
	}
}

void FWindowsPlatformSplash::Hide()
{
	if(GSplashScreenThread)
	{
		if(GSplashScreenWnd)
		{
			// Send message to splash screen window to destroy itself
			PostMessage(GSplashScreenWnd, WM_DESTROY, 0, 0);
		}

		// Wait for splash screen thread to finish
		WaitForSingleObject(GSplashScreenThread, INFINITE);

		// Clean up
		CloseHandle(GSplashScreenThread);
		GSplashScreenThread = NULL;
		GSplashScreenWnd = NULL;

		// Close the Z-Order guard window
		if ( GSplashScreenGuard )
		{
			PostMessage(GSplashScreenGuard, WM_DESTROY, 0, 0);
			GSplashScreenGuard = NULL;
		}
	}
}


bool FWindowsPlatformSplash::IsShown()
{
	return (GSplashScreenThread != nullptr);
}

/**
 * Sets the text displayed on the splash screen (for startup/loading progress)
 *
 * @param	InType		Type of text to change
 * @param	InText		Text to display
 */
void FWindowsPlatformSplash::SetSplashText( const SplashTextType::Type InType, const TCHAR* InText )
{
	// We only want to bother drawing startup progress in the editor, since this information is
	// not interesting to an end-user (also, it's not usually localized properly.)
	if( GSplashScreenThread )
	{
		// Only allow copyright text displayed while loading the game.  Editor displays all.
		if( InType == SplashTextType::CopyrightInfo || GIsEditor )
		{
			// Take a critical section since the splash thread may already be repainting using this text
			FScopeLock ScopeLock(&GSplashScreenSynchronizationObject);

			bool bWasUpdated = false;
			{
				// Update splash text
				if( FCString::Strcmp( InText, *GSplashScreenText[ InType ].ToString() ) != 0 )
				{
					GSplashScreenText[ InType ] = FText::FromString( InText );
					bWasUpdated = true;
				}
			}

			if( bWasUpdated )
			{
				// Repaint the window
				const BOOL bErase = false;
				InvalidateRect( GSplashScreenWnd, &GSplashScreenTextRects[ InType ], bErase );
			}
		}
	}
}

#include "Windows/HideWindowsPlatformTypes.h"
