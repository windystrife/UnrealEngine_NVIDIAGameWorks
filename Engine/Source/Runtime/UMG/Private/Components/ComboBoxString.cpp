// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ComboBoxString.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UComboBoxString

UComboBoxString::UComboBoxString(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SComboBox< TSharedPtr<FString> >::FArguments SlateDefaults;
	WidgetStyle = *SlateDefaults._ComboBoxStyle;
	ItemStyle = *SlateDefaults._ItemStyle;
	ItemStyle.SelectorFocusedBrush.TintColor = ItemStyle.SelectorFocusedBrush.TintColor.GetSpecifiedColor();
	ItemStyle.ActiveHoveredBrush.TintColor = ItemStyle.ActiveHoveredBrush.TintColor.GetSpecifiedColor();
	ItemStyle.ActiveBrush.TintColor = ItemStyle.ActiveBrush.TintColor.GetSpecifiedColor();
	ItemStyle.InactiveHoveredBrush.TintColor = ItemStyle.InactiveHoveredBrush.TintColor.GetSpecifiedColor();
	ItemStyle.InactiveBrush.TintColor = ItemStyle.InactiveBrush.TintColor.GetSpecifiedColor();
	ItemStyle.EvenRowBackgroundHoveredBrush.TintColor = ItemStyle.EvenRowBackgroundHoveredBrush.TintColor.GetSpecifiedColor();
	ItemStyle.EvenRowBackgroundBrush.TintColor = ItemStyle.EvenRowBackgroundBrush.TintColor.GetSpecifiedColor();
	ItemStyle.OddRowBackgroundHoveredBrush.TintColor = ItemStyle.OddRowBackgroundHoveredBrush.TintColor.GetSpecifiedColor();
	ItemStyle.OddRowBackgroundBrush.TintColor = ItemStyle.OddRowBackgroundBrush.TintColor.GetSpecifiedColor();
	ItemStyle.TextColor = ItemStyle.TextColor.GetSpecifiedColor();
	ItemStyle.SelectedTextColor = ItemStyle.SelectedTextColor.GetSpecifiedColor();
	ItemStyle.DropIndicator_Above.TintColor = ItemStyle.DropIndicator_Above.TintColor.GetSpecifiedColor();
	ItemStyle.DropIndicator_Onto.TintColor = ItemStyle.DropIndicator_Onto.TintColor.GetSpecifiedColor();
	ItemStyle.DropIndicator_Below.TintColor = ItemStyle.DropIndicator_Below.TintColor.GetSpecifiedColor();

	ForegroundColor = FLinearColor::Black;
	bIsFocusable = true;

	ContentPadding = FMargin(4.0, 2.0);
	MaxListHeight = 450.0f;
	HasDownArrow = true;
	EnableGamepadNavigationMode = true;
	// We don't want to try and load fonts on the server.
	if ( !IsRunningDedicatedServer() )
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		Font = FSlateFontInfo(RobotoFontObj.Object, 16, FName("Bold"));
	}
}

void UComboBoxString::PostInitProperties()
{
	Super::PostInitProperties();

	// Initialize the set of options from the default set only once.
	for (const FString& DefaultOption : DefaultOptions)
	{
		AddOption(DefaultOption);
	}
}

void UComboBoxString::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyComboBox.Reset();
	ComboBoxContent.Reset();
}

void UComboBoxString::PostLoad()
{
	Super::PostLoad();

	// Initialize the set of options from the default set only once.
	for (const FString& DefaultOption : DefaultOptions)
	{
		AddOption(DefaultOption);
	}

	if (GetLinkerCustomVersion(FEditorObjectVersion::GUID) < FEditorObjectVersion::ComboBoxControllerSupportUpdate)
	{
		EnableGamepadNavigationMode = false;
	}
}

TSharedRef<SWidget> UComboBoxString::RebuildWidget()
{
	int32 InitialIndex = FindOptionIndex(SelectedOption);
	if ( InitialIndex != -1 )
	{
		CurrentOptionPtr = Options[InitialIndex];
	}

	MyComboBox =
		SNew(SComboBox< TSharedPtr<FString> >)
		.ComboBoxStyle(&WidgetStyle)
		.ItemStyle(&ItemStyle)
		.ForegroundColor(ForegroundColor)
		.OptionsSource(&Options)
		.InitiallySelectedItem(CurrentOptionPtr)
		.ContentPadding(ContentPadding)
		.MaxListHeight(MaxListHeight)
		.HasDownArrow(HasDownArrow)
		.EnableGamepadNavigationMode(EnableGamepadNavigationMode)
		.OnGenerateWidget(BIND_UOBJECT_DELEGATE(SComboBox< TSharedPtr<FString> >::FOnGenerateWidget, HandleGenerateWidget))
		.OnSelectionChanged(BIND_UOBJECT_DELEGATE(SComboBox< TSharedPtr<FString> >::FOnSelectionChanged, HandleSelectionChanged))
		.OnComboBoxOpening(BIND_UOBJECT_DELEGATE(FOnComboBoxOpening, HandleOpening))
		.IsFocusable(bIsFocusable)
		[
			SAssignNew(ComboBoxContent, SBox)
		];

	if ( InitialIndex != -1 )
	{
		// Generate the widget for the initially selected widget if needed
		ComboBoxContent->SetContent(HandleGenerateWidget(CurrentOptionPtr));
	}

	return MyComboBox.ToSharedRef();
}

