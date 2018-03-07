// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailWidgets.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorStyleSet.h"
#include "Framework/MultiBox/SToolBarButtonBlock.h"


// Based on a stripped-down FToolBarComboButtonBlock
class FToolSelector : public FMultiBlock
{
public:
	FToolSelector(const FUIAction& InAction, const FOnGetContent& InMenuContentGenerator, const TAttribute<FText>& InLabel = TAttribute<FText>(), const TAttribute<FText>& InSmallText = TAttribute<FText>(), const TAttribute<FText>& InToolTip = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIcon = TAttribute<FSlateIcon>());

protected:
	virtual void CreateMenuEntry(class FMenuBuilder& MenuBuilder) const override;

	virtual TSharedRef<class IMultiBlockBaseWidget> ConstructWidget() const override;

	// Friend our corresponding widget class
	friend class SToolSelector;

	FOnGetContent MenuContentGenerator;
	TAttribute<FText> Label;
	TAttribute<FText> SmallText;
	TAttribute<FText> ToolTip;
	TAttribute<FSlateIcon> Icon;
};

class SToolSelector : public SMultiBlockBaseWidget
{
public:

	SLATE_BEGIN_ARGS(SToolSelector)
	{ }
	SLATE_ARGUMENT(TOptional<EVisibility>, LabelVisibility); /** Controls the visibility of the blocks label */
	SLATE_ATTRIBUTE( FSlateIcon, Icon ); /** Optional overridden icon for this tool bar button.  IF not set, then the action's icon will be used instead. */
	SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	virtual void BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName) override;

protected:
	TSharedRef<SWidget> OnGetMenuContent();
	bool IsEnabled() const;

	/** @return True if this toolbar button is using a dynamically set icon */
	bool HasDynamicIcon() const;

	/** @return The icon for the toolbar button; may be dynamic, so check HasDynamicIcon */
	const FSlateBrush* GetIcon() const;

	/** @return The small icon for the toolbar button; may be dynamic, so check HasDynamicIcon */
	const FSlateBrush* GetSmallIcon() const;

	EVisibility GetVisibility() const;
	EVisibility GetIconVisibility(bool bIsASmallIcon) const;

	TAttribute< EVisibility > LabelVisibility;

	/** Optional overridden icon for this tool bar button.  IF not set, then the action's icon will be used instead. */
	TAttribute<FSlateIcon> Icon;
};

FToolSelector::FToolSelector(const FUIAction& InAction, const FOnGetContent& InMenuContentGenerator, const TAttribute<FText>& InLabel /*= TAttribute<FText>()*/, const TAttribute<FText>& InSmallText /*= TAttribute<FText>()*/, const TAttribute<FText>& InToolTip /*= TAttribute<FText>()*/, const TAttribute<FSlateIcon>& InIcon /*= TAttribute<FSlateIcon>()*/)
	: FMultiBlock(InAction),
	MenuContentGenerator(InMenuContentGenerator),
	Label(InLabel),
	SmallText(InSmallText),
	ToolTip(InToolTip),
	Icon(InIcon)
{
}

void FToolSelector::CreateMenuEntry(class FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.AddWrapperSubMenu(Label.Get(), ToolTip.Get(), MenuContentGenerator, Icon.Get());
}

TSharedRef<class IMultiBlockBaseWidget> FToolSelector::ConstructWidget() const
{
	return SNew(SToolSelector).Icon(Icon);
}

//////////////////////////////////////////////////////////////////////////

void SToolSelector::Construct(const FArguments& InArgs)
{
	if ( InArgs._LabelVisibility.IsSet() )
	{
		LabelVisibility = InArgs._LabelVisibility.GetValue();
	}
	else
	{
		LabelVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(SharedThis(this), &SToolSelector::GetIconVisibility, false));
	}

	Icon = InArgs._Icon;
}

