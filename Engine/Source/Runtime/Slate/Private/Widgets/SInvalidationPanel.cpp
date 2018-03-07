// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SInvalidationPanel.h"
#include "Rendering/DrawElements.h"
#include "Misc/App.h"
#include "Application/SlateApplicationBase.h"
#include "Styling/CoreStyle.h"
#include "Layout/WidgetPath.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetCaching.h"

//DECLARE_CYCLE_STAT(TEXT("Invalidation Time"), STAT_InvalidationTime, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Cached Elements"), STAT_SlateNumCachedElements, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Invalidated Elements"), STAT_SlateNumInvalidatedElements, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Volatile Widgets"), STAT_SlateNumVolatileWidgets, STATGROUP_Slate);

DECLARE_CYCLE_STAT(TEXT("SInvalidationPanel::Tick"), STAT_SlateInvalidationTick, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("SInvalidationPanel::Paint"), STAT_SlateInvalidationPaint, STATGROUP_Slate);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

/** True if we should allow widgets to be cached in the UI at all. */
TAutoConsoleVariable<int32> InvalidationDebugging(
	TEXT("Slate.InvalidationDebugging"),
	false,
	TEXT("Whether to show invalidation debugging visualization"));

bool SInvalidationPanel::IsInvalidationDebuggingEnabled()
{
	return InvalidationDebugging.GetValueOnGameThread() == 1;
}

void SInvalidationPanel::EnableInvalidationDebugging(bool bEnable)
{
	InvalidationDebugging.AsVariable()->Set(bEnable);
}

/** True if we should allow widgets to be cached in the UI at all. */
TAutoConsoleVariable<int32> EnableWidgetCaching(
	TEXT("Slate.EnableWidgetCaching"),
	true,
	TEXT("Whether to attempt to cache any widgets through invalidation panels."));

bool SInvalidationPanel::GetEnableWidgetCaching()
{
	return EnableWidgetCaching.GetValueOnGameThread() == 1;
}

void SInvalidationPanel::SetEnableWidgetCaching(bool bEnable)
{
	EnableWidgetCaching.AsVariable()->Set(bEnable);
}

TAutoConsoleVariable<int32> AlwaysInvalidate(
	TEXT("Slate.AlwaysInvalidate"),
	false,
	TEXT("Forces invalidation panels to cache, but to always invalidate."));

#endif

static int32 CacheRenderData = 1;
static FAutoConsoleVariableRef CVarCacheRenderData(
	TEXT("Slate.CacheRenderData"),
	CacheRenderData,
	TEXT("Invalidation panels will cache render data, otherwise cache only widget draw elements."));

static bool ShouldCacheRenderData()
{
	return WITH_ENGINE && CacheRenderData != 0;
}

void SInvalidationPanel::Construct( const FArguments& InArgs )
{
	FSlateApplicationBase::Get().OnGlobalInvalidate().AddSP( this, &SInvalidationPanel::OnGlobalInvalidate );

	ChildSlot
	[
		InArgs._Content.Widget
	];

	bNeedsCaching = true;
	bIsInvalidating = false;
	bCanCache = true;
	RootCacheNode = nullptr;
	LastUsedCachedNodeIndex = 0;
	LastHitTestIndex = 0;

	bCacheRelativeTransforms = InArgs._CacheRelativeTransforms;

	bCacheRenderData = ShouldCacheRenderData();
}