void UComboBoxString::AddOption(const FString& Option)
{
	Options.Add(MakeShareable(new FString(Option)));

	RefreshOptions();
}

bool UComboBoxString::RemoveOption(const FString& Option)
{
	int32 OptionIndex = FindOptionIndex(Option);

	if ( OptionIndex != -1 )
	{
		if ( Options[OptionIndex] == CurrentOptionPtr )
		{
			ClearSelection();
		}

		Options.RemoveAt(OptionIndex);

		RefreshOptions();

		return true;
	}

	return false;
}

int32 UComboBoxString::FindOptionIndex(const FString& Option) const
{
	for ( int32 OptionIndex = 0; OptionIndex < Options.Num(); OptionIndex++ )
	{
		const TSharedPtr<FString>& OptionAtIndex = Options[OptionIndex];

		if ( ( *OptionAtIndex ) == Option )
		{
			return OptionIndex;
		}
	}

	return -1;
}

FString UComboBoxString::GetOptionAtIndex(int32 Index) const
{
	if (Index >= 0 && Index < Options.Num())
	{
		return *(Options[Index]);
	}
	return FString();
}

void UComboBoxString::ClearOptions()
{
	ClearSelection();

	Options.Empty();

	if ( MyComboBox.IsValid() )
	{
		MyComboBox->RefreshOptions();
	}
}

void UComboBoxString::ClearSelection()
{
	CurrentOptionPtr.Reset();

	if ( MyComboBox.IsValid() )
	{
		MyComboBox->ClearSelection();
	}

	if ( ComboBoxContent.IsValid() )
	{
		ComboBoxContent->SetContent(SNullWidget::NullWidget);
	}
}

void UComboBoxString::RefreshOptions()
{
	if ( MyComboBox.IsValid() )
	{
		MyComboBox->RefreshOptions();
	}
}

void UComboBoxString::SetSelectedOption(FString Option)
{
	int32 InitialIndex = FindOptionIndex(Option);
	if (InitialIndex != -1)
	{
		CurrentOptionPtr = Options[InitialIndex];
		SelectedOption = Option;

		if ( ComboBoxContent.IsValid() )
		{
			MyComboBox->SetSelectedItem(CurrentOptionPtr);
			ComboBoxContent->SetContent(HandleGenerateWidget(CurrentOptionPtr));
		}
	}
}

FString UComboBoxString::GetSelectedOption() const
{
	if (CurrentOptionPtr.IsValid())
	{
		return *CurrentOptionPtr;
	}
	return FString();
}

int32 UComboBoxString::GetOptionCount() const
{
	return Options.Num();
}

TSharedRef<SWidget> UComboBoxString::HandleGenerateWidget(TSharedPtr<FString> Item) const
{
	FString StringItem = Item.IsValid() ? *Item : FString();

	// Call the user's delegate to see if they want to generate a custom widget bound to the data source.
	if ( !IsDesignTime() && OnGenerateWidgetEvent.IsBound() )
	{
		UWidget* Widget = OnGenerateWidgetEvent.Execute(StringItem);
		if ( Widget != NULL )
		{
			return Widget->TakeWidget();
		}
	}

	// If a row wasn't generated just create the default one, a simple text block of the item's name.
	return SNew(STextBlock)
		.Text(FText::FromString(StringItem))
		.Font(Font);
}

void UComboBoxString::HandleSelectionChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectionType)
{
	CurrentOptionPtr = Item;
	SelectedOption = CurrentOptionPtr.IsValid() ? CurrentOptionPtr.ToSharedRef().Get() : FString();

	// When the selection changes we always generate another widget to represent the content area of the combobox.
	if ( ComboBoxContent.IsValid() )
	{
		ComboBoxContent->SetContent(HandleGenerateWidget(CurrentOptionPtr));
	}

	if ( !IsDesignTime() )
	{
		OnSelectionChanged.Broadcast(Item.IsValid() ? *Item : FString(), SelectionType);
	}
}

void UComboBoxString::HandleOpening()
{
	OnOpening.Broadcast();
}

#if WITH_EDITOR

const FText UComboBoxString::GetPaletteCategory()
{
	return LOCTEXT("Input", "Input");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
