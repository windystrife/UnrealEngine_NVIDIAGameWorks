// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SAnimNotifyPanel.h"
#include "Rendering/DrawElements.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/PropertyPortFlags.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Animation/AnimSequence.h"
#include "Toolkits/AssetEditorManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Input/SButton.h"
#include "Animation/AnimMontage.h"
#include "Animation/EditorNotifyObject.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ScopedTransaction.h"
#include "AssetRegistryModule.h"
#include "AssetSelection.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "BlueprintActionDatabase.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/BlendSpaceBase.h"
#include "TabSpawners.h"
#include "SInlineEditableTextBlock.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Modules/ModuleManager.h"
#include "IEditableSkeleton.h"
#include "ISkeletonEditorModule.h"

// Track Panel drawing
const float NotificationTrackHeight = 20.0f;

// AnimNotify Drawing
const float NotifyHeightOffset = 0.f;
const float NotifyHeight = NotificationTrackHeight + 3.f;
const FVector2D ScrubHandleSize(8.f, NotifyHeight);
const FVector2D AlignmentMarkerSize(8.f, NotifyHeight);
const FVector2D TextBorderSize(1.f, 1.f);

#define LOCTEXT_NAMESPACE "AnimNotifyPanel"

DECLARE_DELEGATE_OneParam( FOnDeleteNotify, struct FAnimNotifyEvent*)
DECLARE_DELEGATE_RetVal_FourParams( FReply, FOnNotifyNodeDragStarted, TSharedRef<SAnimNotifyNode>, const FPointerEvent&, const FVector2D&, const bool)
DECLARE_DELEGATE_RetVal_FiveParams(FReply, FOnNotifyNodesDragStarted, TArray<TSharedPtr<SAnimNotifyNode>>, TSharedRef<SWidget>, const FVector2D&, const FVector2D&, const bool)
DECLARE_DELEGATE_RetVal( float, FOnGetDraggedNodePos )
DECLARE_DELEGATE_TwoParams( FPanTrackRequest, int32, FVector2D)
DECLARE_DELEGATE(FCopyNodes)
DECLARE_DELEGATE_FourParams(FPasteNodes, SAnimNotifyTrack*, float, ENotifyPasteMode::Type, ENotifyPasteMultipleMode::Type)
DECLARE_DELEGATE_RetVal_OneParam(EVisibility, FOnGetTimingNodeVisibilityForNode, TSharedPtr<SAnimNotifyNode>)

class FNotifyDragDropOp;

FText MakeTooltipFromTime(const UAnimSequenceBase* InSequence, float InSeconds, float InDuration)
{
	const FText Frame = FText::AsNumber(InSequence->GetFrameAtTime(InSeconds));
	const FText Seconds = FText::AsNumber(InSeconds);

	if (InDuration > 0.0f)
	{
		const FText Duration = FText::AsNumber(InDuration);
		return FText::Format(LOCTEXT("NodeToolTipLong", "@ {0} sec (frame {1}) for {2} sec"), Seconds, Frame, Duration);
	}
	else
	{
		return FText::Format(LOCTEXT("NodeToolTipShort", "@ {0} sec (frame {1})"), Seconds, Frame);
	}
}

// Read common info from the clipboard
bool ReadNotifyPasteHeader(FString& OutPropertyString, const TCHAR*& OutBuffer, float& OutOriginalTime, float& OutOriginalLength, int32& OutTrackSpan)
{
	OutBuffer = NULL;
	OutOriginalTime = -1.f;

	FPlatformApplicationMisc::ClipboardPaste(OutPropertyString);

	if (!OutPropertyString.IsEmpty())
	{
		//Remove header text
		const FString HeaderString(TEXT("COPY_ANIMNOTIFYEVENT"));

		//Check for string identifier in order to determine whether the text represents an FAnimNotifyEvent.
		if (OutPropertyString.StartsWith(HeaderString) && OutPropertyString.Len() > HeaderString.Len())
		{
			int32 HeaderSize = HeaderString.Len();
			OutBuffer = *OutPropertyString;
			OutBuffer += HeaderSize;

			FString ReadLine;
			// Read the original time from the first notify
			FParse::Line(&OutBuffer, ReadLine);
			FParse::Value(*ReadLine, TEXT("OriginalTime="), OutOriginalTime);
			FParse::Value(*ReadLine, TEXT("OriginalLength="), OutOriginalLength);
			FParse::Value(*ReadLine, TEXT("TrackSpan="), OutTrackSpan);
			return true;
		}
	}

	return false;
}

namespace ENodeObjectTypes
{
	enum Type
	{
		NOTIFY,
		SYNC_MARKER
	};
};

struct INodeObjectInterface
{
	virtual ENodeObjectTypes::Type GetType() const = 0;
	virtual FAnimNotifyEvent* GetNotifyEvent() = 0;
	virtual int GetTrackIndex() const = 0;
	virtual float GetTime(EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) const = 0;
	virtual float GetDuration() = 0;
	virtual FName GetName() = 0;
	virtual TOptional<FLinearColor> GetEditorColor() = 0;
	virtual FText GetNodeTooltip(const UAnimSequenceBase* Sequence) = 0;
	virtual TOptional<UObject*> GetObjectBeingDisplayed() = 0;
	virtual bool IsBranchingPoint() = 0;
	bool operator<(const INodeObjectInterface& Rhs) const { return GetTime() < Rhs.GetTime(); }

	virtual void SetTime(float Time, EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) = 0;
	virtual void SetDuration(float Duration) = 0;

	virtual void HandleDrop(class UAnimSequenceBase* Sequence, float Time, int32 TrackIndex) = 0;
	virtual void CacheName() = 0;

	virtual void Delete(UAnimSequenceBase* Seq) = 0;

	virtual void ExportForCopy(UAnimSequenceBase* Seq, FString& StrValue) const = 0;
};

struct FNotifyNodeInterface : public INodeObjectInterface
{
	FAnimNotifyEvent* NotifyEvent;

	FNotifyNodeInterface(FAnimNotifyEvent* InAnimNotifyEvent) : NotifyEvent(InAnimNotifyEvent) {}
	virtual ENodeObjectTypes::Type GetType() const override { return ENodeObjectTypes::NOTIFY; }
	virtual FAnimNotifyEvent* GetNotifyEvent() override { return NotifyEvent; }
	virtual int GetTrackIndex() const override{ return NotifyEvent->TrackIndex; }
	virtual float GetTime(EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) const override{ return NotifyEvent->GetTime(ReferenceFrame); }
	virtual float GetDuration() override { return NotifyEvent->GetDuration(); }
	virtual FName GetName() override { return NotifyEvent->NotifyName; }
	virtual bool IsBranchingPoint() override { return NotifyEvent->IsBranchingPoint(); }
	virtual TOptional<FLinearColor> GetEditorColor() override 
	{
		TOptional<FLinearColor> ReturnColour;
		if (NotifyEvent->Notify)
		{
			ReturnColour = NotifyEvent->Notify->GetEditorColor();
		}
		else if (NotifyEvent->NotifyStateClass)
		{
			ReturnColour = NotifyEvent->NotifyStateClass->GetEditorColor();
		}
		return ReturnColour;
	}

	virtual FText GetNodeTooltip(const UAnimSequenceBase* Sequence) override
	{
		FText ToolTipText = MakeTooltipFromTime(Sequence, NotifyEvent->GetTime(), NotifyEvent->GetDuration());

		if (NotifyEvent->IsBranchingPoint())
		{
			ToolTipText = FText::Format(LOCTEXT("AnimNotify_ToolTipBranchingPoint", "{0} (BranchingPoint)"), ToolTipText);
		}
		return ToolTipText;
	}

	virtual TOptional<UObject*> GetObjectBeingDisplayed() override
	{
		if (NotifyEvent->Notify)
		{
			return TOptional<UObject*>(NotifyEvent->Notify);
		}

		if (NotifyEvent->NotifyStateClass)
		{
			return TOptional<UObject*>(NotifyEvent->NotifyStateClass);
		}
		return TOptional<UObject*>();
	}

	virtual void SetTime(float Time, EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) override { NotifyEvent->SetTime(Time, ReferenceFrame); }
	virtual void SetDuration(float Duration) override { NotifyEvent->SetDuration(Duration); }

	virtual void HandleDrop(class UAnimSequenceBase* Sequence, float Time, int32 TrackIndex) override
	{
		float EventDuration = NotifyEvent->GetDuration();

		NotifyEvent->Link(Sequence, Time, NotifyEvent->GetSlotIndex());
		NotifyEvent->RefreshTriggerOffset(Sequence->CalculateOffsetForNotify(NotifyEvent->GetTime()));

		if (EventDuration > 0.0f)
		{
			NotifyEvent->EndLink.Link(Sequence, NotifyEvent->GetTime() + EventDuration, NotifyEvent->GetSlotIndex());
			NotifyEvent->RefreshEndTriggerOffset(Sequence->CalculateOffsetForNotify(NotifyEvent->EndLink.GetTime()));
		}
		else
		{
			NotifyEvent->EndTriggerTimeOffset = 0.0f;
		}

		NotifyEvent->TrackIndex = TrackIndex;
	}

	virtual void CacheName() override 
	{
		if (NotifyEvent->Notify)
		{
			NotifyEvent->NotifyName = FName(*NotifyEvent->Notify->GetNotifyName());
		}
		else if (NotifyEvent->NotifyStateClass)
		{
			NotifyEvent->NotifyName = FName(*NotifyEvent->NotifyStateClass->GetNotifyName());
		}
	}

	virtual void Delete(UAnimSequenceBase* Seq) override
	{
		for (int32 I = 0; I < Seq->Notifies.Num(); ++I)
		{
			if (NotifyEvent == &(Seq->Notifies[I]))
			{
				Seq->Notifies.RemoveAt(I);
				Seq->MarkPackageDirty();
				break;
			}
		}
	}

	virtual void ExportForCopy(UAnimSequenceBase* Seq, FString& StrValue) const override
	{
		int32 Index = INDEX_NONE;
		for (int32 NotifyIdx = 0; NotifyIdx < Seq->Notifies.Num(); ++NotifyIdx)
		{
			if (NotifyEvent == &Seq->Notifies[NotifyIdx])
			{
				Index = NotifyIdx;
				break;
			}
		}

		check(Index != INDEX_NONE);

		UArrayProperty* ArrayProperty = NULL;
		uint8* PropertyData = Seq->FindNotifyPropertyData(Index, ArrayProperty);
		if (PropertyData && ArrayProperty)
		{
			ArrayProperty->Inner->ExportTextItem(StrValue, PropertyData, PropertyData, Seq, PPF_Copy);
		}
	}
};

struct FSyncMarkerNodeInterface : public INodeObjectInterface
{
	FAnimSyncMarker* SyncMarker;

	FSyncMarkerNodeInterface(FAnimSyncMarker* InSyncMarker) : SyncMarker(InSyncMarker) {}
	virtual ENodeObjectTypes::Type GetType() const override { return ENodeObjectTypes::SYNC_MARKER; }
	virtual FAnimNotifyEvent* GetNotifyEvent() override { return NULL; }
	virtual int GetTrackIndex() const override{ return SyncMarker->TrackIndex; }
	virtual float GetTime(EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) const override { return SyncMarker->Time; }
	virtual float GetDuration() override { return 0.f; }
	virtual FName GetName() override { return SyncMarker->MarkerName; }
	virtual bool IsBranchingPoint() override { return false; }
	virtual TOptional<FLinearColor> GetEditorColor() override
	{
		return FLinearColor::Green;
	}

	virtual FText GetNodeTooltip(const UAnimSequenceBase* Sequence) override
	{
		return MakeTooltipFromTime(Sequence, SyncMarker->Time, 0.f);
	}

	virtual TOptional<UObject*> GetObjectBeingDisplayed() override
	{
		return TOptional<UObject*>();
	}

	virtual void SetTime(float Time, EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) override { SyncMarker->Time = Time; }
	virtual void SetDuration(float Duration) override {}

	virtual void HandleDrop(class UAnimSequenceBase* Sequence, float Time, int32 TrackIndex) override
	{
		SyncMarker->Time = Time;
		SyncMarker->TrackIndex = TrackIndex;
	}

	virtual void CacheName() override {}

	virtual void Delete(UAnimSequenceBase* SeqBase) override
	{
		if(UAnimSequence* Seq = Cast<UAnimSequence>(SeqBase))
		{
			for (int32 I = 0; I < Seq->AuthoredSyncMarkers.Num(); ++I)
			{
				if (SyncMarker == &(Seq->AuthoredSyncMarkers[I]))
				{
					Seq->AuthoredSyncMarkers.RemoveAt(I);
					Seq->MarkPackageDirty();
					break;
				}
			}
		}
	}

	virtual void ExportForCopy(UAnimSequenceBase* SeqBase, FString& StrValue) const override
	{
		if (UAnimSequence* Seq = Cast<UAnimSequence>(SeqBase))
		{
			int32 Index = INDEX_NONE;
			for (int32 SyncMarkerIdx = 0; SyncMarkerIdx < Seq->AuthoredSyncMarkers.Num(); ++SyncMarkerIdx)
			{
				if (SyncMarker == &Seq->AuthoredSyncMarkers[SyncMarkerIdx])
				{
					Index = SyncMarkerIdx;
					break;
				}
			}

			check(Index != INDEX_NONE);

			UArrayProperty* ArrayProperty = NULL;
			uint8* PropertyData = Seq->FindSyncMarkerPropertyData(Index, ArrayProperty);
			if (PropertyData && ArrayProperty)
			{
				ArrayProperty->Inner->ExportTextItem(StrValue, PropertyData, PropertyData, Seq, PPF_Copy);
			}
		}
	}
};

// Struct that allows us to get the max value of 2 numbers at compile time
template<int32 A, int32 B>
struct CompileTimeMax
{
	enum Max{ VALUE = (A > B) ? A : B };
};

// Size of biggest object that we can store in our node, if new node interfaces are added they should be part of this calculation
const int32 MAX_NODE_OBJECT_INTERFACE_SIZE = CompileTimeMax<sizeof(FNotifyNodeInterface), sizeof(FSyncMarkerNodeInterface)>::VALUE;


//////////////////////////////////////////////////////////////////////////
// SAnimNotifyNode

class SAnimNotifyNode : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimNotifyNode )
		: _Sequence()
		, _AnimNotify(nullptr)
		, _AnimSyncMarker(nullptr)
		, _OnNodeDragStarted()
		, _OnUpdatePanel()
		, _PanTrackRequest()
		, _OnDeselectAllNotifies()
		, _ViewInputMin()
		, _ViewInputMax()
		, _MarkerBars()
	{
	}
	SLATE_ARGUMENT( class UAnimSequenceBase*, Sequence )
	SLATE_ARGUMENT( FAnimNotifyEvent *, AnimNotify )
	SLATE_ARGUMENT( FAnimSyncMarker*, AnimSyncMarker)
	SLATE_EVENT( FOnNotifyNodeDragStarted, OnNodeDragStarted )
	SLATE_EVENT( FOnUpdatePanel, OnUpdatePanel )
	SLATE_EVENT( FPanTrackRequest, PanTrackRequest )
	SLATE_EVENT( FDeselectAllNotifies, OnDeselectAllNotifies )
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_ATTRIBUTE( TArray<FTrackMarkerBar>, MarkerBars )
	SLATE_ARGUMENT(TSharedPtr<SAnimTimingNode>, StateEndTimingNode)
	SLATE_END_ARGS()

	void Construct(const FArguments& Declaration);

	// SWidget interface
	virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	// End of SWidget interface

	// SNodePanel::SNode interface
	void UpdateSizeAndPosition(const FGeometry& AllottedGeometry);
	FVector2D GetWidgetPosition() const;
	FVector2D GetNotifyPosition() const;
	FVector2D GetNotifyPositionOffset() const;
	FVector2D GetSize() const;
	bool HitTest(const FGeometry& AllottedGeometry, FVector2D MouseLocalPose) const;

	// Extra hit testing to decide whether or not the duration handles were hit on a state node
	ENotifyStateHandleHit::Type DurationHandleHitTest(const FVector2D& CursorScreenPosition) const;

	UObject* GetObjectBeingDisplayed() const;
	// End of SNodePanel::SNode

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const /*override*/;

	/** Helpers to draw scrub handles and snap offsets */
	void DrawHandleOffset( const float& Offset, const float& HandleCentre, FSlateWindowElementList& OutDrawElements, int32 MarkerLayer, const FGeometry &AllottedGeometry, const FSlateRect& MyCullingRect ) const;
	void DrawScrubHandle( float ScrubHandleCentre, FSlateWindowElementList& OutDrawElements, int32 ScrubHandleID, const FGeometry &AllottedGeometry, const FSlateRect& MyCullingRect, FLinearColor NodeColour ) const;

	FLinearColor GetNotifyColor() const;
	FText GetNotifyText() const;

	/** Node object interface */
	INodeObjectInterface* NodeObjectInterface;

	/** In object storage for our interface struct, saves us having to dynamically allocate what will be a very small struct*/
	uint8 NodeObjectInterfaceStorage[MAX_NODE_OBJECT_INTERFACE_SIZE];

	/** Helper function to create our node interface object */
	template<typename InterfaceType, typename ParamType>
	void MakeNodeInterface(ParamType& InParam)
	{
		check(sizeof(InterfaceType) <= MAX_NODE_OBJECT_INTERFACE_SIZE); //Not enough space, check definiton of MAX_NODE_OBJECT_INTERFACE_SIZE
		NodeObjectInterface = new(NodeObjectInterfaceStorage)InterfaceType(InParam);
	}

	void DropCancelled();

	/** Returns the size of this notifies duration in screen space */
	float GetDurationSize() const { return NotifyDurationSizeX;}

	/** Sets the position the mouse was at when this node was last hit */
	void SetLastMouseDownPosition(const FVector2D& CursorPosition) {LastMouseDownPosition = CursorPosition;}

	/** The minimum possible duration that a notify state can have */
	static const float MinimumStateDuration;

	const FVector2D& GetScreenPosition() const
	{
		return ScreenPosition;
	}

	const float GetLastSnappedTime() const
	{
		return LastSnappedTime;
	}

	void ClearLastSnappedTime()
	{
		LastSnappedTime = -1.0f;
	}

	void SetLastSnappedTime(float NewSnapTime)
	{
		LastSnappedTime = NewSnapTime;
	}

private:
	FText GetNodeTooltip() const;

	/** Detects any overflow on the anim notify track and requests a track pan */
	float HandleOverflowPan( const FVector2D& ScreenCursorPos, float TrackScreenSpaceXPosition );

	/** Finds a snap position if possible for the provided scrub handle, if it is not possible, returns -1.0f */
	float GetScrubHandleSnapPosition(float NotifyLocalX, ENotifyStateHandleHit::Type HandleToCheck);

	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;

	/** The sequence that the AnimNotifyEvent for Notify lives in */
	UAnimSequenceBase* Sequence;
	FSlateFontInfo Font;

	TAttribute<float>			ViewInputMin;
	TAttribute<float>			ViewInputMax;
	FVector2D					CachedAllotedGeometrySize;
	FVector2D					ScreenPosition;
	float						LastSnappedTime;

	bool						bDrawTooltipToRight;
	bool						bBeingDragged;
	bool						bSelected;

	// Index for undo transactions for dragging, as a check to make sure it's active
	int32						DragMarkerTransactionIdx;

	/** The scrub handle currently being dragged, if any */
	ENotifyStateHandleHit::Type CurrentDragHandle;
	
	float						NotifyTimePositionX;
	float						NotifyDurationSizeX;
	float						NotifyScrubHandleCentre;
	
	float						WidgetX;
	FVector2D					WidgetSize;
	
	FVector2D					TextSize;
	float						LabelWidth;
	FVector2D					BranchingPointIconSize;

	/** Last position the user clicked in the widget */
	FVector2D					LastMouseDownPosition;

	/** Delegate that is called when the user initiates dragging */
	FOnNotifyNodeDragStarted	OnNodeDragStarted;

	/** Delegate to pan the track, needed if the markers are dragged out of the track */
	FPanTrackRequest			PanTrackRequest;

	/** Marker bars for snapping to when dragging the markers in a state notify node */
	TAttribute<TArray<FTrackMarkerBar>> MarkerBars;

	/** Delegate to deselect notifies and clear the details panel */
	FDeselectAllNotifies OnDeselectAllNotifies;

	/** Cached owning track geometry */
	FGeometry CachedTrackGeometry;

	TSharedPtr<SOverlay> EndMarkerNodeOverlay;

	friend class SAnimNotifyTrack;
};

class SAnimNotifyPair : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAnimNotifyPair)
	{}

	SLATE_NAMED_SLOT(FArguments, LeftContent)
	
	SLATE_ARGUMENT(TSharedPtr<SAnimNotifyNode>, Node);
	SLATE_EVENT(FOnGetTimingNodeVisibilityForNode, OnGetTimingNodeVisibilityForNode)

	SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	float GetWidgetPaddingLeft();

protected:
	TSharedPtr<SWidget> PairedWidget;
	TSharedPtr<SAnimNotifyNode> NodePtr;
};

void SAnimNotifyPair::Construct(const FArguments& InArgs)
{
	NodePtr = InArgs._Node;
	PairedWidget = InArgs._LeftContent.Widget;
	check(NodePtr.IsValid());
	check(PairedWidget.IsValid());

	float ScaleMult = 1.0f;
	FVector2D NodeSize = NodePtr->ComputeDesiredSize(ScaleMult);
	Visibility = EVisibility::SelfHitTestInvisible;

	this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					PairedWidget->AsShared()
				]
			]
			+ SHorizontalBox::Slot()
			[
				NodePtr->AsShared()
			]
		];
}

float SAnimNotifyPair::GetWidgetPaddingLeft()
{
	return NodePtr->GetWidgetPosition().X - PairedWidget->GetDesiredSize().X;
}

//////////////////////////////////////////////////////////////////////////
// SAnimNotifyTrack

class SAnimNotifyTrack : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimNotifyTrack )
		: _Sequence(NULL)
		, _ViewInputMin()
		, _ViewInputMax()
		, _TrackIndex()
		, _TrackColor(FLinearColor::White)
		, _OnSelectionChanged()
		, _OnUpdatePanel()
		, _OnGetNotifyBlueprintData()
		, _OnGetNotifyStateBlueprintData()
		, _OnGetNotifyNativeClasses()
		, _OnGetNotifyStateNativeClasses()
		, _OnGetScrubValue()
		, _OnGetDraggedNodePos()
		, _OnNodeDragStarted()
		, _OnRequestTrackPan()
		, _OnRequestOffsetRefresh()
		, _OnDeleteNotify()
		, _OnGetIsAnimNotifySelectionValidForReplacement()
		, _OnReplaceSelectedWithNotify()
		, _OnReplaceSelectedWithBlueprintNotify()
		, _OnDeselectAllNotifies()
		, _OnCopyNodes()
		, _OnPasteNodes()
		, _OnSetInputViewRange()
		{}

		SLATE_ARGUMENT( class UAnimSequenceBase*, Sequence )
		SLATE_ARGUMENT( TArray<FAnimNotifyEvent *>, AnimNotifies )
		SLATE_ARGUMENT( TArray<FAnimSyncMarker *>, AnimSyncMarkers)
		SLATE_ATTRIBUTE( float, ViewInputMin )
		SLATE_ATTRIBUTE( float, ViewInputMax )
		SLATE_ATTRIBUTE( TArray<FTrackMarkerBar>, MarkerBars)
		SLATE_ARGUMENT( int32, TrackIndex )
		SLATE_ARGUMENT( FLinearColor, TrackColor )
		SLATE_ATTRIBUTE(EVisibility, QueuedNotifyTimingNodeVisibility)
		SLATE_ATTRIBUTE(EVisibility, BranchingPointTimingNodeVisibility)
		SLATE_EVENT(FOnTrackSelectionChanged, OnSelectionChanged)
		SLATE_EVENT( FOnUpdatePanel, OnUpdatePanel )
		SLATE_EVENT( FOnGetBlueprintNotifyData, OnGetNotifyBlueprintData )
		SLATE_EVENT( FOnGetBlueprintNotifyData, OnGetNotifyStateBlueprintData )
		SLATE_EVENT( FOnGetNativeNotifyClasses, OnGetNotifyNativeClasses )
		SLATE_EVENT( FOnGetNativeNotifyClasses, OnGetNotifyStateNativeClasses )
		SLATE_EVENT( FOnGetScrubValue, OnGetScrubValue )
		SLATE_EVENT( FOnGetDraggedNodePos, OnGetDraggedNodePos )
		SLATE_EVENT( FOnNotifyNodesDragStarted, OnNodeDragStarted )
		SLATE_EVENT( FPanTrackRequest, OnRequestTrackPan )
		SLATE_EVENT( FRefreshOffsetsRequest, OnRequestOffsetRefresh )
		SLATE_EVENT( FDeleteNotify, OnDeleteNotify )
		SLATE_EVENT( FOnGetIsAnimNotifySelectionValidForReplacement, OnGetIsAnimNotifySelectionValidForReplacement)
		SLATE_EVENT( FReplaceWithNotify, OnReplaceSelectedWithNotify )
		SLATE_EVENT( FReplaceWithBlueprintNotify, OnReplaceSelectedWithBlueprintNotify)
		SLATE_EVENT( FDeselectAllNotifies, OnDeselectAllNotifies)
		SLATE_EVENT( FCopyNodes, OnCopyNodes )
		SLATE_EVENT(FPasteNodes, OnPasteNodes)
		SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
		SLATE_EVENT( FOnGetTimingNodeVisibility, OnGetTimingNodeVisibility )
		SLATE_EVENT(FOnInvokeTab, OnInvokeTab)
		SLATE_END_ARGS()
