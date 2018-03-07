// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Customizations/WidgetNavigationCustomization.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"

#include "Blueprint/WidgetNavigation.h"

#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UMG"

// FWidgetNavigationCustomization
////////////////////////////////////////////////////////////////////////////////

void FWidgetNavigationCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FWidgetNavigationCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TWeakPtr<IPropertyHandle> PropertyHandlePtr(PropertyHandle);

	//IDetailCategoryBuilder& PropertyCategory = DetailLayout.EditCategory("Events", LOCTEXT("Events", "Events"), ECategoryPriority::Uncommon);

	MakeNavRow(PropertyHandlePtr, ChildBuilder, EUINavigation::Left, LOCTEXT("LeftNavigation", "Left"));
	MakeNavRow(PropertyHandlePtr, ChildBuilder, EUINavigation::Right, LOCTEXT("RightNavigation", "Right"));
	MakeNavRow(PropertyHandlePtr, ChildBuilder, EUINavigation::Up, LOCTEXT("UpNavigation", "Up"));
	MakeNavRow(PropertyHandlePtr, ChildBuilder, EUINavigation::Down, LOCTEXT("DownNavigation", "Down"));
	MakeNavRow(PropertyHandlePtr, ChildBuilder, EUINavigation::Next, LOCTEXT("NextNavigation", "Next"));
	MakeNavRow(PropertyHandlePtr, ChildBuilder, EUINavigation::Previous, LOCTEXT("PreviousNavigation", "Previous"));
}

EUINavigationRule FWidgetNavigationCustomization::GetNavigationRule(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav) const
{
	TArray<UObject*> OuterObjects;
	TSharedPtr<IPropertyHandle> PropertyHandlePtr = PropertyHandle.Pin();
	PropertyHandlePtr->GetOuterObjects(OuterObjects);

	EUINavigationRule Rule = EUINavigationRule::Invalid;
	for ( UObject* OuterObject : OuterObjects )
	{
		if ( UWidget* Widget = Cast<UWidget>(OuterObject) )
		{
			EUINavigationRule CurRule = EUINavigationRule::Escape;
			UWidgetNavigation* WidgetNavigation = Widget->Navigation;
			if ( Widget->Navigation != nullptr )
			{
				CurRule = WidgetNavigation->GetNavigationRule(Nav);
			}

			if ( Rule != EUINavigationRule::Invalid && CurRule != Rule )
			{
				return EUINavigationRule::Invalid;
			}
			Rule = CurRule;
		}
	}

	return Rule;
}

FText FWidgetNavigationCustomization::GetNavigationText(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav) const
{
	EUINavigationRule Rule = GetNavigationRule(PropertyHandle, Nav);

	switch (Rule)
	{
	case EUINavigationRule::Escape:
		return LOCTEXT("NavigationEscape", "Escape");
	case EUINavigationRule::Stop:
		return LOCTEXT("NavigationStop", "Stop");
	case EUINavigationRule::Wrap:
		return LOCTEXT("NavigationWrap", "Wrap");
	case EUINavigationRule::Explicit:
		return LOCTEXT("NavigationExplicit", "Explicit");
	case EUINavigationRule::Invalid:
		return LOCTEXT("NavigationMultipleValues", "Multiple Values");
	case EUINavigationRule::Custom:
		break;
	}

	return FText::GetEmpty();
}

FText FWidgetNavigationCustomization::GetExplictWidget(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav) const
{
	TArray<UObject*> OuterObjects;
	TSharedPtr<IPropertyHandle> PropertyHandlePtr = PropertyHandle.Pin();
	PropertyHandlePtr->GetOuterObjects(OuterObjects);

	bool bFirst = true;
	FName Rule = NAME_None;
	for ( UObject* OuterObject : OuterObjects )
	{
		if ( UWidget* Widget = Cast<UWidget>(OuterObject) )
		{
			FName CurRule = NAME_None;
			UWidgetNavigation* WidgetNavigation = Widget->Navigation;
			if ( Widget->Navigation != nullptr )
			{
				CurRule = WidgetNavigation->GetNavigationData(Nav).WidgetToFocus;
				if ( bFirst )
				{
					Rule = CurRule;
					bFirst = false;
				}
			}

			if ( CurRule != Rule )
			{
				return LOCTEXT("NavigationMultipleValues", "Multiple Values");
			}

			Rule = CurRule;
		}
	}

	return FText::FromName(Rule);
}

