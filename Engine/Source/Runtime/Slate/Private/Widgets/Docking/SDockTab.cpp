// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SToolTip.h"
#include "Framework/Docking/SDockingTabStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Docking/SDockingTabWell.h"
#include "Framework/Docking/TabCommands.h"
#include "Framework/Docking/SDockingArea.h"

namespace SDockTabDefs
{
	/** Tab flash rate. Flashes per second */
	static const float TabFlashFrequency = 2.0f;

	/** Tab flash duration. Seconds*/
	static const float TabFlashDuration = 1.0f;

	/** The amount of time to pass before we switch tabs due to drag event */
	static const float DragTimerActivate = 0.75f;

	/** Overrides the tab padding if color overlays are enabled */
	static const float TabVerticalPaddingScaleOverride = 0.85f;
}


static float TotalDraggedDistance = 0;


FReply SDockTab::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( !HasMouseCapture() )
	{
		if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			TotalDraggedDistance = 0;
			ActivateInParent(ETabActivationCause::UserClickedOnTab);

			return FReply::Handled().DetectDrag( SharedThis(this), EKeys::LeftMouseButton );
		}
		else if ( MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton )
		{
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		else if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
		{
			// We clicked on the tab, so it should be active
			ActivateInParent(ETabActivationCause::UserClickedOnTab);
			// ... but let the tab well bring up the context menu or whatever it wants to do with the right click.
			return FReply::Unhandled();
		}
	}

	return FReply::Unhandled();
}

FReply SDockTab::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton )
	{
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SDockTab::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent  )
{
	// Need to remember where within a tab we grabbed
	const FVector2D TabGrabOffset = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
	const FVector2D TabSize = MyGeometry.GetLocalSize();
	const FVector2D TabGrabOffsetFraction = FVector2D(
		FMath::Clamp(TabGrabOffset.X / TabSize.X, 0.0f, 1.0f),
		FMath::Clamp(TabGrabOffset.Y / TabSize.Y, 0.0f, 1.0f) );
	
	auto PinnedParent = ParentPtr.Pin();
	if (PinnedParent.IsValid())
	{
		//See if we can drag tabs contain in this manager
		if (GetTabManager()->GetCanDoDragOperation())
		{
			return PinnedParent->StartDraggingTab(SharedThis(this), TabGrabOffsetFraction, MouseEvent);
		}
		else
		{
			return FReply::Handled();
		}
	}
	else
	{
		// Should never get here (but sometimes does, under unknown circumstances)
		// TODO: investigate how the parent pointer can become invalid
		return FReply::Unhandled();
	}
}

FReply SDockTab::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( HasMouseCapture() )
	{
		if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		else if ( MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton  )
		{
			if ( MyGeometry.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
			{
				RequestCloseTab();
			}
			
			return FReply::Handled().ReleaseMouseCapture();
		}
	}
	return FReply::Unhandled();
}

void SDockTab::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// Register to activate the tab after a delay
	if ( !ActiveTimerHandle.IsValid() )
	{
		ActiveTimerHandle = RegisterActiveTimer( SDockTabDefs::DragTimerActivate, FWidgetActiveTimerDelegate::CreateSP( this, &SDockTab::TriggerActivateTab ) );
	}

	SBorder::OnDragEnter( MyGeometry, DragDropEvent );
}

void SDockTab::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	// Unregister the activation timer if it hasn't fired yet
	if ( ActiveTimerHandle.IsValid() )
	{
		UnRegisterActiveTimer( ActiveTimerHandle.Pin().ToSharedRef() );
	}

	SBorder::OnDragLeave( DragDropEvent );
}

FReply SDockTab::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// Unregister the activation timer if it hasn't fired yet
	if ( ActiveTimerHandle.IsValid() )
	{
		UnRegisterActiveTimer( ActiveTimerHandle.Pin().ToSharedRef() );
	}

	return SBorder::OnDrop( MyGeometry, DragDropEvent );
}