public:

	/** Type used for list widget of tracks */
	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override { UpdateCachedGeometry( AllottedGeometry ); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	// End of SWidget interface

	/**
	 * Update the nodes to match the data that the panel is observing
	 */
	void Update();

	/** Returns the cached rendering geometry of this track */
	const FGeometry& GetCachedGeometry() const { return CachedGeometry; }

	FTrackScaleInfo GetCachedScaleInfo() const { return FTrackScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, CachedGeometry.GetLocalSize()); }

	/** Updates sequences when a notify node has been successfully dragged to a new position
	 *	@param Offset - Offset from the widget to the time handle 
	 */
	void HandleNodeDrop(TSharedPtr<SAnimNotifyNode> Node, float Offset = 0.0f);

	// Number of nodes in the track currently selected
	int32 GetNumSelectedNodes() const { return SelectedNodeIndices.Num(); }

	// Index of the track in the notify panel
	int32 GetTrackIndex() const { return TrackIndex; }

	// Time at the position of the last mouseclick
	float GetLastClickedTime() const { return LastClickedTime; }

	// Removes the node widgets from the track and adds them to the provided Array
	void DisconnectSelectedNodesForDrag(TArray<TSharedPtr<SAnimNotifyNode>>& DragNodes);

	// Adds our current selection to the provided set
	void AppendSelectionToSet(FGraphPanelSelectionSet& SelectionSet);
	// Adds our current selection to the provided array
	void AppendSelectionToArray(TArray<INodeObjectInterface*>& Selection) const;
	// Gets the currently selected SAnimNotifyNode instances
	void AppendSelectedNodeWidgetsToArray(TArray<TSharedPtr<SAnimNotifyNode>>& NodeArray) const;
	// Gets the indices of the selected notifies
	TArray<int32> GetSelectedNotifyIndices() const {return SelectedNodeIndices;}

	INodeObjectInterface* GetNodeObjectInterface(int32 NodeIndex) { return NotifyNodes[NodeIndex]->NodeObjectInterface; }
	/**
	* Deselects all currently selected notify nodes
	* @param bUpdateSelectionSet - Whether we should report a selection change to the panel
	*/
	void DeselectAllNotifyNodes(bool bUpdateSelectionSet = true);

	// get Property Data of one element (NotifyIndex) from Notifies property of Sequence
	static uint8* FindNotifyPropertyData(UAnimSequenceBase* Sequence, int32 NotifyIndex, UArrayProperty*& ArrayProperty);

	// Paste a single Notify into this track from an exported string
	void PasteSingleNotify(FString& NotifyString, float PasteTime);

	// Paste a single Sync Marker into this track from an exported string
	void PasteSingleSyncMarker(FString& MarkerString, float PasteTime);

	// Uses the given track space rect and marquee information to refresh selection information
	void RefreshMarqueeSelectedNodes(FSlateRect& Rect, FNotifyMarqueeOperation& Marquee);

	// Create new notifies
	FAnimNotifyEvent& CreateNewBlueprintNotify(FString NewNotifyName, FString BlueprintPath, float StartTime);
	FAnimNotifyEvent& CreateNewNotify(FString NewNotifyName, UClass* NotifyClass, float StartTime);

	// Get the Blueprint Class from the path of the Blueprint
	static TSubclassOf<UObject> GetBlueprintClassFromPath(FString BlueprintPath);

	// Get the default Notify Name for a given blueprint notify asset
	FString MakeBlueprintNotifyName(FAssetData& NotifyAssetData);

	// Need to make sure tool tips are cleared during node clear up so slate system won't
	// call into invalid notify.
	void ClearNodeTooltips();

protected:

	void CreateCommands();

	// Build up a "New Notify..." menu
	void FillNewNotifyMenu(FMenuBuilder& MenuBuilderbool, bool bIsReplaceWithMenu = false);
	void FillNewNotifyStateMenu(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu  = false);

	// New notify functions
	void CreateNewBlueprintNotifyAtCursor(FString NewNotifyName, FString BlueprintPath);
	void CreateNewNotifyAtCursor(FString NewNotifyName, UClass* NotifyClass);
	void CreateNewSyncMarkerAtCursor(FString NewSyncMarkerName, UClass* NotifyClass);
	void OnNewNotifyClicked();
	void OnNewSyncMarkerClicked();
	void AddNewNotify(const FText& NewNotifyName, ETextCommit::Type CommitInfo);
	void AddNewSyncMarker(const FText& NewMarkerName, ETextCommit::Type CommitInfo);

	// Trigger weight functions
	void OnSetTriggerWeightNotifyClicked(int32 NotifyIndex);
	void SetTriggerWeight(const FText& TriggerWeight, ETextCommit::Type CommitInfo, int32 NotifyIndex);

	// "Replace with... " commands
	void ReplaceSelectedWithBlueprintNotify(FString NewNotifyName, FString BlueprintPath);
	void ReplaceSelectedWithNotify(FString NewNotifyName, UClass* NotifyClass);
	bool IsValidToPlace(UClass* NotifyClass) const;
	void OnSetNodeTimeClicked(int32 NodeIndex);
	void SetNodeTime(const FText& NodeTimeText, ETextCommit::Type CommitInfo, int32 NodeIndex);
	void OnSetNodeFrameClicked(int32 NodeIndex);
	void SetNodeFrame(const FText& NodeFrameText, ETextCommit::Type CommitInfo, int32 NodeIndex);

	// Whether we have one node selected
	bool IsSingleNodeSelected();
	// Checks the clipboard for an anim notify buffer, and returns whether there's only one notify
	bool IsSingleNodeInClipboard();

	void OnSetDurationNotifyClicked(int32 NotifyIndex);
	void SetDuration(const FText& DurationText, ETextCommit::Type CommitInfo, int32 NotifyIndex);

	/** Function to copy anim notify event */
	void OnCopyNotifyClicked(int32 NotifyIndex);

	/** Function to check whether it is possible to paste anim notify event */
	bool CanPasteAnimNotify() const;

	/** Handler for context menu paste command */
	void OnPasteNotifyClicked(ENotifyPasteMode::Type PasteMode, ENotifyPasteMultipleMode::Type MultiplePasteType = ENotifyPasteMultipleMode::Absolute);

	/** Handler for popup window asking the user for a paste time */
	void OnPasteNotifyTimeSet(const FText& TimeText, ETextCommit::Type CommitInfo);

	/** Function to paste a previously copied notify */
	void OnPasteNotify(float TimeToPasteAt, ENotifyPasteMultipleMode::Type MultiplePasteType = ENotifyPasteMultipleMode::Absolute);

	/** Provides direct access to the notify menu from the context menu */
	void OnManageNotifies();

	/** Opens the supplied blueprint in an editor */
	void OnOpenNotifySource(UBlueprint* InSourceBlueprint) const;

	/**
	 * Selects a node on the track. Supports multi selection
	 * @param TrackNodeIndex - Index of the node to select.
	 * @param Append - Whether to append to to current selection or start a new one.
	 * @param bUpdateSelection - Whether to immediately inform Persona of a selection change
	 */
	void SelectTrackObjectNode(int32 TrackNodeIndex, bool Append, bool bUpdateSelection = true);
	
	/**
	 * Toggles the selection status of a notify node, for example when
	 * Control is held when clicking.
	 * @param NotifyIndex - Index of the notify to toggle the selection status of
	 * @param bUpdateSelection - Whether to immediately inform Persona of a selection change
	 */
	void ToggleTrackObjectNodeSelectionStatus(int32 TrackNodeIndex, bool bUpdateSelection = true);

	/**
	 * Deselects requested notify node
	 * @param NotifyIndex - Index of the notify node to deselect
	 * @param bUpdateSelection - Whether to immediately inform Persona of a selection change
	 */
	void DeselectTrackObjectNode(int32 TrackNodeIndex, bool bUpdateSelection = true);

	int32 GetHitNotifyNode(const FGeometry& MyGeometry, const FVector2D& Position);

	TSharedPtr<SWidget> SummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	float CalculateTime( const FGeometry& MyGeometry, FVector2D NodePos, bool bInputIsAbsolute = true );

	// Handler that is called when the user starts dragging a node
	FReply OnNotifyNodeDragStarted( TSharedRef<SAnimNotifyNode> NotifyNode, const FPointerEvent& MouseEvent, const FVector2D& ScreenNodePosition, const bool bDragOnMarker, int32 NotifyIndex );

	const EVisibility GetTimingNodeVisibility(TSharedPtr<SAnimNotifyNode> NotifyNode);

private:

	// Data structure for bluprint notify context menu entries
	struct BlueprintNotifyMenuInfo
	{
		FString NotifyName;
		FString BlueprintPath;
		UClass* BaseClass;
	};

	// Format notify asset data into the information needed for menu display
	void GetNotifyMenuData(TArray<FAssetData>& NotifyAssetData, TArray<BlueprintNotifyMenuInfo>& OutNotifyMenuData);

	// Store the tracks geometry for later use
	void UpdateCachedGeometry(const FGeometry& InGeometry);

	// Returns the padding needed to render the notify in the correct track position
	FMargin GetNotifyTrackPadding(int32 NotifyIndex) const
	{
		float LeftMargin = NotifyPairs[NotifyIndex]->GetWidgetPaddingLeft();
		float RightMargin = CachedGeometry.GetLocalSize().X - NotifyNodes[NotifyIndex]->GetWidgetPosition().X - NotifyNodes[NotifyIndex]->GetSize().X;
		return FMargin(LeftMargin, 0, RightMargin, 0);
	}

	// Returns the padding needed to render the notify in the correct track position
	FMargin GetSyncMarkerTrackPadding(int32 SyncMarkerIndex) const
	{
		float LeftMargin = NotifyNodes[SyncMarkerIndex]->GetWidgetPosition().X;
		float RightMargin = CachedGeometry.GetLocalSize().X - NotifyNodes[SyncMarkerIndex]->GetWidgetPosition().X - NotifyNodes[SyncMarkerIndex]->GetSize().X;
		return FMargin(LeftMargin, 0, RightMargin, 0);
	}

	// Builds a UObject selection set and calls the OnSelectionChanged delegate
	void SendSelectionChanged()
	{
		OnSelectionChanged.ExecuteIfBound();
	}

protected:
	TSharedPtr<FUICommandList> AnimSequenceEditorActions;

	float LastClickedTime;

	class UAnimSequenceBase*				Sequence; // need for menu generation of anim notifies - 
	TArray<TSharedPtr<SAnimNotifyNode>>		NotifyNodes;
	TArray<TSharedPtr<SAnimNotifyPair>>		NotifyPairs;
	TArray<FAnimNotifyEvent*>				AnimNotifies;
	TArray<FAnimSyncMarker*>				AnimSyncMarkers;
	TAttribute<float>						ViewInputMin;
	TAttribute<float>						ViewInputMax;
	TAttribute<FLinearColor>				TrackColor;
	int32									TrackIndex;
	TAttribute<EVisibility>					NotifyTimingNodeVisibility;
	TAttribute<EVisibility>					BranchingPointTimingNodeVisibility;
	FOnTrackSelectionChanged				OnSelectionChanged;
	FOnUpdatePanel							OnUpdatePanel;
	FOnGetBlueprintNotifyData				OnGetNotifyBlueprintData;
	FOnGetBlueprintNotifyData				OnGetNotifyStateBlueprintData;
	FOnGetNativeNotifyClasses				OnGetNotifyNativeClasses;
	FOnGetNativeNotifyClasses				OnGetNotifyStateNativeClasses;
	FOnGetScrubValue						OnGetScrubValue;
	FOnGetDraggedNodePos					OnGetDraggedNodePos;
	FOnNotifyNodesDragStarted				OnNodeDragStarted;
	FPanTrackRequest						OnRequestTrackPan;
	FDeselectAllNotifies					OnDeselectAllNotifies;
	FCopyNodes							OnCopyNodes;
	FPasteNodes								OnPasteNodes;
	FOnSetInputViewRange					OnSetInputViewRange;
	FOnGetTimingNodeVisibility				OnGetTimingNodeVisibility;

	/** Delegate to call when offsets should be refreshed in a montage */
	FRefreshOffsetsRequest					OnRequestRefreshOffsets;

	/** Delegate to call when deleting notifies */
	FDeleteNotify							OnDeleteNotify;

	/** Delegates to call when replacing notifies */
	FOnGetIsAnimNotifySelectionValidForReplacement OnGetIsAnimNotifySelectionValidforReplacement;
	FReplaceWithNotify						OnReplaceSelectedWithNotify;
	FReplaceWithBlueprintNotify				OnReplaceSelectedWithBlueprintNotify;

	FOnInvokeTab							OnInvokeTab;

	TSharedPtr<SBorder>						TrackArea;

	/** Cache the SOverlay used to store all this tracks nodes */
	TSharedPtr<SOverlay> NodeSlots;

	/** Cached for drag drop handling code */
	FGeometry CachedGeometry;

	/** Attribute for accessing any marker positions we have to draw */
	TAttribute<TArray<FTrackMarkerBar>>		MarkerBars;

	/** Nodes that are currently selected */
	TArray<int32> SelectedNodeIndices;
};

//////////////////////////////////////////////////////////////////////////
// 

/** Widget for drawing a single track */
class SNotifyEdTrack : public SCompoundWidget
{
private:
	/** Index of Track in Sequence **/
	int32									TrackIndex;

	/** Anim Sequence **/
	class UAnimSequenceBase*				Sequence;

	/** Pointer to notify panel for drawing*/
	TWeakPtr<SAnimNotifyPanel>			AnimPanelPtr;

	/** Pointer to the track name UI */
	TSharedPtr<SInlineEditableTextBlock> TrackText;

public:
	SLATE_BEGIN_ARGS( SNotifyEdTrack )
		: _TrackIndex(INDEX_NONE)
		, _AnimNotifyPanel()
		, _Sequence()
		, _WidgetWidth()
		, _ViewInputMin()
		, _ViewInputMax()
		, _OnSelectionChanged()
		, _OnUpdatePanel()
		, _OnDeleteNotify()
		, _OnDeselectAllNotifies()
		, _OnCopyNodes()
		, _OnSetInputViewRange()
	{}
	SLATE_ARGUMENT( int32, TrackIndex )
	SLATE_ARGUMENT( TSharedPtr<SAnimNotifyPanel>, AnimNotifyPanel)
	SLATE_ARGUMENT( class UAnimSequenceBase*, Sequence )
	SLATE_ARGUMENT( float, WidgetWidth )
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_ATTRIBUTE( TArray<FTrackMarkerBar>, MarkerBars)
	SLATE_ATTRIBUTE( EVisibility, NotifyTimingNodeVisibility )
	SLATE_ATTRIBUTE( EVisibility, BranchingPointTimingNodeVisibility )
	SLATE_EVENT( FOnTrackSelectionChanged, OnSelectionChanged)
	SLATE_EVENT( FOnGetScrubValue, OnGetScrubValue )
	SLATE_EVENT( FOnGetDraggedNodePos, OnGetDraggedNodePos )
	SLATE_EVENT( FOnUpdatePanel, OnUpdatePanel )
	SLATE_EVENT( FOnGetBlueprintNotifyData, OnGetNotifyBlueprintData )
	SLATE_EVENT( FOnGetBlueprintNotifyData, OnGetNotifyStateBlueprintData )
	SLATE_EVENT( FOnGetNativeNotifyClasses, OnGetNotifyNativeClasses )
	SLATE_EVENT( FOnGetNativeNotifyClasses, OnGetNotifyStateNativeClasses )
	SLATE_EVENT( FOnNotifyNodesDragStarted, OnNodeDragStarted )
	SLATE_EVENT( FRefreshOffsetsRequest, OnRequestRefreshOffsets )
	SLATE_EVENT( FDeleteNotify, OnDeleteNotify )
	SLATE_EVENT( FDeselectAllNotifies, OnDeselectAllNotifies)
	SLATE_EVENT( FCopyNodes, OnCopyNodes )
	SLATE_EVENT( FPasteNodes, OnPasteNodes )
	SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
	SLATE_EVENT( FOnGetTimingNodeVisibility, OnGetTimingNodeVisibility )
	SLATE_EVENT(FOnInvokeTab, OnInvokeTab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	bool CanDeleteTrack();

	/** Pointer to actual anim notify track */
	TSharedPtr<class SAnimNotifyTrack>	NotifyTrack;

	/** Return the tracks name as an FText */
	FText GetTrackName() const
	{
		if(Sequence->AnimNotifyTracks.IsValidIndex(TrackIndex))
		{
			return FText::FromName(Sequence->AnimNotifyTracks[TrackIndex].TrackName);
		}

		/** Should never be possible but better than crashing the editor */
		return LOCTEXT("TrackName_Invalid", "Invalid Track");
	}

	/** Activate the editable text box for the track name */
	void TriggerRename()
	{
		TrackText->EnterEditingMode();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FNotifyDragDropOp : public FDragDropOperation
{
public:
	FNotifyDragDropOp(float& InCurrentDragXPosition) : 
		CurrentDragXPosition(InCurrentDragXPosition), 
		SnapTime(-1.f),
		SelectionTimeLength(0.0f)
	{
	}

	struct FTrackClampInfo
	{
		int32 TrackPos;
		int32 TrackSnapTestPos;
		float TrackMax;
		float TrackMin;
		float TrackWidth;
		TSharedPtr<SAnimNotifyTrack> NotifyTrack;
	};

	DRAG_DROP_OPERATOR_TYPE(FNotifyDragDropOp, FDragDropOperation)

	virtual void OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent ) override
	{
		if ( bDropWasHandled == false )
		{
			int32 NumNodes = SelectedNodes.Num();
			for(int32 CurrentNode = 0 ; CurrentNode < NumNodes ; ++CurrentNode)
			{
				TSharedPtr<SAnimNotifyNode> Node = SelectedNodes[CurrentNode];
				float NodePositionOffset = NodeXOffsets[CurrentNode];
				const FTrackClampInfo& ClampInfo = GetTrackClampInfo(Node->GetScreenPosition());
				ClampInfo.NotifyTrack->HandleNodeDrop(Node, NodePositionOffset);
				Node->DropCancelled();
			}

			Sequence->MarkPackageDirty();
			OnUpdatePanel.ExecuteIfBound();
		}
		
		FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);
	}

	virtual void OnDragged( const class FDragDropEvent& DragDropEvent ) override
	{
		// Reset snapped node pointer
		SnappedNode = NULL;

		NodeGroupPosition = DragDropEvent.GetScreenSpacePosition() + DragOffset;

		FVector2D SelectionBeginPosition = NodeGroupPosition + SelectedNodes[0]->GetNotifyPositionOffset();
		
		FTrackClampInfo* SelectionPositionClampInfo = &GetTrackClampInfo(DragDropEvent.GetScreenSpacePosition());
		if(SelectionPositionClampInfo->NotifyTrack->GetTrackIndex() < TrackSpan)
		{
			// Our selection has moved off the bottom of the notify panel, adjust the clamping information to keep it on the panel
			SelectionPositionClampInfo = &ClampInfos[ClampInfos.Num() - TrackSpan - 1];
		}

		const FGeometry& TrackGeom = SelectionPositionClampInfo->NotifyTrack->GetCachedGeometry();
		const FTrackScaleInfo& TrackScaleInfo = SelectionPositionClampInfo->NotifyTrack->GetCachedScaleInfo();
		
		// Tracks the movement amount to apply to the selection due to a snap.
		float SnapMovement= 0.0f;
		// Clamp the selection into the track
		const float SelectionLocalLength = TrackScaleInfo.PixelsPerInput * SelectionTimeLength;
		const float ClampedEnd = FMath::Clamp(SelectionBeginPosition.X + NodeGroupSize.X, SelectionPositionClampInfo->TrackMin, SelectionPositionClampInfo->TrackMax);
		const float ClampedBegin = FMath::Clamp(SelectionBeginPosition.X, SelectionPositionClampInfo->TrackMin, SelectionPositionClampInfo->TrackMax);
		if(ClampedBegin > SelectionBeginPosition.X)
		{
			SelectionBeginPosition.X = ClampedBegin;
		}
		else if(ClampedEnd < SelectionBeginPosition.X + SelectionLocalLength)
		{
			SelectionBeginPosition.X = ClampedEnd - SelectionLocalLength;
		}

		// Handle node snaps
		for(int32 NodeIdx = 0 ; NodeIdx < SelectedNodes.Num() ; ++NodeIdx)
		{
			TSharedPtr<SAnimNotifyNode> CurrentNode = SelectedNodes[NodeIdx];

			// Clear off any snap time currently stored
			CurrentNode->ClearLastSnappedTime();

			const FTrackClampInfo& NodeClamp = GetTrackClampInfo(CurrentNode->GetScreenPosition());

			FVector2D EventPosition = SelectionBeginPosition + FVector2D(TrackScaleInfo.PixelsPerInput * NodeTimeOffsets[NodeIdx], 0.0f);

			// Look for a snap on the first scrub handle
			FVector2D TrackNodePos = TrackGeom.AbsoluteToLocal(EventPosition);
			const FVector2D OriginalNodePosition = TrackNodePos;
			float SequenceEnd = TrackScaleInfo.InputToLocalX(Sequence->SequenceLength);

			// Always clamp the Y to the current track
			SelectionBeginPosition.Y = SelectionPositionClampInfo->TrackPos;

			float SnapX = GetSnapPosition(NodeClamp, TrackNodePos.X);
			if (FAnimNotifyEvent* CurrentEvent = CurrentNode->NodeObjectInterface->GetNotifyEvent())
			{
				if (SnapX >= 0.f)
				{
					EAnimEventTriggerOffsets::Type Offset = EAnimEventTriggerOffsets::NoOffset;
					if (SnapX == 0.0f || SnapX == SequenceEnd)
					{
						Offset = SnapX > 0.0f ? EAnimEventTriggerOffsets::OffsetBefore : EAnimEventTriggerOffsets::OffsetAfter;
					}
					else
					{
						Offset = (SnapX < TrackNodePos.X) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
					}

					CurrentEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);
					CurrentNode->SetLastSnappedTime(TrackScaleInfo.LocalXToInput(SnapX));

					if (SnapMovement == 0.0f)
					{
						SnapMovement = SnapX - TrackNodePos.X;
						TrackNodePos.X = SnapX;
						SnapTime = TrackScaleInfo.LocalXToInput(SnapX);
						SnappedNode = CurrentNode;
					}
					EventPosition = NodeClamp.NotifyTrack->GetCachedGeometry().LocalToAbsolute(TrackNodePos);
				}
				else
				{
					CurrentEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
				}

				if (CurrentNode.IsValid() && CurrentEvent->GetDuration() > 0)
				{
					// If we didn't snap the beginning of the node, attempt to snap the end
					if (SnapX == -1.0f)
					{
						FVector2D TrackNodeEndPos = TrackNodePos + CurrentNode->GetDurationSize();
						SnapX = GetSnapPosition(*SelectionPositionClampInfo, TrackNodeEndPos.X);

						// Only attempt to snap if the node will fit on the track
						if (SnapX >= CurrentNode->GetDurationSize())
						{
							EAnimEventTriggerOffsets::Type Offset = EAnimEventTriggerOffsets::NoOffset;
							if (SnapX == SequenceEnd)
							{
								// Only need to check the end of the sequence here; end handle can't hit the beginning
								Offset = EAnimEventTriggerOffsets::OffsetBefore;
							}
							else
							{
								Offset = (SnapX < TrackNodeEndPos.X) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
							}
							CurrentEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);

							if (SnapMovement == 0.0f)
							{
								SnapMovement = SnapX - TrackNodeEndPos.X;
								SnapTime = TrackScaleInfo.LocalXToInput(SnapX) - CurrentEvent->GetDuration();
								CurrentNode->SetLastSnappedTime(SnapTime);
								SnappedNode = CurrentNode;
							}
						}
						else
						{
							// Remove any trigger time if we can't fit the node in.
							CurrentEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
						}
					}
				}
			}
		}

