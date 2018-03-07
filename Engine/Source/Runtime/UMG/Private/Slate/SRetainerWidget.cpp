// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Slate/SRetainerWidget.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "Layout/WidgetCaching.h"


DECLARE_CYCLE_STAT(TEXT("Retainer Widget Tick"), STAT_SlateRetainerWidgetTick, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Retainer Widget Paint"), STAT_SlateRetainerWidgetPaint, STATGROUP_Slate);

#if !UE_BUILD_SHIPPING

/** True if we should allow widgets to be cached in the UI at all. */
TAutoConsoleVariable<int32> EnableRetainedRendering(
	TEXT("Slate.EnableRetainedRendering"),
	true,
	TEXT("Whether to attempt to render things in SRetainerWidgets to render targets first."));

static bool IsRetainedRenderingEnabled()
{
	return EnableRetainedRendering.GetValueOnGameThread() == 1;
}

FOnRetainedModeChanged SRetainerWidget::OnRetainerModeChangedDelegate;
#else

static bool IsRetainedRenderingEnabled()
{
	return true;
}

#endif


//---------------------------------------------------

SRetainerWidget::SRetainerWidget()
	: CachedWindowToDesktopTransform(0, 0)
	, DynamicEffect(nullptr)
{
}

SRetainerWidget::~SRetainerWidget()
{
	if( FSlateApplication::IsInitialized() )
	{
#if !UE_BUILD_SHIPPING
		OnRetainerModeChangedDelegate.RemoveAll( this );
#endif
	}
}

void SRetainerWidget::UpdateWidgetRenderer()
{
	// We can't write out linear.  If we write out linear, then we end up with premultiplied alpha
	// in linear space, which blending with gamma space later is difficult...impossible? to get right
	// since the rest of slate does blending in gamma space.
	const bool bWriteContentInGammaSpace = true;

	if (!WidgetRenderer.IsValid())
	{
		WidgetRenderer = MakeShareable(new FWidgetRenderer(bWriteContentInGammaSpace));
	}

	WidgetRenderer->SetUseGammaCorrection(bWriteContentInGammaSpace);
	WidgetRenderer->SetIsPrepassNeeded(false);
	WidgetRenderer->SetClearHitTestGrid(false);

	// Update the render target to match the current gamma rendering preferences.
	if (RenderTarget && RenderTarget->SRGB != !bWriteContentInGammaSpace)
	{
		// Note, we do the opposite here of whatever write is, if we we're writing out gamma,
		// then sRGB writes were not supported, so it won't be an sRGB texture.
		RenderTarget->TargetGamma = !bWriteContentInGammaSpace ? 0.0f : 1.0;
		RenderTarget->SRGB = !bWriteContentInGammaSpace;

		RenderTarget->UpdateResource();
	}
}

void SRetainerWidget::Construct(const FArguments& InArgs)
{
	STAT(MyStatId = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_Slate>(InArgs._StatId);)

	RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->ClearColor = FLinearColor::Transparent;

	SurfaceBrush.SetResourceObject(RenderTarget);

	Window = SNew(SVirtualWindow);
	Window->SetShouldResolveDeferred(false);
	
	UpdateWidgetRenderer();

	MyWidget = InArgs._Content.Widget;

	RenderOnPhase = InArgs._RenderOnPhase;
	RenderOnInvalidation = InArgs._RenderOnInvalidation;

	Phase = InArgs._Phase;
	PhaseCount = InArgs._PhaseCount;

	LastDrawTime = FApp::GetCurrentTime();
	LastTickedFrame = 0;

	bEnableRetainedRenderingDesire = true;
	bEnableRetainedRendering = false;

	bRenderRequested = true;

	RootCacheNode = nullptr;
	LastUsedCachedNodeIndex = 0;

	Window->SetContent(MyWidget.ToSharedRef());

	ChildSlot
	[
		Window.ToSharedRef()
	];

	if ( FSlateApplication::IsInitialized() )
	{
#if !UE_BUILD_SHIPPING
		OnRetainerModeChangedDelegate.AddRaw(this, &SRetainerWidget::OnRetainerModeChanged);

		static bool bStaticInit = false;

		if ( !bStaticInit )
		{
			bStaticInit = true;
			EnableRetainedRendering.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&SRetainerWidget::OnRetainerModeCVarChanged));
		}
#endif
	}
}

bool SRetainerWidget::ShouldBeRenderingOffscreen() const
{
	return bEnableRetainedRenderingDesire && IsRetainedRenderingEnabled();
}

bool SRetainerWidget::IsAnythingVisibleToRender() const
{
	return MyWidget.IsValid() && MyWidget->GetVisibility().IsVisible();
}

void SRetainerWidget::OnRetainerModeChanged()
{
	RefreshRenderingMode();
	Invalidate(EInvalidateWidget::Layout);
}

#if !UE_BUILD_SHIPPING

void SRetainerWidget::OnRetainerModeCVarChanged( IConsoleVariable* CVar )
{
	OnRetainerModeChangedDelegate.Broadcast();
}

#endif