FReply SDockTab::OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	if (!this->HasMouseCapture())
	{
		// We tapped on the tab, so it should be active
		ActivateInParent(ETabActivationCause::UserClickedOnTab);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply SDockTab::OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	if (this->HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

void SDockTab::SetContent( TSharedRef<SWidget> InContent )
{
	Content = InContent;
	if (ParentPtr.IsValid())
	{
		// This is critical to do, otherwise the content might remain if currently active even if expected to be destroyed
		ParentPtr.Pin()->RefreshParentContent();
	}
}

void SDockTab::SetLeftContent( TSharedRef<SWidget> InContent )
{
	this->TabWellContentLeft = InContent;
	if (ParentPtr.IsValid())
	{
		// This is critical to do, otherwise the content might remain if currently active even if expected to be destroyed
		ParentPtr.Pin()->RefreshParentContent();
	}
}

void SDockTab::SetRightContent( TSharedRef<SWidget> InContent )
{
	this->TabWellContentRight = InContent;
	if (ParentPtr.IsValid())
	{
		// This is critical to do, otherwise the content might remain if currently active even if expected to be destroyed
		ParentPtr.Pin()->RefreshParentContent();
	}
}

void SDockTab::SetBackgroundContent(TSharedRef<SWidget> InContent)
{
	this->TabWellContentBackground = InContent;
	if (ParentPtr.IsValid())
	{
		ParentPtr.Pin()->RefreshParentContent();
	}
}


bool SDockTab::IsActive() const
{
	return FGlobalTabmanager::Get()->GetActiveTab() == SharedThis(this);
}

bool SDockTab::IsForeground() const
{
	return ParentPtr.IsValid() ? (ParentPtr.Pin()->GetForegroundTab() == SharedThis(this)) : true;
}

ETabRole SDockTab::GetTabRole() const
{
	return TabRole;
}

ETabRole SDockTab::GetVisualTabRole() const
{
	// If the tab role is NomadTab but is being visualized as a major tab
	if (this->TabRole == ETabRole::NomadTab)
	{
		bool bNomadMajorStyle = false;

		if (DraggedOverDockingArea.IsValid())
		{
			bNomadMajorStyle = DraggedOverDockingArea->GetTabManager() == FGlobalTabmanager::Get();
		}
		else if (GetParent().IsValid() && GetParent()->GetDockArea().IsValid())
		{
			bNomadMajorStyle = GetParent()->GetDockArea()->GetTabManager() == FGlobalTabmanager::Get();
		}
		else
		{
			// We are dragging or have no parent, but we are not dragging over anything, assume major
			bNomadMajorStyle = true;
		}

		if (bNomadMajorStyle)
		{
			return ETabRole::MajorTab;
		}
	}

	return GetTabRole();
}

const FSlateBrush* SDockTab::GetContentAreaBrush() const
{
	return &GetCurrentStyle().ContentAreaBrush;
}

const FSlateBrush* SDockTab::GetTabWellBrush() const
{
	return &GetCurrentStyle().TabWellBrush;
}

TSharedRef<SWidget> SDockTab::GetContent()
{
	return Content;
}

TSharedRef<SWidget> SDockTab::GetLeftContent()
{
	return TabWellContentLeft;
}

TSharedRef<SWidget> SDockTab::GetRightContent()
{
	return TabWellContentRight;
}

TSharedRef<SWidget> SDockTab::GetBackgrounfContent()
{
	return TabWellContentBackground;
}

FMargin SDockTab::GetContentPadding() const
{
	return ContentAreaPadding.Get();
}

void SDockTab::SetLayoutIdentifier( const FTabId& TabId )
{
	LayoutIdentifier = TabId;
}

const FTabId& SDockTab::GetLayoutIdentifier() const
{
	return LayoutIdentifier;
}

void SDockTab::SetParent(TSharedPtr<SDockingTabWell> Parent)
{
	ParentPtr = Parent;
}

TSharedPtr<SDockingTabWell> SDockTab::GetParent() const
{
	return ParentPtr.IsValid() ? ParentPtr.Pin() : TSharedPtr<SDockingTabWell>();
}

TSharedPtr<SDockingTabStack> SDockTab::GetParentDockTabStack() const
{
	TSharedPtr<SDockingTabWell> ParentTabWell = GetParent();
	if( ParentTabWell.IsValid() )
	{
		return ParentTabWell->GetParentDockTabStack();
	}
	return NULL;
}

void SDockTab::RemoveTabFromParent()
{
	OnTabClosed.ExecuteIfBound(SharedThis(this));
	if (ParentPtr.IsValid())
	{
		ParentPtr.Pin()->RemoveAndDestroyTab(SharedThis(this), SDockingNode::TabRemoval_Closed);
	}
}

TSharedPtr<SDockingArea> SDockTab::GetDockArea() const
{
	return ParentPtr.IsValid() ? ParentPtr.Pin()->GetDockArea() : TSharedPtr<SDockingArea>();
}

TSharedPtr<SWindow> SDockTab::GetParentWindow() const
{
	TSharedPtr<SDockingArea> DockingAreaPtr = this->GetDockArea();
	return DockingAreaPtr.IsValid() ? DockingAreaPtr->GetParentWindow() : TSharedPtr<SWindow>();
}

SDockTab::SDockTab()
	: Content(SNew(SSpacer))
	, TabWellContentLeft(SNullWidget::NullWidget)
	, TabWellContentRight(SNullWidget::NullWidget)
	, TabWellContentBackground(SNullWidget::NullWidget)
	, LayoutIdentifier(NAME_None)
	, TabRole(ETabRole::PanelTab)
	, ParentPtr()
	, TabLabel(NSLOCTEXT("DockTab", "DefaultTabTitle", "UNNAMED"))
	, OnTabClosed()
	, OnTabActivated()
	, OnCanCloseTab()
	, ContentAreaPadding( 2 )
	, bShouldAutosize(false)
	, TabColorScale(FLinearColor::Transparent)
	, LastActivationTime(0.0)
{
}

void SDockTab::ActivateInParent(ETabActivationCause InActivationCause)
{
	TSharedPtr<SDockingTabWell> ParentTabWell = ParentPtr.Pin();
	if (ParentTabWell.IsValid())
	{
		ParentTabWell->BringTabToFront(SharedThis(this));
	}

	OnTabActivated.ExecuteIfBound(SharedThis(this), InActivationCause);
}

void SDockTab::SetTabManager( const TSharedPtr<FTabManager>& InTabManager)
{
	MyTabManager = InTabManager;
}

void SDockTab::SetOnPersistVisualState( const FOnPersistVisualState& Handler )
{
	OnPersistVisualState = Handler;
}

void SDockTab::SetCanCloseTab( const FCanCloseTab& InOnTabClosing )
{
	OnCanCloseTab = InOnTabClosing;
}

void SDockTab::SetOnTabClosed( const FOnTabClosedCallback& InDelegate )
{
	OnTabClosed = InDelegate;
}

void SDockTab::SetOnTabActivated( const FOnTabActivatedCallback& InDelegate )
{
	OnTabActivated = InDelegate;
}

TSharedRef<FTabManager> SDockTab::GetTabManager() const
{
	return MyTabManager.Pin().ToSharedRef();
}

void SDockTab::DrawAttention()
{
	GetTabManager()->DrawAttention(SharedThis(this));
}

void SDockTab::ProvideDefaultLabel( const FText& InDefaultLabel )
{
	const bool UserProvidedLabel = TabLabel.IsBound() || !TabLabel.Get().IsEmpty();
	if ( !UserProvidedLabel )
	{
		TabLabel = InDefaultLabel;
	}
}

void SDockTab::ProvideDefaultIcon( const FSlateBrush* InDefaultIcon )
{
	const bool UserProvidedIcon = TabIcon.IsBound() || (TabIcon.Get() && TabIcon.Get() != FStyleDefaults::GetNoBrush());
	if( !UserProvidedIcon )
	{
		TabIcon = InDefaultIcon;
	}
}

void SDockTab::PlaySpawnAnim()
{
	SpawnAnimCurve.Play( this->AsShared() );
}

void SDockTab::FlashTab()
{
	FlashTabCurve = FCurveSequence(0, SDockTabDefs::TabFlashDuration, ECurveEaseFunction::Linear);
	FlashTabCurve.Play( this->AsShared() );
}

float SDockTab::GetFlashValue() const
{
	if(FlashTabCurve.IsPlaying())
	{
		const float Lerp = FlashTabCurve.GetLerp();

		const float SinRateMultiplier = 2.0f * PI * SDockTabDefs::TabFlashDuration * SDockTabDefs::TabFlashFrequency;
		const float SinTerm = 0.5f * (FMath::Sin(Lerp * SinRateMultiplier) + 1.0f);

		const float FadeTerm = 1.0f - Lerp;

		return SinTerm * FadeTerm;
	}

	return 0.0f;
}

void SDockTab::SetDraggedOverDockArea( const TSharedPtr<SDockingArea>& Area )
{
	DraggedOverDockingArea = Area;
}

bool SDockTab::HasSiblingTab(const FTabId& SiblingTabId, const bool TreatIndexNoneAsWildcard) const
{
	TSharedPtr<SDockingTabStack> ParentTabStack = GetParentDockTabStack();
	return (ParentTabStack.IsValid()) ? ParentTabStack->HasTab(FTabMatcher(SiblingTabId, static_cast<ETabState::Type>(ETabState::ClosedTab | ETabState::OpenedTab), TreatIndexNoneAsWildcard)) : false;
}

void SDockTab::Construct( const FArguments& InArgs )
{
	SpawnAnimCurve = FCurveSequence(0, 0.15f);
	SpawnAnimCurve.JumpToEnd();

	// We are just holding on to the content via a referece; not actually presenting it.
	this->Content = InArgs._Content.Widget;
	this->TabWellContentLeft = InArgs._TabWellContentLeft.Widget;
	this->TabWellContentRight = InArgs._TabWellContentRight.Widget;
	this->TabRole = InArgs._TabRole;
	this->OnTabClosed = InArgs._OnTabClosed;
	this->OnCanCloseTab = InArgs._OnCanCloseTab;
	this->OnPersistVisualState = InArgs._OnPersistVisualState;
	this->TabLabel = InArgs._Label;
	this->TabIcon = InArgs._Icon;
	this->bShouldAutosize = InArgs._ShouldAutosize;
	this->TabColorScale = InArgs._TabColorScale;

	MajorTabStyle = &FCoreStyle::Get().GetWidgetStyle<FDockTabStyle>("Docking.MajorTab");
	GenericTabStyle = &FCoreStyle::Get().GetWidgetStyle<FDockTabStyle>("Docking.Tab");

	ContentAreaPadding = InArgs._ContentPadding;

	const FButtonStyle* const CloseButtonStyle = &GetCurrentStyle().CloseButtonStyle;

	SBorder::Construct( SBorder::FArguments()
		.BorderImage( FStyleDefaults::GetNoBrush() )
		.ContentScale( this, &SDockTab::GetAnimatedScale )
		.VAlign(VAlign_Bottom)
		.Padding( 0.0f )
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image( this, &SDockTab::GetImageBrush )
			]

			// Overlay for active tab indication.
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				// Don't allow active tab overlay to absorb mouse clicks
				.Visibility( EVisibility::HitTestInvisible )
				.Padding( this, &SDockTab::GetTabPadding )
				.BorderImage( this, &SDockTab::GetActiveTabOverlayImageBrush )
			]

			// Overlay for flashing a tab for attention
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				// Don't allow flasher tab overlay to absorb mouse clicks
				.Visibility( EVisibility::HitTestInvisible )
				.Padding( this, &SDockTab::GetTabPadding )
				.BorderImage( this, &SDockTab::GetFlashOverlayImageBrush )
				.BorderBackgroundColor( this, &SDockTab::GetFlashColor )
			]

			+ SOverlay::Slot()
			.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SDockTab::GetTabPadding)))
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(EVisibility::Visible)
				.ToolTip(InArgs._ToolTip)
				.ToolTipText(InArgs._ToolTipText.IsSet() ? InArgs._ToolTipText : TAttribute<FText>(this, &SDockTab::GetTabLabel))

				// Tab Icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 5, 0)
				[
					SNew(SBorder)
					// Don't allow active tab overlay to absorb mouse clicks
					.Padding(1.0f)
					.Visibility(EVisibility::HitTestInvisible)
					// Overlay for color-coded tab effect
					.BorderImage(this, &SDockTab::GetColorOverlayImageBrush)
					.BorderBackgroundColor(this, &SDockTab::GetTabColor)
					[
						SAssignNew(IconWidget, SImage)
						.Image(this, &SDockTab::GetTabIcon)
					]
				]
				// Tab Label
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.Padding( 0.0f, 1.0f )
				.VAlign(VAlign_Center)
				[
					SAssignNew(LabelWidget, STextBlock)
					.TextStyle( FCoreStyle::Get(), "Docking.TabFont" )					
					.Text( this, &SDockTab::GetTabLabel )
				]
				
				// @todo toolkit major: Could inject inline content here into tab for standalone asset editing dropdown/dirty state, etc.

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle( CloseButtonStyle )
					.OnClicked( this, &SDockTab::OnCloseButtonClicked )
					.ContentPadding( 0 )
					.ToolTipText(this, &SDockTab::GetCloseButtonToolTipText)
					.Visibility(this, &SDockTab::HandleIsCloseButtonVisible)
					[
						SNew(SSpacer)
						.Size( CloseButtonStyle->Normal.ImageSize )
					]
				]
			]
		]
	);
}

