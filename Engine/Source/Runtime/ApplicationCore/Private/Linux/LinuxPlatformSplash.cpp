// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxPlatformSplash.h"

#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Misc/EngineVersionBase.h"
#include "Containers/Array.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/EngineBuildSettings.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformApplicationMisc.h"

#if WITH_EDITOR

#include "SDL.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

#include "ft2build.h"
#include FT_FREETYPE_H


/**
 * Splash screen functions and static globals
 */

struct Rect
{
	Rect(const int32 t = 0,const int32 l = 0, const int32 r = 0, const int32 b = 0) : Top(t), Left(l), Right(r), Bottom(b) {}
	int32 Top;
	int32 Left;
	int32 Right;
	int32 Bottom;
};


class FLinuxSplashState
{
public:
	~FLinuxSplashState();
	bool InitSplashResources(const FText &AppName, const FString &SplashPath, const FString &IconPath);
	void SetSplashText(const SplashTextType::Type InType, const FText& InText);
	void Pump();

private:
	static SDL_Surface* LoadImage(const FString &InImagePath);
	void DrawCharacter(int32 penx, int32 peny, FT_GlyphSlot Glyph, int32 CurTypeIndex, float Red, float Green, float Blue);
	void RenderStrings();
	bool OpenFonts();
	void Redraw();

	FT_Library FontLibrary = nullptr;
	FT_Face FontSmall = nullptr;
	FT_Face FontNormal = nullptr;
	FT_Face FontLarge = nullptr;

	SDL_Surface *SplashSurface = nullptr;
	SDL_Window *SplashWindow = nullptr;
	SDL_Renderer *SplashRenderer = nullptr;
	SDL_Texture *SplashTexture = nullptr;
	FText SplashText[ SplashTextType::NumTextTypes ];
	Rect SplashTextRects[ SplashTextType::NumTextTypes ];

	unsigned char *ScratchSpace = nullptr;
	bool bNeedsRedraw = false;
	bool bStringsChanged = false;
};

static FLinuxSplashState *GSplashState = nullptr;


FLinuxSplashState::~FLinuxSplashState()
{
	// Just in case SDL's renderer steps on GL state...
	SDL_Window *CurrentWindow = SDL_GL_GetCurrentWindow();
	SDL_GLContext CurrentContext = SDL_GL_GetCurrentContext();

	if (SplashSurface)
	{
		SDL_FreeSurface(SplashSurface);
	}

	if (SplashTexture)
	{
		SDL_DestroyTexture(SplashTexture);
	}

	if (SplashRenderer)
	{
		SDL_DestroyRenderer(SplashRenderer);
	}

	if (SplashWindow)
	{
		SDL_DestroyWindow(SplashWindow);
	}

	if (ScratchSpace)
	{
		FMemory::Free(ScratchSpace);
	}

	if (FontSmall)
	{
		FT_Done_Face(FontSmall);
	}

	if (FontNormal)
	{
		FT_Done_Face(FontNormal);
	}

	if (FontLarge)
	{
		FT_Done_Face(FontLarge);
	}

	if (FontLibrary)
	{
		FT_Done_FreeType(FontLibrary);
	}

	if (CurrentWindow)  // put back any old GL state...
	{
		SDL_GL_MakeCurrent(CurrentWindow, CurrentContext);
	}

	// do not deinit SDL here
}

//---------------------------------------------------------
bool FLinuxSplashState::OpenFonts()
{
	if (FT_Init_FreeType(&FontLibrary))
	{
		UE_LOG(LogHAL, Error, TEXT("*** Unable to initialize font library."));
		return false;
	}

	// small font face
	FString FontPath = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Light.ttf"));
	
	if (FT_New_Face(FontLibrary, TCHAR_TO_UTF8(*FontPath), 0, &FontSmall))
	{
		UE_LOG(LogHAL, Error, TEXT("*** Unable to open small font face for splash screen."));
	}
	else
	{
		FT_Set_Pixel_Sizes(FontSmall, 0, 10);
	}

	// normal font face
	FontPath = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"));
	
	if (FT_New_Face(FontLibrary, TCHAR_TO_UTF8(*FontPath), 0, &FontNormal))
	{
		UE_LOG(LogHAL, Error, TEXT("*** Unable to open normal font face for splash screen."));
	}
	else
	{
		FT_Set_Pixel_Sizes(FontNormal, 0, 12);
	}
	
	// large font face
	FontPath = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"));
	
	if (FT_New_Face(FontLibrary, TCHAR_TO_UTF8(*FontPath), 0, &FontLarge))
	{
		UE_LOG(LogHAL, Error, TEXT("*** Unable to open large font face for splash screen."));		
	}
	else
	{
		FT_Set_Pixel_Sizes(FontLarge, 0, 40);
	}

	return true;
}


