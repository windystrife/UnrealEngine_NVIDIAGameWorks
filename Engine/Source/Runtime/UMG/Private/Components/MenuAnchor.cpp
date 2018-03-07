// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/MenuAnchor.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Blueprint/UserWidget.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UMenuAnchor

UMenuAnchor::UMenuAnchor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ShouldDeferPaintingAfterWindowContent(true)
	, UseApplicationMenuStack(true)
{
	Placement = MenuPlacement_ComboBox;
}

void UMenuAnchor::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyMenuAnchor.Reset();
}

TSharedRef<SWidget> UMenuAnchor::RebuildWidget()
{
	MyMenuAnchor = SNew(SMenuAnchor)
		.Placement(Placement)
		.OnGetMenuContent(BIND_UOBJECT_DELEGATE(FOnGetContent, HandleGetMenuContent))
		.OnMenuOpenChanged(BIND_UOBJECT_DELEGATE(FOnIsOpenChanged, HandleMenuOpenChanged))
		.ShouldDeferPaintingAfterWindowContent(ShouldDeferPaintingAfterWindowContent)
		.UseApplicationMenuStack(UseApplicationMenuStack);

	if ( GetChildrenCount() > 0 )
	{
		MyMenuAnchor->SetContent(GetContentSlot()->Content ? GetContentSlot()->Content->TakeWidget() : SNullWidget::NullWidget);
	}
	
	return MyMenuAnchor.ToSharedRef();
}

void UMenuAnchor::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live slot if it already exists
	if ( MyMenuAnchor.IsValid() )
	{
		MyMenuAnchor->SetContent(InSlot->Content ? InSlot->Content->TakeWidget() : SNullWidget::NullWidget);
	}
}

void UMenuAnchor::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyMenuAnchor.IsValid() )
	{
		MyMenuAnchor->SetContent(SNullWidget::NullWidget);
	}
}

void UMenuAnchor::HandleMenuOpenChanged(bool bIsOpen)
{
	OnMenuOpenChanged.Broadcast(bIsOpen);
}

TSharedRef<SWidget> UMenuAnchor::HandleGetMenuContent()
{
	TSharedPtr<SWidget> SlateMenuWidget;

	if ( OnGetMenuContentEvent.IsBound() )
	{
		UWidget* MenuWidget = OnGetMenuContentEvent.Execute();
		if ( MenuWidget )
		{
			SlateMenuWidget = MenuWidget->TakeWidget();
		}
	}
	else
	{
		if ( MenuClass != nullptr && !MenuClass->HasAnyClassFlags(CLASS_Abstract) )
		{
			if ( UWorld* World = GetWorld() )
			{
				if ( UUserWidget* MenuWidget = CreateWidget<UUserWidget>(World, MenuClass) )
				{
					SlateMenuWidget = MenuWidget->TakeWidget();
				}
			}
		}
	}

	return SlateMenuWidget.IsValid() ? SlateMenuWidget.ToSharedRef() : SNullWidget::NullWidget;
}

void UMenuAnchor::ToggleOpen(bool bFocusOnOpen)
{
	if ( MyMenuAnchor.IsValid() )
	{
		MyMenuAnchor->SetIsOpen(!MyMenuAnchor->IsOpen(), bFocusOnOpen);
	}
}

void UMenuAnchor::Open(bool bFocusMenu)
{
	if ( MyMenuAnchor.IsValid() && !MyMenuAnchor->IsOpen() )
	{
		MyMenuAnchor->SetIsOpen(true, bFocusMenu);
	}
}

void UMenuAnchor::Close()
{
	if ( MyMenuAnchor.IsValid() )
	{
		return MyMenuAnchor->SetIsOpen(false, false);
	}
}

bool UMenuAnchor::IsOpen() const
{
	if ( MyMenuAnchor.IsValid() )
	{
		return MyMenuAnchor->IsOpen();
	}

	return false;
}

bool UMenuAnchor::ShouldOpenDueToClick() const
{
	if ( MyMenuAnchor.IsValid() )
	{
		return MyMenuAnchor->ShouldOpenDueToClick();
	}

	return false;
}

FVector2D UMenuAnchor::GetMenuPosition() const
{
	if ( MyMenuAnchor.IsValid() )
	{
		return MyMenuAnchor->GetMenuPosition();
	}

	return FVector2D(0, 0);
}

bool UMenuAnchor::HasOpenSubMenus() const
{
	if ( MyMenuAnchor.IsValid() )
	{
		return MyMenuAnchor->HasOpenSubMenus();
	}

	return false;
}

#if WITH_EDITOR

const FText UMenuAnchor::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
