// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "GenericPlatform/GenericWindow.h"


#include "SDL.h"

class FLinuxApplication;
struct FGenericWindowDefinition;

DECLARE_LOG_CATEGORY_EXTERN( LogLinuxWindow, Log, All );
DECLARE_LOG_CATEGORY_EXTERN( LogLinuxWindowType, Log, All );
DECLARE_LOG_CATEGORY_EXTERN( LogLinuxWindowEvent, Log, All );

typedef SDL_Window*		SDL_HWindow;

enum class EWindowActivationPolicy;

/**
 * A platform specific implementation of FNativeWindow.
 * Native Windows provide platform-specific backing for and are always owned by an SWindow.
 */
class APPLICATIONCORE_API FLinuxWindow : public FGenericWindow
{
public:
	~FLinuxWindow();

	/** Create a new SDLWindow.
	 *
	 * @param SlateWindows		List of all top level Slate windows.  This function will add the owner window to this list.
	 * @param OwnerWindow		The SlateWindow for which we are crating a backing Win32Window
	 * @param InHInstance		Win32 application instance handle
	 * @param InParent			Parent Win32 window; usually NULL.
	 * @param bShowImmediately	True to show this window as soon as its initialized
	 */
	static TSharedRef< FLinuxWindow > Make();

	SDL_HWindow GetHWnd() const;

	void Initialize( class FLinuxApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FLinuxWindow >& InParent, const bool bShowImmediately );

	/** TODO: describe */
	bool IsRegularWindow() const;

	/** TODO: describe */
	bool IsPopupMenuWindow() const;

	/** TODO: describe */
	bool IsTooltipWindow() const;

	/** TODO: describe */
	bool IsNotificationWindow() const;
	
	/** TODO: describe */
	bool IsTopLevelWindow() const;

	/** TODO: describe */
	bool IsDialogWindow() const;

	/** TODO: describe */
	bool IsDragAndDropWindow() const;
	
	/** TODO: describe */
	bool IsUtilityWindow() const;

	DEPRECATED(4.16, "IsActivateWhenFirstShown() is deprecated. Please use GetActivationPolicy() instead")
	bool IsActivateWhenFirstShown() const;

	/** @return the window activation policy used when showing the window */
	EWindowActivationPolicy GetActivationPolicy() const;
	
	/** TODO: describe */
	bool IsFocusWhenFirstShown() const;

	/** TODO: describe */
	const TSharedPtr< FLinuxWindow >& GetParent() const;

	/** Internal ID using for debugging */
	uint32 GetID() const;

	/** Debugging function - dumps window info to log */
	void LogInfo();

	bool IsNativeMoving() const;
	bool BeginNativeMove();
	void AfterNativeMove();
	void EndNativeMove();

	enum NativeResizeDirection 
	{
		ResizeSouthWest = 0,
		ResizeSouth,
		ResizeSouthEast,
		ResizeEast,
		ResizeNorthEast,
		ResizeNorth,
		ResizeNorthWest,
		ResizeWest,

		InvalidDirection
	};

	bool IsNativeResizing() const;
	bool BeginNativeResize( NativeResizeDirection Direction );
	void AfterNativeResize();
	void EndNativeResize();

public:
	virtual void ReshapeWindow( int32 X, int32 Y, int32 Width, int32 Height ) override;

	virtual void* GetOSWindowHandle() const  override { return HWnd; }

	//	not finished
	virtual bool GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const override;

	virtual void MoveWindowTo ( int32 X, int32 Y ) override;

	virtual void BringToFront( bool bForce = false ) override;

	virtual void Destroy() override;

	virtual void Minimize() override;

	virtual void Maximize() override;

	virtual void Restore() override;

	virtual void Show() override;

	virtual void Hide() override;

	virtual void SetWindowMode( EWindowMode::Type NewWindowMode ) override;

	virtual EWindowMode::Type GetWindowMode() const override { return WindowMode; } 