		SelectionBeginPosition.X += SnapMovement;

		CurrentDragXPosition = SelectionPositionClampInfo->NotifyTrack->GetCachedGeometry().AbsoluteToLocal(FVector2D(SelectionBeginPosition.X,0.0f)).X;

		CursorDecoratorWindow->MoveWindowTo(SelectionBeginPosition - SelectedNodes[0]->GetNotifyPositionOffset());
		NodeGroupPosition = SelectionBeginPosition;

		//scroll view
		float MouseXPos = DragDropEvent.GetScreenSpacePosition().X;
		if(MouseXPos < SelectionPositionClampInfo->TrackMin)
		{
			float ScreenDelta = MouseXPos - SelectionPositionClampInfo->TrackMin;
			RequestTrackPan.Execute(ScreenDelta, FVector2D(SelectionPositionClampInfo->TrackWidth, 1.f));
		}
		else if(MouseXPos > SelectionPositionClampInfo->TrackMax)
		{
			float ScreenDelta = MouseXPos - SelectionPositionClampInfo->TrackMax;
			RequestTrackPan.Execute(ScreenDelta, FVector2D(SelectionPositionClampInfo->TrackWidth, 1.f));
		}
	}

	float GetSnapPosition(const FTrackClampInfo& ClampInfo, float WidgetSpaceNotifyPosition)
	{
		const FTrackScaleInfo& ScaleInfo = ClampInfo.NotifyTrack->GetCachedScaleInfo();

		const float MaxSnapDist = 5.f;

		float CurrentMinSnapDest = MaxSnapDist;
		float SnapPosition = -1.f;

		if(MarkerBars.IsBound())
		{
			const TArray<FTrackMarkerBar>& Bars = MarkerBars.Get();
			if(Bars.Num() > 0)
			{
				for(int32 i = 0; i < Bars.Num(); ++i)
				{
					float WidgetSpaceSnapPosition = ScaleInfo.InputToLocalX(Bars[i].Time); // Do comparison in screen space so that zoom does not cause issues
					float ThisMinSnapDist = FMath::Abs(WidgetSpaceSnapPosition - WidgetSpaceNotifyPosition);
					if(ThisMinSnapDist < CurrentMinSnapDest)
					{
						CurrentMinSnapDest = ThisMinSnapDist;
						SnapPosition = WidgetSpaceSnapPosition;
					}
				}
			}
		}

		if(SnapPosition == -1.0f)
		{
			// Didn't snap to a bar, snap to the track bounds
			float WidgetSpaceEndPosition = ScaleInfo.InputToLocalX(Sequence->SequenceLength);
			float SnapDistBegin = FMath::Abs(-WidgetSpaceNotifyPosition);
			float SnapDistEnd = FMath::Abs(WidgetSpaceEndPosition - WidgetSpaceNotifyPosition);
			if(SnapDistBegin < CurrentMinSnapDest)
			{
				SnapPosition = 0.0f;
			}
			else if(SnapDistEnd < CurrentMinSnapDest)
			{
				SnapPosition = WidgetSpaceEndPosition;
			}
		}

		return SnapPosition;
	}

	FTrackClampInfo& GetTrackClampInfo(const FVector2D NodePos)
	{
		int32 ClampInfoIndex = 0;
		int32 SmallestNodeTrackDist = FMath::Abs(ClampInfos[0].TrackSnapTestPos - NodePos.Y);
		for(int32 i = 0; i < ClampInfos.Num(); ++i)
		{
			int32 Dist = FMath::Abs(ClampInfos[i].TrackSnapTestPos - NodePos.Y);
			if(Dist < SmallestNodeTrackDist)
			{
				SmallestNodeTrackDist = Dist;
				ClampInfoIndex = i;
			}
		}
		return ClampInfos[ClampInfoIndex];
	}

	class UAnimSequenceBase*			Sequence;				// The owning anim sequence
	FVector2D							DragOffset;				// Offset from the mouse to place the decorator
	TArray<FTrackClampInfo>				ClampInfos;				// Clamping information for all of the available tracks
	float&								CurrentDragXPosition;	// Current X position of the drag operation
	FPanTrackRequest					RequestTrackPan;		// Delegate to request a pan along the edges of a zoomed track
	TArray<float>						NodeTimes;				// Times to drop each selected node at
	float								SnapTime;				// The time that the snapped node was snapped to
	TWeakPtr<SAnimNotifyNode>			SnappedNode;			// The node chosen for the snap
	TAttribute<TArray<FTrackMarkerBar>>	MarkerBars;				// Branching point markers
	TArray<TSharedPtr<SAnimNotifyNode>> SelectedNodes;			// The nodes that are in the current selection
	TArray<float>						NodeTimeOffsets;		// Time offsets from the beginning of the selection to the nodes.
	TArray<float>						NodeXOffsets;			// Offsets in X from the widget position to the scrub handle for each node.
	FVector2D							NodeGroupPosition;		// Position of the beginning of the selection
	FVector2D							NodeGroupSize;			// Size of the entire selection
	TSharedPtr<SWidget>					Decorator;				// The widget to display when dragging
	float								SelectionTimeLength;	// Length of time that the selection covers
	int32								TrackSpan;				// Number of tracks that the selection spans
	FOnUpdatePanel						OnUpdatePanel;			// Delegate to redraw the notify panel

	static TSharedRef<FNotifyDragDropOp> New(
		TArray<TSharedPtr<SAnimNotifyNode>>			NotifyNodes, 
		TSharedPtr<SWidget>							Decorator, 
		const TArray<TSharedPtr<SAnimNotifyTrack>>& NotifyTracks, 
		class UAnimSequenceBase*					InSequence, 
		const FVector2D&							CursorPosition, 
		const FVector2D&							SelectionScreenPosition, 
		const FVector2D&							SelectionSize, 
		float&										CurrentDragXPosition, 
		FPanTrackRequest&							RequestTrackPanDelegate, 
		TAttribute<TArray<FTrackMarkerBar>>			MarkerBars,
		FOnUpdatePanel&								UpdatePanel
		)
	{
		TSharedRef<FNotifyDragDropOp> Operation = MakeShareable(new FNotifyDragDropOp(CurrentDragXPosition));
		Operation->Sequence = InSequence;
		Operation->RequestTrackPan = RequestTrackPanDelegate;
		Operation->OnUpdatePanel = UpdatePanel;

		Operation->NodeGroupPosition = SelectionScreenPosition;
		Operation->NodeGroupSize = SelectionSize;
		Operation->DragOffset = SelectionScreenPosition - CursorPosition;
		Operation->MarkerBars = MarkerBars;
		Operation->Decorator = Decorator;
		Operation->SelectedNodes = NotifyNodes;
		Operation->TrackSpan = NotifyNodes[0]->NodeObjectInterface->GetTrackIndex() - NotifyNodes.Last()->NodeObjectInterface->GetTrackIndex();
		
		// Caclulate offsets for the selected nodes
		float BeginTime = MAX_flt;
		for(TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
		{
			float NotifyTime = Node->NodeObjectInterface->GetTime();

			if(NotifyTime < BeginTime)
			{
				BeginTime = NotifyTime;
			}
		}

		// Initialise node data
		for(TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
		{
			float NotifyTime = Node->NodeObjectInterface->GetTime();

			Node->ClearLastSnappedTime();
			Operation->NodeTimeOffsets.Add(NotifyTime - BeginTime);
			Operation->NodeTimes.Add(NotifyTime);
			Operation->NodeXOffsets.Add(Node->GetNotifyPositionOffset().X);

			// Calculate the time length of the selection. Because it is possible to have states
			// with arbitrary durations we need to search all of the nodes and find the furthest
			// possible point
			Operation->SelectionTimeLength = FMath::Max(Operation->SelectionTimeLength, NotifyTime + Node->NodeObjectInterface->GetDuration() - BeginTime);
		}

		Operation->Construct();

		for(int32 i = 0; i < NotifyTracks.Num(); ++i)
		{
			FTrackClampInfo Info;
			Info.NotifyTrack = NotifyTracks[i];
			const FGeometry& CachedGeometry = Info.NotifyTrack->GetCachedGeometry();
			Info.TrackPos = CachedGeometry.AbsolutePosition.Y;
			Info.TrackWidth = CachedGeometry.GetLocalSize().X;
			Info.TrackMin = CachedGeometry.AbsolutePosition.X;
			Info.TrackMax = Info.TrackMin + Info.TrackWidth;
			Info.TrackSnapTestPos = Info.TrackPos + (CachedGeometry.Size.Y / 2);
			Operation->ClampInfos.Add(Info);
		}

		Operation->CursorDecoratorWindow->SetOpacity(0.5f);
		return Operation;
	}
	
	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return Decorator;
	}

	FText GetHoverText() const
	{
		FText HoverText = LOCTEXT("Invalid", "Invalid");

		if(SelectedNodes[0].IsValid())
		{
			HoverText = FText::FromName( SelectedNodes[0]->NodeObjectInterface->GetName() );
		}

		return HoverText;
	}
};

//////////////////////////////////////////////////////////////////////////
// FAnimSequenceEditorCommands

class FAnimSequenceEditorCommands : public TCommands<FAnimSequenceEditorCommands>
{
public:
	FAnimSequenceEditorCommands()
		: TCommands<FAnimSequenceEditorCommands>(
		TEXT("AnimSequenceEditor"),
		NSLOCTEXT("Contexts", "AnimSequenceEditor", "Sequence Editor"),
		NAME_None, FEditorStyle::GetStyleSetName()
		)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND( SomeSequenceAction, "Some Sequence Action", "Does some sequence action", EUserInterfaceActionType::Button, FInputChord() );
	}
public:
	TSharedPtr<FUICommandInfo> SomeSequenceAction;
};

//////////////////////////////////////////////////////////////////////////
// SAnimNotifyNode

const float SAnimNotifyNode::MinimumStateDuration = (1.0f / 30.0f);

void SAnimNotifyNode::Construct(const FArguments& InArgs)
{
	Sequence = InArgs._Sequence;
	Font = FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );
	bBeingDragged = false;
	CurrentDragHandle = ENotifyStateHandleHit::None;
	bDrawTooltipToRight = true;
	bSelected = false;
	DragMarkerTransactionIdx = INDEX_NONE;
	

	if (InArgs._AnimNotify)
	{
		MakeNodeInterface<FNotifyNodeInterface>(InArgs._AnimNotify);
	}
	else if (InArgs._AnimSyncMarker)
	{
		MakeNodeInterface<FSyncMarkerNodeInterface>(InArgs._AnimSyncMarker);
	}
	else
	{
		check(false);	// Must specify something for this node to represent
						// Either AnimNotify or AnimSyncMarker
	}
	// Cache notify name for blueprint / Native notifies.
	NodeObjectInterface->CacheName();

	OnNodeDragStarted = InArgs._OnNodeDragStarted;
	PanTrackRequest = InArgs._PanTrackRequest;
	OnDeselectAllNotifies = InArgs._OnDeselectAllNotifies;

	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;

	MarkerBars = InArgs._MarkerBars;

	if(InArgs._StateEndTimingNode.IsValid())
	{
		// The overlay will use the desired size to calculate the notify node size,
		// compute that once here.
		InArgs._StateEndTimingNode->SlatePrepass(1.0f);
		SAssignNew(EndMarkerNodeOverlay, SOverlay)
			+ SOverlay::Slot()
			[
				InArgs._StateEndTimingNode.ToSharedRef()
			];
	}

	SetToolTipText(TAttribute<FText>(this, &SAnimNotifyNode::GetNodeTooltip));
}

FReply SAnimNotifyNode::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FVector2D ScreenNodePosition = MyGeometry.AbsolutePosition;
	
	// Whether the drag has hit a duration marker
	bool bDragOnMarker = false;
	bBeingDragged = true;

	if(GetDurationSize() > 0.0f)
	{
		// This is a state node, check for a drag on the markers before movement. Use last screen space position before the drag started
		// as using the last position in the mouse event gives us a mouse position after the drag was started.
		ENotifyStateHandleHit::Type MarkerHit = DurationHandleHitTest(LastMouseDownPosition);
		if(MarkerHit == ENotifyStateHandleHit::Start || MarkerHit == ENotifyStateHandleHit::End)
		{
			bDragOnMarker = true;
			bBeingDragged = false;
			CurrentDragHandle = MarkerHit;

			// Modify the owning sequence as we're now dragging the marker and begin a transaction
			check(DragMarkerTransactionIdx == INDEX_NONE);
			DragMarkerTransactionIdx = GEditor->BeginTransaction(NSLOCTEXT("AnimNotifyNode", "StateNodeDragTransation", "Drag State Node Marker"));
			Sequence->Modify();
		}
	}

	return OnNodeDragStarted.Execute(SharedThis(this), MouseEvent, ScreenNodePosition, bDragOnMarker);
}

FLinearColor SAnimNotifyNode::GetNotifyColor() const
{
	TOptional<FLinearColor> Color = NodeObjectInterface->GetEditorColor();
	FLinearColor BaseColor = Color.Get(FLinearColor(1, 1, 0.5f));

	BaseColor.A = 0.67f;

	return BaseColor;
}

FText SAnimNotifyNode::GetNotifyText() const
{
	// Combine comment from notify struct and from function on object
	return FText::FromName( NodeObjectInterface->GetName() );
}

FText SAnimNotifyNode::GetNodeTooltip() const
{
	return NodeObjectInterface->GetNodeTooltip(Sequence);
}

/** @return the Node's position within the graph */
UObject* SAnimNotifyNode::GetObjectBeingDisplayed() const
{
	TOptional<UObject*> Object = NodeObjectInterface->GetObjectBeingDisplayed();
	return Object.Get(Sequence);
}

void SAnimNotifyNode::DropCancelled()
{
	bBeingDragged = false;
}

FVector2D SAnimNotifyNode::ComputeDesiredSize( float ) const
{
	return GetSize();
}

bool SAnimNotifyNode::HitTest(const FGeometry& AllottedGeometry, FVector2D MouseLocalPose) const
{
	FVector2D Position = GetWidgetPosition();
	FVector2D Size = GetSize();

	return MouseLocalPose >= Position && MouseLocalPose <= (Position + Size);
}

ENotifyStateHandleHit::Type SAnimNotifyNode::DurationHandleHitTest(const FVector2D& CursorTrackPosition) const
{
	ENotifyStateHandleHit::Type MarkerHit = ENotifyStateHandleHit::None;

	// Make sure this node has a duration box (meaning it is a state node)
	if(NotifyDurationSizeX > 0.0f)
	{
		// Test for mouse inside duration box with handles included
		float ScrubHandleHalfWidth = ScrubHandleSize.X / 2.0f;

		// Position and size of the notify node including the scrub handles
		FVector2D NotifyNodePosition(NotifyScrubHandleCentre - ScrubHandleHalfWidth, 0.0f);
		FVector2D NotifyNodeSize(NotifyDurationSizeX + ScrubHandleHalfWidth * 2.0f, NotifyHeight);

		FVector2D MouseRelativePosition(CursorTrackPosition - GetWidgetPosition());

		if(MouseRelativePosition > NotifyNodePosition && MouseRelativePosition < (NotifyNodePosition + NotifyNodeSize))
		{
			// Definitely inside the duration box, need to see which handle we hit if any
			if(MouseRelativePosition.X <= (NotifyNodePosition.X + ScrubHandleSize.X))
			{
				// Left Handle
				MarkerHit = ENotifyStateHandleHit::Start;
			}
			else if(MouseRelativePosition.X >= (NotifyNodePosition.X + NotifyNodeSize.X - ScrubHandleSize.X))
			{
				// Right Handle
				MarkerHit = ENotifyStateHandleHit::End;
			}
		}
	}

	return MarkerHit;
}

void SAnimNotifyNode::UpdateSizeAndPosition(const FGeometry& AllottedGeometry)
{
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, AllottedGeometry.Size);

	// Cache the geometry information, the alloted geometry is the same size as the track.
	CachedAllotedGeometrySize = AllottedGeometry.Size;

	NotifyTimePositionX = ScaleInfo.InputToLocalX(NodeObjectInterface->GetTime());
	NotifyDurationSizeX = ScaleInfo.PixelsPerInput * NodeObjectInterface->GetDuration();

	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	TextSize = FontMeasureService->Measure( GetNotifyText(), Font );
	LabelWidth = TextSize.X + (TextBorderSize.X * 2.f) + (ScrubHandleSize.X / 2.f);

	bool bDrawBranchingPoint = NodeObjectInterface->IsBranchingPoint();
	BranchingPointIconSize = FVector2D(TextSize.Y, TextSize.Y);
	if (bDrawBranchingPoint)
	{
		LabelWidth += BranchingPointIconSize.X + TextBorderSize.X * 2.f;
	}

	//Calculate scrub handle box size (the notional box around the scrub handle and the alignment marker)
	float NotifyHandleBoxWidth = FMath::Max(ScrubHandleSize.X, AlignmentMarkerSize.X * 2);

	// Work out where we will have to draw the tool tip
	FVector2D Size = GetSize();
	float LeftEdgeToNotify = NotifyTimePositionX;
	float RightEdgeToNotify = AllottedGeometry.Size.X - NotifyTimePositionX;
	bDrawTooltipToRight = (RightEdgeToNotify > LabelWidth) || (RightEdgeToNotify > LeftEdgeToNotify);

	// Calculate widget width/position based on where we are drawing the tool tip
	WidgetX = bDrawTooltipToRight ? (NotifyTimePositionX - (NotifyHandleBoxWidth / 2.f)) : (NotifyTimePositionX - LabelWidth);
	WidgetSize = bDrawTooltipToRight ? FVector2D(FMath::Max(LabelWidth, NotifyDurationSizeX), NotifyHeight) : FVector2D((LabelWidth + NotifyDurationSizeX), NotifyHeight);
	WidgetSize.X += NotifyHandleBoxWidth;
	
	if(EndMarkerNodeOverlay.IsValid())
	{
		FVector2D OverlaySize = EndMarkerNodeOverlay->GetDesiredSize();
		WidgetSize.X += OverlaySize.X;
	}

	// Widget position of the notify marker
	NotifyScrubHandleCentre = bDrawTooltipToRight ? NotifyHandleBoxWidth / 2.f : LabelWidth;
}

/** @return the Node's position within the track */
FVector2D SAnimNotifyNode::GetWidgetPosition() const
{
	return FVector2D(WidgetX, NotifyHeightOffset);
}

FVector2D SAnimNotifyNode::GetNotifyPosition() const
{
	return FVector2D(NotifyTimePositionX, NotifyHeightOffset);
}

FVector2D SAnimNotifyNode::GetNotifyPositionOffset() const
{
	return GetNotifyPosition() - GetWidgetPosition();
}

FVector2D SAnimNotifyNode::GetSize() const
{
	return WidgetSize;
}

int32 SAnimNotifyNode::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MarkerLayer = LayerId + 1;
	int32 ScrubHandleID = MarkerLayer + 1;
	int32 TextLayerID = ScrubHandleID + 1;
	int32 BranchPointLayerID = TextLayerID + 1;

	FAnimNotifyEvent* AnimNotifyEvent = NodeObjectInterface->GetNotifyEvent();

	// Paint marker node if we have one
	if(EndMarkerNodeOverlay.IsValid())
	{
		FVector2D MarkerSize = EndMarkerNodeOverlay->GetDesiredSize();
		FVector2D MarkerOffset(NotifyDurationSizeX + MarkerSize.X * 0.5f + 5.0f, (NotifyHeight - MarkerSize.Y) * 0.5f);
		EndMarkerNodeOverlay->Paint(Args.WithNewParent(this), AllottedGeometry.MakeChild(MarkerOffset, MarkerSize, 1.0f), MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	const FSlateBrush* StyleInfo = FEditorStyle::GetBrush( TEXT("SpecialEditableTextImageNormal") );
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		AllottedGeometry.ToPaintGeometry(FVector2D(0,0), AllottedGeometry.Size), 
		StyleInfo,
		ESlateDrawEffect::None,
		FLinearColor(1.0f, 1.0f, 1.0f,0.1f));

	FText Text = GetNotifyText();
	FLinearColor NodeColour = bSelected ? FLinearColor(1.0f, 0.5f, 0.0f) : FLinearColor::Red;
	
	float HalfScrubHandleWidth = ScrubHandleSize.X / 2.0f;

	// Show duration of AnimNotifyState
	if( NotifyDurationSizeX > 0.f )
	{
		FLinearColor BoxColor = (NodeObjectInterface->GetTrackIndex() % 2) == 0 ? FLinearColor(0.0f, 1.0f, 0.5f, 0.5f) : FLinearColor(0.0f, 0.5f, 1.0f, 0.5f);
		FVector2D DurationBoxSize = FVector2D(NotifyDurationSizeX, NotifyHeight);
		FVector2D DurationBoxPosition = FVector2D(NotifyScrubHandleCentre, 0.f);
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			LayerId, 
			AllottedGeometry.ToPaintGeometry(DurationBoxPosition, DurationBoxSize), 
			StyleInfo,
			ESlateDrawEffect::None,
			BoxColor);

		DrawScrubHandle(DurationBoxPosition.X + DurationBoxSize.X, OutDrawElements, ScrubHandleID, AllottedGeometry, MyCullingRect, NodeColour);
		
		// Render offsets if necessary
		if(AnimNotifyEvent && AnimNotifyEvent->EndTriggerTimeOffset != 0.f) //Do we have an offset to render?
		{
			float EndTime = AnimNotifyEvent->GetTime() + AnimNotifyEvent->GetDuration();
			if(EndTime != Sequence->SequenceLength) //Don't render offset when we are at the end of the sequence, doesnt help the user
			{
				// ScrubHandle
				float HandleCentre = NotifyDurationSizeX + ScrubHandleSize.X;
				DrawHandleOffset(AnimNotifyEvent->EndTriggerTimeOffset, HandleCentre, OutDrawElements, MarkerLayer, AllottedGeometry, MyCullingRect);
			}
		}
	}

	// Branching point
	bool bDrawBranchingPoint = AnimNotifyEvent && AnimNotifyEvent->IsBranchingPoint();

	// Background
	FVector2D LabelSize = TextSize + TextBorderSize * 2.f;
	LabelSize.X += HalfScrubHandleWidth + (bDrawBranchingPoint ? (BranchingPointIconSize.X + TextBorderSize.X * 2.f) : 0.f);

	float LabelX = bDrawTooltipToRight ? NotifyScrubHandleCentre : NotifyScrubHandleCentre - LabelSize.X;
	float BoxHeight = (NotifyDurationSizeX > 0.f) ? (NotifyHeight - LabelSize.Y) : ((NotifyHeight - LabelSize.Y) / 2.f);

	FVector2D LabelPosition(LabelX, BoxHeight);

	FLinearColor NodeColor = SAnimNotifyNode::GetNotifyColor();

	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		AllottedGeometry.ToPaintGeometry(LabelPosition, LabelSize), 
		StyleInfo,
		ESlateDrawEffect::None,
		NodeColor);

	// Frame
	// Drawing lines is slow, reserved for single selected node
	if( bSelected )
	{
		TArray<FVector2D> LinePoints;

		LinePoints.Empty();
		LinePoints.Add(LabelPosition);
		LinePoints.Add(LabelPosition + FVector2D(LabelSize.X, 0.f));
		LinePoints.Add(LabelPosition + FVector2D(LabelSize.X, LabelSize.Y));
		LinePoints.Add(LabelPosition + FVector2D(0.f, LabelSize.Y));
		LinePoints.Add(LabelPosition);

		FSlateDrawElement::MakeLines( 
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			FLinearColor::Black, 
			false
			);
	}

	// Text
	FVector2D TextPosition = LabelPosition + TextBorderSize;
	if(bDrawTooltipToRight)
	{
		TextPosition.X += HalfScrubHandleWidth;
	}
	TextPosition -= FVector2D(1.f, 1.f);

	FSlateDrawElement::MakeText( 
		OutDrawElements,
		TextLayerID,
		AllottedGeometry.ToPaintGeometry(TextPosition, TextSize),
		Text,
		Font,
		ESlateDrawEffect::None,
		FLinearColor::Black
		);

	DrawScrubHandle(NotifyScrubHandleCentre , OutDrawElements, ScrubHandleID, AllottedGeometry, MyCullingRect, NodeColour);

	if(AnimNotifyEvent && AnimNotifyEvent->TriggerTimeOffset != 0.f) //Do we have an offset to render?
	{
		float NotifyTime = AnimNotifyEvent->GetTime();
		if(NotifyTime != 0.f && NotifyTime != Sequence->SequenceLength) //Don't render offset when we are at the start/end of the sequence, doesn't help the user
		{
			float HandleCentre = NotifyScrubHandleCentre;
			float &Offset = AnimNotifyEvent->TriggerTimeOffset;
			
			DrawHandleOffset(AnimNotifyEvent->TriggerTimeOffset, NotifyScrubHandleCentre, OutDrawElements, MarkerLayer, AllottedGeometry, MyCullingRect);
		}
	}

	// Draw Branching Point
	if (bDrawBranchingPoint)
	{
		FVector2D BranchPointIconPos = LabelPosition + LabelSize - BranchingPointIconSize - FVector2D(bDrawTooltipToRight ? TextBorderSize.X * 2.f : TextBorderSize.X * 4.f, 0.f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			BranchPointLayerID,
			AllottedGeometry.ToPaintGeometry(BranchPointIconPos, BranchingPointIconSize),
			FEditorStyle::GetBrush(TEXT("AnimNotifyEditor.BranchingPoint")),
			ESlateDrawEffect::None,
			FLinearColor::White
			);
	}	

	return TextLayerID;
}

