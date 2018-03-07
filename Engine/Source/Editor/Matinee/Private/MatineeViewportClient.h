// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "EditorViewportClient.h"
#include "Interpolation.h"
#include "Matinee/InterpGroup.h"
#include "MatineeViewportData.h"

class FCanvas;
class FMatinee;
class FSceneViewport;
class SScrollBar;
class SSplitter;
class SViewport;
class UFont;
class UInterpTrack;
struct FSubTrackGroup;

class FMatineeViewportClient : public FEditorViewportClient
{
public:
	FMatineeViewportClient( class FMatinee* InMatinee );
	~FMatineeViewportClient();

	void DrawTimeline(FViewport* Viewport,FCanvas* Canvas);
	void DrawMarkers(FViewport* Viewport,FCanvas* Canvas);
	void DrawGrid(FViewport* Viewport,FCanvas* Canvas, bool bDrawTimeline);
	/** 
	 * Draws a track in the interp editor
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track to draw
	 * @param Group			Group containing the track to draw
	 * @param TrackDrawParams	Params for drawing the track
	 * @params LabelDrawParams	Params for drawing the track label
	 */
	int32 DrawTrack( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, const FInterpTrackDrawParams& TrackDrawParams, const FInterpTrackLabelDrawParams& LabelDrawParams );

	/** 
	 * Creates a "Push Properties Onto Graph" Button
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track owning the group
	 * @param Group			Subgroup to draw
	 * @param GroupIndex		Index of the group in its parent track.
	 * @param LabelDrawParams	Params for drawing the group label
	 * @param bIsSubtrack		Is this a subtrack?
	 */
	void CreatePushPropertiesOntoGraphButton( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, int32 GroupIndex, const FInterpTrackLabelDrawParams& LabelDrawParams, bool bIsSubTrack );

	/** 
	 * Draws a sub track group in the interp editor
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track owning the group
	 * @param InGroup		Subgroup to draw
	 * @param GroupIndex		Index of the group in its parent track.
	 * @params LabelDrawParams	Params for drawing the group label
	 * @param Group			Group that this subgroup is part of
	 */
	void DrawSubTrackGroup( FCanvas* Canvas, UInterpTrack* Track, const FSubTrackGroup& InGroup, int32 GroupIndex, const FInterpTrackLabelDrawParams& LabelDrawParams, UInterpGroup* Group );

	/** 
	 * Draws a track label for a track
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track that needs a label drawn for it.
	 * @param Group			Group containing the track to draw
	 * @param TrackDrawParams	Params for drawing the track
	 * @params LabelDrawParams	Params for drawing the track label
	 */
	void DrawTrackLabel( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, const FInterpTrackDrawParams& TrackDrawParams, const FInterpTrackLabelDrawParams& LabelDrawParams );
	
	/** 
	 * Draws collapsed keyframes when a group is collapsed
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track with keyframe data
	 * @param TrackPos		Position where the collapsed keys should be drawn
	 * @param TickSize		Draw size of each keyframe.
	 */
	void DrawCollapsedTrackKeys( FCanvas* Canvas, UInterpTrack* Track, const FVector2D& TrackPos, const FVector2D& TickSize );
	
	/** 
	 * Draws keyframes for all subtracks in a subgroup.  The keyframes are drawn directly on the group.
	 * @param Canvas		Canvas to draw on
	 * @param SubGroupOwner		Track that owns the subgroup 			
	 * @param GroupIndex		Index of a subgroup to draw
	 * @param DrawInfos		An array of draw information for each keyframe that needs to be drawn
	 * @param TrackPos		Starting position where the keyframes should be drawn.  
	 * @param KeySize		Draw size of each keyframe
	 */
	void DrawSubTrackGroupKeys( FCanvas* Canvas, UInterpTrack* SubGroupOwner, int32 GroupIndex, const TArray<FKeyframeDrawInfo>& KeyDrawInfos, const FVector2D& TrackPos, const FVector2D& KeySize );
	
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);
	
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad=false);
	virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y);
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples=1, bool bGamepad=false);

	/**
	 * Returns the cursor that should be used at the provided mouse coordinates
	 *
	 * @param Viewport	The viewport where the cursor resides
	 * @param X The X position of the mouse
	 * @param Y The Y position of the mouse
	 *
	 * @return The type of cursor to use
	 */
	virtual EMouseCursor::Type GetCursor(FViewport* Viewport,int32 X,int32 Y);

	virtual void Tick(float DeltaSeconds);

	virtual void Serialize(FArchive& Ar);

	/** Exec handler */
	virtual void Exec(const TCHAR* Cmd);

	/** 
	 * Returns the vertical size of the entire group list for this viewport, in pixels
	 */
	int32 ComputeGroupListContentHeight() const;

	/** 
	 * Returns the height of the viewable group list content box in pixels
	 *
	 *  @param ViewportHeight The size of the viewport in pixels
	 *
	 *  @return The height of the viewable content box (may be zero!)
	 */
	int32 ComputeGroupListBoxHeight( const int32 ViewportHeight ) const;


	/**
	 * Selects a color for the specified group (bound to the given group actor)
	 *
	 * @param Group The group to select a label color for
	 * @param GroupActorOrNull The actor currently bound to the specified, or NULL if none is bounds
	 *
	 * @return The color to use to draw the group label
	 */
	FColor ChooseLabelColorForGroupActor( UInterpGroup* Group, AActor* GroupActorOrNull ) const;

	/**
	 * Adds all keypoints based on the hit proxy
	 */
	void AddKeysFromHitProxy( HHitProxy* HitProxy, TArray<FInterpEdSelKey>& Selections );

	//helper functions (formerly static functions in InterpEd)
	int32 DrawLabel( FCanvas* Canvas, float StartX, float StartY, const TCHAR* Text, const FLinearColor& Color );
	float GetGridSpacing(int32 GridNum);
	uint32 CalculateBestFrameStep(float SnapAmount, float PixelsPerSec, float MinPixelsPerGrid);

	//Set the parent for this Viewport so we can keep track if the user has closed it.
	void SetParentTab(TWeakPtr<SDockTab> InParentTab );