EActiveTimerReturnType SDockTab::TriggerActivateTab( double InCurrentTime, float InDeltaTime )
{
	ActivateInParent( ETabActivationCause::UserClickedOnTab );
	return EActiveTimerReturnType::Stop;
}

const FDockTabStyle& SDockTab::GetCurrentStyle() const
{
	if ( GetVisualTabRole() == ETabRole::MajorTab )
	{
		return *MajorTabStyle;
	}
	
	return *GenericTabStyle;
}

const FSlateBrush* SDockTab::GetImageBrush() const
{
	const FDockTabStyle& CurrentStyle = GetCurrentStyle();

	// Pick the right brush based on whether the tab is active or hovered.
	if ( this->IsForeground() )
	{
		return &CurrentStyle.ForegroundBrush;
	}
	else if ( this->IsHovered() )
	{
		return &CurrentStyle.HoveredBrush;
	}
	return &CurrentStyle.NormalBrush;
}


FMargin SDockTab::GetTabPadding() const
{
	FMargin NewPadding = GetCurrentStyle().TabPadding;
	if (TabIcon.Get() != FStyleDefaults::GetNoBrush())
	{
		NewPadding.Top *= SDockTabDefs::TabVerticalPaddingScaleOverride;
		NewPadding.Bottom *= SDockTabDefs::TabVerticalPaddingScaleOverride;
	}
	return NewPadding;
}

