// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SBlendProfilePicker.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorStyleSet.h"
#include "Editor/EditorEngine.h"
#include "EngineGlobals.h"
#include "Animation/BlendProfile.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "BlendProfilePicker"


class SBlendProfileMenuEntry : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SBlendProfileMenuEntry){}
		SLATE_ARGUMENT( FText, LabelOverride )
		/** Called to when an entry is clicked */
		SLATE_EVENT( FExecuteAction, OnOpenClickedDelegate )
		/** Called to when the button remove an entry is clicked */
		SLATE_EVENT( FExecuteAction, OnRemoveClickedDelegate )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		const FText DisplayName = InArgs._LabelOverride;
		OnOpenClickedDelegate = InArgs._OnOpenClickedDelegate;
		OnRemoveClickedDelegate = InArgs._OnRemoveClickedDelegate;

		FSlateFontInfo MenuEntryFont = FEditorStyle::GetFontStyle( "Menu.Label.Font" );

		ChildSlot
		[
			SNew(SButton)
			.ButtonStyle( FEditorStyle::Get(), "Menu.Button" )
			.ForegroundColor( TAttribute<FSlateColor>::Create( TAttribute<FSlateColor>::FGetter::CreateRaw( this, &SBlendProfileMenuEntry::InvertOnHover ) ) )
			.Text(DisplayName)
			.ToolTipText(LOCTEXT("OpenBlendProfileToolTip", "Select this profile for editing."))
			.OnClicked(this, &SBlendProfileMenuEntry::OnOpen)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.ContentPadding( FMargin(4.0, 2.0) )
			[
				SNew( SOverlay )

				+SOverlay::Slot()
				.Padding( FMargin( 12.0, 0.0 ) )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew( STextBlock )
					.Font( MenuEntryFont )
					.ColorAndOpacity( TAttribute<FSlateColor>::Create( TAttribute<FSlateColor>::FGetter::CreateRaw( this, &SBlendProfileMenuEntry::InvertOnHover ) ) )
					.Text( DisplayName )
				]

				+SOverlay::Slot()
				.Padding( FMargin( 0.0, 0.0 ) )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ContentPadding( FMargin(4.0, 0.0) )
					.ButtonStyle( FEditorStyle::Get(), "Docking.Tab.CloseButton" )
					.ToolTipText( FText::Format( LOCTEXT("RemoveBlendProfileToolTipFmt", "Remove {0}"), DisplayName ) )
					.OnClicked(this, &SBlendProfileMenuEntry::OnRemove)
				]
			]
		];
	}

	FReply OnOpen()
	{
		OnOpenClickedDelegate.ExecuteIfBound();
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

	FReply OnRemove()
	{
		OnRemoveClickedDelegate.ExecuteIfBound();
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

private:
	FSlateColor InvertOnHover() const
	{
		if ( this->IsHovered() )
		{
			return FLinearColor::Black;
		}
		else
		{
			return FSlateColor::UseForeground();
		}
	}

	FExecuteAction OnOpenClickedDelegate;
	FExecuteAction OnRemoveClickedDelegate;
};

//////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SBlendProfilePicker::Construct(const FArguments& InArgs, TSharedRef<class IEditableSkeleton> InEditableSkeleton)
{
	bShowNewOption = InArgs._AllowNew;
	bShowClearOption = InArgs._AllowClear;
	bIsStandalone = InArgs._Standalone;
	EditableSkeleton = InEditableSkeleton;

	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}

	if(InArgs._InitialProfile != nullptr && InEditableSkeleton->GetBlendProfiles().Contains(InArgs._InitialProfile))
	{
		SelectedProfileName = InArgs._InitialProfile->GetFName();
	}
	else
	{
		SelectedProfileName = NAME_None;
	}

	BlendProfileSelectedDelegate = InArgs._OnBlendProfileSelected;

	TSharedRef<SWidget> TextBlock = SNew(STextBlock)
		.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
		.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
		.Text(this, &SBlendProfilePicker::GetSelectedProfileName);

	TSharedPtr<SWidget> ButtonContent;

	if (bIsStandalone)
	{
		ButtonContent = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("SkeletonTree.BlendProfile"))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				TextBlock
			];
	}
	else
	{
		ButtonContent = TextBlock;
	}

	ChildSlot
	[
		SNew(SComboButton)
		.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
		.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
		.ContentPadding(2.0f)
		.OnGetMenuContent(this, &SBlendProfilePicker::GetMenuContent)
		.ButtonContent()
		[
			ButtonContent.ToSharedRef()
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SBlendProfilePicker::~SBlendProfilePicker()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->UnregisterForUndo(this);
	}
}

FText SBlendProfilePicker::GetSelectedProfileName() const
{
	UBlendProfile* SelectedProfile = EditableSkeleton.Pin()->GetBlendProfile(SelectedProfileName);
	if(SelectedProfile)
	{
		if (bIsStandalone)
		{
			return FText::Format(FText(LOCTEXT("SelectedNameEntryStandalone", "Blend Profile: {0}")), FText::FromName(SelectedProfileName));
		}
		else
		{
			return FText::Format(FText(LOCTEXT("SelectedNameEntry", "{0}")), FText::FromName(SelectedProfileName));
		}
	}
	if (bIsStandalone)
	{
		return FText(LOCTEXT("NoSelectionEntryStandalone", "Blend Profile: None"));
	}
	else
	{
		return FText(LOCTEXT("NoSelectionEntry", "None"));
	}
}