void SRetainerWidget::SetRetainedRendering(bool bRetainRendering)
{
	bEnableRetainedRenderingDesire = bRetainRendering;
}

void SRetainerWidget::RefreshRenderingMode()
{
	const bool bShouldBeRenderingOffscreen = ShouldBeRenderingOffscreen();

	if ( bEnableRetainedRendering != bShouldBeRenderingOffscreen )
	{
		bEnableRetainedRendering = bShouldBeRenderingOffscreen;

		Window->SetContent(MyWidget.ToSharedRef());
	}
}

void SRetainerWidget::SetContent(const TSharedRef< SWidget >& InContent)
{
	MyWidget = InContent;
	Window->SetContent(InContent);
}

UMaterialInstanceDynamic* SRetainerWidget::GetEffectMaterial() const
{
	return DynamicEffect;
}

void SRetainerWidget::SetEffectMaterial(UMaterialInterface* EffectMaterial)
{
	if ( EffectMaterial )
	{
		DynamicEffect = Cast<UMaterialInstanceDynamic>(EffectMaterial);
		if ( !DynamicEffect )
		{
			DynamicEffect = UMaterialInstanceDynamic::Create(EffectMaterial, GetTransientPackage());
		}

		SurfaceBrush.SetResourceObject(DynamicEffect);
	}
	else
	{
		DynamicEffect = nullptr;
		SurfaceBrush.SetResourceObject(RenderTarget);
	}

	UpdateWidgetRenderer();
}

void SRetainerWidget::SetTextureParameter(FName TextureParameter)
{
	DynamicEffectTextureParameter = TextureParameter;
}
 
void SRetainerWidget::SetWorld(UWorld* World)
{
	OuterWorld = World;
}

void SRetainerWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(RenderTarget);
	Collector.AddReferencedObject(DynamicEffect);
}

FChildren* SRetainerWidget::GetChildren()
{
	if ( bEnableRetainedRendering )
	{
		return &EmptyChildSlot;
	}
	else
	{
		return SCompoundWidget::GetChildren();
	}
}

bool SRetainerWidget::ComputeVolatility() const
{
	return true;
}

FCachedWidgetNode* SRetainerWidget::CreateCacheNode() const
{
	// If the node pool is empty, allocate a few
	if (LastUsedCachedNodeIndex >= NodePool.Num())
	{
		for (int32 i = 0; i < 10; i++)
		{
			NodePool.Add(new FCachedWidgetNode());
		}
	}

	// Return one of the preallocated nodes and increment the next node index.
	FCachedWidgetNode* NewNode = NodePool[LastUsedCachedNodeIndex];
	++LastUsedCachedNodeIndex;

	return NewNode;
}

void SRetainerWidget::InvalidateWidget(SWidget* InvalidateWidget)
{
	if (RenderOnInvalidation)
	{
		bRenderRequested = true;
	}
}

void SRetainerWidget::RequestRender()
{
	bRenderRequested = true;
}