const FSlateBrush* SDockTab::GetColorOverlayImageBrush() const
{
	if (this->TabColorScale.Get().A > 0.0f)
	{
		return &GetCurrentStyle().ColorOverlayIconBrush;
	}
	return FStyleDefaults::GetNoBrush();
}


const FSlateBrush* SDockTab::GetActiveTabOverlayImageBrush() const
{
	if (this->IsActive())
	{
		return &GetCurrentStyle().ActiveBrush;
	}
	return FStyleDefaults::GetNoBrush();
}


FSlateColor SDockTab::GetTabColor() const
{
	return TabColorScale.Get();
}


const FSlateBrush* SDockTab::GetFlashOverlayImageBrush() const
{
	if (FlashTabCurve.IsPlaying())
	{
		// Flashing is really just applying a color overlay, so we can re-use the color overlay brush and apply our flash tint to it
		return &GetCurrentStyle().ColorOverlayTabBrush;
	}
	return FStyleDefaults::GetNoBrush();
}


FSlateColor SDockTab::GetFlashColor() const
{
	if ( GetCurrentStyle().FlashColor.IsColorSpecified() )
	{
		FLinearColor Color = GetCurrentStyle().FlashColor.GetSpecifiedColor();
		Color.A = GetFlashValue();

		return FSlateColor(Color);
	}

	return FLinearColor::White;
}