FReply SAnimNotifyNode::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Don't do scrub handle dragging if we haven't captured the mouse.
	if(!this->HasMouseCapture()) return FReply::Unhandled();

	if(CurrentDragHandle == ENotifyStateHandleHit::None)
	{
		// We've had focus taken away - realease the mouse
		FSlateApplication::Get().ReleaseMouseCapture();
		return FReply::Unhandled();
	}
	
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, CachedAllotedGeometrySize);
	
	float XPositionInTrack = MyGeometry.AbsolutePosition.X - CachedTrackGeometry.AbsolutePosition.X + ScrubHandleSize.X;
	float TrackScreenSpaceXPosition = MyGeometry.AbsolutePosition.X - XPositionInTrack;

	if(CurrentDragHandle == ENotifyStateHandleHit::Start)
	{
		// Check track bounds
		float OldDisplayTime = NodeObjectInterface->GetTime();

		if(MouseEvent.GetScreenSpacePosition().X >= TrackScreenSpaceXPosition && MouseEvent.GetScreenSpacePosition().X <= TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X)
		{
			float NewDisplayTime = ScaleInfo.LocalXToInput((MouseEvent.GetScreenSpacePosition() - MyGeometry.AbsolutePosition + XPositionInTrack).X);
			float NewDuration = NodeObjectInterface->GetDuration() + OldDisplayTime - NewDisplayTime;

			// Check to make sure the duration is not less than the minimum allowed
			if(NewDuration < MinimumStateDuration)
			{
				NewDisplayTime -= MinimumStateDuration - NewDuration;
			}

			NodeObjectInterface->SetTime(NewDisplayTime);
			NodeObjectInterface->SetDuration(NodeObjectInterface->GetDuration() + OldDisplayTime - NodeObjectInterface->GetTime());
		}
		else if(NodeObjectInterface->GetDuration() > MinimumStateDuration)
		{
			float Overflow = HandleOverflowPan(MouseEvent.GetScreenSpacePosition(), TrackScreenSpaceXPosition);

			// Update scale info to the new view inputs after panning
			ScaleInfo.ViewMinInput = ViewInputMin.Get();
			ScaleInfo.ViewMaxInput = ViewInputMax.Get();

			NodeObjectInterface->SetTime(ScaleInfo.LocalXToInput(Overflow < 0.0f ? 0.0f : CachedAllotedGeometrySize.X));
			NodeObjectInterface->SetDuration(NodeObjectInterface->GetDuration() + OldDisplayTime - NodeObjectInterface->GetTime());

			// Adjust incase we went under the minimum
			if(NodeObjectInterface->GetDuration() < MinimumStateDuration)
			{
				float EndTimeBefore = NodeObjectInterface->GetTime() + NodeObjectInterface->GetDuration();
				NodeObjectInterface->SetTime(NodeObjectInterface->GetTime() + NodeObjectInterface->GetDuration() - MinimumStateDuration);
				NodeObjectInterface->SetDuration(MinimumStateDuration);
				float EndTimeAfter = NodeObjectInterface->GetTime() + NodeObjectInterface->GetDuration();
			}
		}

		// Now we know where the marker should be, look for possible snaps on montage marker bars
		if (FAnimNotifyEvent* AnimNotifyEvent = NodeObjectInterface->GetNotifyEvent())
		{
			float NodePositionX = ScaleInfo.InputToLocalX(AnimNotifyEvent->GetTime());
			float MarkerSnap = GetScrubHandleSnapPosition(NodePositionX, ENotifyStateHandleHit::Start);
			if (MarkerSnap != -1.0f)
			{
				// We're near to a snap bar
				EAnimEventTriggerOffsets::Type Offset = (MarkerSnap < NodePositionX) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
				AnimNotifyEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);
				NodePositionX = MarkerSnap;

				// Adjust our start marker
				OldDisplayTime = AnimNotifyEvent->GetTime();
				AnimNotifyEvent->SetTime(ScaleInfo.LocalXToInput(NodePositionX));
				AnimNotifyEvent->SetDuration(AnimNotifyEvent->GetDuration() + OldDisplayTime - AnimNotifyEvent->GetTime());
			}
			else
			{
				AnimNotifyEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
			}
		}
	}
	else
	{
		if(MouseEvent.GetScreenSpacePosition().X >= TrackScreenSpaceXPosition && MouseEvent.GetScreenSpacePosition().X <= TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X)
		{
			float NewDuration = ScaleInfo.LocalXToInput((MouseEvent.GetScreenSpacePosition() - MyGeometry.AbsolutePosition + XPositionInTrack).X) - NodeObjectInterface->GetTime();

			NodeObjectInterface->SetDuration(FMath::Max(NewDuration, MinimumStateDuration));
		}
		else if(NodeObjectInterface->GetDuration() > MinimumStateDuration)
		{
			float Overflow = HandleOverflowPan(MouseEvent.GetScreenSpacePosition(), TrackScreenSpaceXPosition);

			// Update scale info to the new view inputs after panning
			ScaleInfo.ViewMinInput = ViewInputMin.Get();
			ScaleInfo.ViewMaxInput = ViewInputMax.Get();

			NodeObjectInterface->SetDuration(FMath::Max(ScaleInfo.LocalXToInput(Overflow > 0.0f ? CachedAllotedGeometrySize.X : 0.0f) - NodeObjectInterface->GetTime(), MinimumStateDuration));
		}

		// Now we know where the scrub handle should be, look for possible snaps on montage marker bars
		if (FAnimNotifyEvent* AnimNotifyEvent = NodeObjectInterface->GetNotifyEvent())
		{
			float NodePositionX = ScaleInfo.InputToLocalX(AnimNotifyEvent->GetTime() + AnimNotifyEvent->GetDuration());
			float MarkerSnap = GetScrubHandleSnapPosition(NodePositionX, ENotifyStateHandleHit::End);
			if (MarkerSnap != -1.0f)
			{
				// We're near to a snap bar
				EAnimEventTriggerOffsets::Type Offset = (MarkerSnap < NodePositionX) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
				AnimNotifyEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);
				NodePositionX = MarkerSnap;

				// Adjust our end marker
				AnimNotifyEvent->SetDuration(ScaleInfo.LocalXToInput(NodePositionX) - AnimNotifyEvent->GetTime());
			}
			else
			{
				AnimNotifyEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
			}
		}
	}

	return FReply::Handled();
}

FReply SAnimNotifyNode::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bLeftButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;

	if(bLeftButton && CurrentDragHandle != ENotifyStateHandleHit::None)
	{
		// Clear the drag marker and give the mouse back
		CurrentDragHandle = ENotifyStateHandleHit::None;
		OnDeselectAllNotifies.ExecuteIfBound();

		// End drag transaction before handing mouse back
		check(DragMarkerTransactionIdx != INDEX_NONE);
		GEditor->EndTransaction();
		DragMarkerTransactionIdx = INDEX_NONE;

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

float SAnimNotifyNode::GetScrubHandleSnapPosition( float NotifyLocalX, ENotifyStateHandleHit::Type HandleToCheck )
{
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, CachedAllotedGeometrySize);

	const float MaxSnapDist = 5.0f;

	float CurrentMinSnapDistance = MaxSnapDist;
	float SnapPosition = -1.0f;
	float SnapTime = -1.0f;

	if(MarkerBars.IsBound())
	{
		const TArray<FTrackMarkerBar>& Bars = MarkerBars.Get();

		if(Bars.Num() > 0)
		{
			for(auto Iter(Bars.CreateConstIterator()) ; Iter ; ++Iter)
			{
				const FTrackMarkerBar& Bar = *Iter;
				float LocalSnapPosition = ScaleInfo.InputToLocalX(Bar.Time);
				float ThisNodeMinSnapDistance = FMath::Abs(LocalSnapPosition - NotifyLocalX);
				if(ThisNodeMinSnapDistance < CurrentMinSnapDistance)
				{
					CurrentMinSnapDistance = ThisNodeMinSnapDistance;
					SnapPosition = LocalSnapPosition;
				}
			}
		}
	}

	return SnapPosition;
}

FReply SAnimNotifyNode::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::SetDirectly, true);
}

float SAnimNotifyNode::HandleOverflowPan( const FVector2D &ScreenCursorPos, float TrackScreenSpaceXPosition )
{
	float Overflow = 0.0f;
	if(ScreenCursorPos.X < TrackScreenSpaceXPosition)
	{
		// Overflow left edge
		Overflow = ScreenCursorPos.X - TrackScreenSpaceXPosition;
	}
	else
	{
		// Overflow right edge
		Overflow = ScreenCursorPos.X - (TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X);
	}
	PanTrackRequest.ExecuteIfBound(Overflow, CachedAllotedGeometrySize);

	return Overflow;
}

void SAnimNotifyNode::DrawScrubHandle( float ScrubHandleCentre, FSlateWindowElementList& OutDrawElements, int32 ScrubHandleID, const FGeometry &AllottedGeometry, const FSlateRect& MyCullingRect, FLinearColor NodeColour ) const
{
	FVector2D ScrubHandlePosition(ScrubHandleCentre - ScrubHandleSize.X / 2.0f, (NotifyHeight - ScrubHandleSize.Y) / 2.f);
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		ScrubHandleID, 
		AllottedGeometry.ToPaintGeometry(ScrubHandlePosition, ScrubHandleSize), 
		FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.ScrubHandleWhole" ) ),
		ESlateDrawEffect::None,
		NodeColour
		);
}

void SAnimNotifyNode::DrawHandleOffset( const float& Offset, const float& HandleCentre, FSlateWindowElementList& OutDrawElements, int32 MarkerLayer, const FGeometry &AllottedGeometry, const FSlateRect& MyCullingRect ) const
{
	FVector2D MarkerPosition;
	FVector2D MarkerSize = AlignmentMarkerSize;

	if(Offset < 0.f)
	{
		MarkerPosition.Set( (HandleCentre) - AlignmentMarkerSize.X, (NotifyHeight - AlignmentMarkerSize.Y) / 2.f);
	}
	else
	{
		MarkerPosition.Set( HandleCentre + AlignmentMarkerSize.X, (NotifyHeight - AlignmentMarkerSize.Y) / 2.f);
		MarkerSize.X = -AlignmentMarkerSize.X;
	}

	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		MarkerLayer, 
		AllottedGeometry.ToPaintGeometry(MarkerPosition, MarkerSize), 
		FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.NotifyAlignmentMarker" ) ),
		ESlateDrawEffect::None,
		FLinearColor(0.f, 0.f, 1.f)
		);
}

void SAnimNotifyNode::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	ScreenPosition = AllottedGeometry.AbsolutePosition;
}

void SAnimNotifyNode::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if(CurrentDragHandle != ENotifyStateHandleHit::None)
	{
		// Lost focus while dragging a state node, clear the drag and end the current transaction
		CurrentDragHandle = ENotifyStateHandleHit::None;
		OnDeselectAllNotifies.ExecuteIfBound();
		
		check(DragMarkerTransactionIdx != INDEX_NONE);
		GEditor->EndTransaction();
		DragMarkerTransactionIdx = INDEX_NONE;
	}
}

bool SAnimNotifyNode::SupportsKeyboardFocus() const
{
	// Need to support focus on the node so we can end drag transactions if the user alt-tabs
	// from the editor while in the proceess of dragging a state notify duration marker.
	return true;
}

FCursorReply SAnimNotifyNode::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	// Show resize cursor if the cursor is hoverring over either of the scrub handles of a notify state node
	if(IsHovered() && GetDurationSize() > 0.0f)
	{
		FVector2D RelMouseLocation = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());

		const float HandleHalfWidth = ScrubHandleSize.X / 2.0f;
		const float DistFromFirstHandle = FMath::Abs(RelMouseLocation.X - NotifyScrubHandleCentre);
		const float DistFromSecondHandle = FMath::Abs(RelMouseLocation.X - (NotifyScrubHandleCentre + NotifyDurationSizeX));

		if(DistFromFirstHandle < HandleHalfWidth || DistFromSecondHandle < HandleHalfWidth || CurrentDragHandle != ENotifyStateHandleHit::None)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
		}
	}
	return FCursorReply::Unhandled();
}

//////////////////////////////////////////////////////////////////////////
// SAnimNotifyTrack

void SAnimNotifyTrack::Construct(const FArguments& InArgs)
{
	FAnimSequenceEditorCommands::Register();
	CreateCommands();
	SetClipping(EWidgetClipping::ClipToBounds);

	Sequence = InArgs._Sequence;
	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	AnimNotifies = InArgs._AnimNotifies;
	AnimSyncMarkers = InArgs._AnimSyncMarkers;
	OnUpdatePanel = InArgs._OnUpdatePanel;
	OnGetNotifyBlueprintData = InArgs._OnGetNotifyBlueprintData;
	OnGetNotifyStateBlueprintData = InArgs._OnGetNotifyStateBlueprintData;
	OnGetNotifyNativeClasses = InArgs._OnGetNotifyNativeClasses;
	OnGetNotifyStateNativeClasses = InArgs._OnGetNotifyStateNativeClasses;
	TrackIndex = InArgs._TrackIndex;
	OnGetScrubValue = InArgs._OnGetScrubValue;
	OnGetDraggedNodePos = InArgs._OnGetDraggedNodePos;
	OnNodeDragStarted = InArgs._OnNodeDragStarted;
	TrackColor = InArgs._TrackColor;
	MarkerBars = InArgs._MarkerBars;
	OnRequestTrackPan = InArgs._OnRequestTrackPan;
	OnRequestRefreshOffsets = InArgs._OnRequestOffsetRefresh;
	OnDeleteNotify = InArgs._OnDeleteNotify;
	OnGetIsAnimNotifySelectionValidforReplacement = InArgs._OnGetIsAnimNotifySelectionValidForReplacement;
	OnReplaceSelectedWithNotify = InArgs._OnReplaceSelectedWithNotify;
	OnReplaceSelectedWithBlueprintNotify = InArgs._OnReplaceSelectedWithBlueprintNotify;
	OnDeselectAllNotifies = InArgs._OnDeselectAllNotifies;
	OnCopyNodes = InArgs._OnCopyNodes;
	OnPasteNodes = InArgs._OnPasteNodes;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;
	OnGetTimingNodeVisibility = InArgs._OnGetTimingNodeVisibility;
	OnInvokeTab = InArgs._OnInvokeTab;

	this->ChildSlot
	[
			SAssignNew( TrackArea, SBorder )
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
			.Padding( FMargin(0.f, 0.f) )
	];

	Update();
}

FVector2D SAnimNotifyTrack::ComputeDesiredSize( float ) const
{
	FVector2D Size;
	Size.X = 200;
	Size.Y = NotificationTrackHeight;
	return Size;
}

int32 SAnimNotifyTrack::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* StyleInfo = FEditorStyle::GetBrush( TEXT( "Persona.NotifyEditor.NotifyTrackBackground" ) );
	FLinearColor Color = TrackColor.Get();

	FPaintGeometry MyGeometry = AllottedGeometry.ToPaintGeometry();
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		MyGeometry, 
		StyleInfo,
		ESlateDrawEffect::None, 
		Color
		);

	int32 CustomLayerId = LayerId + 1; 

	// draw line for every 1/4 length
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, AllottedGeometry.Size);
	const int32 NumOfFrames = Sequence->GetNumberOfFrames();
	if (NumOfFrames > 0)
	{
		int32 Divider = SScrubWidget::GetDivider( ViewInputMin.Get(), ViewInputMax.Get(), AllottedGeometry.Size, Sequence->SequenceLength, Sequence->GetNumberOfFrames());
		const FAnimKeyHelper Helper(Sequence->SequenceLength, NumOfFrames);

		float TimePerKey = Helper.TimePerKey();
		for (int32 I=1; I<Helper.GetNumKeys(); ++I)
		{
			if ( I % Divider == 0 )
			{
				float XPos = ScaleInfo.InputToLocalX(TimePerKey*I);

				TArray<FVector2D> LinePoints;
				LinePoints.Add(FVector2D(XPos, 0.f));
				LinePoints.Add(FVector2D(XPos, AllottedGeometry.Size.Y));

				FSlateDrawElement::MakeLines( 
					OutDrawElements,
					CustomLayerId,
					MyGeometry,
					LinePoints,
					ESlateDrawEffect::None,
					FLinearColor::Black
					);
			}
		}
	}

	++CustomLayerId;
	for ( int32 I=0; I<NotifyNodes.Num(); ++I )
	{
		if ( NotifyNodes[I].Get()->bBeingDragged == false )
		{
			NotifyNodes[I].Get()->UpdateSizeAndPosition(AllottedGeometry);
		}
	}

	++CustomLayerId;

	float Value = 0.f;

	if ( OnGetScrubValue.IsBound() )
	{
		Value = OnGetScrubValue.Execute();
	}

	{
		float XPos = ScaleInfo.InputToLocalX(Value);

		TArray<FVector2D> LinePoints;
		LinePoints.Add(FVector2D(XPos, 0.f));
		LinePoints.Add(FVector2D(XPos, AllottedGeometry.Size.Y));


		FSlateDrawElement::MakeLines( 
			OutDrawElements,
			CustomLayerId,
			MyGeometry,
			LinePoints,
			ESlateDrawEffect::None,
			FLinearColor::Red
			);
	}

	++CustomLayerId;

	if ( OnGetDraggedNodePos.IsBound() )
	{
		Value = OnGetDraggedNodePos.Execute();

		if(Value >= 0.0f)
		{
			float XPos = Value;
			TArray<FVector2D> LinePoints;
			LinePoints.Add(FVector2D(XPos, 0.f));
			LinePoints.Add(FVector2D(XPos, AllottedGeometry.Size.Y));


			FSlateDrawElement::MakeLines( 
				OutDrawElements,
				CustomLayerId,
				MyGeometry,
				LinePoints,
				ESlateDrawEffect::None,
				FLinearColor(1.0f, 0.5f, 0.0f)
				);
		}
	}

	++CustomLayerId;

	// Draggable Bars
	for ( int32 I=0; MarkerBars.IsBound() && I < MarkerBars.Get().Num(); I++ )
	{
		// Draw lines
		FTrackMarkerBar Bar = MarkerBars.Get()[I];

		float XPos = ScaleInfo.InputToLocalX(Bar.Time);

		TArray<FVector2D> LinePoints;
		LinePoints.Add(FVector2D(XPos, 0.f));
		LinePoints.Add(FVector2D(XPos, AllottedGeometry.Size.Y));

		FSlateDrawElement::MakeLines( 
			OutDrawElements,
			CustomLayerId,
			MyGeometry,
			LinePoints,
			ESlateDrawEffect::None,
			Bar.DrawColour
			);
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, CustomLayerId, InWidgetStyle, bParentEnabled);
}

FReply SAnimNotifyTrack::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float ZoomDelta = -0.1f * MouseEvent.GetWheelDelta();

	const FVector2D WidgetSpace = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const float ZoomRatio = FMath::Clamp( (WidgetSpace.X / MyGeometry.Size.X), 0.f, 1.f);

	{
		const float InputViewSize = ViewInputMax.Get() - ViewInputMin.Get();
		const float InputChange = InputViewSize * ZoomDelta;

		float ViewMinInput = ViewInputMin.Get() - (InputChange * ZoomRatio);
		float ViewMaxInput = ViewInputMax.Get() + (InputChange * (1.f - ZoomRatio));

		OnSetInputViewRange.ExecuteIfBound(ViewMinInput, ViewMaxInput);
	}

	return FReply::Handled();
}

FCursorReply SAnimNotifyTrack::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (ViewInputMin.Get() > 0.f || ViewInputMax.Get() < Sequence->SequenceLength)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}

	return FCursorReply::Unhandled();
}

// Fill "new notify state" menu, or "replace with notify state menu"
void SAnimNotifyTrack::FillNewNotifyStateMenu(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu /* = false */)
{
	// Run the native query first to update the allowed classes for blueprints.
	TArray<UClass*> NotifyStateClasses;
	OnGetNotifyStateNativeClasses.ExecuteIfBound(NotifyStateClasses);

	// Collect blueprint notify data
	TArray<FAssetData> NotifyAssetData;
	TArray<BlueprintNotifyMenuInfo> NotifyMenuData;
	OnGetNotifyStateBlueprintData.ExecuteIfBound(NotifyAssetData);
	GetNotifyMenuData(NotifyAssetData, NotifyMenuData);

	for(BlueprintNotifyMenuInfo& NotifyData : NotifyMenuData)
	{
		const FText LabelText = FText::FromString(NotifyData.NotifyName);

		FUIAction UIAction;
		FText Description = FText::GetEmpty();
		if (!bIsReplaceWithMenu)
		{
			Description = LOCTEXT("AddsAnExistingAnimNotify", "Add an existing notify");
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::CreateNewBlueprintNotifyAtCursor,
				NotifyData.NotifyName,
				NotifyData.BlueprintPath);
			UIAction.CanExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::IsValidToPlace,
				NotifyData.BaseClass);
		}
		else
		{
			Description = LOCTEXT("ReplaceWithAnExistingAnimNotify", "Replace with an existing notify");
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::ReplaceSelectedWithBlueprintNotify,
				NotifyData.NotifyName,
				NotifyData.BlueprintPath);
			UIAction.CanExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::IsValidToPlace,
				NotifyData.BaseClass);
		}

		MenuBuilder.AddMenuEntry(LabelText, Description, FSlateIcon(), UIAction);
	}

	MenuBuilder.BeginSection("NativeNotifyStates", LOCTEXT("NewStateNotifyMenu_Native", "Native Notify States"));
	{
		for(UClass* Class : NotifyStateClasses)
		{
			if (Class->HasAllClassFlags(CLASS_Abstract))
			{
				continue; // skip abstract classes
			}

			const FText Description = LOCTEXT("NewNotifyStateSubMenu_NativeToolTip", "Add an existing native notify state");
			const FText LabelText = Class->GetDisplayNameText();
			const FString Label = LabelText.ToString();

			FUIAction UIAction;
			if (!bIsReplaceWithMenu)
			{
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::CreateNewNotifyAtCursor,
					Label,
					Class);
				UIAction.CanExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::IsValidToPlace,
					Class);
			}
			else
			{
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::ReplaceSelectedWithNotify,
					Label,
					Class);
				UIAction.CanExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::IsValidToPlace,
					Class);
			}

			MenuBuilder.AddMenuEntry(LabelText, Description, FSlateIcon(), UIAction);
		}
	}
	MenuBuilder.EndSection();
}

