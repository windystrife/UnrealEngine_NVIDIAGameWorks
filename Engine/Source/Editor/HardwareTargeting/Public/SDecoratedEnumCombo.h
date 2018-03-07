// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Styling/SlateColor.h"
#include "Layout/Margin.h"
#include "EditorStyleSet.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"

template<typename TEnumType>
class SDecoratedEnumCombo : public SCompoundWidget
{
public:
	struct FComboOption
	{
		FComboOption(TEnumType InValue, FSlateIcon InIcon, FText InText, bool bInChoosable = true)
			: Value(InValue), Icon(InIcon), Text(InText), bChoosable(bInChoosable)
		{}

		TEnumType Value;
		FSlateIcon Icon;
		FText Text;
		bool bChoosable;
	};

	DECLARE_DELEGATE_OneParam(FOnEnumChanged, TEnumType)

	SLATE_BEGIN_ARGS( SDecoratedEnumCombo )
		: _ContentPadding(6.f)
	{}

		SLATE_EVENT(FOnEnumChanged, OnEnumChanged)
		SLATE_ARGUMENT(FMargin, ContentPadding)
		SLATE_ATTRIBUTE(TEnumType, SelectedEnum)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs, TArray<FComboOption> InOptions )
	{
		ContentPadding = InArgs._ContentPadding;
		OnEnumChanged = InArgs._OnEnumChanged;
		Options = MoveTemp(InOptions);
		SelectedEnum = InArgs._SelectedEnum;

		ChildSlot
		[
			SNew(SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
			.ForegroundColor(FSlateColor::UseForeground())
			.ContentPadding(ContentPadding)
			.OnGetMenuContent(this, &SDecoratedEnumCombo::OnGetComboContent)
			.ButtonContent()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(this, &SDecoratedEnumCombo::GetCurrentIcon)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SDecoratedEnumCombo::GetCurrentText)
				]
			]
		];
	}

private:

	TArray<FComboOption> Options;
	TAttribute<TEnumType> SelectedEnum;
	FMargin ContentPadding;
	FOnEnumChanged OnEnumChanged;
	TWeakPtr<SWidget> MenuContent;

	FText GetCurrentText() const
	{
		auto CurrentEnum = SelectedEnum.Get();
		if (Options.IsValidIndex(CurrentEnum))
		{
			return Options[CurrentEnum].Text;
		}
		return FText();
	}

	const FSlateBrush* GetCurrentIcon() const
	{
		auto CurrentEnum = SelectedEnum.Get();
		if (Options.IsValidIndex(CurrentEnum))
		{
			return Options[CurrentEnum].Icon.GetIcon();
		}
		return nullptr;
	}

	FReply OnChangeSelected(int32 NewIndex)
	{
		auto MenuContentPin = MenuContent.Pin();
		if (MenuContentPin.IsValid())
		{
			TSharedRef<SWindow> ParentContextMenuWindow = FSlateApplication::Get().FindWidgetWindow( MenuContentPin.ToSharedRef() ).ToSharedRef();
			FSlateApplication::Get().RequestDestroyWindow( ParentContextMenuWindow );
		}

		if (Options.IsValidIndex(NewIndex))
		{
			OnEnumChanged.ExecuteIfBound(Options[NewIndex].Value);
		}
		return FReply::Handled();
	}

	TSharedRef<SWidget> OnGetComboContent()
	{
		TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			const auto& Option = Options[Index];
			if (!Option.bChoosable)
			{
				continue;
			}

			HorizontalBox->AddSlot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ForegroundColor(FSlateColor::UseForeground())
				.ContentPadding(ContentPadding)
				.OnClicked(this, &SDecoratedEnumCombo::OnChangeSelected, Index)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(SImage).Image(Option.Icon.GetIcon())
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock).Text(Option.Text)
					]
				]
			];
		}

		MenuContent = HorizontalBox;

		return HorizontalBox;
	}
};