float SDockTab::GetOverlapWidth() const
{
	return GetCurrentStyle().OverlapWidth;
}

FText SDockTab::GetTabLabel() const
{
	return TabLabel.Get();
}

void SDockTab::SetLabel( const TAttribute<FText>& InTabLabel )
{
	TabLabel = InTabLabel;
}

const FSlateBrush* SDockTab::GetTabIcon() const
{
	return TabIcon.Get();
}

void SDockTab::SetTabToolTipWidget(TSharedPtr<SToolTip> InTabToolTipWidget)
{
	IconWidget->SetToolTip(InTabToolTipWidget);
	LabelWidget->SetToolTip(InTabToolTipWidget);	
}

void SDockTab::SetTabIcon( const TAttribute<const FSlateBrush*> InTabIcon )
{
	TabIcon = InTabIcon;
}


bool SDockTab::ShouldAutosize() const
{
	return bShouldAutosize;
}

FReply SDockTab::OnCloseButtonClicked()
{
	RequestCloseTab();

	return FReply::Handled();
}

FText SDockTab::GetCloseButtonToolTipText() const
{
	TSharedPtr<FUICommandInfo> CloseCommand =
		GetVisualTabRole() == ETabRole::MajorTab ? FTabCommands::Get().CloseMajorTab : FTabCommands::Get().CloseMinorTab;

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Label"), CloseCommand->GetLabel());

	FText InputText = CloseCommand->GetInputText();
	if (InputText.IsEmptyOrWhitespace())
	{
		Arguments.Add(TEXT("InputText"), InputText);
	}
	else
	{
		Arguments.Add(TEXT("InputText"), FText::Format(NSLOCTEXT("DockTab", "CloseButtonInputText", " ({0})"), InputText));
	}

	return FText::Format(NSLOCTEXT("DockTab", "CloseButtonToolTip", "{Label}{InputText}"), Arguments);
}

