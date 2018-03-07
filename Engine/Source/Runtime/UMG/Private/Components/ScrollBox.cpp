// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ScrollBox.h"
#include "Containers/Ticker.h"
#include "Components/ScrollBoxSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UScrollBox

UScrollBox::UScrollBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Orientation(Orient_Vertical)
	, ScrollBarVisibility(ESlateVisibility::Visible)
	, ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
	, ScrollbarThickness(5.0f, 5.0f)
	, AlwaysShowScrollbar(false)
	, AllowOverscroll(true)
	, NavigationDestination(EDescendantScrollDestination::IntoView)
	, NavigationScrollPadding(0.0f)
{
	bIsVariable = false;

	SScrollBox::FArguments Defaults;
	Visibility = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());
	Clipping = EWidgetClipping::ClipToBounds;

	WidgetStyle = *Defaults._Style;
	WidgetBarStyle = *Defaults._ScrollBarStyle;
	bAllowRightClickDragScrolling = true;
}

void UScrollBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyScrollBox.Reset();
}

UClass* UScrollBox::GetSlotClass() const
{
	return UScrollBoxSlot::StaticClass();
}

void UScrollBox::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live canvas if it already exists
	if ( MyScrollBox.IsValid() )
	{
		CastChecked<UScrollBoxSlot>(InSlot)->BuildSlot(MyScrollBox.ToSharedRef());
	}
}

void UScrollBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyScrollBox.IsValid() )
	{
		TSharedPtr<SWidget> Widget = InSlot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyScrollBox->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

TSharedRef<SWidget> UScrollBox::RebuildWidget()
{
	MyScrollBox = SNew(SScrollBox)
		.Style(&WidgetStyle)
		.ScrollBarStyle(&WidgetBarStyle)
		.Orientation(Orientation)
		.ConsumeMouseWheel(ConsumeMouseWheel)
		.NavigationDestination(NavigationDestination)
		.NavigationScrollPadding(NavigationScrollPadding)
		.OnUserScrolled(BIND_UOBJECT_DELEGATE(FOnUserScrolled, SlateHandleUserScrolled));

	for ( UPanelSlot* PanelSlot : Slots )
	{
		if ( UScrollBoxSlot* TypedSlot = Cast<UScrollBoxSlot>(PanelSlot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyScrollBox.ToSharedRef());
		}
	}
	
	return MyScrollBox.ToSharedRef();
}

void UScrollBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyScrollBox->SetScrollOffset(DesiredScrollOffset);
	MyScrollBox->SetOrientation(Orientation);
	MyScrollBox->SetScrollBarVisibility(UWidget::ConvertSerializedVisibilityToRuntime(ScrollBarVisibility));
	MyScrollBox->SetScrollBarThickness(ScrollbarThickness);
	MyScrollBox->SetScrollBarAlwaysVisible(AlwaysShowScrollbar);
	MyScrollBox->SetAllowOverscroll(AllowOverscroll ? EAllowOverscroll::Yes : EAllowOverscroll::No);
	MyScrollBox->SetScrollBarRightClickDragAllowed(bAllowRightClickDragScrolling);
}

float UScrollBox::GetScrollOffset() const
{
	if ( MyScrollBox.IsValid() )
	{
		return MyScrollBox->GetScrollOffset();
	}

	return 0;
}

void UScrollBox::SetScrollOffset(float NewScrollOffset)
{
	DesiredScrollOffset = NewScrollOffset;

	if ( MyScrollBox.IsValid() )
	{
		MyScrollBox->SetScrollOffset(NewScrollOffset);
	}
}

void UScrollBox::ScrollToStart()
{
	if ( MyScrollBox.IsValid() )
	{
		MyScrollBox->ScrollToStart();
	}
}

void UScrollBox::ScrollToEnd()
{
	if ( MyScrollBox.IsValid() )
	{
		MyScrollBox->ScrollToEnd();
	}
}

void UScrollBox::ScrollWidgetIntoView(UWidget* WidgetToFind, bool AnimateScroll, EDescendantScrollDestination InScrollDestination)
{
	TSharedPtr<SWidget> SlateWidgetToFind;
	if (WidgetToFind)
	{
		SlateWidgetToFind = WidgetToFind->GetCachedWidget();
	}

	if (MyScrollBox.IsValid())
	{
		// NOTE: Pass even if null! This, in effect, cancels a request to scroll which is necessary to avoid warnings/ensures 
		//       when we request to scroll to a widget and later remove that widget!
		MyScrollBox->ScrollDescendantIntoView(SlateWidgetToFind, AnimateScroll, InScrollDestination);
	}
}