	virtual bool IsMaximized() const override;

	virtual bool IsVisible() const override;

	virtual bool GetRestoredDimensions(int32& X, int32& Y, int32& Width, int32& Height) override;

	virtual void SetWindowFocus() override;

	virtual void SetOpacity( const float InOpacity ) override;

	virtual void Enable( bool bEnable ) override;

	virtual bool IsPointInWindow( int32 X, int32 Y ) const override;

	virtual int32 GetWindowBorderSize() const override;

	virtual bool IsForegroundWindow() const override;
	/**
	 * Sets the window text - usually the title but can also be text content for things like controls
	 *
	 * @param Text	The window's title or content text
	 */
	virtual void SetText(const TCHAR* const Text) override;

	/** @return	Gives the native window a chance to adjust our stored window size before we cache it off */
	virtual void AdjustCachedSize( FVector2D& Size ) const override;

	virtual float GetDPIScaleFactor() const override
	{
		return DPIScaleFactor;
	}

	void SetDPIScaleFactor(float Value)
	{
		DPIScaleFactor = Value;
	}

	/**
	 * Returns window border sizes (frame extents).
	 * 
	 * @param BorderWidth - width of a vertical border (left side)
	 * @param BorderHeight - height of a horizontal border (from the top)
	 */
	void GetNativeBordersSize(int32& OutLeftBorderWidth, int32& OutTopBorderHeight) const;

	/**
	 *  Caches window properties that are too expensive to query all the time.
	 *  (e.g. border sizes)
	 */
	void CacheNativeProperties();

private:

	/**
	 * Protect the constructor; only TSharedRefs of this class can be made.
	 */
	FLinuxWindow();

private:

	FLinuxApplication* OwningApplication;

	/** linux window handle */
	SDL_HWindow HWnd;

	/** Store the window region size for querying whether a point lies within the window */
	int32 RegionWidth;
	int32 RegionHeight;
	
	/** The mode that the window is in (windowed, fullscreen, windowedfullscreen ) */
	EWindowMode::Type WindowMode;

	RECT PreFullscreenWindowRect;

	/** Virtual width and height of the window.  This is only different than the actual width and height for
	    windows which we're trying to optimize because their size changes frequently.  We'll create a larger
		window and have Windows draw it "cropped" so that it appears smaller, rather than actually resizing
		it and incurring a GPU buffer resize performance hit */
	int32 VirtualWidth;
	int32 VirtualHeight;

	/** TODO: describe */
	bool bIsVisible;

	/** TODO: describe */
	bool bWasFullscreen;

	/** TODO: describe */
	bool bIsPopupWindow;

	/** TODO: describe */
	bool bIsTooltipWindow;

	/** TODO: describe */
	bool bIsConsoleWindow;

	/** TODO: describe */
	bool bIsDialogWindow;

	/** TODO: describe */
	bool bIsNotificationWindow;

	/** TODO: describe */
	bool bIsTopLevelWindow;

	/** TODO: describe */
	bool bIsDragAndDropWindow;

	/** TODO: describe */
	bool bIsUtilityWindow;

	/** Whether the is inside this window */
	bool bIsPointerInsideWindow;

	/** SDL ID (for debugging purposes) */
	uint32 WindowSDLID;

	/** Parent window as given by Slate */
	TSharedPtr< FLinuxWindow > ParentWindow;
	static SDL_HitTestResult HitTest( SDL_Window *SDLwin, const SDL_Point *point, void *data );

	/** Cached width of the left border */
	int32 LeftBorderWidth;

	/** Cached height of the top border */
	int32 TopBorderHeight;

	/** Whether native properties cache is valid */
	bool bValidNativePropertiesCache;

	/**
	 * Ratio of pixels to SlateUnits in this window.
	 * E.g. DPIScale of 2.0 means there is a 2x2 pixel square for every 1x1 SlateUnit.
	 */
	float DPIScaleFactor;
};