// Fill "new notify" menu, or "replace with notify menu"
void SAnimNotifyTrack::FillNewNotifyMenu(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu /* = false */)
{
	TArray<UClass*> NativeNotifyClasses;
	OnGetNotifyNativeClasses.ExecuteIfBound(NativeNotifyClasses);

	TArray<FAssetData> NotifyAssetData;
	TArray<BlueprintNotifyMenuInfo> NotifyMenuData;
	OnGetNotifyBlueprintData.ExecuteIfBound(NotifyAssetData);
	GetNotifyMenuData(NotifyAssetData, NotifyMenuData);

	for(BlueprintNotifyMenuInfo& NotifyData : NotifyMenuData)
	{
		const FText LabelText = FText::FromString( NotifyData.NotifyName );

		FUIAction UIAction;
		FText Description = FText::GetEmpty();
		if (!bIsReplaceWithMenu)
		{
			Description = LOCTEXT("NewNotifySubMenu_ToolTip", "Add an existing notify");
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::CreateNewBlueprintNotifyAtCursor,
				NotifyData.NotifyName,
				NotifyData.BlueprintPath);
			UIAction.CanExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::IsValidToPlace,
				NotifyData.BaseClass);
		}
		else
		{
			Description = LOCTEXT("ReplaceWithNotifySubMenu_ToolTip", "Replace with an existing notify");
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::ReplaceSelectedWithBlueprintNotify,
				NotifyData.NotifyName,
				NotifyData.BlueprintPath);
			UIAction.CanExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::IsValidToPlace,
				NotifyData.BaseClass);
		}
		
		MenuBuilder.AddMenuEntry(LabelText, Description, FSlateIcon(), UIAction);
	}

	MenuBuilder.BeginSection("NativeNotifies", LOCTEXT("NewNotifyMenu_Native", "Native Notifies"));
	{
		for(UClass* Class : NativeNotifyClasses)
		{
			if (Class->HasAllClassFlags(CLASS_Abstract))
			{
				continue; // skip abstract classes
			}

			const FText LabelText = Class->GetDisplayNameText();
			const FString Label = LabelText.ToString();

			FUIAction UIAction;
			FText Description = FText::GetEmpty();
			if (!bIsReplaceWithMenu)
			{
				Description = LOCTEXT("NewNotifySubMenu_NativeToolTip", "Add an existing native notify");
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::CreateNewNotifyAtCursor,
					Label,
					Class);
				UIAction.CanExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::IsValidToPlace,
					Class);
			}
			else
			{
				Description = LOCTEXT("ReplaceWithNotifySubMenu_NativeToolTip", "Replace with an existing native notify");
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::ReplaceSelectedWithNotify,
					Label,
					Class);
				UIAction.CanExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::IsValidToPlace,
					Class);
			}

			MenuBuilder.AddMenuEntry(LabelText, Description, FSlateIcon(), UIAction);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimNotifyCustom", LOCTEXT("NewNotifySubMenu_Custom", "Custom"));
	{
		// now add custom anim notifiers
		USkeleton* SeqSkeleton = Sequence->GetSkeleton();
		if (SeqSkeleton)
		{
			for (int32 I = 0; I<SeqSkeleton->AnimationNotifies.Num(); ++I)
			{
				FName NotifyName = SeqSkeleton->AnimationNotifies[I];
				FString Label = NotifyName.ToString();

				FText Description = FText::GetEmpty();
				if (!bIsReplaceWithMenu)
				{
					Description = LOCTEXT("NewNotifySubMenu_ToolTip", "Add an existing notify");
				}
				else
				{
					Description = LOCTEXT("ReplaceWithNotifySubMenu_ToolTip", "Replace with an existing notify");
				}

				FUIAction UIAction;
				if (!bIsReplaceWithMenu)
				{
					UIAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::CreateNewNotifyAtCursor,
						Label,
						(UClass*)nullptr);
				}
				else
				{
					UIAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::ReplaceSelectedWithNotify,
						Label,
						(UClass*)nullptr);
				}

				MenuBuilder.AddMenuEntry( FText::FromString( Label ), Description, FSlateIcon(), UIAction);
			}
		}
	}
	MenuBuilder.EndSection();

	if (!bIsReplaceWithMenu)
	{
		MenuBuilder.BeginSection("AnimNotifyCreateNew");
		{
			FUIAction UIAction;
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::OnNewNotifyClicked);
			MenuBuilder.AddMenuEntry(LOCTEXT("NewNotify", "New Notify"), LOCTEXT("NewNotifyToolTip", "Create a new animation notify"), FSlateIcon(), UIAction);
		}
		MenuBuilder.EndSection();
	}
}

FAnimNotifyEvent& SAnimNotifyTrack::CreateNewBlueprintNotify(FString NewNotifyName, FString BlueprintPath, float StartTime)
{
	TSubclassOf<UObject> BlueprintClass = GetBlueprintClassFromPath(BlueprintPath);
	check(BlueprintClass);
	return CreateNewNotify(NewNotifyName, BlueprintClass, StartTime);
}

FAnimNotifyEvent& SAnimNotifyTrack::CreateNewNotify(FString NewNotifyName, UClass* NotifyClass, float StartTime)
{
	// Insert a new notify record and spawn the new notify object
	int32 NewNotifyIndex = Sequence->Notifies.Add(FAnimNotifyEvent());
	FAnimNotifyEvent& NewEvent = Sequence->Notifies[NewNotifyIndex];
	NewEvent.NotifyName = FName(*NewNotifyName);

	NewEvent.Link(Sequence, StartTime);
	NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(Sequence->CalculateOffsetForNotify(StartTime));
	NewEvent.TrackIndex = TrackIndex;

	if( NotifyClass )
	{
		class UObject* AnimNotifyClass = NewObject<UObject>(Sequence, NotifyClass, NAME_None, RF_Transactional);
		NewEvent.NotifyStateClass = Cast<UAnimNotifyState>(AnimNotifyClass);
		NewEvent.Notify = Cast<UAnimNotify>(AnimNotifyClass);

		// Set default duration to 1 frame for AnimNotifyState.
		if( NewEvent.NotifyStateClass )
		{
			NewEvent.NotifyName = FName(*NewEvent.NotifyStateClass->GetNotifyName());
			NewEvent.SetDuration(1 / 30.f);
			NewEvent.EndLink.Link(Sequence, NewEvent.EndLink.GetTime());
		}
		else
		{
			NewEvent.NotifyName = FName(*NewEvent.Notify->GetNotifyName());
		}
	}
	else
	{
		NewEvent.Notify = NULL;
		NewEvent.NotifyStateClass = NULL;
	}

	if(NewEvent.Notify)
	{
		TArray<FAssetData> SelectedAssets;
		AssetSelectionUtils::GetSelectedAssets(SelectedAssets);

		for( TFieldIterator<UObjectProperty> PropIt(NewEvent.Notify->GetClass()); PropIt; ++PropIt )
		{
			if(PropIt->GetBoolMetaData(TEXT("ExposeOnSpawn")))
			{
				UObjectProperty* Property = *PropIt;
				const FAssetData* Asset = SelectedAssets.FindByPredicate([Property](const FAssetData& Other)
				{
					return Other.GetAsset()->IsA(Property->PropertyClass);
				});

				if( Asset )
				{
					uint8* Offset = (*PropIt)->ContainerPtrToValuePtr<uint8>(NewEvent.Notify);
					(*PropIt)->ImportText( *Asset->GetAsset()->GetPathName(), Offset, 0, NewEvent.Notify );
					break;
				}
			}
		}

		NewEvent.Notify->OnAnimNotifyCreatedInEditor(NewEvent);
	}
	else if (NewEvent.NotifyStateClass)
	{
		NewEvent.NotifyStateClass->OnAnimNotifyCreatedInEditor(NewEvent);
	}

	Sequence->MarkPackageDirty();

	return NewEvent;
}

void SAnimNotifyTrack::CreateNewBlueprintNotifyAtCursor(FString NewNotifyName, FString BlueprintPath)
{
	TSubclassOf<UObject> BlueprintClass = GetBlueprintClassFromPath(BlueprintPath);
	check(BlueprintClass);
	CreateNewNotifyAtCursor(NewNotifyName, BlueprintClass);
}

void SAnimNotifyTrack::CreateNewNotifyAtCursor(FString NewNotifyName, UClass* NotifyClass)
{
	const FScopedTransaction Transaction(LOCTEXT("AddNotifyEvent", "Add Anim Notify"));
	Sequence->Modify();
	CreateNewNotify(NewNotifyName, NotifyClass, LastClickedTime);
	OnUpdatePanel.ExecuteIfBound();
}

void SAnimNotifyTrack::CreateNewSyncMarkerAtCursor(FString NewSyncMarkerName, UClass* NotifyClass)
{
	UAnimSequence* Seq = CastChecked<UAnimSequence>(Sequence);

	FScopedTransaction Transaction(LOCTEXT("AddSyncMarker", "Add Sync Marker"));
	Seq->Modify();
	int32 NewIndex = Seq->AuthoredSyncMarkers.Add(FAnimSyncMarker());
	FAnimSyncMarker& SyncMarker = Seq->AuthoredSyncMarkers[NewIndex];
	SyncMarker.MarkerName = FName(*NewSyncMarkerName);
	SyncMarker.TrackIndex = TrackIndex;
	SyncMarker.Time = LastClickedTime;
	OnUpdatePanel.ExecuteIfBound();

	UBlendSpaceBase::UpdateBlendSpacesUsingAnimSequence(Seq);
}

void SAnimNotifyTrack::ReplaceSelectedWithBlueprintNotify(FString NewNotifyName, FString BlueprintPath)
{
	OnReplaceSelectedWithBlueprintNotify.ExecuteIfBound(NewNotifyName, BlueprintPath);
}

void SAnimNotifyTrack::ReplaceSelectedWithNotify(FString NewNotifyName, UClass* NotifyClass)
{
	OnReplaceSelectedWithNotify.ExecuteIfBound(NewNotifyName, NotifyClass);
}

bool SAnimNotifyTrack::IsValidToPlace(UClass* NotifyClass) const
{
	if (NotifyClass && NotifyClass->IsChildOf(UAnimNotify::StaticClass()))
	{
		UAnimNotify* DefaultNotify = NotifyClass->GetDefaultObject<UAnimNotify>();
		return DefaultNotify->CanBePlaced(Sequence);
	}

	if (NotifyClass && NotifyClass->IsChildOf(UAnimNotifyState::StaticClass()))
	{
		UAnimNotifyState* DefaultNotifyState = NotifyClass->GetDefaultObject<UAnimNotifyState>();
		return DefaultNotifyState->CanBePlaced(Sequence);
	}

	return true;
}

TSubclassOf<UObject> SAnimNotifyTrack::GetBlueprintClassFromPath(FString BlueprintPath)
{
	TSubclassOf<UObject> BlueprintClass = NULL;
	if (!BlueprintPath.IsEmpty())
	{
		UBlueprint* BlueprintLibPtr = LoadObject<UBlueprint>(NULL, *BlueprintPath, NULL, 0, NULL);
		BlueprintClass = BlueprintLibPtr->GeneratedClass;
	}
	return BlueprintClass;
}

FReply SAnimNotifyTrack::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bLeftMouseButton =  MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	bool bRightMouseButton =  MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
	bool bShift = MouseEvent.IsShiftDown();
	bool bCtrl = MouseEvent.IsControlDown();

	if ( bRightMouseButton )
	{
		TSharedPtr<SWidget> WidgetToFocus;
		WidgetToFocus = SummonContextMenu(MyGeometry, MouseEvent);

		return (WidgetToFocus.IsValid())
			? FReply::Handled().ReleaseMouseCapture().SetUserFocus(WidgetToFocus.ToSharedRef(), EFocusCause::SetDirectly)
			: FReply::Handled().ReleaseMouseCapture();
	}
	else if ( bLeftMouseButton )
	{
		FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
		CursorPos = MyGeometry.AbsoluteToLocal(CursorPos);
		int32 NotifyIndex = GetHitNotifyNode(MyGeometry, CursorPos);

		if(NotifyIndex == INDEX_NONE)
		{
			// Clicked in empty space, clear selection
			OnDeselectAllNotifies.ExecuteIfBound();
		}
		else
		{
			if(bCtrl)
			{
				ToggleTrackObjectNodeSelectionStatus(NotifyIndex);
			}
			else
			{
				SelectTrackObjectNode(NotifyIndex, bShift);
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SAnimNotifyTrack::SelectTrackObjectNode(int32 TrackNodeIndex, bool Append, bool bUpdateSelection)
{
	if( TrackNodeIndex != INDEX_NONE )
	{
		// Deselect all other notifies if necessary.
		if (Sequence && !Append)
		{
			OnDeselectAllNotifies.ExecuteIfBound();
		}

		// Check to see if we've already selected this node
		if (!SelectedNodeIndices.Contains(TrackNodeIndex))
		{
			// select new one
			if (NotifyNodes.IsValidIndex(TrackNodeIndex))
			{
				TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[TrackNodeIndex];
				Node->bSelected = true;
				SelectedNodeIndices.Add(TrackNodeIndex);

				if(bUpdateSelection)
				{
					SendSelectionChanged();
				}
			}
		}
	}
}

void SAnimNotifyTrack::ToggleTrackObjectNodeSelectionStatus( int32 TrackNodeIndex, bool bUpdateSelection )
{
	check(NotifyNodes.IsValidIndex(TrackNodeIndex));

	bool bSelected = SelectedNodeIndices.Contains(TrackNodeIndex);
	if(bSelected)
	{
		SelectedNodeIndices.Remove(TrackNodeIndex);
	}
	else
	{
		SelectedNodeIndices.Add(TrackNodeIndex);
	}

	TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[TrackNodeIndex];
	Node->bSelected = !Node->bSelected;

	if(bUpdateSelection)
	{
		SendSelectionChanged();
	}
}

void SAnimNotifyTrack::DeselectTrackObjectNode( int32 TrackNodeIndex, bool bUpdateSelection )
{
	check(NotifyNodes.IsValidIndex(TrackNodeIndex));
	TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[TrackNodeIndex];
	Node->bSelected = false;

	int32 ItemsRemoved = SelectedNodeIndices.Remove(TrackNodeIndex);
	check(ItemsRemoved > 0);

	if(bUpdateSelection)
	{
		SendSelectionChanged();
	}
}

void SAnimNotifyTrack::DeselectAllNotifyNodes(bool bUpdateSelectionSet)
{
	for(TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
	{
		Node->bSelected = false;
	}
	SelectedNodeIndices.Empty();

	if(bUpdateSelectionSet)
	{
		SendSelectionChanged();
	}
}

TSharedPtr<SWidget> SAnimNotifyTrack::SummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
	int32 NodeIndex = GetHitNotifyNode(MyGeometry, MyGeometry.AbsoluteToLocal(CursorPos));
	LastClickedTime = CalculateTime(MyGeometry, MouseEvent.GetScreenSpacePosition());

	const bool bCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bCloseWindowAfterMenuSelection, AnimSequenceEditorActions );
	FUIAction NewAction;

	INodeObjectInterface* NodeObject = NodeIndex != INDEX_NONE ? NotifyNodes[NodeIndex]->NodeObjectInterface : nullptr;
	FAnimNotifyEvent* NotifyEvent = NodeObject ? NodeObject->GetNotifyEvent(): nullptr;
	int32 NotifyIndex = NotifyEvent ? AnimNotifies.IndexOfByKey(NotifyEvent) : INDEX_NONE;

	MenuBuilder.BeginSection("AnimNotify", LOCTEXT("NotifyHeading", "Notify") );
	{
		if (NodeObject)
		{
			if (!NotifyNodes[NodeIndex]->bSelected)
			{
				SelectTrackObjectNode(NodeIndex, MouseEvent.IsControlDown());
			}

			FNumberFormattingOptions Options;
			Options.MinimumFractionalDigits = 5;

			// Add item to directly set notify time
			const FText CurrentTime = FText::AsNumber(NodeObject->GetTime(), &Options);
			const FText TimeMenuText = FText::Format(LOCTEXT("TimeMenuText", "Set Notify Begin Time: {0}..."), CurrentTime);

			NewAction.ExecuteAction.BindRaw(this, &SAnimNotifyTrack::OnSetNodeTimeClicked, NodeIndex);
			NewAction.CanExecuteAction.BindRaw(this, &SAnimNotifyTrack::IsSingleNodeSelected);

			MenuBuilder.AddMenuEntry(TimeMenuText, LOCTEXT("SetTimeToolTip", "Set the time of this notify directly"), FSlateIcon(), NewAction);

			// Add item to directly set notify frame
			const FText Frame = FText::AsNumber(Sequence->GetFrameAtTime(NodeObject->GetTime()));
			const FText FrameMenuText = FText::Format(LOCTEXT("FrameMenuText", "Set Notify Frame: {0}..."), Frame);

			NewAction.ExecuteAction.BindRaw(this, &SAnimNotifyTrack::OnSetNodeFrameClicked, NodeIndex);
			NewAction.CanExecuteAction.BindRaw(this, &SAnimNotifyTrack::IsSingleNodeSelected);

			MenuBuilder.AddMenuEntry(FrameMenuText, LOCTEXT("SetFrameToolTip", "Set the frame of this notify directly"), FSlateIcon(), NewAction);

			if (NotifyEvent)
			{
				// add menu to get threshold weight for triggering this notify
				NewAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::OnSetTriggerWeightNotifyClicked, NotifyIndex);
				NewAction.CanExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::IsSingleNodeSelected);

				const FText Threshold = FText::AsNumber(NotifyEvent->TriggerWeightThreshold, &Options);
				const FText MinTriggerWeightText = FText::Format(LOCTEXT("MinTriggerWeight", "Min Trigger Weight: {0}..."), Threshold);
				MenuBuilder.AddMenuEntry(MinTriggerWeightText, LOCTEXT("MinTriggerWeightToolTip", "The minimum weight to trigger this notify"), FSlateIcon(), NewAction);

				// Add menu for changing duration if this is an AnimNotifyState
				if (NotifyEvent->NotifyStateClass)
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::OnSetDurationNotifyClicked, NotifyIndex);
					NewAction.CanExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::IsSingleNodeSelected);

					FText SetAnimStateDurationText = FText::Format(LOCTEXT("SetAnimStateDuration", "Set AnimNotifyState duration ({0})"), FText::AsNumber(NotifyEvent->GetDuration()));
					MenuBuilder.AddMenuEntry(SetAnimStateDurationText, LOCTEXT("SetAnimStateDuration_ToolTip", "The duration of this AnimNotifyState"), FSlateIcon(), NewAction);
				}
			}

		}
		else
		{
			MenuBuilder.AddSubMenu(
				NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuAddNotify", "Add Notify..."),
				NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuAddNotifyToolTip", "Add AnimNotifyEvent"),
				FNewMenuDelegate::CreateRaw( this, &SAnimNotifyTrack::FillNewNotifyMenu, false ) );

			MenuBuilder.AddSubMenu(
				NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuAddNotifyState", "Add Notify State..."),
				NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuAddNotifyStateToolTip","Add AnimNotifyState"),
				FNewMenuDelegate::CreateRaw( this, &SAnimNotifyTrack::FillNewNotifyStateMenu, false ) );

			if (Sequence->IsA(UAnimSequence::StaticClass()))
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("NewSyncMarker", "Add Sync Marker"),
					LOCTEXT("NewSyncMarkerToolTip", "Create a new animation sync marker"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SAnimNotifyTrack::OnNewSyncMarkerClicked)));
			}

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("NewNotifySubMenu", "ManageNotifies", "Manage Notifies..."),
				NSLOCTEXT("NewNotifySubMenu", "ManageNotifiesToolTip", "Opens the Manage Notifies window"),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP( this, &SAnimNotifyTrack::OnManageNotifies ) ) );
		}
	}
	MenuBuilder.EndSection(); //AnimNotify

	NewAction.CanExecuteAction = 0;

	MenuBuilder.BeginSection("AnimEdit", LOCTEXT("NotifyEditHeading", "Edit") );
	{
		if ( NodeObject )
		{
			// copy notify menu item
			NewAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::OnCopyNotifyClicked, NodeIndex);
			MenuBuilder.AddMenuEntry(LOCTEXT("Copy", "Copy"), LOCTEXT("CopyToolTip", "Copy animation notify event"), FSlateIcon(), NewAction);

			// allow it to delete
			NewAction.ExecuteAction = OnDeleteNotify;
			MenuBuilder.AddMenuEntry(LOCTEXT("Delete", "Delete"), LOCTEXT("DeleteToolTip", "Delete animation notify"), FSlateIcon(), NewAction);

			if (NotifyEvent)
			{
				// For the "Replace With..." menu, make sure the current AnimNotify selection is valid for replacement
				if (OnGetIsAnimNotifySelectionValidforReplacement.IsBound() && OnGetIsAnimNotifySelectionValidforReplacement.Execute())
				{
					// If this is an AnimNotifyState (has duration) allow it to be replaced with other AnimNotifyStates
					if (NotifyEvent->NotifyStateClass)
					{
						MenuBuilder.AddSubMenu(
							NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuReplaceWithNotifyState", "Replace with Notify State..."),
							NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuReplaceWithNotifyStateToolTip", "Replace with AnimNotifyState"),
							FNewMenuDelegate::CreateRaw(this, &SAnimNotifyTrack::FillNewNotifyStateMenu, true));
					}
					// If this is a regular AnimNotify (no duration) allow it to be replaced with other AnimNotifies
					else
					{
						MenuBuilder.AddSubMenu(
							NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuReplaceWithNotify", "Replace with Notify..."),
							NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuReplaceWithNotifyToolTip", "Replace with AnimNotifyEvent"),
							FNewMenuDelegate::CreateRaw(this, &SAnimNotifyTrack::FillNewNotifyMenu, true));
					}
				}
			}
		}
		else
		{
			FString PropertyString;
			const TCHAR* Buffer;
			float OriginalTime;
			float OriginalLength;
			int32 TrackSpan;

			//Check whether can we show menu item to paste anim notify event
			if( ReadNotifyPasteHeader(PropertyString, Buffer, OriginalTime, OriginalLength,TrackSpan) )
			{
				// paste notify menu item
				if (IsSingleNodeInClipboard())
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::OnPasteNotifyClicked, ENotifyPasteMode::MousePosition, ENotifyPasteMultipleMode::Absolute);

					MenuBuilder.AddMenuEntry(LOCTEXT("Paste", "Paste"), LOCTEXT("PasteToolTip", "Paste animation notify event here"), FSlateIcon(), NewAction);
				}
				else
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::OnPasteNotifyClicked, ENotifyPasteMode::MousePosition, ENotifyPasteMultipleMode::Relative);

					MenuBuilder.AddMenuEntry(LOCTEXT("PasteMultRel", "Paste Multiple Relative"), LOCTEXT("PasteMultRelToolTip", "Paste multiple notifies beginning at the mouse cursor, maintaining the same relative spacing as the source."), FSlateIcon(), NewAction);

					NewAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::OnPasteNotifyClicked, ENotifyPasteMode::MousePosition, ENotifyPasteMultipleMode::Absolute);

					MenuBuilder.AddMenuEntry(LOCTEXT("PasteMultAbs", "Paste Multiple Absolute"), LOCTEXT("PasteMultAbsToolTip", "Paste multiple notifies beginning at the mouse cursor, maintaining absolute spacing."), FSlateIcon(), NewAction);
				}

				if(OriginalTime < Sequence->SequenceLength)
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::OnPasteNotifyClicked, ENotifyPasteMode::OriginalTime, ENotifyPasteMultipleMode::Absolute);

					FText DisplayText = FText::Format( LOCTEXT("PasteAtOriginalTime", "Paste at original time ({0})"), FText::AsNumber( OriginalTime) );
					MenuBuilder.AddMenuEntry(DisplayText, LOCTEXT("PasteAtOriginalTimeToolTip", "Paste animation notify event at the time it was set to when it was copied"), FSlateIcon(), NewAction);
				}
				
			}
		}
	}
	MenuBuilder.EndSection(); //AnimEdit

	if (NotifyEvent)
	{
		UObject* NotifyObject = NotifyEvent->Notify;
		NotifyObject = NotifyObject ? NotifyObject : NotifyEvent->NotifyStateClass;

		if (NotifyObject && Cast<UBlueprintGeneratedClass>(NotifyObject->GetClass()))
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(NotifyObject->GetClass()->ClassGeneratedBy))
			{
				MenuBuilder.BeginSection("ViewSource", LOCTEXT("NotifyViewHeading", "View"));

				NewAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::OnOpenNotifySource, Blueprint);
				MenuBuilder.AddMenuEntry(LOCTEXT("OpenNotifyBlueprint", "Open Notify Blueprint"), LOCTEXT("OpenNotifyBlueprintTooltip", "Opens the source blueprint for this notify"), FSlateIcon(), NewAction);

				MenuBuilder.EndSection(); //ViewSource
			}
		}
	}

	FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

	// Display the newly built menu
	FSlateApplication::Get().PushMenu(SharedThis(this), WidgetPath, MenuBuilder.MakeWidget(), CursorPos, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

	return TSharedPtr<SWidget>();
}

void SAnimNotifyTrack::CreateCommands()
{
	check(!AnimSequenceEditorActions.IsValid());
	AnimSequenceEditorActions = MakeShareable(new FUICommandList);

	const FAnimSequenceEditorCommands& Commands = FAnimSequenceEditorCommands::Get();

/*
	FUICommandList& ActionList = *AnimSequenceEditorActions;

	ActionList.MapAction( 
		Commands.DeleteNotify, 
		FExecuteAction::CreateRaw( this, &SAnimNotifyTrack::OnDeleteNotifyClicked )
		);*/

}

void SAnimNotifyTrack::OnCopyNotifyClicked(int32 NotifyIndex)
{
	OnCopyNodes.ExecuteIfBound();
}

bool SAnimNotifyTrack::CanPasteAnimNotify() const
{
	FString PropertyString;
	const TCHAR* Buffer;
	float OriginalTime;
	float OriginalLength;
	int32 TrackSpan;
	return ReadNotifyPasteHeader(PropertyString, Buffer, OriginalTime, OriginalLength, TrackSpan);
}