public:

	/** True if this window is the 'director tracks' window and should only draw director track groups */
	bool bIsDirectorTrackWindow;

	/** True if we want the animation timeline bar to be rendered and interactive for this window */
	bool bWantTimeline;

	/** Scroll bar thumb position (actually, this is the negated thumb position.) */
	int32	ThumbPos_Vert;
	/** Previously Saved Viewport... used to track if we need to update the scroll */
	int32	PrevViewportHeight;

	int32 OldMouseX, OldMouseY;
	int32 BoxStartX, BoxStartY;
	int32 BoxEndX, BoxEndY;
	int32 DistanceDragged;

	/** Used to accumulate velocity for autoscrolling when the user is dragging items near the edge of the viewport. */
	FVector2D ScrollAccum;

	class FMatinee* InterpEd;

	/** The parent window tab for this viewport */
	TWeakPtr<SDockTab> ParentTab;

	/** The object and data we are currently dragging, if NULL we are not dragging any objects. */
	FInterpEdInputData DragData;
	FInterpEdInputInterface* DragObject;

	bool	bPanning;
	bool	bMouseDown;
	bool	bGrabbingHandle;
	bool	bNavigating;
	bool	bBoxSelecting;
	bool	bTransactionBegun;
	bool	bGrabbingMarker;
	
	/** The font to use for drawing labels. */
	UFont*	LabelFont;

private:
	
	UTexture2D* CamLockedIcon;
	UTexture2D* CamUnlockedIcon;
	UTexture2D* ForwardEventOnTex;
	UTexture2D* ForwardEventOffTex;
	UTexture2D* BackwardEventOnTex;
	UTexture2D* BackwardEventOffTex;
	UTexture2D* DisableTrackTex;
	UTexture2D* GraphOnTex;
	UTexture2D* GraphOffTex;
	UTexture2D* TrajectoryOnTex;
};

/*-----------------------------------------------------------------------------
	SMatineeViewport
-----------------------------------------------------------------------------*/
class SMatineeViewport : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SMatineeViewport)
	{}
	SLATE_END_ARGS()

	/** Destructor */
	virtual ~SMatineeViewport();

	/** SCompoundWidget interface */
	void Construct(const FArguments& InArgs, TWeakPtr<FMatinee> InMatinee);

	/** Scroll bar */
	TSharedPtr<SScrollBar> ScrollBar_Vert;

	/**
	 * Updates the scroll bar for the current state of the window's size and content layout.  This should be called
	 *  when either the window size changes or the vertical size of the content contained in the window changes.
	 */
	void AdjustScrollBar();

	void OnScroll( float InScrollOffsetFraction );

	/**
	 * @return	The scroll bar's thumb position, which is the top of the scroll bar. 
	 */
	int32 GetThumbPosition() const
	{
		return -InterpEdVC->ThumbPos_Vert;
	}

	/**
	 * Sets the thumb position from the given parameter. 
	 * 
	 * @param	NewPosition	The new thumb position. 
	 */
	void SetThumbPosition( int32 NewPosition )
	{
		InterpEdVC->ThumbPos_Vert = -NewPosition;
	}

	/** 
	 * Returns true if the viewport is visible
	 */
	bool IsVisible() const;

	/**
	 * Returns the Mouse position in the viewport
	 */
	FIntPoint GetMousePos();

	/** The Viewport Client */
	TSharedPtr<FMatineeViewportClient> InterpEdVC;

	/** Slate Viewport hooks */
	TSharedPtr<SViewport> ViewportWidget;
	TSharedPtr<FSceneViewport> Viewport;

private:
	float ScrollBarThumbSize;
};

class SMatineeTrackView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMatineeTrackView)
	{}
	SLATE_END_ARGS()

	/** Destructor */
	virtual ~SMatineeTrackView() {};

	/** SCompoundWidget interface */
	void Construct(
		const FArguments& InArgs,
		TWeakPtr<SMatineeViewport> InTrackWindow,
		TWeakPtr<SMatineeViewport> InDirectorTrackWindow
		);

	void UpdateWindowDisplay(bool bShowDirector, bool bShowTrack);
	bool IsSplit();

private:
	TWeakPtr<SMatineeViewport> TrackWindow;
	TWeakPtr<SMatineeViewport> DirectorTrackWindow;
	
	TSharedPtr<SSplitter> Splitter;
};