TSharedRef<SWidget> SBlendProfilePicker::GetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	const bool bHasSettingsSection = bShowNewOption || bShowClearOption;

	if(bHasSettingsSection)
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("MenuSettings", "Settings"));
		{
			if(bShowNewOption)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("CreateNew", "Create New Blend Profile"),
					LOCTEXT("CreateNew_ToolTip", "Creates a new blend profile inside the skeleton."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SBlendProfilePicker::OnCreateNewProfile)));
			}

			if(bShowClearOption)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("Clear", "Clear"),
					LOCTEXT("Clear_ToolTip", "Clear the selected blend profile."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SBlendProfilePicker::OnClearSelection)));
			}
		}
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("Profiles", "Available Blend Profiles"));
	{
		if (EditableSkeleton.IsValid())
		{
			for (UBlendProfile* Profile : EditableSkeleton.Pin()->GetBlendProfiles())
			{
				MenuBuilder.AddWidget(
					SNew(SBlendProfileMenuEntry)
						.LabelOverride(FText::FromString(Profile->GetName()))
						.OnOpenClickedDelegate(FExecuteAction::CreateSP(this, &SBlendProfilePicker::OnProfileSelected, Profile->GetFName()))
						.OnRemoveClickedDelegate(FExecuteAction::CreateSP(this, &SBlendProfilePicker::OnProfileRemoved, Profile->GetFName())),
					FText(),
					true
				);
			}
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SBlendProfilePicker::OnProfileSelected(FName InBlendProfileName)
{
	SelectedProfileName = InBlendProfileName;
	BlendProfileSelectedDelegate.ExecuteIfBound(EditableSkeleton.Pin()->GetBlendProfile(SelectedProfileName));
}

void SBlendProfilePicker::OnClearSelection()
{
	SelectedProfileName = NAME_None;
	BlendProfileSelectedDelegate.ExecuteIfBound(nullptr);
}

void SBlendProfilePicker::OnProfileRemoved(FName InBlendProfileName)
{
	EditableSkeleton.Pin()->RemoveBlendProfile(EditableSkeleton.Pin()->GetBlendProfile(InBlendProfileName));
	SelectedProfileName = NAME_None;
	BlendProfileSelectedDelegate.ExecuteIfBound(nullptr);
}

void SBlendProfilePicker::OnCreateNewProfile()
{
	TSharedRef<STextEntryPopup> TextEntry = SNew(STextEntryPopup)
		.Label(LOCTEXT("NewProfileName", "Profile Name"))
		.OnTextCommitted(this, &SBlendProfilePicker::OnCreateNewProfileComitted);

	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
}

void SBlendProfilePicker::OnCreateNewProfileComitted(const FText& NewName, ETextCommit::Type CommitType)
{
	FSlateApplication::Get().DismissAllMenus();

	if(CommitType == ETextCommit::OnEnter && EditableSkeleton.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("Trans_NewProfile", "Create new blend profile."));

		FName NameToUse = FName(*NewName.ToString());

		// Only create if we don't have a matching profile
		if(UBlendProfile* FoundProfile = EditableSkeleton.Pin()->GetBlendProfile(NameToUse))
		{
			OnProfileSelected(FoundProfile->GetFName());
		}
		else if(UBlendProfile* NewProfile = EditableSkeleton.Pin()->CreateNewBlendProfile(NameToUse))
		{
			OnProfileSelected(NewProfile->GetFName());
		}
	}
}

void SBlendProfilePicker::SetSelectedProfile(UBlendProfile* InProfile, bool bBroadcast /*= true*/)
{
	if(EditableSkeleton.Pin()->GetBlendProfiles().Contains(InProfile))
	{
		SelectedProfileName = InProfile->GetFName();
		if(bBroadcast)
		{
			BlendProfileSelectedDelegate.ExecuteIfBound(InProfile);
		}
	}
	else if(!InProfile)
	{
		OnClearSelection();
	}
}

UBlendProfile* const SBlendProfilePicker::GetSelectedBlendProfile() const
{
	return EditableSkeleton.Pin()->GetBlendProfile(SelectedProfileName);
}

FName SBlendProfilePicker::GetSelectedBlendProfileName() const
{
	UBlendProfile* SelectedProfile = EditableSkeleton.Pin()->GetBlendProfile(SelectedProfileName);

	return SelectedProfile ? SelectedProfile->GetFName() : NAME_None;
}

void SBlendProfilePicker::PostUndo(bool bSuccess)
{
	BlendProfileSelectedDelegate.ExecuteIfBound(EditableSkeleton.Pin()->GetBlendProfile(SelectedProfileName));
}

void SBlendProfilePicker::PostRedo(bool bSuccess)
{
	BlendProfileSelectedDelegate.ExecuteIfBound(EditableSkeleton.Pin()->GetBlendProfile(SelectedProfileName));
}

#undef LOCTEXT_NAMESPACE