void SAnimNotifyTrack::OnPasteNotifyClicked(ENotifyPasteMode::Type PasteMode, ENotifyPasteMultipleMode::Type MultiplePasteType)
{
	float ClickTime = PasteMode == ENotifyPasteMode::MousePosition ? LastClickedTime : -1.0f;
	OnPasteNodes.ExecuteIfBound(this, ClickTime, PasteMode, MultiplePasteType);
}

void SAnimNotifyTrack::OnManageNotifies()
{
	OnInvokeTab.ExecuteIfBound(FPersonaTabs::SkeletonAnimNotifiesID);
}

void SAnimNotifyTrack::OnOpenNotifySource(UBlueprint* InSourceBlueprint) const
{
	FAssetEditorManager::Get().OpenEditorForAsset(InSourceBlueprint);
}

void SAnimNotifyTrack::SetTriggerWeight(const FText& TriggerWeight, ETextCommit::Type CommitInfo, int32 NotifyIndex)
{
	if ( CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus )
	{
		if ( AnimNotifies.IsValidIndex(NotifyIndex) )
		{
			float NewWeight = FMath::Max(FCString::Atof( *TriggerWeight.ToString() ), ZERO_ANIMWEIGHT_THRESH);
			AnimNotifies[NotifyIndex]->TriggerWeightThreshold = NewWeight;
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

bool SAnimNotifyTrack::IsSingleNodeSelected()
{
	return SelectedNodeIndices.Num() == 1;
}

bool SAnimNotifyTrack::IsSingleNodeInClipboard()
{
	FString PropString;
	const TCHAR* Buffer;
	float OriginalTime;
	float OriginalLength;
	int32 TrackSpan;
	uint32 Count = 0;
	if (ReadNotifyPasteHeader(PropString, Buffer, OriginalTime, OriginalLength, TrackSpan))
	{
		// If reading a single line empties the buffer then we only have one notify in there.
		FString TempLine;
		FParse::Line(&Buffer, TempLine);
		return *Buffer == 0;
	}
	return false;
}

void SAnimNotifyTrack::OnSetTriggerWeightNotifyClicked(int32 NotifyIndex)
{
	if (AnimNotifies.IsValidIndex(NotifyIndex))
	{
		FString DefaultText = FString::Printf(TEXT("%0.6f"), AnimNotifies[NotifyIndex]->TriggerWeightThreshold);

		// Show dialog to enter weight
		TSharedRef<STextEntryPopup> TextEntry =
			SNew(STextEntryPopup)
			.Label( LOCTEXT("TriggerWeightNotifyClickedLabel", "Trigger Weight") )
			.DefaultText( FText::FromString(DefaultText) )
			.OnTextCommitted( this, &SAnimNotifyTrack::SetTriggerWeight, NotifyIndex );

		FSlateApplication::Get().PushMenu(
			AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
			FWidgetPath(),
			TextEntry,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
			);
	}
}

void SAnimNotifyTrack::OnSetDurationNotifyClicked(int32 NotifyIndex)
{
	if (AnimNotifies.IsValidIndex(NotifyIndex))
	{
		FString DefaultText = FString::Printf(TEXT("%f"), AnimNotifies[NotifyIndex]->GetDuration());

		// Show dialog to enter weight
		TSharedRef<STextEntryPopup> TextEntry =
			SNew(STextEntryPopup)
			.Label(LOCTEXT("DurationNotifyClickedLabel", "Duration"))
			.DefaultText( FText::FromString(DefaultText) )
			.OnTextCommitted( this, &SAnimNotifyTrack::SetDuration, NotifyIndex );

		FSlateApplication::Get().PushMenu(
			AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
			FWidgetPath(),
			TextEntry,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
			);
	}
}

void SAnimNotifyTrack::SetDuration(const FText& DurationText, ETextCommit::Type CommitInfo, int32 NotifyIndex)
{
	if ( CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus )
	{
		if ( AnimNotifies.IsValidIndex(NotifyIndex) )
		{
			float NewDuration = FMath::Max(FCString::Atof( *DurationText.ToString() ), SAnimNotifyNode::MinimumStateDuration);
			float MaxDuration = Sequence->SequenceLength - AnimNotifies[NotifyIndex]->GetTime();
			NewDuration = FMath::Min(NewDuration, MaxDuration);
			AnimNotifies[NotifyIndex]->SetDuration(NewDuration);

			// If we have a delegate bound to refresh the offsets, call it.
			// This is used by the montage editor to keep the offsets up to date.
			OnRequestRefreshOffsets.ExecuteIfBound();
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SAnimNotifyTrack::OnNewNotifyClicked()
{
	// Show dialog to enter new track name
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label( LOCTEXT("NewNotifyLabel", "Notify Name") )
		.OnTextCommitted( this, &SAnimNotifyTrack::AddNewNotify );

	// Show dialog to enter new event name
	FSlateApplication::Get().PushMenu(
		AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
		FWidgetPath(),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
		);
}

void SAnimNotifyTrack::OnNewSyncMarkerClicked()
{
	// Show dialog to enter new track name
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewSyncMarkerLabel", "Sync Marker Name"))
		.OnTextCommitted(this, &SAnimNotifyTrack::AddNewSyncMarker);

	// Show dialog to enter new event name
	FSlateApplication::Get().PushMenu(
		AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
		FWidgetPath(),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
		);
}

void SAnimNotifyTrack::AddNewNotify(const FText& NewNotifyName, ETextCommit::Type CommitInfo)
{
	USkeleton* SeqSkeleton = Sequence->GetSkeleton();
	if ((CommitInfo == ETextCommit::OnEnter) && SeqSkeleton)
	{
		const FScopedTransaction Transaction( LOCTEXT("AddNewNotifyEvent", "Add New Anim Notify") );
		FName NewName = FName( *NewNotifyName.ToString() );

		ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
		TSharedRef<IEditableSkeleton> EditableSkeleton = SkeletonEditorModule.CreateEditableSkeleton(SeqSkeleton);

		EditableSkeleton->AddNotify(NewName);

		FBlueprintActionDatabase::Get().RefreshAssetActions(SeqSkeleton);

		CreateNewNotifyAtCursor(NewNotifyName.ToString(), (UClass*)nullptr);
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SAnimNotifyTrack::AddNewSyncMarker(const FText& NewNotifyName, ETextCommit::Type CommitInfo)
{
	USkeleton* SeqSkeleton = Sequence->GetSkeleton();
	if ((CommitInfo == ETextCommit::OnEnter) && SeqSkeleton)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddNewSyncMarker", "Add New Sync Marker"));
		//FName NewName = FName(*NewNotifyName.ToString());
		//SeqSkeleton->AddNewAnimationNotify(NewName);

		FBlueprintActionDatabase::Get().RefreshAssetActions(SeqSkeleton);

		CreateNewSyncMarkerAtCursor(NewNotifyName.ToString(), (UClass*)nullptr);
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SAnimNotifyTrack::Update()
{
	NotifyPairs.Empty();
	NotifyNodes.Empty();

	TrackArea->SetContent(
		SAssignNew( NodeSlots, SOverlay )
		);

	if ( AnimNotifies.Num() > 0 )
	{
		TArray<TSharedPtr<FTimingRelevantElementBase>> TimingElements;
		SAnimTimingPanel::GetTimingRelevantElements(Sequence, TimingElements);
		for (int32 NotifyIndex = 0; NotifyIndex < AnimNotifies.Num(); ++NotifyIndex)
		{
			TSharedPtr<FTimingRelevantElementBase> Element;
			FAnimNotifyEvent* Event = AnimNotifies[NotifyIndex];

			for(int32 Idx = 0 ; Idx < TimingElements.Num() ; ++Idx)
			{
				Element = TimingElements[Idx];

				if(Element->GetType() == ETimingElementType::NotifyStateBegin
				   || Element->GetType() == ETimingElementType::BranchPointNotify
				   || Element->GetType() == ETimingElementType::QueuedNotify)
				{
					// Only the notify type will return the type flags above
					FTimingRelevantElement_Notify* NotifyElement = static_cast<FTimingRelevantElement_Notify*>(Element.Get());
					if(Event == &Sequence->Notifies[NotifyElement->NotifyIndex])
					{
						break;
					}
				}
			}

			TSharedPtr<SAnimNotifyNode> AnimNotifyNode = nullptr;
			TSharedPtr<SAnimNotifyPair> NotifyPair = nullptr;
			TSharedPtr<SAnimTimingNode> TimingNode = nullptr;
			TSharedPtr<SAnimTimingNode> EndTimingNode = nullptr;

			// Create visibility attribute to control timing node visibility for notifies
			TAttribute<EVisibility> TimingNodeVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
				[this]()
				{
					if(OnGetTimingNodeVisibility.IsBound())
					{
						return OnGetTimingNodeVisibility.Execute(ETimingElementType::QueuedNotify);
					}
					return EVisibility(EVisibility::Hidden);
				}));

			SAssignNew(TimingNode, SAnimTimingNode)
				.InElement(Element)
				.bUseTooltip(false)
				.Visibility(TimingNodeVisibility);

			if(Event->NotifyStateClass)
			{
				TSharedPtr<FTimingRelevantElementBase>* FoundStateEndElement = TimingElements.FindByPredicate([Event](TSharedPtr<FTimingRelevantElementBase>& ElementToTest)
				{
					if(ElementToTest.IsValid() && ElementToTest->GetType() == ETimingElementType::NotifyStateEnd)
					{
						FTimingRelevantElement_NotifyStateEnd* StateElement = static_cast<FTimingRelevantElement_NotifyStateEnd*>(ElementToTest.Get());
						return &(StateElement->Sequence->Notifies[StateElement->NotifyIndex]) == Event;
					}
					return false;
				});

				if(FoundStateEndElement)
				{
					// Create an end timing node if we have a state
					SAssignNew(EndTimingNode, SAnimTimingNode)
						.InElement(*FoundStateEndElement)
						.bUseTooltip(false)
						.Visibility(TimingNodeVisibility);
				}
			}

			SAssignNew(AnimNotifyNode, SAnimNotifyNode)
				.Sequence(Sequence)
				.AnimNotify(Event)
				.OnNodeDragStarted(this, &SAnimNotifyTrack::OnNotifyNodeDragStarted, NotifyIndex)
				.OnUpdatePanel(OnUpdatePanel)
				.PanTrackRequest(OnRequestTrackPan)
				.ViewInputMin(ViewInputMin)
				.ViewInputMax(ViewInputMax)
				.MarkerBars(MarkerBars)
				.OnDeselectAllNotifies(OnDeselectAllNotifies)
				.StateEndTimingNode(EndTimingNode);

			SAssignNew(NotifyPair, SAnimNotifyPair)
			.LeftContent()
			[
				TimingNode.ToSharedRef()
			]
			.Node(AnimNotifyNode);

			NodeSlots->AddSlot()
			.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SAnimNotifyTrack::GetNotifyTrackPadding, NotifyIndex)))
			.VAlign(VAlign_Center)
			[
				NotifyPair->AsShared()
			];

			NotifyNodes.Add(AnimNotifyNode);
			NotifyPairs.Add(NotifyPair);
		}
	}

	for (FAnimSyncMarker* SyncMarker : AnimSyncMarkers)
	{
		TSharedPtr<SAnimNotifyNode> AnimSyncMarkerNode = nullptr;
		TSharedPtr<SAnimTimingNode> EndTimingNode = nullptr;

		const int32 NodeIndex = NotifyNodes.Num();
		SAssignNew(AnimSyncMarkerNode, SAnimNotifyNode)
			.Sequence(Sequence)
			.AnimSyncMarker(SyncMarker)
			.OnNodeDragStarted(this, &SAnimNotifyTrack::OnNotifyNodeDragStarted, NodeIndex)
			.OnUpdatePanel(OnUpdatePanel)
			.PanTrackRequest(OnRequestTrackPan)
			.ViewInputMin(ViewInputMin)
			.ViewInputMax(ViewInputMax)
			.MarkerBars(MarkerBars)
			.OnDeselectAllNotifies(OnDeselectAllNotifies)
			.StateEndTimingNode(EndTimingNode);

		NodeSlots->AddSlot()
			.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SAnimNotifyTrack::GetSyncMarkerTrackPadding, NodeIndex)))
			.VAlign(VAlign_Center)
			[
				AnimSyncMarkerNode->AsShared()
			];

		NotifyNodes.Add(AnimSyncMarkerNode);
	}
}

int32 SAnimNotifyTrack::GetHitNotifyNode(const FGeometry& MyGeometry, const FVector2D& CursorPosition)
{
	for (int32 I = NotifyNodes.Num() - 1; I >= 0; --I) //Run through from 'top most' Notify to bottom
	{
		if (NotifyNodes[I].Get()->HitTest(MyGeometry, CursorPosition))
		{
			return I;
		}
	}

	return INDEX_NONE;
}

FReply SAnimNotifyTrack::OnNotifyNodeDragStarted(TSharedRef<SAnimNotifyNode> NotifyNode, const FPointerEvent& MouseEvent, const FVector2D& ScreenNodePosition, const bool bDragOnMarker, int32 NotifyIndex)
{
	// Check to see if we've already selected the triggering node
	if (!NotifyNode->bSelected)
	{
		SelectTrackObjectNode(NotifyIndex, MouseEvent.IsShiftDown());
	}

	// Sort our nodes so we're acessing them in time order
	SelectedNodeIndices.Sort([this](const int32& A, const int32& B)
	{
		float TimeA = NotifyNodes[A]->NodeObjectInterface->GetTime();
		float TimeB = NotifyNodes[B]->NodeObjectInterface->GetTime();
		return TimeA < TimeB;
	});

	// If we're dragging one of the direction markers we don't need to call any further as we don't want the drag drop op
	if (!bDragOnMarker)
	{
		TArray<TSharedPtr<SAnimNotifyNode>> NodesToDrag;
		const float FirstNodeX = NotifyNodes[SelectedNodeIndices[0]]->GetWidgetPosition().X;

		TSharedRef<SOverlay> DragBox = SNew(SOverlay);
		for (auto Iter = SelectedNodeIndices.CreateIterator(); Iter; ++Iter)
		{
			TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[*Iter];
			NodesToDrag.Add(Node);
		}

		FVector2D DecoratorPosition = NodesToDrag[0]->GetWidgetPosition();
		DecoratorPosition = CachedGeometry.LocalToAbsolute(DecoratorPosition);
		return OnNodeDragStarted.Execute(NodesToDrag, DragBox, MouseEvent.GetScreenSpacePosition(), DecoratorPosition, bDragOnMarker);
	}
	else
	{
		// Capture the mouse in the node
		return FReply::Handled().CaptureMouse(NotifyNode).UseHighPrecisionMouseMovement(NotifyNode);
	}
}

FReply SAnimNotifyTrack::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
		CursorPos = MyGeometry.AbsoluteToLocal(CursorPos);
		int32 HitIndex = GetHitNotifyNode(MyGeometry, CursorPos);

		if (HitIndex != INDEX_NONE)
		{
			// Hit a node, record the mouse position for use later so we can know when / where a
			// drag happened on the node handles if necessary.
			NotifyNodes[HitIndex]->SetLastMouseDownPosition(CursorPos);

			return FReply::Handled().DetectDrag(NotifyNodes[HitIndex].ToSharedRef(), EKeys::LeftMouseButton);
		}
	}

	return FReply::Unhandled();
}

float SAnimNotifyTrack::CalculateTime(const FGeometry& MyGeometry, FVector2D NodePos, bool bInputIsAbsolute)
{
	if (bInputIsAbsolute)
	{
		NodePos = MyGeometry.AbsoluteToLocal(NodePos);
	}
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, MyGeometry.Size);
	return FMath::Clamp<float>(ScaleInfo.LocalXToInput(NodePos.X), 0.f, Sequence->SequenceLength);
}

FReply SAnimNotifyTrack::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	return FReply::Unhandled();
}

void SAnimNotifyTrack::HandleNodeDrop(TSharedPtr<SAnimNotifyNode> Node, float Offset)
{
	ensure(Node.IsValid());

	OnDeselectAllNotifies.ExecuteIfBound();
	const FScopedTransaction Transaction(LOCTEXT("MoveNotifyEvent", "Move Anim Notify"));
	Sequence->Modify();
	if (Node->NodeObjectInterface->GetType() == ENodeObjectTypes::SYNC_MARKER)
	{
		UBlendSpaceBase::UpdateBlendSpacesUsingAnimSequence(Sequence);
	}

	float LocalX = GetCachedGeometry().AbsoluteToLocal(Node->GetScreenPosition() + Offset).X;
	float SnapTime = Node->GetLastSnappedTime();
	float Time = SnapTime != -1.0f ? SnapTime : GetCachedScaleInfo().LocalXToInput(LocalX);
	Node->NodeObjectInterface->HandleDrop(Sequence, Time, TrackIndex);
}

void SAnimNotifyTrack::DisconnectSelectedNodesForDrag(TArray<TSharedPtr<SAnimNotifyNode>>& DragNodes)
{
	if(SelectedNodeIndices.Num() == 0)
	{
		return;
	}

	const float FirstNodeX = NotifyNodes[SelectedNodeIndices[0]]->GetWidgetPosition().X;

	for(auto Iter = SelectedNodeIndices.CreateIterator(); Iter; ++Iter)
	{
		TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[*Iter];
		if (Node->NodeObjectInterface->GetNotifyEvent())
		{
			TSharedPtr<SAnimNotifyPair> Pair = NotifyPairs[*Iter];
			NodeSlots->RemoveSlot(Pair->AsShared());
		}
		else
		{
			NodeSlots->RemoveSlot(Node->AsShared());
		}

		DragNodes.Add(Node);
	}
}

void SAnimNotifyTrack::AppendSelectionToSet(FGraphPanelSelectionSet& SelectionSet)
{
	// Add our selection to the provided set
	for(int32 Index : SelectedNodeIndices)
	{
		if (FAnimNotifyEvent* Event = NotifyNodes[Index]->NodeObjectInterface->GetNotifyEvent())
		{
			if (Event->Notify)
			{
				SelectionSet.Add(Event->Notify);
			}
			else if (Event->NotifyStateClass)
			{
				SelectionSet.Add(Event->NotifyStateClass);
			}
		}
	}
}

void SAnimNotifyTrack::AppendSelectionToArray(TArray<INodeObjectInterface*>& Selection) const
{
	for(int32 Idx : SelectedNodeIndices)
	{
		Selection.Add(NotifyNodes[Idx]->NodeObjectInterface);
	}
}

void SAnimNotifyTrack::PasteSingleNotify(FString& NotifyString, float PasteTime)
{
	int32 NewIdx = Sequence->Notifies.Add(FAnimNotifyEvent());
	UArrayProperty* ArrayProperty = NULL;
	uint8* PropertyData = Sequence->FindNotifyPropertyData(NewIdx, ArrayProperty);

	if(PropertyData && ArrayProperty)
	{
		ArrayProperty->Inner->ImportText(*NotifyString, PropertyData, PPF_Copy, NULL);

		FAnimNotifyEvent& NewNotify = Sequence->Notifies[NewIdx];

		// We have to link to the montage / sequence again, we need a correct time set and we could be pasting to a new montage / sequence
		int32 NewSlotIndex = 0;
		float NewNotifyTime = PasteTime != 1.0f ? PasteTime : NewNotify.GetTime();
		NewNotifyTime = FMath::Clamp(NewNotifyTime, 0.0f, Sequence->SequenceLength);

		if(UAnimMontage* Montage = Cast<UAnimMontage>(Sequence))
		{
			// We have a montage, validate slots
			int32 OldSlotIndex = NewNotify.GetSlotIndex();
			if(Montage->SlotAnimTracks.IsValidIndex(OldSlotIndex))
			{
				// Link to the same slot index
				NewSlotIndex = OldSlotIndex;
			}
		}
		NewNotify.Link(Sequence, PasteTime, NewSlotIndex);

		NewNotify.TriggerTimeOffset = GetTriggerTimeOffsetForType(Sequence->CalculateOffsetForNotify(NewNotify.GetTime()));
		NewNotify.TrackIndex = TrackIndex;

		bool bValidNotify = true;
		if(NewNotify.Notify)
		{
			UAnimNotify* NewNotifyObject = Cast<UAnimNotify>(StaticDuplicateObject(NewNotify.Notify, Sequence));
			check(NewNotifyObject);
			bValidNotify = NewNotifyObject->CanBePlaced(Sequence);
			NewNotify.Notify = NewNotifyObject;
		}
		else if(NewNotify.NotifyStateClass)
		{
			UAnimNotifyState* NewNotifyStateObject = Cast<UAnimNotifyState>(StaticDuplicateObject(NewNotify.NotifyStateClass, Sequence));
			check(NewNotifyStateObject);
			NewNotify.NotifyStateClass = NewNotifyStateObject;
			bValidNotify = NewNotifyStateObject->CanBePlaced(Sequence);
			// Clamp duration into the sequence
			NewNotify.SetDuration(FMath::Clamp(NewNotify.GetDuration(), 1 / 30.0f, Sequence->SequenceLength - NewNotify.GetTime()));
			NewNotify.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Sequence->CalculateOffsetForNotify(NewNotify.GetTime() + NewNotify.GetDuration()));
			NewNotify.EndLink.Link(Sequence, NewNotify.EndLink.GetTime());
		}

		if (!bValidNotify)
		{
			// Paste failed, remove the notify
			Sequence->Notifies.RemoveAt(NewIdx);

			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToPaste", "The notify is not allowed to be in this asset."));
		}
	}
	else
	{
		// Paste failed, remove the notify
		Sequence->Notifies.RemoveAt(NewIdx);
	}

	OnDeselectAllNotifies.ExecuteIfBound();
	Sequence->MarkPackageDirty();
	OnUpdatePanel.ExecuteIfBound();
}

void SAnimNotifyTrack::PasteSingleSyncMarker(FString& MarkerString, float PasteTime)
{
	if(UAnimSequence* AnimSeq = Cast<UAnimSequence>(Sequence))
	{
		int32 NewIdx = AnimSeq->AuthoredSyncMarkers.Add(FAnimSyncMarker());
		UArrayProperty* ArrayProperty = NULL;
		uint8* PropertyData = AnimSeq->FindSyncMarkerPropertyData(NewIdx, ArrayProperty);

		if (PropertyData && ArrayProperty)
		{
			ArrayProperty->Inner->ImportText(*MarkerString, PropertyData, PPF_Copy, NULL);

			FAnimSyncMarker& SyncMarker = AnimSeq->AuthoredSyncMarkers[NewIdx];

			if (PasteTime != -1.0f)
			{
				SyncMarker.Time = PasteTime;
			}

			// Make sure the notify is within the track area
			SyncMarker.Time = FMath::Clamp(SyncMarker.Time, 0.0f, Sequence->SequenceLength);
			SyncMarker.TrackIndex = TrackIndex;
		}
		else
		{
			// Paste failed, remove the notify
			AnimSeq->AuthoredSyncMarkers.RemoveAt(NewIdx);
		}

		UBlendSpaceBase::UpdateBlendSpacesUsingAnimSequence(Sequence);

		OnDeselectAllNotifies.ExecuteIfBound();
		Sequence->MarkPackageDirty();
		OnUpdatePanel.ExecuteIfBound();
	}
}