SInvalidationPanel::~SInvalidationPanel()
{
	for ( int32 i = 0; i < NodePool.Num(); i++ )
	{
		delete NodePool[i];
	}

	if ( FSlateApplication::IsInitialized() )
	{
		FSlateApplication::Get().ReleaseResourcesForLayoutCache(this);
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
bool SInvalidationPanel::GetCanCache() const
{
	return bCanCache && EnableWidgetCaching.GetValueOnGameThread() == 1;
}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
bool SInvalidationPanel::IsCachingNeeded() const
{
	return bNeedsCaching || AlwaysInvalidate.GetValueOnGameThread() == 1;
}
#endif

bool SInvalidationPanel::IsCachingNeeded(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect) const
{
	if (bCacheRelativeTransforms)
	{
		// If the container we're in has changed in either scale or the rotation matrix has changed, 
		if ( AllottedGeometry.GetAccumulatedLayoutTransform().GetScale() != LastAllottedGeometry.GetAccumulatedLayoutTransform().GetScale() ||
			 AllottedGeometry.GetAccumulatedRenderTransform().GetMatrix() != LastAllottedGeometry.GetAccumulatedRenderTransform().GetMatrix() )
		{
			return true;
		}
	}
	else
	{
		// If the container we're in has changed in any way we need to invalidate for sure.
		if ( AllottedGeometry.GetAccumulatedLayoutTransform() != LastAllottedGeometry.GetAccumulatedLayoutTransform() ||
			AllottedGeometry.GetAccumulatedRenderTransform() != LastAllottedGeometry.GetAccumulatedRenderTransform() )
		{
			return true;
		}
	}

	if ( AllottedGeometry.GetLocalSize() != LastAllottedGeometry.GetLocalSize() )
	{
		return true;
	}

	// If our clip rect changes size, we've definitely got to invalidate.
	const FVector2D ClipRectSize = MyCullingRect.GetSize().RoundToVector();
	if ( ClipRectSize != LastClipRectSize )
	{
		return true;
	}

	if (LastClippingIndex != OutDrawElements.GetClippingIndex())
	{
		return true;
	}

	TOptional<FSlateClippingState> ClippingState = OutDrawElements.GetClippingState();
	if (LastClippingState != ClippingState)
	{
		return true;
	}

	int32 ClippingStateCount = OutDrawElements.GetClippingManager().GetClippingStates().Num();
	if (LastClippingStateOffset != ClippingStateCount)
	{
		return true;
	}
	
	return false;
}

void SInvalidationPanel::SetCanCache(bool InCanCache)
{
	bCanCache = InCanCache;

	InvalidateCache();
}

void SInvalidationPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCOPE_CYCLE_COUNTER(STAT_SlateInvalidationTick);

	if ( GetCanCache() )
	{
		//SCOPE_CYCLE_COUNTER(STAT_InvalidationTime);

		const bool bWasCachingNeeded = IsCachingNeeded();

		bool bShouldCacheRenderData = ShouldCacheRenderData();
		if (bCacheRenderData != bShouldCacheRenderData)
		{
			bCacheRenderData = bShouldCacheRenderData;
			InvalidateCache();
		}
		
		// TODO We may be double pre-passing here, if the invalidation happened at the end of last frame,
		// we'll have already done one pre-pass before getting here.
		if ( bWasCachingNeeded )
		{
			SlatePrepass(AllottedGeometry.Scale);
			CachePrepass(SharedThis(this));
		}
	}
}

FChildren* SInvalidationPanel::GetChildren()
{
	if ( GetCanCache() == false || IsCachingNeeded() )
	{
		return SCompoundWidget::GetChildren();
	}
	else
	{
		return &EmptyChildSlot;
	}
}

void SInvalidationPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(CachedResources);
}

void SInvalidationPanel::InvalidateWidget(SWidget* InvalidateWidget)
{
	bNeedsCaching = true;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( InvalidateWidget != nullptr && IsInvalidationDebuggingEnabled() )
	{
		InvalidatorWidgets.Add(InvalidateWidget->AsShared(), 1);
	}
#endif
}

FCachedWidgetNode* SInvalidationPanel::CreateCacheNode() const
{
	// If the node pool is empty, allocate a few
	if ( LastUsedCachedNodeIndex >= NodePool.Num() )
	{
		for ( int32 i = 0; i < 10; i++ )
		{
			NodePool.Add(new FCachedWidgetNode());
		}
	}

	// Return one of the preallocated nodes and increment the next node index.
	FCachedWidgetNode* NewNode = NodePool[LastUsedCachedNodeIndex];
	++LastUsedCachedNodeIndex;

	return NewNode;
}

void SInvalidationPanel::OnGlobalInvalidate()
{
	InvalidateCache();
}