void SToolSelector::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	TSharedRef<const FMultiBox> MultiBox( OwnerMultiBoxWidget.Pin()->GetMultiBox() );
	
	TSharedRef<const FToolSelector> ToolSelectorButtonBlock = StaticCastSharedRef<const FToolSelector>(MultiBlock.ToSharedRef());

	TAttribute<FText> Label;
	TAttribute<FText> SmallText;

	// If we were supplied an image than go ahead and use that, otherwise we use a null widget
	TSharedRef<SWidget> IconWidget = SNullWidget::NullWidget;
	TSharedRef<SWidget> SmallIconWidget = SNullWidget::NullWidget;
	if( !HasDynamicIcon() )
	{
		// Not dynamic, so resolve the icons now
		const FSlateBrush* IconBrush = GetIcon();
		const FSlateBrush* SmallIconBrush = GetSmallIcon();

		if( IconBrush->GetResourceName() != NAME_None )
		{
			IconWidget =
				SNew( SImage )
				.Visibility( this, &SToolSelector::GetIconVisibility, false )
				.Image( IconBrush );
		}
		if( SmallIconBrush->GetResourceName() != NAME_None )
		{
			SmallIconWidget =
				SNew( SImage )
				.Visibility( this, &SToolSelector::GetIconVisibility, true )
				.Image( SmallIconBrush );
		}
	}
	else 
	{
		// Dynamic, so we need to preserve the callback used
		IconWidget =
			SNew( SImage )
			.Visibility( this, &SToolSelector::GetIconVisibility, false )
			.Image( this, &SToolSelector::GetIcon );

		SmallIconWidget =
			SNew( SImage )
			.Visibility( this, &SToolSelector::GetIconVisibility, true )
			.Image( this, &SToolSelector::GetSmallIcon );
	}

	Label = ToolSelectorButtonBlock->Label;
	SmallText = ToolSelectorButtonBlock->SmallText;

	// Add this widget to the search list of the multibox
	if (MultiBlock->GetSearchable())
		OwnerMultiBoxWidget.Pin()->AddSearchElement(this->AsWidget(), Label.Get());
	
	const FString MetaTag = FString::Printf(TEXT("LandscapeToolButton.%s"), Label.IsSet() == true ? *Label.Get().ToString() : TEXT("NoLabel"));

	static FTextBlockStyle LabelStyle = FTextBlockStyle(FEditorStyle::GetWidgetStyle< FTextBlockStyle >(FEditorStyle::Join(StyleName, ".Label"))).SetShadowOffset(FVector2D::UnitVector);
	static FTextBlockStyle SmallTextStyle = FTextBlockStyle(LabelStyle).SetFontSize(LabelStyle.Font.Size - 1).SetColorAndOpacity(FSlateColor::UseSubduedForeground());

	// Create the content for our button
	TSharedRef< SWidget > ButtonContent =
		SNew( SVerticalBox )
		.AddMetaData<FTagMetaData>(FTagMetaData(FName(*MetaTag)))
		// Icon image
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center) // Center the icon horizontally, so that large labels don't stretch out the artwork
		[
			IconWidget
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SmallIconWidget
		]

		// Small text
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(&LabelStyle)
			.Text(SmallText)
		]

		// Label text
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center) // Center the label text horizontally
		[
			SNew(STextBlock)
				.Visibility(LabelVisibility)
				.TextStyle(&SmallTextStyle)
				.Text(Label)
		];
		
	EMultiBlockLocation::Type BlockLocation = GetMultiBlockLocation();
	FName BlockStyle = EMultiBlockLocation::ToName(FEditorStyle::Join( StyleName, ".Button" ), BlockLocation);

	ChildSlot
	[
		SNew(SComboButton)
			.ContentPadding(0)
			.ButtonStyle( FEditorStyle::Get(), BlockStyle )
			.ToolTipText(ToolSelectorButtonBlock->ToolTip)
			.ForegroundColor(FSlateColor::UseForeground())
			.ButtonContent()
			[
				ButtonContent
			]
			.OnGetMenuContent(this, &SToolSelector::OnGetMenuContent)
	];

	ChildSlot.Padding(FEditorStyle::GetMargin(FEditorStyle::Join(StyleName, ".SToolBarComboButtonBlock.Padding")));

	// Bind our widget's enabled state to whether or not our action can execute
	SetEnabled(TAttribute<bool>(this, &SToolSelector::IsEnabled));

	// Bind our widget's visible state to whether or not the button should be visible
	SetVisibility(TAttribute<EVisibility>(this, &SToolSelector::GetVisibility));
}