void SAnimNotifyTrack::AppendSelectedNodeWidgetsToArray(TArray<TSharedPtr<SAnimNotifyNode>>& NodeArray) const
{
	for(TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
	{
		if(Node->bSelected)
		{
			NodeArray.Add(Node);
		}
	}
}

void SAnimNotifyTrack::RefreshMarqueeSelectedNodes(FSlateRect& Rect, FNotifyMarqueeOperation& Marquee)
{
	if(Marquee.Operation != FNotifyMarqueeOperation::Replace)
	{
		// Maintain the original selection from before the operation
		for(int32 Idx = 0 ; Idx < NotifyNodes.Num() ; ++Idx)
		{
			TSharedPtr<SAnimNotifyNode> Notify = NotifyNodes[Idx];
			bool bWasSelected = Marquee.OriginalSelection.Contains(Notify);
			if(bWasSelected)
			{
				SelectTrackObjectNode(Idx, true, false);
			}
			else if(SelectedNodeIndices.Contains(Idx))
			{
				DeselectTrackObjectNode(Idx, false);
			}
		}
	}

	for(int32 Index = 0 ; Index < NotifyNodes.Num() ; ++Index)
	{
		TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[Index];
		FSlateRect NodeRect = FSlateRect(Node->GetWidgetPosition(), Node->GetWidgetPosition() + Node->GetSize());

		if(FSlateRect::DoRectanglesIntersect(Rect, NodeRect))
		{
			// Either select or deselect the intersecting node, depending on the type of selection operation
			if(Marquee.Operation == FNotifyMarqueeOperation::Remove)
			{
				if(SelectedNodeIndices.Contains(Index))
				{
					DeselectTrackObjectNode(Index, false);
				}
			}
			else
			{
				SelectTrackObjectNode(Index, true, false);
			}
		}
	}
}

FString SAnimNotifyTrack::MakeBlueprintNotifyName(FAssetData& NotifyAssetData)
{
	FString DefaultNotifyName = NotifyAssetData.AssetName.ToString();
	DefaultNotifyName = DefaultNotifyName.Replace(TEXT("AnimNotify_"), TEXT(""), ESearchCase::CaseSensitive);
	DefaultNotifyName = DefaultNotifyName.Replace(TEXT("AnimNotifyState_"), TEXT(""), ESearchCase::CaseSensitive);

	return DefaultNotifyName;
}

void SAnimNotifyTrack::ClearNodeTooltips()
{
	FText EmptyTooltip;

	for (TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
	{
		Node->SetToolTipText(EmptyTooltip);
	}
}

void SAnimNotifyTrack::GetNotifyMenuData(TArray<FAssetData>& NotifyAssetData, TArray<BlueprintNotifyMenuInfo>& OutNotifyMenuData)
{
	for(FAssetData& NotifyData : NotifyAssetData)
	{
		OutNotifyMenuData.AddZeroed();
		BlueprintNotifyMenuInfo& MenuInfo = OutNotifyMenuData.Last();

		MenuInfo.BlueprintPath = NotifyData.ObjectPath.ToString();
		MenuInfo.NotifyName = MakeBlueprintNotifyName(NotifyData);
		// this functionality is only available in native class
		// so we don't have to call BP function but just call native on the check of validity
		FString NativeParentClassName;
		if (NotifyData.GetTagValue("NativeParentClass", NativeParentClassName))
		{
			UObject* Outer = nullptr;
			ResolveName(Outer, NativeParentClassName, false, false);
			MenuInfo.BaseClass = FindObject<UClass>(ANY_PACKAGE, *NativeParentClassName);
		}
	}

	OutNotifyMenuData.Sort([](const BlueprintNotifyMenuInfo& A, const BlueprintNotifyMenuInfo& B)
	{
		return A.NotifyName < B.NotifyName;
	});
}

void SAnimNotifyTrack::OnSetNodeTimeClicked(int32 NodeIndex)
{
	if (NotifyNodes.IsValidIndex(NodeIndex))
	{
		INodeObjectInterface* NodeObject = NotifyNodes[NodeIndex]->NodeObjectInterface;
		FString DefaultText = FString::Printf(TEXT("%0.6f"), NodeObject->GetTime());

		// Show dialog to enter time
		TSharedRef<STextEntryPopup> TextEntry =
			SNew(STextEntryPopup)
			.Label(LOCTEXT("NotifyTimeClickedLabel", "Notify Time"))
			.DefaultText(FText::FromString(DefaultText))
			.OnTextCommitted(this, &SAnimNotifyTrack::SetNodeTime, NodeIndex);

		FSlateApplication::Get().PushMenu(
			AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
			FWidgetPath(),
			TextEntry,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
			);
	}
}

void SAnimNotifyTrack::SetNodeTime(const FText& NodeTimeText, ETextCommit::Type CommitInfo, int32 NodeIndex)
{
	if(CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		if (NotifyNodes.IsValidIndex(NodeIndex))
		{
			INodeObjectInterface* NodeObject = NotifyNodes[NodeIndex]->NodeObjectInterface;

			float NewTime = FMath::Clamp(FCString::Atof(*NodeTimeText.ToString()), 0.0f, Sequence->SequenceLength - NodeObject->GetDuration());

			NodeObject->SetTime(NewTime);

			if (FAnimNotifyEvent* Event = NodeObject->GetNotifyEvent())
			{
				Event->RefreshTriggerOffset(Sequence->CalculateOffsetForNotify(Event->GetTime()));
				if (Event->GetDuration() > 0.0f)
				{
					Event->RefreshEndTriggerOffset(Sequence->CalculateOffsetForNotify(Event->GetTime() + Event->GetDuration()));
				}
			}
			OnUpdatePanel.ExecuteIfBound();
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SAnimNotifyTrack::OnSetNodeFrameClicked(int32 NodeIndex)
{
	if (NotifyNodes.IsValidIndex(NodeIndex))
	{
		INodeObjectInterface* NodeObject = NotifyNodes[NodeIndex]->NodeObjectInterface;

		const FText Frame = FText::AsNumber(Sequence->GetFrameAtTime(NodeObject->GetTime()));

		FString DefaultText = FString::Printf(TEXT("%s"), *Frame.ToString());

		// Show dialog to enter frame
		TSharedRef<STextEntryPopup> TextEntry =
			SNew(STextEntryPopup)
			.Label(LOCTEXT("NotifyFrameClickedLabel", "Notify Frame"))
			.DefaultText(FText::FromString(DefaultText))
			.OnTextCommitted(this, &SAnimNotifyTrack::SetNodeFrame, NodeIndex);

		FSlateApplication::Get().PushMenu(
			AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
			FWidgetPath(),
			TextEntry,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
			);
	}
}

void SAnimNotifyTrack::SetNodeFrame(const FText& NodeFrameText, ETextCommit::Type CommitInfo, int32 NodeIndex)
{
	if(CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		if (NotifyNodes.IsValidIndex(NodeIndex))
		{
			INodeObjectInterface* NodeObject = NotifyNodes[NodeIndex]->NodeObjectInterface;

			int32 Frame = FCString::Atof(*NodeFrameText.ToString());
			float NewTime = FMath::Clamp(Sequence->GetTimeAtFrame(Frame), 0.0f, Sequence->SequenceLength - NodeObject->GetDuration());

			NodeObject->SetTime(NewTime);

			if (FAnimNotifyEvent* Event = NodeObject->GetNotifyEvent())
			{
				Event->RefreshTriggerOffset(Sequence->CalculateOffsetForNotify(Event->GetTime()));
				if (Event->GetDuration() > 0.0f)
				{
					Event->RefreshEndTriggerOffset(Sequence->CalculateOffsetForNotify(Event->GetTime() + Event->GetDuration()));
				}
			}
			OnUpdatePanel.ExecuteIfBound();
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

const EVisibility SAnimNotifyTrack::GetTimingNodeVisibility(TSharedPtr<SAnimNotifyNode> NotifyNode)
{
	if(OnGetTimingNodeVisibility.IsBound())
	{
		if(FAnimNotifyEvent* Event = NotifyNode->NodeObjectInterface->GetNotifyEvent())
		{
			return Event->IsBranchingPoint() ? OnGetTimingNodeVisibility.Execute(ETimingElementType::BranchPointNotify) : OnGetTimingNodeVisibility.Execute(ETimingElementType::QueuedNotify);
		}
	}

	// No visibility defined, not visible
	return EVisibility::Hidden;
}

void SAnimNotifyTrack::UpdateCachedGeometry(const FGeometry& InGeometry)
{
	CachedGeometry = InGeometry;

	for(TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
	{
		Node->CachedTrackGeometry = InGeometry;
	}
}

//////////////////////////////////////////////////////////////////////////
// SSequenceEdTrack

void SNotifyEdTrack::Construct(const FArguments& InArgs)
{
	Sequence = InArgs._Sequence;
	TrackIndex = InArgs._TrackIndex;
	FAnimNotifyTrack& Track = Sequence->AnimNotifyTracks[InArgs._TrackIndex];
	// @Todo anim: we need to fix this to allow track color to be customizable. 
	// for now name, and track color are given
	Track.TrackColor = ((TrackIndex & 1) != 0) ? FLinearColor(0.9f, 0.9f, 0.9f, 0.9f) : FLinearColor(0.5f, 0.5f, 0.5f);

	TSharedRef<SAnimNotifyPanel> PanelRef = InArgs._AnimNotifyPanel.ToSharedRef();
	AnimPanelPtr = InArgs._AnimNotifyPanel;

	//////////////////////////////
	this->ChildSlot
	[
		SNew( SBorder )
		.Padding( FMargin(2.0f, 2.0f) )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				// Notification editor panel
				SAssignNew(NotifyTrack, SAnimNotifyTrack)
				.Sequence(Sequence)
				.TrackIndex(TrackIndex)
				.AnimNotifies(Track.Notifies)
				.AnimSyncMarkers(Track.SyncMarkers)
				.ViewInputMin(InArgs._ViewInputMin)
				.ViewInputMax(InArgs._ViewInputMax)
				.OnSelectionChanged(InArgs._OnSelectionChanged)
				.OnUpdatePanel(InArgs._OnUpdatePanel)
				.OnGetNotifyBlueprintData(InArgs._OnGetNotifyBlueprintData)
				.OnGetNotifyStateBlueprintData(InArgs._OnGetNotifyStateBlueprintData)
				.OnGetNotifyNativeClasses(InArgs._OnGetNotifyNativeClasses)
				.OnGetNotifyStateNativeClasses(InArgs._OnGetNotifyStateNativeClasses)
				.OnGetScrubValue(InArgs._OnGetScrubValue)
				.OnGetDraggedNodePos(InArgs._OnGetDraggedNodePos)
				.OnNodeDragStarted(InArgs._OnNodeDragStarted)
				.MarkerBars(InArgs._MarkerBars)
				.TrackColor(Track.TrackColor)
				.OnRequestTrackPan(FPanTrackRequest::CreateSP(PanelRef, &SAnimNotifyPanel::PanInputViewRange))
				.OnRequestOffsetRefresh(InArgs._OnRequestRefreshOffsets)
				.OnDeleteNotify(InArgs._OnDeleteNotify)
				.OnGetIsAnimNotifySelectionValidForReplacement(PanelRef, &SAnimNotifyPanel::IsNotifySelectionValidForReplacement)
				.OnReplaceSelectedWithNotify(PanelRef, &SAnimNotifyPanel::OnReplaceSelectedWithNotify)
				.OnReplaceSelectedWithBlueprintNotify(PanelRef, &SAnimNotifyPanel::OnReplaceSelectedWithNotifyBlueprint)
				.OnDeselectAllNotifies(InArgs._OnDeselectAllNotifies)
				.OnCopyNodes(InArgs._OnCopyNodes)
				.OnPasteNodes(InArgs._OnPasteNodes)
				.OnSetInputViewRange(InArgs._OnSetInputViewRange)
				.OnGetTimingNodeVisibility(InArgs._OnGetTimingNodeVisibility)
				.OnInvokeTab(InArgs._OnInvokeTab)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SBox )
				.WidthOverride(InArgs._WidgetWidth)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.FillWidth(1)
					[
						// Name of track
						SAssignNew(TrackText, SInlineEditableTextBlock)
						.Text(this, &SNotifyEdTrack::GetTrackName)
						.ColorAndOpacity(Track.TrackColor)
						.OnTextCommitted(PanelRef, &SAnimNotifyPanel::OnCommitTrackName, TrackIndex)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						// Name of track
						SNew(SButton)
						.Text(LOCTEXT("AddTrackButtonLabel", "+"))
						.ToolTipText( LOCTEXT("AddTrackTooltip", "Add track above here") )
						.OnClicked( PanelRef, &SAnimNotifyPanel::InsertTrack, TrackIndex + 1 )
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						// Name of track
						SNew(SButton)
						.Text(LOCTEXT("RemoveTrackButtonLabel", "-"))
						.ToolTipText( LOCTEXT("RemoveTrackTooltip", "Remove this track") )
						.OnClicked( PanelRef, &SAnimNotifyPanel::DeleteTrack, TrackIndex )
						.IsEnabled( SNotifyEdTrack::CanDeleteTrack() )
					]
				]
			]
		]
	];
}

bool SNotifyEdTrack::CanDeleteTrack()
{
	return AnimPanelPtr.Pin()->CanDeleteTrack(TrackIndex);
}

//////////////////////////////////////////////////////////////////////////
// FAnimNotifyPanelCommands

void FAnimNotifyPanelCommands::RegisterCommands()
{
	UI_COMMAND(DeleteNotify, "Delete", "Deletes the selected notifies.", EUserInterfaceActionType::Button, FInputChord(EKeys::Platform_Delete));
}

//////////////////////////////////////////////////////////////////////////
// SAnimNotifyPanel

void SAnimNotifyPanel::Construct(const FArguments& InArgs, FSimpleMulticastDelegate& OnPostUndo)
{
	SAnimTrackPanel::Construct( SAnimTrackPanel::FArguments()
		.WidgetWidth(InArgs._WidgetWidth)
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.InputMin(InArgs._InputMin)
		.InputMax(InArgs._InputMax)
		.OnSetInputViewRange(InArgs._OnSetInputViewRange));

	Sequence = InArgs._Sequence;
	MarkerBars = InArgs._MarkerBars;
	OnAnimNotifiesChanged = InArgs._OnAnimNotifiesChanged;
	OnInvokeTab = InArgs._OnInvokeTab;

	FAnimNotifyPanelCommands::Register();
	BindCommands();

	// @todo anim : this is kinda hack to make sure it has 1 track is alive
	// we can do this whenever import or asset is created, but it's more places to handle than here
	// the function name in that case will need to change
	Sequence->InitializeNotifyTrack();
	Sequence->RegisterOnNotifyChanged(UAnimSequenceBase::FOnNotifyChanged::CreateSP( this, &SAnimNotifyPanel::RefreshNotifyTracks )  );

	OnPostUndo.Add(FSimpleDelegate::CreateSP( this, &SAnimNotifyPanel::PostUndo ) );

	CurrentPosition = InArgs._CurrentPosition;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	WidgetWidth = InArgs._WidgetWidth;
	OnGetScrubValue = InArgs._OnGetScrubValue;
	OnRequestRefreshOffsets = InArgs._OnRequestRefreshOffsets;
	OnGetTimingNodeVisibility = InArgs._OnGetTimingNodeVisibility;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SExpandableArea )
			.AreaTitle( LOCTEXT("Notifies", "Notifies") )
			.AddMetaData<FTagMetaData>(TEXT("AnimNotify.Notify"))
			.BodyContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1)
				[
					SAssignNew(PanelArea, SBorder)
					.BorderImage(FEditorStyle::GetBrush("NoBorder"))
					.Padding(FMargin(2.0f, 2.0f))
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SAssignNew(NotifyTrackScrollBar, SScrollBar)
						.Orientation(EOrientation::Orient_Horizontal)
						.AlwaysShowScrollbar(true)
						.OnUserScrolled(this, &SAnimNotifyPanel::OnNotifyTrackScrolled)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(WidgetWidth)
					]
				]
			]
		]
	];

	InputViewRangeChanged(ViewInputMin.Get(), ViewInputMax.Get());

	OnPropertyChangedHandle = FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate::CreateSP(this, &SAnimNotifyPanel::OnPropertyChanged);
	OnPropertyChangedHandleDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.Add(OnPropertyChangedHandle);

	// Base notify classes used to search asset data for children.
	NotifyClassNames.Add(TEXT("Class'/Script/Engine.AnimNotify'"));
	NotifyStateClassNames.Add(TEXT("Class'/Script/Engine.AnimNotifyState'"));

	PopulateNotifyBlueprintClasses(NotifyClassNames);
	PopulateNotifyBlueprintClasses(NotifyStateClassNames);

	Update();
}

SAnimNotifyPanel::~SAnimNotifyPanel()
{
	Sequence->UnregisterOnNotifyChanged(this);

	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandleDelegateHandle);
}

FName SAnimNotifyPanel::GetNewTrackName() const
{
	TArray<FName> TrackNames;
	TrackNames.Reserve(50);

	for (const FAnimNotifyTrack& Track : Sequence->AnimNotifyTracks)
	{
		TrackNames.Add(Track.TrackName);
	}

	FName NameToTest;
	int32 TrackIndex = 1;
	
	do 
	{
		NameToTest = *FString::FromInt(TrackIndex++);
	} while (TrackNames.Contains(NameToTest));

	return NameToTest;
}

FReply SAnimNotifyPanel::InsertTrack(int32 TrackIndexToInsert)
{
	// before insert, make sure everything behind is fixed
	for (int32 I=TrackIndexToInsert; I<Sequence->AnimNotifyTracks.Num(); ++I)
	{
		FAnimNotifyTrack& Track = Sequence->AnimNotifyTracks[I];

		const int32 NewTrackIndex = I + 1;

		for (FAnimNotifyEvent* Notify : Track.Notifies)
		{
			// fix notifies indices
			Notify->TrackIndex = NewTrackIndex;
		}

		for (FAnimSyncMarker* SyncMarker : Track.SyncMarkers)
		{
			// fix notifies indices
			SyncMarker->TrackIndex = NewTrackIndex;
		}
	}

	FAnimNotifyTrack NewItem;
	NewItem.TrackName = GetNewTrackName();
	NewItem.TrackColor = FLinearColor::White;

	Sequence->AnimNotifyTracks.Insert(NewItem, TrackIndexToInsert);
	Sequence->MarkPackageDirty();

	Update();

	//Now we have updated we can request rename on the track UI
	int32 UITrackIndex = NotifyEditorTracks.Num() - 1 - TrackIndexToInsert;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAnimNotifyPanel::TriggerRename, UITrackIndex));
	return FReply::Handled();
}

EActiveTimerReturnType SAnimNotifyPanel::TriggerRename(double InCurrentTime, float InDeltaTime, int32 TrackIndex)
{
	NotifyEditorTracks[TrackIndex]->TriggerRename();
	return EActiveTimerReturnType::Stop;
}

FReply SAnimNotifyPanel::DeleteTrack(int32 TrackIndexToDelete)
{
	if (Sequence->AnimNotifyTracks.IsValidIndex(TrackIndexToDelete))
	{
		if (Sequence->AnimNotifyTracks[TrackIndexToDelete].Notifies.Num() == 0)
		{
			// before insert, make sure everything behind is fixed
			for (int32 I=TrackIndexToDelete+1; I<Sequence->AnimNotifyTracks.Num(); ++I)
			{
				FAnimNotifyTrack& Track = Sequence->AnimNotifyTracks[I];
				const int32 NewTrackIndex = I - 1;

				for (FAnimNotifyEvent* Notify : Track.Notifies)
				{
					// fix notifies indices
					Notify->TrackIndex = NewTrackIndex;
				}

				for (FAnimSyncMarker* SyncMarker : Track.SyncMarkers)
				{
					// fix notifies indices
					SyncMarker->TrackIndex = NewTrackIndex;
				}
			}

			Sequence->AnimNotifyTracks.RemoveAt(TrackIndexToDelete);
			Sequence->MarkPackageDirty();
			Update();
		}
	}
	return FReply::Handled();
}

bool SAnimNotifyPanel::CanDeleteTrack(int32 TrackIndexToDelete)
{
	if (Sequence->AnimNotifyTracks.Num() > 1 && Sequence->AnimNotifyTracks.IsValidIndex(TrackIndexToDelete))
	{
		return Sequence->AnimNotifyTracks[TrackIndexToDelete].Notifies.Num() == 0;
	}

	return false;
}

void SAnimNotifyPanel::OnCommitTrackName(const FText& InText, ETextCommit::Type CommitInfo, int32 TrackIndexToName)
{
	if (Sequence->AnimNotifyTracks.IsValidIndex(TrackIndexToName))
	{
		FScopedTransaction Transaction(FText::Format(LOCTEXT("RenameNotifyTrack", "Rename Notify Track to '{0}'"), InText));
		Sequence->Modify();

		FText TrimText = FText::TrimPrecedingAndTrailing(InText);
		Sequence->AnimNotifyTracks[TrackIndexToName].TrackName = FName(*TrimText.ToString());
	}
}

void SAnimNotifyPanel::Update()
{
	if(Sequence != NULL)
	{
		Sequence->RefreshCacheData();
	}

	OnAnimNotifiesChanged.ExecuteIfBound();
}

void SAnimNotifyPanel::RefreshNotifyTracks()
{
	check (Sequence);

	TSharedPtr<SVerticalBox> NotifySlots;
	PanelArea->SetContent(
		SAssignNew( NotifySlots, SVerticalBox )
		);

	// Clear node tool tips to stop slate referencing them and possibly
	// causing a crash if the notify has gone away
	for (TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->ClearNodeTooltips();
	}

	NotifyAnimTracks.Empty();
	NotifyEditorTracks.Empty();

	for(int32 I=Sequence->AnimNotifyTracks.Num()-1; I>=0; --I)
	{
		FAnimNotifyTrack& Track = Sequence->AnimNotifyTracks[I];
		TSharedPtr<SNotifyEdTrack> EdTrack;

		NotifySlots->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			SAssignNew(EdTrack, SNotifyEdTrack)
			.TrackIndex(I)
			.Sequence(Sequence)
			.AnimNotifyPanel(SharedThis(this))
			.WidgetWidth(WidgetWidth)
			.ViewInputMin(ViewInputMin)
			.ViewInputMax(ViewInputMax)
			.OnGetScrubValue(OnGetScrubValue)
			.OnGetDraggedNodePos(this, &SAnimNotifyPanel::CalculateDraggedNodePos)
			.OnUpdatePanel(this, &SAnimNotifyPanel::Update)
			.OnGetNotifyBlueprintData(this, &SAnimNotifyPanel::OnGetNotifyBlueprintData, &NotifyClassNames)
			.OnGetNotifyStateBlueprintData(this, &SAnimNotifyPanel::OnGetNotifyBlueprintData, &NotifyStateClassNames)
			.OnGetNotifyNativeClasses(this, &SAnimNotifyPanel::OnGetNativeNotifyData, UAnimNotify::StaticClass(), &NotifyClassNames)
			.OnGetNotifyStateNativeClasses(this, &SAnimNotifyPanel::OnGetNativeNotifyData, UAnimNotifyState::StaticClass(), &NotifyStateClassNames)
			.OnSelectionChanged(this, &SAnimNotifyPanel::OnTrackSelectionChanged)
			.OnNodeDragStarted(this, &SAnimNotifyPanel::OnNotifyNodeDragStarted)
			.MarkerBars(MarkerBars)
			.OnRequestRefreshOffsets(OnRequestRefreshOffsets)
			.OnDeleteNotify(this, &SAnimNotifyPanel::DeleteSelectedNodeObjects)
			.OnDeselectAllNotifies(this, &SAnimNotifyPanel::DeselectAllNotifies)
			.OnCopyNodes(this, &SAnimNotifyPanel::CopySelectedNodesToClipboard)
			.OnPasteNodes(this, &SAnimNotifyPanel::OnPasteNodes)
			.OnSetInputViewRange(this, &SAnimNotifyPanel::InputViewRangeChanged)
			.OnGetTimingNodeVisibility(OnGetTimingNodeVisibility)
			.OnInvokeTab(OnInvokeTab)
		];

		NotifyAnimTracks.Add(EdTrack->NotifyTrack);
		NotifyEditorTracks.Add(EdTrack);
	}
}

float SAnimNotifyPanel::CalculateDraggedNodePos() const
{
	return CurrentDragXPosition;
}

FReply SAnimNotifyPanel::OnNotifyNodeDragStarted(TArray<TSharedPtr<SAnimNotifyNode>> NotifyNodes, TSharedRef<SWidget> Decorator, const FVector2D& ScreenCursorPos, const FVector2D& ScreenNodePosition, const bool bDragOnMarker)
{
	TSharedRef<SOverlay> NodeDragDecorator = SNew(SOverlay);
	TArray<TSharedPtr<SAnimNotifyNode>> Nodes;

	for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->DisconnectSelectedNodesForDrag(Nodes);
	}

	FVector2D OverlayOrigin = Nodes[0]->GetScreenPosition();
	FVector2D OverlayExtents = OverlayOrigin;
	OverlayExtents.X += Nodes[0]->GetDurationSize();
	for(int32 Idx = 1 ; Idx < Nodes.Num() ; ++Idx)
	{
		TSharedPtr<SAnimNotifyNode> Node = Nodes[Idx];
		FVector2D NodePosition = Node->GetScreenPosition();
		float NodeDuration = Node->GetDurationSize();

		if(NodePosition.X < OverlayOrigin.X)
		{
			OverlayOrigin.X = NodePosition.X;
		}
		else if(NodePosition.X + NodeDuration > OverlayExtents.X)
		{
			OverlayExtents.X = NodePosition.X + NodeDuration;
		}

		if(NodePosition.Y < OverlayOrigin.Y)
		{
			OverlayOrigin.Y = NodePosition.Y;
		}
		else if(NodePosition.Y + NotifyHeight > OverlayExtents.Y)
		{
			OverlayExtents.Y = NodePosition.Y + NotifyHeight;
		}
	}
	OverlayExtents -= OverlayOrigin;

	for(TSharedPtr<SAnimNotifyNode> Node : Nodes)
	{
		FVector2D OffsetFromFirst(Node->GetScreenPosition() - OverlayOrigin);

		NodeDragDecorator->AddSlot()
			.Padding(FMargin(OffsetFromFirst.X, OffsetFromFirst.Y, 0.0f, 0.0f))
			[
				Node->AsShared()
			];
	}

	FPanTrackRequest PanRequestDelegate = FPanTrackRequest::CreateSP(this, &SAnimNotifyPanel::PanInputViewRange);
	FOnUpdatePanel UpdateDelegate = FOnUpdatePanel::CreateSP(this, &SAnimNotifyPanel::Update);
	return FReply::Handled().BeginDragDrop(FNotifyDragDropOp::New(Nodes, NodeDragDecorator, NotifyAnimTracks, Sequence, ScreenCursorPos, OverlayOrigin, OverlayExtents, CurrentDragXPosition, PanRequestDelegate, MarkerBars, UpdateDelegate));
}