EVisibility SDockTab::HandleIsCloseButtonVisible() const
{
	return MyTabManager.Pin()->IsTabCloseable(SharedThis(this)) ? EVisibility::Visible : EVisibility::Hidden;
}

bool SDockTab::CanCloseTab() const
{
	const bool bCanCloseTabNow = MyTabManager.Pin()->IsTabCloseable(SharedThis(this)) && (!OnCanCloseTab.IsBound() || OnCanCloseTab.Execute());
	return bCanCloseTabNow;
}

bool SDockTab::RequestCloseTab()
{	
	this->PersistVisualState();
	// The tab can be closed if the delegate is not bound or if the delegate call indicates we cannot close it
	const bool bCanCloseTabNow = CanCloseTab();
	if( bCanCloseTabNow )
	{
		RemoveTabFromParent();
	}
	return bCanCloseTabNow;
}

void SDockTab::PersistVisualState()
{
	OnPersistVisualState.ExecuteIfBound();
}


FVector2D SDockTab::GetAnimatedScale() const
{
	static FVector2D FullyOpen = FVector2D::UnitVector;
	static FVector2D FullyClosed = FVector2D(1.0f, 0.0f);
	return FMath::Lerp(FullyClosed, FullyOpen, SpawnAnimCurve.GetLerp());
}

void SDockTab::UpdateActivationTime()
{
	LastActivationTime = FSlateApplication::Get().GetCurrentTime();
}