bool SRetainerWidget::PaintRetainedContent(const FPaintArgs& Args)
{
	// In order to get material parameter collections to function properly, we need the current world's Scene
	// properly propagated through to any widgets that depend on that functionality. The SceneViewport and RetainerWidget the 
	// only location where this information exists in Slate, so we push the current scene onto the current
	// Slate application so that we can leverage it in later calls.
	UWorld* TickWorld = OuterWorld.Get();
	if (TickWorld && TickWorld->Scene && IsInGameThread())
	{
		FSlateApplication::Get().GetRenderer()->RegisterCurrentScene(TickWorld->Scene);
	}
	else if (IsInGameThread())
	{
		FSlateApplication::Get().GetRenderer()->RegisterCurrentScene(nullptr);
	}

	if (RenderOnPhase)
	{
		if (LastTickedFrame != GFrameCounter && (GFrameCounter % PhaseCount) == Phase)
		{
			bRenderRequested = true;
		}
	}

	SCOPE_CYCLE_COUNTER( STAT_SlateRetainerWidgetTick );
	if ( bRenderRequested )
	{
		LastTickedFrame = GFrameCounter;
		const double TimeSinceLastDraw = FApp::GetCurrentTime() - LastDrawTime;

		FPaintGeometry PaintGeometry = CachedAllottedGeometry.ToPaintGeometry();
		FVector2D RenderSize = PaintGeometry.GetLocalSize() * PaintGeometry.GetAccumulatedRenderTransform().GetMatrix().GetScale().GetVector();

		const uint32 RenderTargetWidth  = FMath::RoundToInt(RenderSize.X);
		const uint32 RenderTargetHeight = FMath::RoundToInt(RenderSize.Y);

		const FVector2D ViewOffset = PaintGeometry.DrawPosition.RoundToVector();

		// Keep the visibilities the same, the proxy window should maintain the same visible/non-visible hit-testing of the retainer.
		Window->SetVisibility(GetVisibility());

		// Need to prepass.
		Window->SlatePrepass(CachedAllottedGeometry.Scale);

		// Reset the cached node pool index so that we effectively reset the pool.
		LastUsedCachedNodeIndex = 0;
		RootCacheNode = nullptr;

		if ( RenderTargetWidth != 0 && RenderTargetHeight != 0 )
		{
			if ( MyWidget->GetVisibility().IsVisible() )
			{
				if ( RenderTarget->GetSurfaceWidth() != RenderTargetWidth ||
					 RenderTarget->GetSurfaceHeight() != RenderTargetHeight )
				{
					const bool bForceLinearGamma = false;
					RenderTarget->InitCustomFormat(RenderTargetWidth, RenderTargetHeight, PF_B8G8R8A8, bForceLinearGamma);
				}

				const float Scale = CachedAllottedGeometry.Scale;

				const FVector2D DrawSize = FVector2D(RenderTargetWidth, RenderTargetHeight);
				const FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize * ( 1 / Scale ), FSlateLayoutTransform(Scale, PaintGeometry.DrawPosition));

				// Update the surface brush to match the latest size.
				SurfaceBrush.ImageSize = DrawSize;

				WidgetRenderer->ViewOffset = -ViewOffset;

				SRetainerWidget* MutableThis = const_cast<SRetainerWidget*>(this);
				TSharedRef<SRetainerWidget> SharedMutableThis = SharedThis(MutableThis);
				
				//FPaintArgs PaintArgs(Window.ToSharedRef().Get(), Args.GetGrid(), Args.GetWindowToDesktopTransform(), FApp::GetCurrentTime(), Args.GetDeltaTime());
				FPaintArgs PaintArgs(*this, Args.GetGrid(), Args.GetWindowToDesktopTransform(), FApp::GetCurrentTime(), Args.GetDeltaTime());

				RootCacheNode = CreateCacheNode();
				RootCacheNode->Initialize(Args, SharedMutableThis, WindowGeometry);

				WidgetRenderer->DrawWindow(
					PaintArgs.EnableCaching(SharedMutableThis, RootCacheNode, true, true),
					RenderTarget,
					Window.ToSharedRef(),
					WindowGeometry,
					WindowGeometry.GetLayoutBoundingRect(),
					TimeSinceLastDraw);

				bRenderRequested = false;

				LastDrawTime = FApp::GetCurrentTime();

				return true;
			}
		}
	}

	return false;
}

int32 SRetainerWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	STAT(FScopeCycleCounter PaintCycleCounter(MyStatId);)

	SRetainerWidget* MutableThis = const_cast<SRetainerWidget*>( this );

	MutableThis->RefreshRenderingMode();

	if ( bEnableRetainedRendering && IsAnythingVisibleToRender() )
	{
		SCOPE_CYCLE_COUNTER( STAT_SlateRetainerWidgetPaint );
		CachedAllottedGeometry = AllottedGeometry;
		CachedWindowToDesktopTransform = Args.GetWindowToDesktopTransform();

		TSharedRef<SRetainerWidget> SharedMutableThis = SharedThis(MutableThis);

		const bool bNewFramePainted = MutableThis->PaintRetainedContent(Args);

		if ( RenderTarget->GetSurfaceWidth() >= 1 && RenderTarget->GetSurfaceHeight() >= 1 )
		{
			const FLinearColor ComputedColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get() * SurfaceBrush.GetTint(InWidgetStyle));
			// Retainer widget uses pre-multiplied alpha, so pre-multiply the color by the alpha to respect opacity.
			const FLinearColor PremultipliedColorAndOpacity(ComputedColorAndOpacity * ComputedColorAndOpacity.A);

			const bool bDynamicMaterialInUse = (DynamicEffect != nullptr);
			if (bDynamicMaterialInUse)
			{
				DynamicEffect->SetTextureParameterValue(DynamicEffectTextureParameter, RenderTarget);
			}

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				&SurfaceBrush,
				// We always write out the content in gamma space, so when we render the final version we need to
				// render without gamma correction enabled.
				ESlateDrawEffect::PreMultipliedAlpha | ESlateDrawEffect::NoGamma,
				FLinearColor(PremultipliedColorAndOpacity.R, PremultipliedColorAndOpacity.G, PremultipliedColorAndOpacity.B, PremultipliedColorAndOpacity.A)
			);
			
			if (RootCacheNode)
			{
				RootCacheNode->RecordHittestGeometry(Args.GetGrid(), Args.GetLastHitTestIndex(), LayerId, FVector2D(0, 0));
			}

			// Any deferred painted elements of the retainer should be drawn directly by the main renderer, not rendered into the render target,
			// as most of those sorts of things will break the rendering rect, things like tooltips, and popup menus.
			for ( auto& DeferredPaint : WidgetRenderer->DeferredPaints )
			{
				OutDrawElements.QueueDeferredPainting(DeferredPaint->Copy(Args));
			}
		}

		return LayerId;
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

FVector2D SRetainerWidget::ComputeDesiredSize(float LayoutScaleMuliplier) const
{
	if ( bEnableRetainedRendering )
	{
		return MyWidget->GetDesiredSize();
	}
	else
	{
		return SCompoundWidget::ComputeDesiredSize(LayoutScaleMuliplier);
	}
}