TSharedRef<SWidget> SToolSelector::OnGetMenuContent()
{
	TSharedRef<const FToolSelector> ToolSelectorButtonBlock = StaticCastSharedRef<const FToolSelector>(MultiBlock.ToSharedRef());
	return ToolSelectorButtonBlock->MenuContentGenerator.Execute();
}

bool SToolSelector::IsEnabled() const
{
	const FUIAction& UIAction = MultiBlock->GetDirectActions();
	if( UIAction.CanExecuteAction.IsBound() )
	{
		return UIAction.CanExecuteAction.Execute();
	}

	return true;
}

EVisibility SToolSelector::GetVisibility() const
{
	const FUIAction& UIAction = MultiBlock->GetDirectActions();
	if (UIAction.IsActionVisibleDelegate.IsBound())
	{
		return UIAction.IsActionVisibleDelegate.Execute() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

bool SToolSelector::HasDynamicIcon() const
{
	return Icon.IsBound();
}

const FSlateBrush* SToolSelector::GetIcon() const
{
	const FSlateIcon& ActualIcon = Icon.Get();
	return ActualIcon.GetIcon();
}

const FSlateBrush* SToolSelector::GetSmallIcon() const
{
	const FSlateIcon& ActualIcon = Icon.Get();
	return ActualIcon.GetSmallIcon();
}

EVisibility SToolSelector::GetIconVisibility(bool bIsASmallIcon) const
{
	return (FMultiBoxSettings::UseSmallToolBarIcons.Get() ^ bIsASmallIcon) ? EVisibility::Collapsed : EVisibility::Visible;
}

//////////////////////////////////////////////////////////////////////////

FToolSelectorBuilder::FToolSelectorBuilder(TSharedPtr< const FUICommandList > InCommandList, FMultiBoxCustomization InCustomization, TSharedPtr<FExtender> InExtender /*= TSharedPtr<FExtender>()*/, EOrientation Orientation /*= Orient_Horizontal*/)
	: FToolBarBuilder(InCommandList, InCustomization, InExtender, Orientation)
{
}

void FToolSelectorBuilder::AddComboButton(const FUIAction& InAction, const FOnGetContent& InMenuContentGenerator, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InSmallText, const TAttribute<FText>& InToolTipOverride, const TAttribute<FSlateIcon>& InIcon)
{
	ApplySectionBeginning();

	TSharedRef<FToolSelector> NewToolBarComboButtonBlock(new FToolSelector(InAction, InMenuContentGenerator, InLabelOverride, InSmallText, InToolTipOverride, InIcon));

	//if (LabelVisibility.IsSet())
	//{
	//	NewToolBarComboButtonBlock->SetLabelVisibility(LabelVisibility.GetValue());
	//}

	MultiBox->AddMultiBlock(NewToolBarComboButtonBlock);
}

//////////////////////////////////////////////////////////////////////////

FToolMenuBuilder::FToolMenuBuilder(const bool bInShouldCloseWindowAfterMenuSelection, TSharedPtr<const FUICommandList> InCommandList, TSharedPtr<FExtender> InExtender /*= TSharedPtr<FExtender>()*/, const bool bCloseSelfOnly /*= false*/)
	: FMenuBuilder(bInShouldCloseWindowAfterMenuSelection, InCommandList, InExtender, bCloseSelfOnly)
{
}

void FToolMenuBuilder::AddToolButton(const TSharedPtr<const FUICommandInfo> InCommand, FName InExtensionHook /*= NAME_None*/, const TAttribute<FText>& InLabelOverride /*= TAttribute<FText>()*/, const TAttribute<FText>& InToolTipOverride /*= TAttribute<FText>()*/, const TAttribute<FSlateIcon>& InIconOverride /*= TAttribute<FSlateIcon>()*/)
{
	ApplySectionBeginning();

	ApplyHook(InExtensionHook, EExtensionHook::Before);

	TSharedRef<FToolBarButtonBlock> NewToolBarButtonBlock(new FToolBarButtonBlock(InCommand.ToSharedRef(), CommandListStack.Last(), InLabelOverride, InToolTipOverride, InIconOverride));

	NewToolBarButtonBlock->SetLabelVisibility(EVisibility::Visible);
	NewToolBarButtonBlock->SetIsFocusable(false);

	MultiBox->AddMultiBlock(NewToolBarButtonBlock);

	ApplyHook(InExtensionHook, EExtensionHook::After);
}