void SAnimNotifyPanel::PostUndo()
{
	if(Sequence != NULL)
	{
		Sequence->RefreshCacheData();
	}
}

void SAnimNotifyPanel::OnDeletePressed()
{
	// If there's no focus on the panel it's likely the user is not editing notifies
	// so don't delete anything when the key is pressed.
	if(HasKeyboardFocus() || HasFocusedDescendants()) 
	{
		DeleteSelectedNodeObjects();
	}
}

void SAnimNotifyPanel::DeleteSelectedNodeObjects()
{
	TArray<INodeObjectInterface*> SelectedNodes;
	for (TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->AppendSelectionToArray(SelectedNodes);
	}

	const bool bContainsSyncMarkers = SelectedNodes.ContainsByPredicate([](const INodeObjectInterface* Interface) { return Interface->GetType() == ENodeObjectTypes::NOTIFY; });

	if (SelectedNodes.Num() > 0)
	{
		FScopedTransaction Transaction(LOCTEXT("DeleteMarkers", "Delete Animation Markers"));
		Sequence->Modify(true);

		// Delete from highest index to lowest
		SelectedNodes.Sort();
		for (int NodeIndex = SelectedNodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
		{
			INodeObjectInterface* NodeObject = SelectedNodes[NodeIndex];
			NodeObject->Delete(Sequence);
		}

		if (bContainsSyncMarkers)
		{
			UBlendSpaceBase::UpdateBlendSpacesUsingAnimSequence(Sequence);
		}
	}

	// clear selection and update the panel
	TArray<UObject*> Objects;
	OnSelectionChanged.ExecuteIfBound(Objects);

	Update();
}

void SAnimNotifyPanel::SetSequence(class UAnimSequenceBase *	InSequence)
{
	if (InSequence != Sequence)
	{
		Sequence = InSequence;
		// @todo anim : this is kinda hack to make sure it has 1 track is alive
		// we can do this whenever import or asset is created, but it's more places to handle than here
		// the function name in that case will need to change
		Sequence->InitializeNotifyTrack();
		Update();
	}
}

void SAnimNotifyPanel::OnTrackSelectionChanged()
{
	// Need to collect selection info from all tracks
	TArray<UObject*> NotifyObjects;

	for(int32 TrackIdx = 0 ; TrackIdx < NotifyAnimTracks.Num() ; ++TrackIdx)
	{
		TSharedPtr<SAnimNotifyTrack> Track = NotifyAnimTracks[TrackIdx];
		TArray<int32> TrackIndices = Track->GetSelectedNotifyIndices();
		for(int32 Idx : TrackIndices)
		{
			INodeObjectInterface* NodeObjectInterface = Track->GetNodeObjectInterface(Idx);
			if (FAnimNotifyEvent* NotifyEvent = NodeObjectInterface->GetNotifyEvent())
			{
				FString ObjName = MakeUniqueObjectName(GetTransientPackage(), UEditorNotifyObject::StaticClass()).ToString();
				UEditorNotifyObject* NewNotifyObject = NewObject<UEditorNotifyObject>(GetTransientPackage(), FName(*ObjName), RF_Public | RF_Standalone | RF_Transient);
				NewNotifyObject->InitFromAnim(Sequence, FOnAnimObjectChange::CreateSP(this, &SAnimNotifyPanel::OnNotifyObjectChanged));
				NewNotifyObject->InitialiseNotify(NotifyAnimTracks.Num() - TrackIdx - 1, Idx);
				NotifyObjects.AddUnique(NewNotifyObject);
			}
		}
	}

	OnSelectionChanged.ExecuteIfBound(NotifyObjects);
}

void SAnimNotifyPanel::DeselectAllNotifies()
{
	for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->DeselectAllNotifyNodes(false);
	}

	// Broadcast the change so the editor can update
	OnAnimNotifiesChanged.ExecuteIfBound();

	OnTrackSelectionChanged();
}

void SAnimNotifyPanel::CopySelectedNodesToClipboard() const
{
	// Grab the selected events
	TArray<INodeObjectInterface*> SelectedNodes;
	for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->AppendSelectionToArray(SelectedNodes);
	}

	const FString HeaderString(TEXT("COPY_ANIMNOTIFYEVENT"));
	
	if (SelectedNodes.Num() > 0)
	{
		FString StrValue(HeaderString);

		// Sort by track
		SelectedNodes.Sort([](const INodeObjectInterface& A, const INodeObjectInterface& B)
		{
			return (A.GetTrackIndex() > B.GetTrackIndex()) || (A.GetTrackIndex() == B.GetTrackIndex() && A.GetTime() < B.GetTime());
		});

		// Need to find how many tracks this selection spans and the minimum time to use as the beginning of the selection
		int32 MinTrack = MAX_int32;
		int32 MaxTrack = MIN_int32;
		float MinTime = MAX_flt;
		for (const INodeObjectInterface* NodeObject : SelectedNodes)
		{
			MinTrack = FMath::Min(MinTrack, NodeObject->GetTrackIndex());
			MaxTrack = FMath::Max(MaxTrack, NodeObject->GetTrackIndex());
			MinTime = FMath::Min(MinTime, NodeObject->GetTime());
		}

		int32 TrackSpan = MaxTrack - MinTrack + 1;

		StrValue += FString::Printf(TEXT("OriginalTime=%f,"), MinTime);
		StrValue += FString::Printf(TEXT("OriginalLength=%f,"), Sequence->SequenceLength);
		StrValue += FString::Printf(TEXT("TrackSpan=%d"), TrackSpan);

		for(const INodeObjectInterface* NodeObject : SelectedNodes)
		{
			// Locate the notify in the sequence, we need the sequence index; but also need to
			// keep the order we're currently in.

			StrValue += "\n";
			StrValue += FString::Printf(TEXT("AbsTime=%f,NodeObjectType=%i,"), NodeObject->GetTime(), (int32)NodeObject->GetType());

			NodeObject->ExportForCopy(Sequence, StrValue);
		}
		FPlatformApplicationMisc::ClipboardCopy(*StrValue);
	}
}

bool SAnimNotifyPanel::IsNotifySelectionValidForReplacement()
{
	// Grab the selected events
	TArray<INodeObjectInterface*> SelectedNodes;
	for (TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->AppendSelectionToArray(SelectedNodes);
	}

	bool bSelectionContainsAnimNotify = false;
	bool bSelectionContainsAnimNotifyState = false;
	for (INodeObjectInterface* NodeObject : SelectedNodes)
	{
		FAnimNotifyEvent* NotifyEvent = NodeObject->GetNotifyEvent();
		if (NotifyEvent)
		{
			if (NotifyEvent->Notify)
			{
				bSelectionContainsAnimNotify = true;
			}
			else if (NotifyEvent->NotifyStateClass)
			{
				bSelectionContainsAnimNotifyState = true;
			}
			// Custom AnimNotifies have no class, but they are like AnimNotify class notifies in that they have no duration
			else
			{
				bSelectionContainsAnimNotify = true;
			}
		}
	}

	// Only allow replacement for selections that contain _only_ AnimNotifies, or _only_ AnimNotifyStates, but not both
	// (Want to disallow replacement of AnimNotify with AnimNotifyState, and vice-versa)
	bool bIsValidSelection = bSelectionContainsAnimNotify != bSelectionContainsAnimNotifyState;

	return bIsValidSelection;
}


void SAnimNotifyPanel::OnReplaceSelectedWithNotify(FString NewNotifyName, UClass* NewNotifyClass)
{
	TArray<INodeObjectInterface*> SelectedNodes;
	for (TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->AppendSelectionToArray(SelectedNodes);
	}

	// Sort these since order is important for deletion
	SelectedNodes.Sort();

	const FScopedTransaction Transaction(LOCTEXT("ReplaceAnimNotify", "Replace Anim Notify"));
	Sequence->Modify(true);

	for (INodeObjectInterface* NodeObject : SelectedNodes)
	{
		FAnimNotifyEvent* OldEvent = NodeObject->GetNotifyEvent();
		if (OldEvent)
		{
			float BeginTime = OldEvent->GetTime();
			float Length = OldEvent->GetDuration();
			int32 TargetTrackIndex = (NotifyAnimTracks.Num()-1) - OldEvent->TrackIndex;
			float TriggerTimeOffset = OldEvent->TriggerTimeOffset;
			float EndTriggerTimeOffset = OldEvent->EndTriggerTimeOffset;
			int32 SlotIndex = OldEvent->GetSlotIndex();
			int32 EndSlotIndex = OldEvent->EndLink.GetSlotIndex();
			int32 SegmentIndex = OldEvent->GetSegmentIndex();
			int32 EndSegmentIndex = OldEvent->GetSegmentIndex();
			EAnimLinkMethod::Type LinkMethod = OldEvent->GetLinkMethod();
			EAnimLinkMethod::Type EndLinkMethod = OldEvent->EndLink.GetLinkMethod();

			// Delete old one before creating new one to avoid potential array re-allocation when array temporarily increases by 1 in size
			NodeObject->Delete(Sequence);
			FAnimNotifyEvent& NewEvent = NotifyAnimTracks[TargetTrackIndex]->CreateNewNotify(NewNotifyName, NewNotifyClass, BeginTime);

			NewEvent.TriggerTimeOffset = TriggerTimeOffset;
			NewEvent.ChangeSlotIndex(SlotIndex);
			NewEvent.SetSegmentIndex(SegmentIndex);
			NewEvent.ChangeLinkMethod(LinkMethod);

			// For Anim Notify States, handle the end time and link
			if (NewEvent.NotifyStateClass != nullptr)
			{
				NewEvent.SetDuration(Length);
				NewEvent.EndTriggerTimeOffset = EndTriggerTimeOffset;
				NewEvent.EndLink.ChangeSlotIndex(EndSlotIndex);
				NewEvent.EndLink.SetSegmentIndex(EndSegmentIndex);
				NewEvent.EndLink.ChangeLinkMethod(EndLinkMethod);
			}
						
			NewEvent.Update();
		}
	}

	// clear selection  
	TArray<UObject*> Objects;
	OnSelectionChanged.ExecuteIfBound(Objects);
	// TODO: set selection to new notifies?
	// update the panel
	Update();
}

void SAnimNotifyPanel::OnReplaceSelectedWithNotifyBlueprint(FString NewBlueprintNotifyName, FString NewBlueprintNotifyClass)
{
	TSubclassOf<UObject> BlueprintClass = SAnimNotifyTrack::GetBlueprintClassFromPath(NewBlueprintNotifyClass);
	OnReplaceSelectedWithNotify(NewBlueprintNotifyName, BlueprintClass);
}

void SAnimNotifyPanel::OnPasteNodes(SAnimNotifyTrack* RequestTrack, float ClickTime, ENotifyPasteMode::Type PasteMode, ENotifyPasteMultipleMode::Type MultiplePasteType)
{
	int32 PasteIdx = RequestTrack->GetTrackIndex();
	int32 NumTracks = NotifyAnimTracks.Num();
	FString PropString;
	const TCHAR* Buffer;
	float OrigBeginTime;
	float OrigLength;
	int32 TrackSpan;
	int32 FirstTrack = -1;
	float ScaleMultiplier = 1.0f;

	if(ReadNotifyPasteHeader(PropString, Buffer, OrigBeginTime, OrigLength, TrackSpan))
	{
		DeselectAllNotifies();

		FScopedTransaction Transaction(LOCTEXT("PasteNotifyEvent", "Paste Anim Notifies"));
		Sequence->Modify();

		if(ClickTime == -1.0f)
		{
			// We want to place the notifies exactly where they were
			ClickTime = OrigBeginTime;
		}

		// Expand the number of tracks if we don't have enough.
		check(TrackSpan > 0);
		if(PasteIdx - (TrackSpan - 1) < 0)
		{
			int32 TracksToAdd = PasteIdx + TrackSpan - 1;
			while(TracksToAdd)
			{
				InsertTrack(PasteIdx++);
				--TracksToAdd;
			}
			NumTracks = NotifyAnimTracks.Num();
			RequestTrack = NotifyAnimTracks[NumTracks - 1 - PasteIdx].Get();
		}

		// Scaling for relative paste
		if(MultiplePasteType == ENotifyPasteMultipleMode::Relative)
		{
			ScaleMultiplier = Sequence->SequenceLength / OrigLength;
		}

		// Process each line of the paste buffer and spawn notifies
		FString CurrentLine;
		while(FParse::Line(&Buffer, CurrentLine))
		{
			int32 OriginalTrack;
			float OrigTime;
			int32 NodeObjectType;
			float PasteTime = -1.0f;
			if (FParse::Value(*CurrentLine, TEXT("TrackIndex="), OriginalTrack) && FParse::Value(*CurrentLine, TEXT("AbsTime="), OrigTime) && FParse::Value(*CurrentLine, TEXT("NodeObjectType="), NodeObjectType))
			{
				const int32 FirstComma = CurrentLine.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart);
				const int32 SecondComma = CurrentLine.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstComma + 1);
				FString NotifyExportString = CurrentLine.RightChop(SecondComma+1);

				// Store the first track so we know where to place notifies
				if(FirstTrack < 0)
				{
					FirstTrack = OriginalTrack;
				}
				int32 TrackOffset = OriginalTrack - FirstTrack;

				float TimeOffset = OrigTime - OrigBeginTime;
				float TimeToPaste = ClickTime + TimeOffset * ScaleMultiplier;

				// Have to invert the index here as tracks are stored in reverse
				TSharedPtr<SAnimNotifyTrack> TrackToUse = NotifyAnimTracks[NotifyAnimTracks.Num() - 1 - (PasteIdx + TrackOffset)];
				if (NodeObjectType == ENodeObjectTypes::NOTIFY)
				{
					TrackToUse->PasteSingleNotify(NotifyExportString, TimeToPaste);
				}
				else if (NodeObjectType == ENodeObjectTypes::SYNC_MARKER)
				{
					TrackToUse->PasteSingleSyncMarker(NotifyExportString, TimeToPaste);
				}
				else
				{
					check(false); //Unknown value in paste
				}
			}
		}
	}
}

void SAnimNotifyPanel::OnPropertyChanged(UObject* ChangedObject, FPropertyChangedEvent& PropertyEvent)
{
	// Bail if it isn't a notify
	if(!ChangedObject->GetClass()->IsChildOf(UAnimNotify::StaticClass()) &&
	   !ChangedObject->GetClass()->IsChildOf(UAnimNotifyState::StaticClass()))
	{
		return;
	}

	// Don't process if it's an interactive change; wait till we receive the final event.
	if(PropertyEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		for(FAnimNotifyEvent& Event : Sequence->Notifies)
		{
			if(Event.Notify == ChangedObject || Event.NotifyStateClass == ChangedObject)
			{
				// If we've changed a notify present in the sequence, refresh our tracks.
				Sequence->RefreshCacheData();
				RefreshNotifyTracks();
			}
		}

		// Broadcast the change so the editor can update
		OnAnimNotifiesChanged.ExecuteIfBound();
	}
}

void SAnimNotifyPanel::BindCommands()
{
	check(!UICommandList.IsValid());

	UICommandList = MakeShareable(new FUICommandList);
	const FAnimNotifyPanelCommands& Commands = FAnimNotifyPanelCommands::Get();

	UICommandList->MapAction(
		Commands.DeleteNotify,
		FExecuteAction::CreateSP(this, &SAnimNotifyPanel::OnDeletePressed));
}

FReply SAnimNotifyPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAnimNotifyPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SAnimTrackPanel::OnMouseButtonDown(MyGeometry, MouseEvent);

	bool bLeftButton = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);

	if(bLeftButton)
	{
		TArray<TSharedPtr<SAnimNotifyNode>> SelectedNodes;
		for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
		{
			Track->AppendSelectedNodeWidgetsToArray(SelectedNodes);
		}

		Marquee.Start(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), Marquee.OperationTypeFromMouseEvent(MouseEvent), SelectedNodes);
		if(Marquee.Operation == FNotifyMarqueeOperation::Replace)
		{
			// Remove and Add operations preserve selections, replace starts afresh
			DeselectAllNotifies();
		}

		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	return FReply::Unhandled();
}

FReply SAnimNotifyPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(Marquee.bActive)
	{
		OnTrackSelectionChanged();
		Marquee = FNotifyMarqueeOperation();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return SAnimTrackPanel::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SAnimNotifyPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply BaseReply = SAnimTrackPanel::OnMouseMove(MyGeometry, MouseEvent);
	if(!BaseReply.IsEventHandled())
	{
		bool bLeftButton = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);

		if(bLeftButton && Marquee.bActive)
		{
			Marquee.Rect.UpdateEndPoint(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
			RefreshMarqueeSelectedNodes(MyGeometry);
		}
		return FReply::Handled();
	}

	return BaseReply;
}

int32 SAnimNotifyPanel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SAnimTrackPanel::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	FVector2D Origin = AllottedGeometry.AbsoluteToLocal(Marquee.Rect.GetUpperLeft());
	FVector2D Extents = AllottedGeometry.AbsoluteToLocal(Marquee.Rect.GetSize());

	if(Marquee.IsValid())
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(Marquee.Rect.GetUpperLeft(), Marquee.Rect.GetSize()),
			FEditorStyle::GetBrush(TEXT("MarqueeSelection"))
			);
	}

	return LayerId;
}

void SAnimNotifyPanel::RefreshMarqueeSelectedNodes(const FGeometry& PanelGeo)
{
	if(Marquee.IsValid())
	{
		FSlateRect MarqueeRect = Marquee.Rect.ToSlateRect();
		for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
		{
			if(Marquee.Operation == FNotifyMarqueeOperation::Replace || Marquee.OriginalSelection.Num() == 0)
			{
				Track->DeselectAllNotifyNodes(false);
			}

			const FGeometry& TrackGeo = Track->GetCachedGeometry();

			FSlateRect TrackClip = TrackGeo.GetLayoutBoundingRect();
			FSlateRect PanelClip = PanelGeo.GetLayoutBoundingRect();
			FVector2D PanelSpaceOrigin = TrackClip.GetTopLeft() - PanelClip.GetTopLeft();
			FVector2D TrackSpaceOrigin = MarqueeRect.GetTopLeft() - PanelSpaceOrigin;
			FSlateRect MarqueeTrackSpace(TrackSpaceOrigin, TrackSpaceOrigin + MarqueeRect.GetSize());

			Track->RefreshMarqueeSelectedNodes(MarqueeTrackSpace, Marquee);
		}
	}
}

FReply SAnimNotifyPanel::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Marquee.bActive = true;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

void SAnimNotifyPanel::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if(Marquee.bActive)
	{
		OnTrackSelectionChanged();
	}
	Marquee = FNotifyMarqueeOperation();
}

void SAnimNotifyPanel::PopulateNotifyBlueprintClasses(TArray<FString>& InOutAllowedClasses)
{
	TArray<FAssetData> TempArray;
	OnGetNotifyBlueprintData(TempArray, &InOutAllowedClasses);
}

void SAnimNotifyPanel::OnGetNotifyBlueprintData(TArray<FAssetData>& OutNotifyData, TArray<FString>* InOutAllowedClassNames)
{
	// If we have nothing to seach with, early out
	if(InOutAllowedClassNames == NULL || InOutAllowedClassNames->Num() == 0)
	{
		return;
	}

	TArray<FAssetData> AssetDataList;
	TArray<FString> FoundClasses;

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Collect a full list of assets with the specified class
	AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), AssetDataList);

	static const FName BPParentClassName(TEXT("ParentClass"));
	static const FName BPGenClassName(TEXT("GeneratedClass"));

	int32 BeginClassCount = InOutAllowedClassNames->Num();
	int32 CurrentClassCount = -1;

	while(BeginClassCount != CurrentClassCount)
	{
		BeginClassCount = InOutAllowedClassNames->Num();

		for(int32 AssetIndex = 0; AssetIndex < AssetDataList.Num(); ++AssetIndex)
		{
			FAssetData& AssetData = AssetDataList[AssetIndex];
			FString TagValue = AssetData.GetTagValueRef<FString>(BPParentClassName);

			if(InOutAllowedClassNames->Contains(TagValue))
			{
				FString GenClass = AssetData.GetTagValueRef<FString>(BPGenClassName);
				const uint32 ClassFlags = AssetData.GetTagValueRef<uint32>("ClassFlags");
				if (ClassFlags & CLASS_Abstract)
				{
					continue;
				}

				if(!OutNotifyData.Contains(AssetData))
				{
					// Output the assetdata and record it as found in this request
					OutNotifyData.Add(AssetData);
					FoundClasses.Add(GenClass);
				}

				if(!InOutAllowedClassNames->Contains(GenClass))
				{
					// Expand the class list to account for a new possible parent class found
					InOutAllowedClassNames->Add(GenClass);
				}
			}
		}

		CurrentClassCount = InOutAllowedClassNames->Num();
	}

	// Count native classes, so we don't remove them from the list
	int32 NumNativeClasses = 0;
	for(FString& AllowedClass : *InOutAllowedClassNames)
	{
		if(!AllowedClass.EndsWith(FString(TEXT("_C'"))))
		{
			++NumNativeClasses;
		}
	}

	if(FoundClasses.Num() < InOutAllowedClassNames->Num() - NumNativeClasses)
	{
		// Less classes found, some may have been deleted or reparented
		for(int32 ClassIndex = InOutAllowedClassNames->Num() - 1 ; ClassIndex >= 0 ; --ClassIndex)
		{
			FString& ClassName = (*InOutAllowedClassNames)[ClassIndex];
			if(ClassName.EndsWith(FString(TEXT("_C'"))) && !FoundClasses.Contains(ClassName))
			{
				InOutAllowedClassNames->RemoveAt(ClassIndex);
			}
		}
	}
}

void SAnimNotifyPanel::OnGetNativeNotifyData(TArray<UClass*>& OutClasses, UClass* NotifyOutermost, TArray<FString>* OutAllowedBlueprintClassNames)
{
	for(TObjectIterator<UClass> It ; It ; ++It)
	{
		UClass* Class = *It;

		if(Class->IsChildOf(NotifyOutermost) && Class->HasAllClassFlags(CLASS_Native) && !Class->IsInBlueprint())
		{
			OutClasses.Add(Class);
			// Form class name to search later
			FString ClassName = FString::Printf(TEXT("%s'%s'"), *Class->GetClass()->GetName(), *Class->GetPathName());
			OutAllowedBlueprintClassNames->AddUnique(ClassName);
		}
	}
}

void SAnimNotifyPanel::OnNotifyObjectChanged(UObject* EditorBaseObj, bool bRebuild)
{
	if(UEditorNotifyObject* NotifyObject = Cast<UEditorNotifyObject>(EditorBaseObj))
	{
		// TODO: We should really un-invert these.
		int32 WidgetTrackIdx = NotifyAnimTracks.Num() - NotifyObject->TrackIndex - 1;
		if(NotifyAnimTracks.IsValidIndex(WidgetTrackIdx))
		{
			NotifyAnimTracks[WidgetTrackIdx]->Update();
		}

		// Broadcast the change so the editor can update
		OnAnimNotifiesChanged.ExecuteIfBound();
	}
}

void SAnimNotifyPanel::OnNotifyTrackScrolled(float InScrollOffsetFraction)
{
	float Ratio = (ViewInputMax.Get() - ViewInputMin.Get()) / Sequence->SequenceLength;
	float MaxOffset = (Ratio < 1.0f) ? 1.0f - Ratio : 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);

	// Calculate new view ranges
	float NewMin = InScrollOffsetFraction * Sequence->SequenceLength;
	float NewMax = (InScrollOffsetFraction + Ratio) * Sequence->SequenceLength;
	
	InputViewRangeChanged(NewMin, NewMax);
}

void SAnimNotifyPanel::InputViewRangeChanged(float ViewMin, float ViewMax)
{
	float Ratio = (ViewMax - ViewMin) / Sequence->SequenceLength;
	float OffsetFraction = ViewMin / Sequence->SequenceLength;
	NotifyTrackScrollBar->SetState(OffsetFraction, Ratio);

	SAnimTrackPanel::InputViewRangeChanged(ViewMin, ViewMax);
}

#undef LOCTEXT_NAMESPACE
