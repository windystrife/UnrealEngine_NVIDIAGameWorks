// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "Engine/Engine.h"
#include "Runtime/MovieSceneCapture/Public/MovieSceneCaptureHandle.h"
#include "GameEngine.generated.h"

class Error;
class FSceneViewport;
class UGameViewportClient;
class UNetDriver;

/**
 * Engine that manages core systems that enable a game.
 */
UCLASS(config=Engine, transient)
class ENGINE_API UGameEngine
	: public UEngine
{
	GENERATED_UCLASS_BODY()

	/** Maximium delta time the engine uses to populate FApp::DeltaTime. If 0, unbound. */
	UPROPERTY(config)
	float MaxDeltaTime;

	/** Maximium time (in seconds) between the flushes of the logs on the server (best effort). If 0, this will happen every tick. */
	UPROPERTY(config)
	float ServerFlushLogInterval;

	UPROPERTY(transient)
	UGameInstance* GameInstance;

public:

	/** The game viewport window */
	TWeakPtr<class SWindow> GameViewportWindow;
	/** The primary scene viewport */
	TSharedPtr<class FSceneViewport> SceneViewport;
	/** The game viewport widget */
	TSharedPtr<class SViewport> GameViewportWidget;
	
	/**
	 * Creates the viewport widget where the games Slate UI is added to.
	 *
	 * @param GameViewportClient	The viewport client to use in the game
	 * @param MovieCapture			Optional Movie capture implementation for this viewport
	 */
	void CreateGameViewportWidget( UGameViewportClient* GameViewportClient );

	/**
	 * Creates the game viewport
	 *
	 * @param GameViewportClient	The viewport client to use in the game
	 * @param MovieCapture			Optional Movie capture implementation for this viewport
	 */
	void CreateGameViewport( UGameViewportClient* GameViewportClient );
	
	/**
	 * Creates the game window
	 */
	static TSharedRef<SWindow> CreateGameWindow();

	static void SafeFrameChanged();

	/**
	 * Modifies the game window resolution settings if any overrides have been specified on the command line
	 *
	 * @param ResolutionX	[in/out] Width of the game window, in pixels
	 * @param ResolutionY	[in/out] Height of the game window, in pixels
	 * @param WindowMode	[in/out] What window mode the game should be in
	 */
	static void ConditionallyOverrideSettings( int32& ResolutionX, int32& ResolutionY, EWindowMode::Type& WindowMode );
	
	/**
	 * Determines the resolution of the game window, ensuring that the requested size is never bigger than the available desktop size
	 *
	 * @param ResolutionX	[in/out] Width of the game window, in pixels
	 * @param ResolutionY	[in/out] Height of the game window, in pixels
	 * @param WindowMode	[in/out] What window mode the game should be in
	 */
	static void DetermineGameWindowResolution( int32& ResolutionX, int32& ResolutionY, EWindowMode::Type& WindowMode );

	/**
	 * Changes the game window to use the game viewport instead of any loading screen
	 * or movie that might be using it instead
	 */
	void SwitchGameWindowToUseGameViewport();

	/**
	 * Called when the game window closes (ends the game)
	 */
	void OnGameWindowClosed( const TSharedRef<SWindow>& WindowBeingClosed );
	
	/**
	 * Called when the game window is moved
	 */
	void OnGameWindowMoved( const TSharedRef<SWindow>& WindowBeingMoved );

	/**
	 * Redraws all viewports.
	 * @param	bShouldPresent	Whether we want this frame to be presented
	 */
	virtual void RedrawViewports( bool bShouldPresent = true ) override;

public:

	// UObject interface

	virtual void FinishDestroy() override;

public:

	// UEngine interface

	virtual void Init(class IEngineLoop* InEngineLoop) override;
	virtual void Start() override;
	virtual void PreExit() override;
	virtual void Tick( float DeltaSeconds, bool bIdleMode ) override;
	virtual float GetMaxTickRate( float DeltaTime, bool bAllowFrameRateSmoothing = true ) const override;
	virtual void ProcessToggleFreezeCommand( UWorld* InWorld ) override;
	virtual void ProcessToggleFreezeStreamingCommand( UWorld* InWorld ) override;
	virtual bool NetworkRemapPath(UNetDriver* Driver, FString& Str, bool bReading = true) override;
	virtual bool ShouldDoAsyncEndOfFrameTasks() const override;

public:

	// FExec interface

	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog ) override;

public:

	// Exec command handlers
	bool HandleCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleExitCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleMinimizeCommand( const TCHAR *Cmd, FOutputDevice &Ar );
	bool HandleGetMaxTickRateCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleCancelCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );

#if !UE_BUILD_SHIPPING
	bool HandleApplyUserSettingsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif // !UE_BUILD_SHIPPING

	/** Returns the GameViewport widget */
	virtual TSharedPtr<SViewport> GetGameViewportWidget() const override
	{
		return GameViewportWidget;
	}

	/**
	 * This is a global, parameterless function used by the online subsystem modules.
	 * It should never be used in gamecode - instead use the appropriate world context function 
	 * in order to properly support multiple concurrent UWorlds.
	 */
	UWorld* GetGameWorld();

protected:

	const FSceneViewport* GetGameSceneViewport(UGameViewportClient* ViewportClient) const;

	/** Handle to a movie capture implementation to create on startup */
	FMovieSceneCaptureHandle StartupMovieCaptureHandle;

	virtual void HandleBrowseToDefaultMapFailure(FWorldContext& Context, const FString& TextURL, const FString& Error) override;

private:

	virtual void HandleNetworkFailure_NotifyGameInstance(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType) override;
	virtual void HandleTravelFailure_NotifyGameInstance(UWorld* World, ETravelFailure::Type FailureType) override;

	/** Last time the logs have been flushed. */
	double LastTimeLogsFlushed;
};