//---------------------------------------------------------
void FLinuxSplashState::DrawCharacter(int32 PenX, int32 PenY, FT_GlyphSlot Glyph, int32 CurTypeIndex, float Red, float Green, float Blue)
{
	// drawing boundaries
	const int32 MinX = SplashTextRects[CurTypeIndex].Left;
	const int32 MaxX = SplashTextRects[CurTypeIndex].Right;
	const int32 MaxY = SplashTextRects[CurTypeIndex].Bottom;
	const int32 MinY = SplashTextRects[CurTypeIndex].Top;

	// glyph dimensions
	const int32 GlyphWidth = Glyph->bitmap.width;
	const int32 GlyphHeight = Glyph->bitmap.rows;
	const int32 GlyphPitch = Glyph->bitmap.pitch;

	unsigned char *Pixels = Glyph->bitmap.buffer;

	const int SplashWidth = SplashSurface->w;
	const int SplashBPP = SplashSurface->format->BytesPerPixel;

	// draw glyph raster to texture
	for (int GlyphY = 0; GlyphY < GlyphHeight; GlyphY++)
	{
		for (int GlyphX = 0; GlyphX < GlyphWidth; GlyphX++)
		{
			// find pixel position in splash image
			const int PosX = PenX + GlyphX + (Glyph->metrics.horiBearingX >> 6);
			const int PosY = PenY + GlyphY - (Glyph->metrics.horiBearingY >> 6);
			
			// make sure pixel is in drawing rectangle
			if (PosX < MinX || PosX >= MaxX || PosY < MinY || PosY >= MaxY)
				continue;

			// get index of pixel in glyph bitmap			
			const int32 SourceIndex = (GlyphY * GlyphPitch) + GlyphX;
			int32 DestIndex = (PosY * SplashWidth + PosX) * SplashBPP;

			// write pixel
			const float Alpha = Pixels[SourceIndex] / 255.0f;

			ScratchSpace[DestIndex] = ScratchSpace[DestIndex]*(1.0 - Alpha) + Alpha*Red;
			DestIndex++;
			ScratchSpace[DestIndex] = ScratchSpace[DestIndex]*(1.0 - Alpha) + Alpha*Green;
			DestIndex++;
			ScratchSpace[DestIndex] = ScratchSpace[DestIndex]*(1.0 - Alpha) + Alpha*Blue;
		}
	}
}


