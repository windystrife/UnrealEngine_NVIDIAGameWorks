// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// LocalPlayer
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "Input/Reply.h"
#include "Engine/GameViewportClient.h"
#include "UObject/CoreOnline.h"
#include "SceneTypes.h"
#include "Engine/Player.h"
#include "LocalPlayer.generated.h"


#define INVALID_CONTROLLERID 255

class AActor;
class FSceneView;
class UGameInstance;
class ULocalPlayer;
struct FMinimalViewInfo;
struct FSceneViewProjectionData;

/** A context object that binds to a LocalPlayer. Useful for UI or other things that need to pass around player references */
struct ENGINE_API FLocalPlayerContext
{
	FLocalPlayerContext();
	FLocalPlayerContext(const class ULocalPlayer* InLocalPlayer, UWorld* InWorld = nullptr);
	FLocalPlayerContext(const class APlayerController* InPlayerController);
	FLocalPlayerContext(const FLocalPlayerContext& InPlayerContext);

	/** Is this context initialized and still valid? */
	bool IsValid() const;

	/** Is this context initialized */
	bool IsInitialized() const;

	/** This function tests if the given Actor is connected to the Local Player in any way. 
		It tests against the APlayerController, APlayerState, and APawn. */
	bool IsFromLocalPlayer(const AActor* ActorToTest) const;

	/** Returns the world context. */
	UWorld* GetWorld() const;

	/** Returns the local player. */
	class ULocalPlayer* GetLocalPlayer() const;

	/** Returns the player controller. */
	class APlayerController* GetPlayerController() const;