int32 SInvalidationPanel::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	SCOPE_CYCLE_COUNTER(STAT_SlateInvalidationPaint);

	if ( GetCanCache() )
	{
		//SCOPE_CYCLE_COUNTER(STAT_InvalidationTime);

		//FPlatformMisc::BeginNamedEvent(FColor::Magenta, "Slate::InvalidationPanel::Paint");

		const bool bWasCachingNeeded = IsCachingNeeded() || IsCachingNeeded(OutDrawElements, AllottedGeometry, MyCullingRect);

		if ( bWasCachingNeeded )
		{
			//FPlatformMisc::BeginNamedEvent(FColor::Red, "Slate::Invalidation");

			SInvalidationPanel* MutableThis = const_cast<SInvalidationPanel*>( this );
			TSharedRef<SInvalidationPanel> SharedMutableThis = SharedThis(MutableThis);

			// Always set the caching flag to false first, during the paint / tick pass we may change something
			// to volatile and need to re-cache.
			bNeedsCaching = false;

			// Mark that we're in the process of invalidating.
			bIsInvalidating = true;

			CachedWindowElements = FSlateApplication::Get().GetCachableElementList(OutDrawElements.GetWindow(), this);

			// Reset the render data handle in case it was in use, and we're not overriding it this frame.
			CachedRenderData.Reset();

			// Reset the cached node pool index so that we effectively reset the pool.
			LastUsedCachedNodeIndex = 0;

			RootCacheNode = CreateCacheNode();
			RootCacheNode->Initialize(Args, SharedMutableThis, AllottedGeometry);

			//TODO: When SWidget::Paint is called don't drag self if volatile, and we're doing a cache pass.
			CachedMaxChildLayer = SCompoundWidget::OnPaint(
				Args.EnableCaching(SharedMutableThis, RootCacheNode, true, false),
				AllottedGeometry,
				MyCullingRect,
				*CachedWindowElements.Get(),
				LayerId,
				InWidgetStyle,
				bParentEnabled);

			{
				const TArray<FSlateDrawElement>& CachedElements = CachedWindowElements->GetDrawElements();
				const int32 CachedElementCount = CachedElements.Num();
				for ( int32 Index = 0; Index < CachedElementCount; Index++ )
				{
					const FSlateDrawElement& LocalElement = CachedElements[Index];
					if ( const FSlateBrush* BrushResource = LocalElement.GetDataPayload().BrushResource )
					{
						if ( UObject* ResourceObject = BrushResource->GetResourceObject() )
						{
							CachedResources.Add(ResourceObject);
						}
					}
				}
			}

			if ( bCacheRelativeTransforms )
			{
				CachedAbsolutePosition = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation();
			}

			LastClippingStateOffset = OutDrawElements.GetClippingManager().GetClippingStates().Num();
			LastClippingIndex = OutDrawElements.GetClippingIndex();
			LastClippingState = OutDrawElements.GetClippingState();

			const int32 ClippingStateOffset = OutDrawElements.GetClippingManager().MergeClippingStates(CachedWindowElements->GetClippingManager().GetClippingStates());

			TArray<FSlateDrawElement>& CachedElements = CachedWindowElements->GetDrawElements();
			for (int32 Index = 0; Index < CachedElements.Num(); Index++)
			{
				FSlateDrawElement& CachedElement = CachedElements[Index];
				if (CachedElement.GetClippingIndex() == -1)
				{
					CachedElement.SetClippingIndex(LastClippingIndex);
				}
				else
				{
					CachedElement.SetClippingIndex(ClippingStateOffset + CachedElement.GetClippingIndex());
				}
			}

			if (bCacheRenderData)
			{
				CachedRenderData = CachedWindowElements->CacheRenderData(this);
			}

			LastHitTestIndex = Args.GetLastHitTestIndex();

			LastAllottedGeometry = AllottedGeometry;
			LastClipRectSize = MyCullingRect.GetSize().RoundToVector();

			bIsInvalidating = false;

			//FPlatformMisc::EndNamedEvent();
		}
		else
		{
			OutDrawElements.GetClippingManager().MergeClippingStates(CachedWindowElements->GetClippingManager().GetClippingStates());
		}

		FVector2D AbsoluteDeltaPosition = FVector2D::ZeroVector;
		if ( bCacheRelativeTransforms )
		{
			AbsoluteDeltaPosition = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation() - CachedAbsolutePosition;
		}

		//FPlatformMisc::BeginNamedEvent(FColor::Yellow, "Slate::RecordHitTestGeometry");
		


		// The hit test grid is actually populated during the initial cache phase, so don't bother
		// recording the hit test geometry on the same frame that we regenerate the cache.
		if ( bWasCachingNeeded == false )
		{
			INC_DWORD_STAT_BY(STAT_SlateNumCachedElements, CachedWindowElements->GetDrawElements().Num());

			RootCacheNode->RecordHittestGeometry(Args.GetGrid(), Args.GetLastHitTestIndex(), LayerId, AbsoluteDeltaPosition);
		}
		else
		{
			INC_DWORD_STAT_BY(STAT_SlateNumInvalidatedElements, CachedWindowElements->GetDrawElements().Num());
		}
		//FPlatformMisc::EndNamedEvent();

		int32 OutMaxChildLayer = CachedMaxChildLayer;

		if (bCacheRenderData)
		{
			FSlateDrawElement::MakeCachedBuffer(OutDrawElements, LayerId, CachedRenderData, AbsoluteDeltaPosition);
		}
		else
		{
			OutDrawElements.MergeElementList(CachedWindowElements.Get(), AbsoluteDeltaPosition);
		}

		// Paint the volatile elements
		if ( CachedWindowElements.IsValid() )
		{
			//FPlatformMisc::BeginNamedEvent(FColor::Red, *FReflectionMetaData::GetWidgetDebugInfo(this));

			const TArray<TSharedPtr<FSlateWindowElementList::FVolatilePaint>>& VolatileElements = CachedWindowElements->GetVolatileElements();
			INC_DWORD_STAT_BY(STAT_SlateNumVolatileWidgets, VolatileElements.Num());

			int32 VolatileLayerId = CachedWindowElements->PaintVolatile(OutDrawElements, Args.GetCurrentTime(), Args.GetDeltaTime(), AbsoluteDeltaPosition * AllottedGeometry.Scale);
			OutMaxChildLayer = FMath::Max(OutMaxChildLayer, VolatileLayerId);

			//FPlatformMisc::EndNamedEvent();
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		if ( IsInvalidationDebuggingEnabled() )
		{
			// Draw a green or red border depending on if we were invalidated this frame.
			{
				check(Args.IsCaching() == false);
				//const bool bShowOutlineAsCached = Args.IsCaching() || bWasCachingNeeded == false;
				const FLinearColor DebugTint = bWasCachingNeeded ? FLinearColor::Red : ( bCacheRelativeTransforms ? FLinearColor::Blue : FLinearColor::Green );

				FGeometry ScaledOutline = AllottedGeometry.MakeChild(FVector2D(0, 0), AllottedGeometry.GetLocalSize() * AllottedGeometry.Scale, Inverse(AllottedGeometry.Scale));

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					++OutMaxChildLayer,
					ScaledOutline.ToPaintGeometry(),
					FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
					ESlateDrawEffect::None,
					DebugTint
				);
			}

			static const FName InvalidationPanelName(TEXT("SInvalidationPanel"));

			const FSlateBrush* VolatileBrush = FCoreStyle::Get().GetBrush(TEXT("FocusRectangle"));

			// Draw a yellow outline around any volatile elements.
			const TArray<TSharedPtr<FSlateWindowElementList::FVolatilePaint>>& VolatileElements = CachedWindowElements->GetVolatileElements();
			for ( const TSharedPtr<FSlateWindowElementList::FVolatilePaint>& VolatileElement : VolatileElements )
			{
				// Ignore drawing the volatility rect for child invalidation panels, that's not really important, since
				// they're always volatile and it will make it hard to see when they're invalidated.
				if ( const SWidget* Widget = VolatileElement->GetWidget() )
				{
					if ( Widget->GetType() == InvalidationPanelName )
					{
						continue;
					}
				}

				FGeometry VolatileGeometry = VolatileElement->GetGeometry();
				if (!AbsoluteDeltaPosition.IsZero())
				{
					// Account for relative translation delta
					VolatileGeometry.AppendTransform(FSlateLayoutTransform(AbsoluteDeltaPosition));
				}
				
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					++OutMaxChildLayer,
					VolatileGeometry.ToPaintGeometry(),
					VolatileBrush,
					ESlateDrawEffect::None,
					FLinearColor::Yellow
				);
			}

			// Draw a red flash for any widget that invalidated us recently, we slowly 
			// fade out the flashes over time, unless the widget invalidates us again.
			for ( TMap<TWeakPtr<SWidget>, double>::TIterator It(InvalidatorWidgets); It; ++It )
			{
				TSharedPtr<SWidget> SafeInvalidator = It.Key().Pin();
				if ( SafeInvalidator.IsValid() )
				{
					FWidgetPath WidgetPath;
					if ( FSlateApplication::Get().GeneratePathToWidgetUnchecked(SafeInvalidator.ToSharedRef(), WidgetPath, EVisibility::All) )
					{
						FArrangedWidget ArrangedWidget = WidgetPath.FindArrangedWidget(SafeInvalidator.ToSharedRef()).Get(FArrangedWidget::NullWidget);
						ArrangedWidget.Geometry.AppendTransform( FSlateLayoutTransform(Inverse(Args.GetWindowToDesktopTransform())) );

						FSlateDrawElement::MakeBox(
							OutDrawElements,
							++OutMaxChildLayer,
							ArrangedWidget.Geometry.ToPaintGeometry(),
							FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
							ESlateDrawEffect::None,
							FLinearColor::Red.CopyWithNewOpacity(0.75f * It.Value())
						);
					}

					It.Value() -= FApp::GetDeltaTime();

					if ( It.Value() <= 0 )
					{
						It.RemoveCurrent();
					}
				}
				else
				{
					It.RemoveCurrent();
				}
			}
		}

#endif

		//FPlatformMisc::EndNamedEvent();

		return OutMaxChildLayer;
	}
	else
	{
		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
}

void SInvalidationPanel::SetContent(const TSharedRef< SWidget >& InContent)
{
	InvalidateCache();

	ChildSlot
	[
		InContent
	];
}

bool SInvalidationPanel::ComputeVolatility() const
{
	return true;
}