void UScrollBox::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FScrollBoxStyle* StylePtr = Style_DEPRECATED->GetStyle<FScrollBoxStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}

		if ( BarStyle_DEPRECATED != nullptr )
		{
			const FScrollBarStyle* StylePtr = BarStyle_DEPRECATED->GetStyle<FScrollBarStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetBarStyle = *StylePtr;
			}

			BarStyle_DEPRECATED = nullptr;
		}
	}
}

void UScrollBox::SetOrientation(EOrientation NewOrientation)
{
	Orientation = NewOrientation;

	if (MyScrollBox.IsValid())
	{
		MyScrollBox->SetOrientation(Orientation);
	}
}

void UScrollBox::SetScrollBarVisibility(ESlateVisibility NewScrollBarVisibility)
{
	ScrollBarVisibility = NewScrollBarVisibility;

	if (MyScrollBox.IsValid())
	{
		switch (ScrollBarVisibility)
		{
			case ESlateVisibility::Collapsed:				MyScrollBox->SetScrollBarVisibility(EVisibility::Collapsed); break;
			case ESlateVisibility::Hidden:					MyScrollBox->SetScrollBarVisibility(EVisibility::Hidden); break;
			case ESlateVisibility::HitTestInvisible:		MyScrollBox->SetScrollBarVisibility(EVisibility::HitTestInvisible); break;
			case ESlateVisibility::SelfHitTestInvisible:	MyScrollBox->SetScrollBarVisibility(EVisibility::SelfHitTestInvisible); break;
			case ESlateVisibility::Visible:					MyScrollBox->SetScrollBarVisibility(EVisibility::Visible); break;
		}
	}
}

void UScrollBox::SetScrollbarThickness(const FVector2D& NewScrollbarThickness)
{
	ScrollbarThickness = NewScrollbarThickness;

	if (MyScrollBox.IsValid())
	{
		MyScrollBox->SetScrollBarThickness(ScrollbarThickness);
	}
}

void UScrollBox::SetAlwaysShowScrollbar(bool NewAlwaysShowScrollbar)
{
	AlwaysShowScrollbar = NewAlwaysShowScrollbar;

	if (MyScrollBox.IsValid())
	{
		MyScrollBox->SetScrollBarAlwaysVisible(AlwaysShowScrollbar);
	}
}

void UScrollBox::SetAllowOverscroll(bool NewAllowOverscroll)
{
	AllowOverscroll = NewAllowOverscroll;

	if (MyScrollBox.IsValid())
	{
		MyScrollBox->SetAllowOverscroll(AllowOverscroll ? EAllowOverscroll::Yes : EAllowOverscroll::No);
	}
}

void UScrollBox::SlateHandleUserScrolled(float CurrentOffset)
{
	OnUserScrolled.Broadcast(CurrentOffset);
}

#if WITH_EDITOR

const FText UScrollBox::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

void UScrollBox::OnDescendantSelectedByDesigner( UWidget* DescendantWidget )
{
	UWidget* SelectedChild = UWidget::FindChildContainingDescendant( this, DescendantWidget );
	if ( SelectedChild )
	{
		ScrollWidgetIntoView( SelectedChild, true );

		if ( TickHandle.IsValid() )
		{
			FTicker::GetCoreTicker().RemoveTicker( TickHandle );
			TickHandle.Reset();
		}
	}
}

void UScrollBox::OnDescendantDeselectedByDesigner( UWidget* DescendantWidget )
{
	if ( TickHandle.IsValid() )
	{
		FTicker::GetCoreTicker().RemoveTicker( TickHandle );
		TickHandle.Reset();
	}

	// because we get a deselect before we get a select, we need to delay this call until we're sure we didn't scroll to another widget.
	TickHandle = FTicker::GetCoreTicker().AddTicker( FTickerDelegate::CreateLambda( [=]( float ) -> bool
	                                                                                {
		                                                                                this->ScrollToStart();
		                                                                                return false;
		                                                                            } ) );
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