void FWidgetNavigationCustomization::OnCommitExplictWidgetText(const FText& ItemFText, ETextCommit::Type CommitInfo, TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav)
{
	TArray<UObject*> OuterObjects;
	TSharedPtr<IPropertyHandle> PropertyHandlePtr = PropertyHandle.Pin();
	PropertyHandlePtr->GetOuterObjects(OuterObjects);

	const FScopedTransaction Transaction(LOCTEXT("InitializeNavigation", "Edit Widget Navigation"));

	FName GotoWidgetName = FName(*ItemFText.ToString());

	for ( UObject* OuterObject : OuterObjects )
	{
		if ( UWidget* Widget = Cast<UWidget>(OuterObject) )
		{
			FWidgetReference WidgetReference = Editor.Pin()->GetReferenceFromPreview(Widget);

			SetNav(WidgetReference.GetPreview(), Nav, TOptional<EUINavigationRule>(), GotoWidgetName);
			SetNav(WidgetReference.GetTemplate(), Nav, TOptional<EUINavigationRule>(), GotoWidgetName);
		}
	}
}

EVisibility FWidgetNavigationCustomization::GetExplictWidgetFieldVisibility(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav) const
{
	EUINavigationRule Rule = GetNavigationRule(PropertyHandle, Nav);
	return Rule == EUINavigationRule::Explicit ? EVisibility::Visible : EVisibility::Collapsed;
}

void FWidgetNavigationCustomization::MakeNavRow(TWeakPtr<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, EUINavigation Nav, FText NavName)
{
	ChildBuilder.AddCustomRow(NavName)
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(NavName)
		]
		.ValueContent()
		.MaxDesiredWidth(300)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SComboButton)
				.HAlign(HAlign_Center)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FWidgetNavigationCustomization::GetNavigationText, PropertyHandle, Nav)
				]
				.ContentPadding(FMargin(2.0f, 1.0f))
				.MenuContent()
				[
					MakeNavMenu(PropertyHandle, Nav)
				]
			]

			// Explicit Navigation Widget
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("WidgetName", "Widget Name?"))
				.Text(this, &FWidgetNavigationCustomization::GetExplictWidget, PropertyHandle, Nav)
				.OnTextCommitted(this, &FWidgetNavigationCustomization::OnCommitExplictWidgetText, PropertyHandle, Nav)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Visibility(this, &FWidgetNavigationCustomization::GetExplictWidgetFieldVisibility, PropertyHandle, Nav)
			]
		];
}

TSharedRef<class SWidget> FWidgetNavigationCustomization::MakeNavMenu(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav)
{
	// create build configurations menu
	FMenuBuilder MenuBuilder(true, NULL);
	{
		FUIAction EscapeAction(FExecuteAction::CreateSP(this, &FWidgetNavigationCustomization::HandleNavMenuEntryClicked, PropertyHandle, Nav, EUINavigationRule::Escape));
		MenuBuilder.AddMenuEntry(LOCTEXT("NavigationRuleEscape", "Escape"), LOCTEXT("NavigationRuleEscapeHint", "Navigation is allowed to escape the bounds of this widget."), FSlateIcon(), EscapeAction);

		FUIAction StopAction(FExecuteAction::CreateSP(this, &FWidgetNavigationCustomization::HandleNavMenuEntryClicked, PropertyHandle, Nav, EUINavigationRule::Stop));
		MenuBuilder.AddMenuEntry(LOCTEXT("NavigationRuleStop", "Stop"), LOCTEXT("NavigationRuleStopHint", "Navigation stops at the bounds of this widget."), FSlateIcon(), StopAction);

		FUIAction WrapAction(FExecuteAction::CreateSP(this, &FWidgetNavigationCustomization::HandleNavMenuEntryClicked, PropertyHandle, Nav, EUINavigationRule::Wrap));
		MenuBuilder.AddMenuEntry(LOCTEXT("NavigationRuleWrap", "Wrap"), LOCTEXT("NavigationRuleWrapHint", "Navigation will wrap to the opposite bound of this object."), FSlateIcon(), WrapAction);

		FUIAction ExplicitAction(FExecuteAction::CreateSP(this, &FWidgetNavigationCustomization::HandleNavMenuEntryClicked, PropertyHandle, Nav, EUINavigationRule::Explicit));
		MenuBuilder.AddMenuEntry(LOCTEXT("NavigationRuleExplicit", "Explicit"), LOCTEXT("NavigationRuleExplicitHint", "Navigation will go to a specified widget."), FSlateIcon(), ExplicitAction);

		//FUIAction CustomAction(FExecuteAction::CreateSP(this, &FWidgetNavigationCustomization::HandleNavMenuEntryClicked, PropertyHandle, Nav, EUINavigationRule::Custom));
		//MenuBuilder.AddMenuEntry(LOCTEXT("NavigationRuleCustom", "Custom"), LOCTEXT("NavigationRuleCustomHint", "Custom function can determine what widget is navigated to."), FSlateIcon(), CustomAction);
	}

	return MenuBuilder.MakeWidget();
}