//---------------------------------------------------------
void FLinuxSplashState::RenderStrings()
{
	FT_UInt LastGlyph = 0;
	FT_Vector Kerning;
	bool bRightJustify = false;
	float Red, Blue, Green; // font color

	if (!bStringsChanged)
	{
		return;
	}

	bStringsChanged = false;
	bNeedsRedraw = true;

	const int SplashWidth = SplashSurface->w;
	const int SplashHeight =  SplashSurface->h;
	const int SplashBPP = SplashSurface->format->BytesPerPixel;

	// clear the rendering scratch pad.
	FMemory::Memcpy(ScratchSpace, SplashSurface->pixels, SplashWidth*SplashHeight*SplashBPP);

	// draw each type of string
	for (int CurTypeIndex=0; CurTypeIndex<SplashTextType::NumTextTypes; CurTypeIndex++)
	{
		FT_Face Font = nullptr;
		
		// initial pen position
		uint32 PenX = SplashTextRects[ CurTypeIndex].Left;
		uint32 PenY = SplashTextRects[ CurTypeIndex].Bottom;
		Kerning.x = 0;
		
		// Set font color and text position
		if (CurTypeIndex == SplashTextType::StartupProgress)
		{
			Red = Green = Blue = 200.0f;
			Font = FontSmall;
		}
		else if (CurTypeIndex == SplashTextType::VersionInfo1)
		{
			Red = Green = Blue = 240.0f;
			Font = FontNormal;
		}
		else if (CurTypeIndex == SplashTextType::GameName)
		{
			Red = Green = Blue = 240.0f;
			Font = FontLarge;
			PenX = SplashTextRects[ CurTypeIndex].Right;
			bRightJustify = true;
		}
		else
		{
			Red = Green = Blue = 160.0f;
			Font = FontSmall;
		}
		
		// sanity check: make sure we have a font loaded.
		if (!Font)
		{
			continue;
		}

		// adjust verticle pos to allow for descenders
		PenY += Font->descender >> 6;

		// convert strings to glyphs and place them in bitmap.
		{
			FString Text = SplashText[CurTypeIndex].ToString();

			for (int i=0; i<Text.Len(); i++)
			{
				FT_ULong CharacterCode;
				
				// fetch next glyph
				if (bRightJustify)
				{
					CharacterCode = (Uint32)(Text[Text.Len() - i - 1]);
				}
				else
				{
					CharacterCode = (Uint32)(Text[i]);				
				}
							
				FT_UInt GlyphIndex = FT_Get_Char_Index(Font, CharacterCode);
				FT_Load_Glyph(Font, GlyphIndex, FT_LOAD_DEFAULT);

				
				if (Font->glyph->format != FT_GLYPH_FORMAT_BITMAP)
				{
					FT_Render_Glyph(Font->glyph, FT_RENDER_MODE_NORMAL);
				}

				// pen advance and kerning
				if (bRightJustify)
				{
					if (LastGlyph != 0)
					{
						FT_Get_Kerning(Font, GlyphIndex, LastGlyph, FT_KERNING_DEFAULT, &Kerning);
					}
					
					PenX -= (Font->glyph->metrics.horiAdvance - Kerning.x) >> 6;	
				}
				else
				{
					if (LastGlyph != 0)
					{
						FT_Get_Kerning(Font, LastGlyph, GlyphIndex, FT_KERNING_DEFAULT, &Kerning);
					}	
				}

				LastGlyph = GlyphIndex;

				// draw character
				DrawCharacter(PenX, PenY, Font->glyph, CurTypeIndex, Red, Green, Blue);
							
				if (!bRightJustify)
				{
					PenX += (Font->glyph->metrics.horiAdvance - Kerning.x) >> 6;
				}
			}
		}

		// store rendered text as texture
		SDL_UpdateTexture(SplashTexture, NULL, ScratchSpace, SplashWidth * SplashBPP);
	}
}

/**
 * @brief Helper function to load an image in any format.
 *
 * @param ImagePath an (absolute) path to the image
 *
 * @return Splash surface
 */
SDL_Surface* FLinuxSplashState::LoadImage(const FString &ImagePath)
{
	TArray<uint8> RawFileData;

	// Load the image buffer first (unless it's BMP)
	if (!ImagePath.EndsWith(TEXT("bmp"), ESearchCase::IgnoreCase) && FFileHelper::LoadFileToArray(RawFileData, *ImagePath))
	{
		auto& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		auto Format = ImageWrapperModule.DetectImageFormat(RawFileData.GetData(), RawFileData.Num());
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);

		if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
		{
			const TArray<uint8>* RawData = nullptr;
			if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
			{
				SDL_Surface *Surface = SDL_CreateRGBSurfaceWithFormat(0, ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), 32, SDL_PIXELFORMAT_BGRA8888);
				if (Surface)
				{
					const int Width = Surface->w;
					const int Height =  Surface->h;
					const int BytesPerPixel = Surface->format->BytesPerPixel;
					FMemory::Memcpy(Surface->pixels, (void*)RawData->GetData(), Width * Height * BytesPerPixel);
					return Surface;
				}
			}
		}
	}

	// If for some reason the image cannot be loaded, use the default BMP function
	return SDL_LoadBMP(TCHAR_TO_UTF8(*ImagePath));
}


// This class helps cleans up SDL_Surfaces when they go out of scope.
class SdlSurfacePtr
{
public:
	SdlSurfacePtr(SDL_Surface *InSurface) : Surface(InSurface) {}
	~SdlSurfacePtr() { if (Surface) { SDL_FreeSurface(Surface); } }
private:
	SDL_Surface *Surface;
};