	/** Templated version of GetPlayerController() */
	template<class T>
	FORCEINLINE T* GetPlayerController(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{ 
			return CastChecked<T>(GetPlayerController(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetPlayerController());
		}
	}

	/** Getter for the Game State Base */
	class AGameStateBase* GetGameState() const;

	/** Templated Getter for the Game State */
	template<class T>
	FORCEINLINE T* GetGameState(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{
			return CastChecked<T>(GetGameState(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetGameState());
		}
	}

	/** Getter for the Player State */
	class APlayerState* GetPlayerState() const;

	/** Templated Getter for the Player State */
	template<class T>
	FORCEINLINE T* GetPlayerState(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{
			return CastChecked<T>(GetPlayerState(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetPlayerState());
		}
	}

	/** Getter for this player's HUD */
	class AHUD* GetHUD() const;

	/** Templated Getter for the HUD */
	template<class T>
	FORCEINLINE T* GetHUD(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{
			return CastChecked<T>(GetHUD(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetHUD());
		}
	}

	/** Getter for the base pawn of this player */
	class APawn* GetPawn() const;

	/** Templated getter for the player's pawn */
	template<class T>
	FORCEINLINE T* GetPawn(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{
			return CastChecked<T>(GetPawn(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetPawn());
		}
	}

private:	

	/* Set the local player. */
	void SetLocalPlayer( const class ULocalPlayer* InLocalPlayer );

	/* Set the local player via a player controller. */
	void SetPlayerController( const class APlayerController* InPlayerController );

	TWeakObjectPtr<class ULocalPlayer>		LocalPlayer;

	TWeakObjectPtr<UWorld>					World;
};

/**
 *	Each player that is active on the current client has a LocalPlayer. It stays active across maps
 *	There may be several spawned in the case of splitscreen/coop.
 *	There may be 0 spawned on servers.
 */
UCLASS(Within=Engine, config=Engine, transient)
class ENGINE_API ULocalPlayer : public UPlayer
{
	GENERATED_UCLASS_BODY()

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	ULocalPlayer(FVTableHelper& Helper) : Super(Helper), SlateOperations(FReply::Unhandled()) {}

	/** The FUniqueNetId which this player is associated with. */
	TSharedPtr<const FUniqueNetId> CachedUniqueNetId;

	/** The master viewport containing this player's view. */
	UPROPERTY()
	class UGameViewportClient* ViewportClient;

	/** The coordinates for the upper left corner of the master viewport subregion allocated to this player. 0-1 */
	FVector2D Origin;

	/** The size of the master viewport subregion allocated to this player. 0-1 */
	FVector2D Size;

	/** The location of the player's view the previous frame. */
	FVector LastViewLocation;

	/** How to constrain perspective viewport FOV */
	UPROPERTY(config)
	TEnumAsByte<enum EAspectRatioAxisConstraint> AspectRatioAxisConstraint;

	/** The class of PlayerController to spawn for players logging in. */
	UPROPERTY()
	TSubclassOf<class APlayerController> PendingLevelPlayerControllerClass;

	/** set when we've sent a split join request */
	UPROPERTY(VisibleAnywhere, transient, Category=LocalPlayer)
	uint32 bSentSplitJoin:1;

private:
	FSceneViewStateReference ViewState;
	FSceneViewStateReference StereoViewState;
	FSceneViewStateReference MonoViewState;

	/** The controller ID which this player accepts input from. */
	UPROPERTY()
	int32 ControllerId;

public:
	// UObject interface
	virtual void PostInitProperties() override;
	virtual void FinishDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	// FExec interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar) override;
	// End of FExec interface

	/** 
	 * Exec command handlers
	 */

	bool HandleDNCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleExitCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListMoveBodyCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListAwakeBodiesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListSimBodiesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleMoveComponentTimesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListSkelMeshesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListPawnComponentsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleExecCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleDrawEventsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleStreamingVolumesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleCancelMatineeCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	
protected:
	/**
	 * Retrieve the viewpoint of this player.
	 * @param OutViewInfo - Upon return contains the view information for the player.
	 * @param StereoPass - Which stereoscopic pass, if any, to get the viewport for.  This will include eye offsetting
	 */
	virtual void GetViewPoint(FMinimalViewInfo& OutViewInfo, EStereoscopicPass StereoPass = eSSP_FULL) const;

	/** @todo document */
	void ExecMacro( const TCHAR* Filename, FOutputDevice& Ar );

	/** FReply used to defer some slate operations. */
	FReply SlateOperations;

public:

	/**
	 *  Getter for slate operations.
	 */
	FReply& GetSlateOperations() { return SlateOperations; }
	const FReply& GetSlateOperations() const { return SlateOperations; }

	/**
	 * Get the world the players actor belongs to
	 *
	 * @return  Returns the world of the LocalPlayer's PlayerController. NULL if the LocalPlayer does not have a PlayerController
	 */
	virtual UWorld* GetWorld() const override;

	/**
	 * Get the game instance associated with this local player
	 * 
	 * @return GameInstance related to local player
	 */
	UGameInstance* GetGameInstance() const;


	/**
	* Calculate the view init settings for drawing from this view actor
	*
	* @param	OutInitOptions - output view struct. Not every field is initialized, some of them are only filled in by CalcSceneView
	* @param	Viewport - current client viewport
	* @param	ViewDrawer - optional drawing in the view
	* @param	StereoPass - whether we are drawing the full viewport, or a stereo left / right pass
	* @return	true if the view options were filled in. false in various fail conditions.
	*/
	bool CalcSceneViewInitOptions(
		struct FSceneViewInitOptions& OutInitOptions, 
		FViewport* Viewport,
		class FViewElementDrawer* ViewDrawer = NULL,
		EStereoscopicPass StereoPass = eSSP_FULL);

	/**
	 * Calculate the view settings for drawing from this view actor
	 *
	 * @param	View - output view struct
	 * @param	OutViewLocation - output actor location
	 * @param	OutViewRotation - output actor rotation
	 * @param	Viewport - current client viewport
	 * @param	ViewDrawer - optional drawing in the view
	 * @param	StereoPass - whether we are drawing the full viewport, or a stereo left / right pass
	 */
	virtual FSceneView* CalcSceneView(class FSceneViewFamily* ViewFamily,
		FVector& OutViewLocation, 
		FRotator& OutViewRotation, 
		FViewport* Viewport,
		class FViewElementDrawer* ViewDrawer = NULL,
		EStereoscopicPass StereoPass = eSSP_FULL );

	/**
	 * Called at creation time for internal setup
	 */
	virtual void PlayerAdded(class UGameViewportClient* InViewportClient, int32 InControllerID);

	/**
	 * Called to initialize the online delegates
	 */
	virtual void InitOnlineSession();

	/**
	 * Called when the player is removed from the viewport client
	 */
	virtual void PlayerRemoved();

	/**
	 * Create an actor for this player.
	 * @param URL - The URL the player joined with.
	 * @param OutError - If an error occurred, returns the error description.
	 * @param InWorld - World in which to spawn the play actor
	 * @return False if an error occurred, true if the play actor was successfully spawned.	 
	 */
	virtual bool SpawnPlayActor(const FString& URL,FString& OutError, UWorld* InWorld);
	
	/** Send a splitscreen join command to the server to allow a splitscreen player to connect to the game
	 * the client must already be connected to a server for this function to work
	 * @note this happens automatically for all viewports that exist during the initial server connect
	 * 	so it's only necessary to manually call this for viewports created after that
	 * if the join fails (because the server was full, for example) all viewports on this client will be disconnected
	 */
	virtual void SendSplitJoin();
	
	/**
	 * Change the ControllerId for this player; if the specified ControllerId is already taken by another player, changes the ControllerId
	 * for the other player to the ControllerId currently in use by this player.
	 *
	 * @param	NewControllerId		the ControllerId to assign to this player.
	 */
	virtual void SetControllerId(int32 NewControllerId);

	/**
	 * Returns the controller ID for the player
	 */
	int32 GetControllerId() const { return ControllerId; }

	/** 
	 * Retrieves this player's name/tag from the online subsystem
	 * if this function returns a non-empty string, the returned name will replace the "Name" URL parameter
	 * passed around in the level loading and connection code, which normally comes from DefaultEngine.ini
	 * 
	 * @return Name of player if specified (by onlinesubsystem or otherwise), Empty string otherwise
	 */
	virtual FString GetNickname() const;

	/** 
	 * Retrieves any game-specific login options for this player
	 * if this function returns a non-empty string, the returned option or options be added
	 * passed in to the level loading and connection code.  Options are in URL format,
	 * key=value, with multiple options concatenated together with an & between each key/value pair
	 * 
	 * @return URL Option or options for this game, Empty string otherwise
	 */
	virtual FString GetGameLoginOptions() const { return TEXT(""); }

	/** 
	 * Retrieves this player's unique net ID from the online subsystem 
	 *
	 * @return unique Id associated with this player
	 */
	TSharedPtr<const FUniqueNetId> GetUniqueNetIdFromCachedControllerId() const;

	/** 
	 * Retrieves this player's unique net ID that was previously cached
	 *
	 * @return unique Id associated with this player
	 */
	TSharedPtr<const FUniqueNetId> GetCachedUniqueNetId() const;

	/** Sets the players current cached unique net id */
	void SetCachedUniqueNetId(TSharedPtr<const FUniqueNetId> NewUniqueNetId);

	/** 
	 * Retrieves the preferred unique net id. This is for backwards compatibility for games that don't use the cached unique net id logic
	 *
	 * @return unique Id associated with this player
	 */
	TSharedPtr<const FUniqueNetId> GetPreferredUniqueNetId() const;

	/** Returns true if the cached unique net id, is the one assigned to the controller id from the OSS */
	bool IsCachedUniqueNetIdPairedWithControllerId() const;

	/**
	 * This function will give you two points in Pixel Space that surround the World Space box.
	 *
	 * @param	ActorBox		The World Space Box
	 * @param	OutLowerLeft	The Lower Left corner of the Pixel Space box
	 * @param	OutUpperRight	The Upper Right corner of the pixel space box
	 * @return  False if there is no viewport, or if the box is behind the camera completely
	 */
	bool GetPixelBoundingBox(const FBox& ActorBox, FVector2D& OutLowerLeft, FVector2D& OutUpperRight, const FVector2D* OptionalAllotedSize = NULL);

	/**
	 * This function will give you a point in Pixel Space from a World Space position
	 *
	 * @param	InPoint		The point in world space
	 * @param	OutPoint	The point in pixel space
	 * @return  False if there is no viewport, or if the box is behind the camera completely
	 */
	bool GetPixelPoint(const FVector& InPoint, FVector2D& OutPoint, const FVector2D* OptionalAllotedSize = NULL);


	/**
	 * Helper function for deriving various bits of data needed for projection
	 *
	 * @param	Viewport				The ViewClient's viewport
     * @param	StereoPass			    Whether this is a full viewport pass, or a left/right eye pass
	 * @param	ProjectionData			The structure to be filled with projection data
	 * @return  False if there is no viewport, or if the Actor is null
	 */
	virtual bool GetProjectionData(FViewport* Viewport, EStereoscopicPass StereoPass, FSceneViewProjectionData& ProjectionData) const;

	/**
	 * Determines whether this player is the first and primary player on their machine.
	 * @return	true if this player is not using splitscreen, or is the first player in the split-screen layout.
	 */
	bool IsPrimaryPlayer() const;

	/** Locked view state needs access to GetViewPoint. */
	friend class FLockedViewState;
};