// Callback for clicking a menu entry for a navigations rule.
void FWidgetNavigationCustomization::HandleNavMenuEntryClicked(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav, EUINavigationRule Rule)
{
	TArray<UObject*> OuterObjects;
	TSharedPtr<IPropertyHandle> PropertyHandlePtr = PropertyHandle.Pin();
	PropertyHandlePtr->GetOuterObjects(OuterObjects);

	const FScopedTransaction Transaction(LOCTEXT("InitializeNavigation", "Edit Widget Navigation"));

	for (UObject* OuterObject : OuterObjects)
	{
		if (UWidget* Widget = Cast<UWidget>(OuterObject))
		{
			FWidgetReference WidgetReference = Editor.Pin()->GetReferenceFromPreview(Widget);

			SetNav(WidgetReference.GetPreview(), Nav, Rule, TOptional<FName>());
			SetNav(WidgetReference.GetTemplate(), Nav, Rule, TOptional<FName>());
		}
	}
}

void FWidgetNavigationCustomization::SetNav(UWidget* Widget, EUINavigation Nav, TOptional<EUINavigationRule> Rule, TOptional<FName> WidgetToFocus)
{
	if (!Widget)
	{
		return;
	}

	Widget->Modify();

	UWidgetNavigation* WidgetNavigation = Widget->Navigation;
	if (!Widget->Navigation)
	{
		WidgetNavigation = NewObject<UWidgetNavigation>(Widget);
	}

	FWidgetNavigationData* DirectionNavigation = nullptr;

	switch ( Nav )
	{
	case EUINavigation::Left:
		DirectionNavigation = &WidgetNavigation->Left;
		break;
	case EUINavigation::Right:
		DirectionNavigation = &WidgetNavigation->Right;
		break;
	case EUINavigation::Up:
		DirectionNavigation = &WidgetNavigation->Up;
		break;
	case EUINavigation::Down:
		DirectionNavigation = &WidgetNavigation->Down;
		break;
	case EUINavigation::Next:
		DirectionNavigation = &WidgetNavigation->Next;
		break;
	case EUINavigation::Previous:
		DirectionNavigation = &WidgetNavigation->Previous;
		break;
	default:
		// Should not be possible.
		check(false);
		return;
	}

	if ( Rule.IsSet() )
	{
		DirectionNavigation->Rule = Rule.GetValue();
	}

	if ( WidgetToFocus.IsSet() )
	{
		DirectionNavigation->WidgetToFocus = WidgetToFocus.GetValue();
	}

	if ( WidgetNavigation->IsDefault() )
	{
		// If the navigation rules are all set to the defaults, remove the navigation
		// information from the widget.
		Widget->Navigation = nullptr;
	}
	else
	{
		Widget->Navigation = WidgetNavigation;
	}
}

#undef LOCTEXT_NAMESPACE