/** Helper function to init resources used by the splash window */
bool FLinuxSplashState::InitSplashResources(const FText &AppName, const FString &SplashPath, const FString &IconPath)
{
	checkf(SplashWindow == nullptr, TEXT("FLinuxSplashState::InitSplashResources() has been called multiple times."));

	if (!FPlatformApplicationMisc::InitSDL()) //	will not initialize more than once
	{
		UE_LOG(LogInit, Warning, TEXT("FLinuxSplashState::InitSplashResources() : InitSDL() failed, there will be no splash."));
		return false;
	}

	// load splash .bmp image
	SplashSurface = LoadImage(SplashPath);
	if (SplashSurface == nullptr)
	{
		UE_LOG(LogHAL, Warning, TEXT("FLinuxSplashState::InitSplashResources() : Could not load splash BMP! SDL_Error: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		return false;
	}

	const int SplashWidth = SplashSurface->w;
	const int SplashHeight =  SplashSurface->h;
	const int SplashBPP = SplashSurface->format->BytesPerPixel;

	if (SplashWidth <= 0 || SplashHeight <= 0)
	{
		UE_LOG(LogHAL, Warning, TEXT("Invalid splash image dimensions."));
		return -1;
	}

	SDL_Surface *SplashIconImage = LoadImage(IconPath);
	SdlSurfacePtr SplashIconPtr(SplashIconImage);
	if (SplashIconImage == nullptr)
	{
		UE_LOG(LogHAL, Warning, TEXT("FLinuxSplashState::InitSplashResources() : Splash icon could not be created! SDL_Error: %s"), UTF8_TO_TCHAR(SDL_GetError()));
	}

	// Just in case SDL's renderer steps on GL state...
	SDL_Window *CurrentWindow = SDL_GL_GetCurrentWindow();
	SDL_GLContext CurrentContext = SDL_GL_GetCurrentContext();

	// on modern X11, your windows might turn gray if they don't pump the event queue fast enough.
	// But this is because they opt-in to an optional window manager protocol by default; legacy
	// apps and those that know they'll be slow to respond to events--like splash screens--can
	// just choose to not support the protocol. Since we're a splash screen, it doesn't matter if
	// we would be unresponsive, since we accept no input. So don't opt-in.
	// We pump the event queue during SetSplashText, but it might be at a slow rate.
	const char *_OriginalHint = SDL_GetHint(SDL_HINT_VIDEO_X11_NET_WM_PING);  // (SDL_SetHint will free this pointer.)
	char *OriginalHint = _OriginalHint ? SDL_strdup(_OriginalHint) : NULL;
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_PING, "0");
	SplashWindow = SDL_CreateWindow(TCHAR_TO_UTF8(*AppName.ToString()),
									SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
									SplashWidth, SplashHeight,
									SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN
									);

	if (SplashWindow == nullptr)
	{
		UE_LOG(LogHAL, Error, TEXT("FLinuxSplashState::InitSplashResources() : Splash screen window could not be created! SDL_Error: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_PING, OriginalHint ? OriginalHint : "1");
		SDL_free(OriginalHint);
		return false;
	}

	if (SplashIconImage != nullptr)
	{
		SDL_SetWindowIcon(SplashWindow, SplashIconImage);  // SDL_SetWindowIcon() makes a copy of this.
	}

	SplashRenderer = SDL_CreateRenderer(SplashWindow, -1, 0);

	// it's safe to set the hint back once the Renderer is created (since it might recreate the window to get a GL or whatever visual).
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_PING, OriginalHint ? OriginalHint : "1");
	SDL_free(OriginalHint);

	if (SplashRenderer == nullptr)
	{
		UE_LOG(LogHAL, Error, TEXT("FLinuxSplashState::InitSplashResources() : Splash screen renderer could not be created! SDL_Error: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		return false;
	}

	SplashTexture = SDL_CreateTexture(SplashRenderer, SplashSurface->format->format, SDL_TEXTUREACCESS_STATIC, SplashSurface->w, SplashSurface->h);
	if (SplashTexture == nullptr)
	{
		UE_LOG(LogHAL, Error, TEXT("FLinuxSplashState::InitSplashResources() : Splash screen texture could not be created! SDL_Error: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		return false;
	}

	// allocate scratch space for rendering text
	ScratchSpace = reinterpret_cast<unsigned char *>(FMemory::Malloc(SplashHeight*SplashWidth*SplashBPP));

	// Setup bounds for game name
	SplashTextRects[ SplashTextType::GameName ].Top = 0;
	SplashTextRects[ SplashTextType::GameName ].Bottom = 50;
	SplashTextRects[ SplashTextType::GameName ].Left = 12;
	SplashTextRects[ SplashTextType::GameName ].Right = SplashWidth - 12;

	// Setup bounds for version info text 1
	SplashTextRects[ SplashTextType::VersionInfo1 ].Top = SplashHeight - 60;
	SplashTextRects[ SplashTextType::VersionInfo1 ].Bottom = SplashHeight - 40;
	SplashTextRects[ SplashTextType::VersionInfo1 ].Left = 10;
	SplashTextRects[ SplashTextType::VersionInfo1 ].Right = SplashWidth - 10;

	// Setup bounds for copyright info text
	if (GIsEditor)
	{
		SplashTextRects[ SplashTextType::CopyrightInfo ].Top = SplashHeight - 44;
		SplashTextRects[ SplashTextType::CopyrightInfo ].Bottom = SplashHeight - 24;
	}
	else
	{
		SplashTextRects[ SplashTextType::CopyrightInfo ].Top = SplashHeight - 16;
		SplashTextRects[ SplashTextType::CopyrightInfo ].Bottom = SplashHeight - 6;
	}

	SplashTextRects[ SplashTextType::CopyrightInfo ].Left = 10;
	SplashTextRects[ SplashTextType::CopyrightInfo ].Right = SplashWidth - 20;

	// Setup bounds for startup progress text
	SplashTextRects[ SplashTextType::StartupProgress ].Top = SplashHeight - 20;
	SplashTextRects[ SplashTextType::StartupProgress ].Bottom = SplashHeight;
	SplashTextRects[ SplashTextType::StartupProgress ].Left = 10;
	SplashTextRects[ SplashTextType::StartupProgress ].Right = SplashWidth - 20;

	OpenFonts();

	bStringsChanged = true;
	RenderStrings();
	SDL_ShowWindow(SplashWindow);
	Redraw();

	if (CurrentWindow)  // put back any old GL state...
	{
		SDL_GL_MakeCurrent(CurrentWindow, CurrentContext);
	}

	return true;
}

/**
 * Sets the text displayed on the splash screen (for startup/loading progress)
 *
 * @param	InType		Type of text to change
 * @param	InText		Text to display
 */
void FLinuxSplashState::SetSplashText( const SplashTextType::Type InType, const FText& InText )
{
#if WITH_EDITOR
	if (!InText.EqualTo(SplashText[ InType ]))
	{
		SplashText[ InType ] = InText;
		bStringsChanged = true;
	}
#endif
}

void FLinuxSplashState::Redraw()
{
	if (bNeedsRedraw || bStringsChanged)
	{
		// Just in case SDL's renderer steps on GL state...
		SDL_Window *CurrentWindow = SDL_GL_GetCurrentWindow();
		SDL_GLContext CurrentContext = SDL_GL_GetCurrentContext();

		RenderStrings();
		SDL_RenderCopy(SplashRenderer, SplashTexture, NULL, NULL);
		SDL_RenderPresent(SplashRenderer);
		bNeedsRedraw = false;

		if (CurrentWindow)  // put back any old GL state...
		{
			SDL_GL_MakeCurrent(CurrentWindow, CurrentContext);
		}
	}
}

void FLinuxSplashState::Pump()
{
	if (SplashWindow)
	{
		SDL_Event Event;
		while (SDL_PollEvent(&Event))
		{
			if ((Event.type == SDL_WINDOWEVENT) && (Event.window.event == SDL_WINDOWEVENT_EXPOSED) && (SDL_GetWindowID(SplashWindow) == Event.window.windowID))
			{
				bNeedsRedraw = true;
			}
		}
		Redraw();
	}
}
#endif //WITH_EDITOR


/**
 * Open a splash screen if there's not one already and if it's not disabled.
 *
 */
void FLinuxPlatformSplash::Show( )
{
#if WITH_EDITOR
	// need to do a splash screen?
	if(GSplashState || FParse::Param(FCommandLine::Get(),TEXT("NOSPLASH")) == true)
	{
		return;
	}

	// decide on which splash screen to show
	const FText GameName = FText::FromString(FApp::GetProjectName());

	bool IsCustom = false;

	// first look for the splash, do not init anything if not found
	FString SplashPath;
	{
		const TCHAR* SplashImage = GIsEditor ? (GameName.IsEmpty() ? TEXT("EdSplashDefault") : TEXT("EdSplash")) : (GameName.IsEmpty() ? TEXT("SplashDefault") : TEXT("Splash"));
		if (!GetSplashPath(SplashImage, SplashPath, IsCustom))
		{
			UE_LOG(LogHAL, Warning, TEXT("Splash screen image not found."));
			return;	// early out
		}
	}

	// look for the icon separately, also avoid initialization if not found
	FString IconPath;
	{
		const TCHAR* EditorIcons[] =
		{
			TEXT("EdIcon"),
			TEXT("EdIconDefault"),
			nullptr
		};

		const TCHAR* GameIcons[] =
		{
			TEXT("Icon"),
			TEXT("IconDefault"),
			nullptr
		};

		const TCHAR** IconsToTry = GIsEditor? EditorIcons : GameIcons;
		bool bIconFound = false;
		for (const TCHAR* IconImage = *IconsToTry; IconImage != nullptr && !bIconFound; IconImage = *(++IconsToTry))
		{
			bool bDummy;
			if (GetSplashPath(IconImage, IconPath, bDummy))
			{
				bIconFound = true;
			}
		}

		if (!bIconFound)
		{
			UE_LOG(LogHAL, Warning, TEXT("Game icon not found."));
			return;	// early out
		}
	}

	GSplashState = new FLinuxSplashState;

	// Don't set the game name if the splash screen is custom.
	if ( !IsCustom )
	{
		GSplashState->SetSplashText( SplashTextType::GameName, GameName );
	}

	// In the editor, we'll display loading info
	FText AppName;
	if( GIsEditor )
	{
		// Set initial startup progress info
		{
			GSplashState->SetSplashText( SplashTextType::StartupProgress,
				NSLOCTEXT("UnrealEd", "SplashScreen_InitialStartupProgress", "Loading..." ) );
		}

		// Set version info
		{
			const FText Version = FText::FromString( FEngineVersion::Current().ToString( FEngineBuildSettings::IsPerforceBuild() ? EVersionComponent::Branch : EVersionComponent::Patch ) );

			FText VersionInfo;
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

			GSplashState->SetSplashText( SplashTextType::VersionInfo1, VersionInfo );
		}

		// Display copyright information in editor splash screen
		{
			const FText CopyrightInfo = NSLOCTEXT( "UnrealEd", "SplashScreen_CopyrightInfo", "Copyright \x00a9 1998-2017   Epic Games, Inc.   All rights reserved." );
			GSplashState->SetSplashText( SplashTextType::CopyrightInfo, CopyrightInfo );
		}
	}

	if (!GSplashState->InitSplashResources(AppName, SplashPath, IconPath))
	{
		delete GSplashState;
		GSplashState = nullptr;
	}
#endif //WITH_EDITOR
}


/**
* Done with splash screen. Close it and clean up.
*/
void FLinuxPlatformSplash::Hide()
{
#if WITH_EDITOR
	delete GSplashState;
	GSplashState = nullptr;
#endif
}


/**
* Sets the text displayed on the splash screen (for startup/loading progress)
*
* @param	InType		Type of text to change
* @param	InText		Text to display
*/
void FLinuxPlatformSplash::SetSplashText( const SplashTextType::Type InType, const TCHAR* InText )
{
#if WITH_EDITOR
	if (GSplashState)
	{
		// We only want to bother drawing startup progress in the editor, since this information is
		// not interesting to an end-user (also, it's not usually localized properly.)
		if (InType == SplashTextType::CopyrightInfo || GIsEditor)
		{
			GSplashState->SetSplashText(InType, FText::FromString(InText));
		}
		GSplashState->Pump();
	}

#endif //WITH_EDITOR
}
