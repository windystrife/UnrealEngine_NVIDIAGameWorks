// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetDetailsCustomization.h"
#include "SCompoundWidget.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "MultiBoxBuilder.h"
#include "PhysicsAssetEditorActions.h"
#include "PhysicsAssetEditor.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsAssetEditorSkeletalMeshComponent.h"
#include "SInlineEditableTextBlock.h"
#include "EditorFontGlyphs.h"
#include "SUniformGridPanel.h"
#include "SlateApplication.h"
#include "SButton.h"
#include "SImage.h"
#include "ScopedTransaction.h"
#include "SComboButton.h"
#include "SEditableTextBox.h"
#include "PropertyHandle.h"
#include "PhysicsAssetEditorActions.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetDetailsCustomization"

TSharedRef<IDetailCustomization> FPhysicsAssetDetailsCustomization::MakeInstance(TWeakPtr<FPhysicsAssetEditor> InPhysicsAssetEditor)
{
	return MakeShared<FPhysicsAssetDetailsCustomization>(InPhysicsAssetEditor);
}

void FPhysicsAssetDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	CommandList = MakeShared<FUICommandList>();

	BindCommands();

	DetailLayout.HideCategory(TEXT("Profiles"));

	PhysicalAnimationProfilesHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPhysicsAsset, PhysicalAnimationProfiles));
	ConstraintProfilesHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPhysicsAsset, ConstraintProfiles));

	DetailLayout.EditCategory(TEXT("Physical Animation Profiles"))
	.AddProperty(PhysicalAnimationProfilesHandle)
	.CustomWidget()
	.WholeRowContent()
	[
		MakePhysicalAnimationProfilesWidget()
	];

	DetailLayout.EditCategory(TEXT("Constraint Profiles"))
	.AddProperty(ConstraintProfilesHandle)
	.CustomWidget()
	.WholeRowContent()
	[
		MakeConstraintProfilesWidget()
	];
}

void FPhysicsAssetDetailsCustomization::BindCommands()
{
	const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

	CommandList->MapAction(
		Commands.NewPhysicalAnimationProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::NewPhysicalAnimationProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanCreateNewPhysicalAnimationProfile)
	);

	CommandList->MapAction(
		Commands.DuplicatePhysicalAnimationProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::DuplicatePhysicalAnimationProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanDuplicatePhysicalAnimationProfile)
	);

	CommandList->MapAction(
		Commands.DeleteCurrentPhysicalAnimationProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::DeleteCurrentPhysicalAnimationProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanDeleteCurrentPhysicalAnimationProfile)
	);

	CommandList->MapAction(
		Commands.AddBodyToPhysicalAnimationProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::AddBodyToPhysicalAnimationProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanAddBodyToPhysicalAnimationProfile)
	);

	CommandList->MapAction(
		Commands.RemoveBodyFromPhysicalAnimationProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::RemoveBodyFromPhysicalAnimationProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanRemoveBodyFromPhysicalAnimationProfile)
	);

	CommandList->MapAction(
		Commands.NewConstraintProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::NewConstraintProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanCreateNewConstraintProfile)
	);

	CommandList->MapAction(
		Commands.DuplicateConstraintProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::DuplicateConstraintProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanDuplicateConstraintProfile)
	);

	CommandList->MapAction(
		Commands.DeleteCurrentConstraintProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::DeleteCurrentConstraintProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanDeleteCurrentConstraintProfile)
	);

	CommandList->MapAction(
		Commands.AddConstraintToCurrentConstraintProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::AddConstraintToCurrentConstraintProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanAddConstraintToCurrentConstraintProfile)
	);

	CommandList->MapAction(
		Commands.RemoveConstraintFromCurrentConstraintProfile,
		FExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::RemoveConstraintFromCurrentConstraintProfile),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetDetailsCustomization::CanRemoveConstraintFromCurrentConstraintProfile)
	);
}

static TSharedRef< SWidget > FillPhysicalAnimationProfileOptions(TSharedRef<FUICommandList> InCommandList, TSharedPtr<FPhysicsAssetEditorSharedData> SharedData)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList);

	const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

	if(SharedData->PhysicsAsset)
	{
		MenuBuilder.BeginSection("CurrentProfile", LOCTEXT("PhysicsAssetEditor_CurrentPhysicalAnimationMenu", "Current Profile"));
		{
			MenuBuilder.AddMenuEntry(Commands.DuplicatePhysicalAnimationProfile);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("PhysicalAnimationProfile", LOCTEXT("PhysicsAssetEditor_PhysicalAnimationMenu", "Physical Animation Profiles"));
		{
			TArray<FName> ProfileNames;
			ProfileNames.Add(NAME_None);
			ProfileNames.Append(SharedData->PhysicsAsset->GetPhysicalAnimationProfileNames());
					
			//Make sure we don't have multiple Nones if user forgot to name profile
			for(int32 ProfileIdx = ProfileNames.Num()-1; ProfileIdx > 0; --ProfileIdx)
			{
				if(ProfileNames[ProfileIdx] == NAME_None)
				{
					ProfileNames.RemoveAtSwap(ProfileIdx);
				}
			}
				
			for(FName ProfileName : ProfileNames)
			{
				FUIAction Action;
				Action.ExecuteAction = FExecuteAction::CreateLambda( [SharedData, ProfileName]()
				{
					FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);	//Ensure focus is removed because the menu has already closed and the cached value (the one the user has typed) is going to apply to the new profile
					SharedData->PhysicsAsset->CurrentPhysicalAnimationProfileName = ProfileName;
					for(USkeletalBodySetup* BS : SharedData->PhysicsAsset->SkeletalBodySetups)
					{
						if(FPhysicalAnimationProfile* Profile = BS->FindPhysicalAnimationProfile(ProfileName))
						{
							BS->CurrentPhysicalAnimationProfile = *Profile;
						}
					}
				});

				Action.GetActionCheckState = FGetActionCheckState::CreateLambda([SharedData, ProfileName]()
				{
					return SharedData->PhysicsAsset->CurrentPhysicalAnimationProfileName == ProfileName ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				});

				auto SearchClickedLambda = [ProfileName, SharedData]()
				{
					SharedData->ClearSelectedBody();	//clear selection
					for (int32 BSIndex = 0; BSIndex < SharedData->PhysicsAsset->SkeletalBodySetups.Num(); ++BSIndex)
					{
						const USkeletalBodySetup* BS = SharedData->PhysicsAsset->SkeletalBodySetups[BSIndex];
						if (BS->FindPhysicalAnimationProfile(ProfileName))
						{
							SharedData->SetSelectedBodyAnyPrim(BSIndex, true);
						}
					}

					FSlateApplication::Get().DismissAllMenus();

					return FReply::Handled();
				};
					
				TSharedRef<SWidget> PhysAnimProfileButton = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(ProfileName.ToString()))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.f, 0.f, 0.f, 0.f)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.ToolTipText(LOCTEXT("SelectBodies", "Select all bodies that are assigned to this profile."))
						.OnClicked_Lambda(SearchClickedLambda)
						[
							SNew(SBox)
							.WidthOverride(MultiBoxConstants::MenuIconSize)
							.HeightOverride(MultiBoxConstants::MenuIconSize)
							.Visibility_Lambda([ProfileName](){ return ProfileName == NAME_None ? EVisibility::Collapsed : EVisibility::Visible; })
							[
								SNew(SImage)
								.Image(FSlateIcon(FEditorStyle::GetStyleSetName(), "Symbols.SearchGlass").GetIcon())
							]
								
						]
					];


				//MenuBuilder.AddMenuEntry( FText::FromString(ProfileName.ToString()), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::Check);
				MenuBuilder.AddMenuEntry(Action, PhysAnimProfileButton, NAME_None, TAttribute<FText>(), EUserInterfaceActionType::Check);
			}
		}

		MenuBuilder.EndSection();
	}

			
	return MenuBuilder.MakeWidget();
}

static TSharedRef< SWidget > FillConstraintProfilesOptions(TSharedRef<FUICommandList> InCommandList, TSharedPtr<FPhysicsAssetEditorSharedData> SharedData)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList);

	const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

	if(SharedData->PhysicsAsset)
	{
		MenuBuilder.BeginSection("CurrentProfile", LOCTEXT("PhysicsAssetEditor_CurrentProfileMenu", "Current Profile"));
		{
			MenuBuilder.AddMenuEntry(Commands.DuplicateConstraintProfile);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("ConstraintProfiles", LOCTEXT("PhysicsAssetEditor_ConstraintProfileMenu", "Constraint Profiles"));
		{
			TArray<FName> ProfileNames;
			ProfileNames.Add(NAME_None);
			ProfileNames.Append(SharedData->PhysicsAsset->GetConstraintProfileNames());

			//Make sure we don't have multiple Nones if user forgot to name profile
			for (int32 ProfileIdx = ProfileNames.Num() - 1; ProfileIdx > 0; --ProfileIdx)
			{
				if (ProfileNames[ProfileIdx] == NAME_None)
				{
					ProfileNames.RemoveAtSwap(ProfileIdx);
				}
			}

			for (FName ProfileName : ProfileNames)
			{
				FUIAction Action;
				Action.ExecuteAction = FExecuteAction::CreateLambda([SharedData, ProfileName]()
				{
					FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);	//Ensure focus is removed because the menu has already closed and the cached value (the one the user has typed) is going to apply to the new profile
					SharedData->PhysicsAsset->CurrentConstraintProfileName = ProfileName;
					for (UPhysicsConstraintTemplate* CS : SharedData->PhysicsAsset->ConstraintSetup)
					{
						CS->ApplyConstraintProfile(ProfileName, CS->DefaultInstance, /*DefaultIfNotFound=*/ false);	//keep settings as they currently are if user wants to add to profile
					}

					SharedData->EditorSkelComp->SetConstraintProfileForAll(ProfileName, /*bDefaultIfNotFound=*/ true);
				});

				Action.GetActionCheckState = FGetActionCheckState::CreateLambda([SharedData, ProfileName]()
				{
					return SharedData->PhysicsAsset->CurrentConstraintProfileName == ProfileName ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				});

				auto SearchClickedLambda = [ProfileName, SharedData]()
				{
					SharedData->ClearSelectedConstraints();	//clear selection
					for (int32 CSIndex = 0; CSIndex < SharedData->PhysicsAsset->ConstraintSetup.Num(); ++CSIndex)
					{
						const UPhysicsConstraintTemplate* CS = SharedData->PhysicsAsset->ConstraintSetup[CSIndex];
						if (CS->ContainsConstraintProfile(ProfileName))
						{
							SharedData->SetSelectedConstraint(CSIndex, true);
						}
					}
							
					FSlateApplication::Get().DismissAllMenus();

					return FReply::Handled();
				};

				TSharedRef<SWidget> ConstraintProfileButton = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ProfileName.ToString()))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f, 0.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("SelectConstraints", "Select all constraints that are assigned to this profile."))
					.OnClicked_Lambda(SearchClickedLambda)
					[
						SNew(SBox)
						.WidthOverride(MultiBoxConstants::MenuIconSize)
						.HeightOverride(MultiBoxConstants::MenuIconSize)
						.Visibility_Lambda([ProfileName]() { return ProfileName == NAME_None ? EVisibility::Collapsed : EVisibility::Visible; })
						[
							SNew(SImage)
							.Image(FSlateIcon(FEditorStyle::GetStyleSetName(), "Symbols.SearchGlass").GetIcon())
						]
					]
				];

				MenuBuilder.AddMenuEntry(Action, ConstraintProfileButton, NAME_None, TAttribute<FText>(), EUserInterfaceActionType::Check);
			}
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void FPhysicsAssetDetailsCustomization::HandlePhysicalAnimationProfileNameCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if(InCommitType != ETextCommit::OnCleared)
	{
		

		TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();

		int32 PhysicalAnimationProfileIndex = INDEX_NONE;
		SharedData->PhysicsAsset->PhysicalAnimationProfiles.Find(SharedData->PhysicsAsset->CurrentPhysicalAnimationProfileName, PhysicalAnimationProfileIndex);
		if(PhysicalAnimationProfileIndex != INDEX_NONE)
		{
			TSharedPtr<IPropertyHandle> ChildHandle = PhysicalAnimationProfilesHandle->GetChildHandle(PhysicalAnimationProfileIndex);

			const FScopedTransaction Transaction(LOCTEXT("RenamePhysicalAnimationProfile", "Rename Physical Animation Profile"));

			const FName OldProfileName = SharedData->PhysicsAsset->CurrentPhysicalAnimationProfileName;

			SharedData->PhysicsAsset->Modify();
			SharedData->PhysicsAsset->CurrentPhysicalAnimationProfileName = *InText.ToString();
			ChildHandle->SetValue( SharedData->PhysicsAsset->CurrentPhysicalAnimationProfileName);
		}
	}
}

void FPhysicsAssetDetailsCustomization::HandleConstraintProfileNameCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if(InCommitType != ETextCommit::OnCleared)
	{
		TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();

		int32 ConstraintProfileIndex = INDEX_NONE;
		SharedData->PhysicsAsset->ConstraintProfiles.Find(SharedData->PhysicsAsset->CurrentConstraintProfileName, ConstraintProfileIndex);
		if(ConstraintProfileIndex != INDEX_NONE)
		{
			TSharedPtr<IPropertyHandle> ChildHandle = ConstraintProfilesHandle->GetChildHandle(ConstraintProfileIndex);

			const FScopedTransaction Transaction(LOCTEXT("RenameConstraintProfile", "Rename Constraint Profile"));

			const FName OldProfileName = SharedData->PhysicsAsset->CurrentConstraintProfileName;

			SharedData->PhysicsAsset->Modify();
			SharedData->PhysicsAsset->CurrentConstraintProfileName = *InText.ToString();
			ChildHandle->SetValue(SharedData->PhysicsAsset->CurrentConstraintProfileName);
		}
	}
}

TSharedRef<SWidget> FPhysicsAssetDetailsCustomization::CreateProfileButton(const FText& InGlyph, TSharedPtr<FUICommandInfo> InCommand)
{
	check(InCommand.IsValid());

	TWeakPtr<FUICommandInfo> LocalCommandPtr = InCommand;

	return SNew(SButton)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.ButtonStyle(FEditorStyle::Get(), "FlatButton")
		.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
		.ToolTipText(InCommand->GetDescription())
		.IsEnabled_Lambda([this, LocalCommandPtr]()
		{
			return CommandList->CanExecuteAction(LocalCommandPtr.Pin().ToSharedRef());
		})
		.OnClicked(FOnClicked::CreateLambda([this, LocalCommandPtr]()
		{
			return CommandList->ExecuteAction(LocalCommandPtr.Pin().ToSharedRef()) ? FReply::Handled() : FReply::Unhandled();
		}))
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "PhysicsAssetEditor.Profiles.Font")
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
				.Text(InGlyph)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 0, 0)
			[
				SNew(STextBlock)
				.TextStyle( FEditorStyle::Get(), "PhysicsAssetEditor.Profiles.Font" )
				.Text(InCommand->GetLabel())
			]
		];
}

TSharedRef<SWidget> FPhysicsAssetDetailsCustomization::MakePhysicalAnimationProfilesWidget()
{
	const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

	TWeakPtr<FPhysicsAssetEditor> LocalPhysicsAssetEditorPtr = PhysicsAssetEditorPtr;

	return SNew(SHorizontalBox)
		.ToolTipText(LOCTEXT("CurrentPhysicalAnimationProfileWidgetTooltip", "Select and edit the current physical animation profile."))
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "ToolBar.Button")
			.OnGetMenuContent_Static(&FillPhysicalAnimationProfileOptions, CommandList.ToSharedRef(), PhysicsAssetEditorPtr.Pin()->GetSharedData())
			.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
			.ButtonContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 2.0f, 3.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CurrentProfile", "Current Profile"))
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 2.0f, 0.0f)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([LocalPhysicsAssetEditorPtr]()
						{
							return FText::FromName(LocalPhysicsAssetEditorPtr.Pin()->GetSharedData()->PhysicsAsset->CurrentPhysicalAnimationProfileName);
						})
						.IsEnabled_Lambda([LocalPhysicsAssetEditorPtr]()
						{
							return LocalPhysicsAssetEditorPtr.Pin()->GetSharedData()->PhysicsAsset->CurrentPhysicalAnimationProfileName != NAME_None;
						})
						.Style(FEditorStyle::Get(), "PhysicsAssetEditor.Profiles.EditableTextBoxStyle")
						.OnTextCommitted(FOnTextCommitted::CreateSP(this, &FPhysicsAssetDetailsCustomization::HandlePhysicalAnimationProfileNameCommitted))
					]
				]
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FMargin(1.0f, 1.0f))
			+SUniformGridPanel::Slot(0, 0)
			[
				CreateProfileButton(FEditorFontGlyphs::File, Commands.NewPhysicalAnimationProfile)
			]
			+SUniformGridPanel::Slot(1, 0)
			[
				CreateProfileButton(FEditorFontGlyphs::Trash, Commands.DeleteCurrentPhysicalAnimationProfile)
			]
			+SUniformGridPanel::Slot(0, 1)
			[
				CreateProfileButton(FEditorFontGlyphs::Plus_Circle, Commands.AddBodyToPhysicalAnimationProfile)
			]
			+SUniformGridPanel::Slot(1, 1)
			[
				CreateProfileButton(FEditorFontGlyphs::Minus_Circle, Commands.RemoveBodyFromPhysicalAnimationProfile)
			]
		];
}

TSharedRef<SWidget> FPhysicsAssetDetailsCustomization::MakeConstraintProfilesWidget()
{
	const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

	TWeakPtr<FPhysicsAssetEditor> LocalPhysicsAssetEditorPtr = PhysicsAssetEditorPtr;

	return SNew(SHorizontalBox)
		.ToolTipText(LOCTEXT("CurrentConstraintProfileWidgetTooltip", "Select and edit the current constraint profile."))
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "ToolBar.Button")
			.OnGetMenuContent_Static(&FillConstraintProfilesOptions, CommandList.ToSharedRef(), PhysicsAssetEditorPtr.Pin()->GetSharedData())
			.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
			.ButtonContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 2.0f, 3.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CurrentProfile", "Current Profile"))
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 2.0f, 0.0f)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([LocalPhysicsAssetEditorPtr]()
						{
							return FText::FromName(LocalPhysicsAssetEditorPtr.Pin()->GetSharedData()->PhysicsAsset->CurrentConstraintProfileName);
						})
						.IsEnabled_Lambda([LocalPhysicsAssetEditorPtr]()
						{
							return LocalPhysicsAssetEditorPtr.Pin()->GetSharedData()->PhysicsAsset->CurrentConstraintProfileName != NAME_None;
						})
						.Style(FEditorStyle::Get(), "PhysicsAssetEditor.Profiles.EditableTextBoxStyle")
						.OnTextCommitted(FOnTextCommitted::CreateSP(this, &FPhysicsAssetDetailsCustomization::HandleConstraintProfileNameCommitted))
					]
				]
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FMargin(1.0f, 1.0f))
			+SUniformGridPanel::Slot(0, 0)
			[
				CreateProfileButton(FEditorFontGlyphs::File, Commands.NewConstraintProfile)
			]
			+SUniformGridPanel::Slot(1, 0)
			[
				CreateProfileButton(FEditorFontGlyphs::Trash, Commands.DeleteCurrentConstraintProfile)
			]
			+SUniformGridPanel::Slot(0, 1)
			[
				CreateProfileButton(FEditorFontGlyphs::Plus_Circle, Commands.AddConstraintToCurrentConstraintProfile)
			]
			+SUniformGridPanel::Slot(1, 1)
			[
				CreateProfileButton(FEditorFontGlyphs::Minus_Circle, Commands.RemoveConstraintFromCurrentConstraintProfile)
			]
		];
}

void FPhysicsAssetDetailsCustomization::ApplyPhysicalAnimationProfile(FName InName)
{
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	UPhysicsAsset* PhysicsAsset = SharedData->PhysicsAsset;
	SharedData->PhysicsAsset->CurrentPhysicalAnimationProfileName = InName;
	for(USkeletalBodySetup* BodySetup : SharedData->PhysicsAsset->SkeletalBodySetups)
	{
		if(FPhysicalAnimationProfile* Profile = BodySetup->FindPhysicalAnimationProfile(InName))
		{
			BodySetup->CurrentPhysicalAnimationProfile = *Profile;
		}
	}
}

void FPhysicsAssetDetailsCustomization::NewPhysicalAnimationProfile()
{
	const FScopedTransaction Transaction(LOCTEXT("AddPhysicalAnimationProfile", "Add Physical Animation Profile"));	
	TSharedPtr<IPropertyHandleArray> ArrayHandle = PhysicalAnimationProfilesHandle->AsArray();
	ArrayHandle->AddItem();
	
	// now apply the new profile
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	FName ProfileName = SharedData->PhysicsAsset->PhysicalAnimationProfiles.Last();
	ApplyPhysicalAnimationProfile(ProfileName);
}

bool FPhysicsAssetDetailsCustomization::CanCreateNewPhysicalAnimationProfile() const
{
	return PhysicsAssetEditorPtr.Pin()->IsNotSimulation();
}

void FPhysicsAssetDetailsCustomization::DuplicatePhysicalAnimationProfile()
{
	int32 PhysicalAnimationProfileIndex = INDEX_NONE;
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	UPhysicsAsset* PhysicsAsset = SharedData->PhysicsAsset;
	PhysicsAsset->PhysicalAnimationProfiles.Find(PhysicsAsset->CurrentPhysicalAnimationProfileName, PhysicalAnimationProfileIndex);
	if(PhysicalAnimationProfileIndex != INDEX_NONE)
	{
		const FScopedTransaction Transaction(LOCTEXT("DuplicatePhysicalAnimationProfile", "Duplicate Physical Animation Profile"));	
		TSharedPtr<IPropertyHandleArray> ArrayHandle = PhysicalAnimationProfilesHandle->AsArray();
		ArrayHandle->DuplicateItem(PhysicalAnimationProfileIndex);

		// now apply the new profile

		FName ProfileName = PhysicsAsset->PhysicalAnimationProfiles[PhysicalAnimationProfileIndex];
		ApplyPhysicalAnimationProfile(ProfileName);
	}
}

bool FPhysicsAssetDetailsCustomization::CanDuplicatePhysicalAnimationProfile() const
{
	UPhysicsAsset* PhysicsAsset = PhysicsAssetEditorPtr.Pin()->GetSharedData()->PhysicsAsset;
	return PhysicsAssetEditorPtr.Pin()->IsNotSimulation() && PhysicsAsset->CurrentPhysicalAnimationProfileName != NAME_None;
}

void FPhysicsAssetDetailsCustomization::DeleteCurrentPhysicalAnimationProfile()
{
	int32 PhysicalAnimationProfileIndex = INDEX_NONE;
	UPhysicsAsset* PhysicsAsset = PhysicsAssetEditorPtr.Pin()->GetSharedData()->PhysicsAsset;
	PhysicsAsset->PhysicalAnimationProfiles.Find(PhysicsAsset->CurrentPhysicalAnimationProfileName, PhysicalAnimationProfileIndex);
	if(PhysicalAnimationProfileIndex != INDEX_NONE)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeletePhysicalAnimationProfile", "Delete Physical Animation Profile"));
		PhysicalAnimationProfilesHandle->AsArray()->DeleteItem(PhysicalAnimationProfileIndex);
		ApplyPhysicalAnimationProfile(NAME_None);
	}
}

bool FPhysicsAssetDetailsCustomization::CanDeleteCurrentPhysicalAnimationProfile() const
{
	UPhysicsAsset* PhysicsAsset = PhysicsAssetEditorPtr.Pin()->GetSharedData()->PhysicsAsset;
	return PhysicsAssetEditorPtr.Pin()->IsNotSimulation() && PhysicsAsset->CurrentPhysicalAnimationProfileName != NAME_None;
}

void FPhysicsAssetDetailsCustomization::AddBodyToPhysicalAnimationProfile()
{
	const FScopedTransaction Transaction(LOCTEXT("AssignToPhysicalAnimationProfile", "Assign To Physical Animation Profile"));
	
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	UPhysicsAsset* PhysicsAsset = SharedData->PhysicsAsset;
	for(int32 BodySetupIndex = 0; BodySetupIndex < SharedData->SelectedBodies.Num(); ++BodySetupIndex)
	{
		USkeletalBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[BodySetupIndex].Index];
		if(BodySetup)
		{
			BodySetup->Modify();
			FName ProfileName = BodySetup->GetCurrentPhysicalAnimationProfileName();
			if (!BodySetup->FindPhysicalAnimationProfile(ProfileName))
			{
				BodySetup->CurrentPhysicalAnimationProfile = FPhysicalAnimationProfile();
				BodySetup->AddPhysicalAnimationProfile(ProfileName);
			}
		}
	}
}

bool FPhysicsAssetDetailsCustomization::CanAddBodyToPhysicalAnimationProfile() const
{
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	TWeakPtr<FPhysicsAssetEditorSharedData> WeakSharedData = SharedData;
	UPhysicsAsset* PhysicsAsset = SharedData->PhysicsAsset;	

	auto PhysicalAnimationProfileExistsForAll = [WeakSharedData]()
	{
		TSharedPtr<FPhysicsAssetEditorSharedData> LocalSharedData = WeakSharedData.Pin();

		for(int32 BodySetupIndex = 0; BodySetupIndex < LocalSharedData->SelectedBodies.Num(); ++BodySetupIndex)
		{
			USkeletalBodySetup* BodySetup = LocalSharedData->PhysicsAsset->SkeletalBodySetups[LocalSharedData->SelectedBodies[BodySetupIndex].Index];
			if(BodySetup)
			{
				if (!BodySetup->FindPhysicalAnimationProfile(BodySetup->GetCurrentPhysicalAnimationProfileName()))
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}

		return true;
	};

	const bool bSelectedBodies = SharedData->SelectedBodies.Num() > 0;
	return (PhysicsAssetEditorPtr.Pin()->IsNotSimulation() && bSelectedBodies && !PhysicalAnimationProfileExistsForAll() && PhysicsAsset->CurrentPhysicalAnimationProfileName != NAME_None);
}

void FPhysicsAssetDetailsCustomization::RemoveBodyFromPhysicalAnimationProfile()
{
	const FScopedTransaction Transaction(LOCTEXT("UnassignFromPhysicalAnimationProfile", "Unassign From Physical Animation Profile"));

	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	UPhysicsAsset* PhysicsAsset = SharedData->PhysicsAsset;
	for(int32 BodySetupIndex = 0; BodySetupIndex < SharedData->SelectedBodies.Num(); ++BodySetupIndex)
	{
		USkeletalBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[BodySetupIndex].Index];
		if(BodySetup)
		{
			FName ProfileName = BodySetup->GetCurrentPhysicalAnimationProfileName();
			BodySetup->RemovePhysicalAnimationProfile(ProfileName);
		}
	}
}

bool FPhysicsAssetDetailsCustomization::CanRemoveBodyFromPhysicalAnimationProfile() const
{
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	TWeakPtr<FPhysicsAssetEditorSharedData> WeakSharedData = SharedData;
	UPhysicsAsset* PhysicsAsset = SharedData->PhysicsAsset;

	auto PhysicalAnimationProfileExistsForAny = [WeakSharedData]()
	{
		TSharedPtr<FPhysicsAssetEditorSharedData> LocalSharedData = WeakSharedData.Pin();

		for(int32 BodySetupIndex = 0; BodySetupIndex < LocalSharedData->SelectedBodies.Num(); ++BodySetupIndex)
		{
			USkeletalBodySetup* BodySetup = LocalSharedData->PhysicsAsset->SkeletalBodySetups[LocalSharedData->SelectedBodies[BodySetupIndex].Index];
			if(BodySetup && BodySetup->FindPhysicalAnimationProfile(BodySetup->GetCurrentPhysicalAnimationProfileName()))
			{
				return true;
			}
		}

		return false;
	};

	const bool bSelectedBodies = SharedData->SelectedBodies.Num() > 0;
	return (PhysicsAssetEditorPtr.Pin()->IsNotSimulation() && bSelectedBodies && PhysicalAnimationProfileExistsForAny() && PhysicsAsset->CurrentPhysicalAnimationProfileName != NAME_None);
}

void FPhysicsAssetDetailsCustomization::ApplyConstraintProfile(FName InName)
{
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();

	SharedData->PhysicsAsset->CurrentConstraintProfileName = InName;
	for (UPhysicsConstraintTemplate* CS : SharedData->PhysicsAsset->ConstraintSetup)
	{
		CS->ApplyConstraintProfile(InName, CS->DefaultInstance, /*DefaultIfNotFound=*/ false);	//keep settings as they currently are if user wants to add to profile
	}

	SharedData->EditorSkelComp->SetConstraintProfileForAll(InName, /*bDefaultIfNotFound=*/ true);
}

bool FPhysicsAssetDetailsCustomization::ConstraintProfileExistsForAny() const
{
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	const FName ProfileName = SharedData->PhysicsAsset->CurrentConstraintProfileName;
	for(int32 ConstraintIndex = 0; ConstraintIndex < SharedData->SelectedConstraints.Num(); ++ConstraintIndex)
	{
		UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[SharedData->SelectedConstraints[ConstraintIndex].Index];
		if(ConstraintSetup && ConstraintSetup->ContainsConstraintProfile(ProfileName))
		{
			return true;
		}
	}

	return false;
}

void FPhysicsAssetDetailsCustomization::NewConstraintProfile()
{
	const FScopedTransaction Transaction(LOCTEXT("AddConstraintProfile", "Add Constraint Profile"));	
	TSharedPtr<IPropertyHandleArray> ArrayHandle = ConstraintProfilesHandle->AsArray();
	ArrayHandle->AddItem();

	// now apply the new profile
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	FName ProfileName = SharedData->PhysicsAsset->ConstraintProfiles.Last();

	ApplyConstraintProfile(ProfileName);
}

bool FPhysicsAssetDetailsCustomization::CanCreateNewConstraintProfile() const
{
	return PhysicsAssetEditorPtr.Pin()->IsNotSimulation();
}

void FPhysicsAssetDetailsCustomization::DuplicateConstraintProfile()
{
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	UPhysicsAsset* PhysicsAsset = SharedData->PhysicsAsset;
	int32 ConstraintProfileIndex = INDEX_NONE;
	PhysicsAsset->ConstraintProfiles.Find(PhysicsAsset->CurrentConstraintProfileName, ConstraintProfileIndex);
	if(ConstraintProfileIndex != INDEX_NONE)
	{
		const FScopedTransaction Transaction(LOCTEXT("DuplicateConstraintProfile", "Duplicate Constraint Profile"));	
		TSharedPtr<IPropertyHandleArray> ArrayHandle = ConstraintProfilesHandle->AsArray();
		ArrayHandle->DuplicateItem(ConstraintProfileIndex);

		// now apply the new profile
		FName ProfileName = PhysicsAsset->ConstraintProfiles[ConstraintProfileIndex];
		ApplyConstraintProfile(ProfileName);
	}
}

bool FPhysicsAssetDetailsCustomization::CanDuplicateConstraintProfile() const
{
	UPhysicsAsset* PhysicsAsset = PhysicsAssetEditorPtr.Pin()->GetSharedData()->PhysicsAsset;
	return PhysicsAssetEditorPtr.Pin()->IsNotSimulation() && PhysicsAsset->CurrentConstraintProfileName != NAME_None;
}

void FPhysicsAssetDetailsCustomization::DeleteCurrentConstraintProfile()
{
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	int32 ConstraintProfileIndex = INDEX_NONE;
	SharedData->PhysicsAsset->ConstraintProfiles.Find(SharedData->PhysicsAsset->CurrentConstraintProfileName, ConstraintProfileIndex);
	if(ConstraintProfileIndex != INDEX_NONE)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteConstraintProfile", "Delete Constraint Profile"));
		ConstraintProfilesHandle->AsArray()->DeleteItem(ConstraintProfileIndex);
		ApplyConstraintProfile(NAME_None);
	}
}

bool FPhysicsAssetDetailsCustomization::CanDeleteCurrentConstraintProfile() const
{
	UPhysicsAsset* PhysicsAsset = PhysicsAssetEditorPtr.Pin()->GetSharedData()->PhysicsAsset;
	return PhysicsAssetEditorPtr.Pin()->IsNotSimulation() && PhysicsAsset->CurrentConstraintProfileName != NAME_None;
}

void FPhysicsAssetDetailsCustomization::AddConstraintToCurrentConstraintProfile()
{
	const FScopedTransaction Transaction(LOCTEXT("AssignToConstraintProfile", "Assign To Constraint Profile"));
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	for(int32 ConstraintIndex = 0; ConstraintIndex < SharedData->SelectedConstraints.Num(); ++ConstraintIndex)
	{
		UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[SharedData->SelectedConstraints[ConstraintIndex].Index];
		FName ProfileName = ConstraintSetup->GetCurrentConstraintProfileName();
		if (!ConstraintSetup->ContainsConstraintProfile(ProfileName))
		{
			ConstraintSetup->Modify();
			ConstraintSetup->AddConstraintProfile(ProfileName);
		}
	}
}

bool FPhysicsAssetDetailsCustomization::CanAddConstraintToCurrentConstraintProfile() const
{
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	TWeakPtr<FPhysicsAssetEditorSharedData> WeakSharedData = SharedData;
	UPhysicsAsset* PhysicsAsset = SharedData->PhysicsAsset;

	auto ConstraintProfileExistsForAll = [WeakSharedData]()
	{
		TSharedPtr<FPhysicsAssetEditorSharedData> LocalSharedData = WeakSharedData.Pin();

		const FName ProfileName = LocalSharedData->PhysicsAsset->CurrentConstraintProfileName;
		for(int32 ConstraintIndex = 0; ConstraintIndex < LocalSharedData->SelectedConstraints.Num(); ++ConstraintIndex)
		{
			UPhysicsConstraintTemplate* ConstraintSetup = LocalSharedData->PhysicsAsset->ConstraintSetup[LocalSharedData->SelectedConstraints[ConstraintIndex].Index];
			if(ConstraintSetup)
			{
				if(!ConstraintSetup->ContainsConstraintProfile(ProfileName))
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}

		return true;
	};

	const bool bSelectedConstraints = SharedData->SelectedConstraints.Num() > 0;
	return (PhysicsAssetEditorPtr.Pin()->IsNotSimulation() && bSelectedConstraints && PhysicsAsset->CurrentConstraintProfileName != NAME_None && !ConstraintProfileExistsForAll());
}

void FPhysicsAssetDetailsCustomization::RemoveConstraintFromCurrentConstraintProfile()
{
	const FScopedTransaction Transaction(LOCTEXT("UnassignFromConstraintProfile", "Unassign From Constraint Profile"));
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	for(int32 ConstraintIndex = 0; ConstraintIndex < SharedData->SelectedConstraints.Num(); ++ConstraintIndex)
	{
		UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[SharedData->SelectedConstraints[ConstraintIndex].Index];

		ConstraintSetup->Modify();
		FName ProfileName = ConstraintSetup->GetCurrentConstraintProfileName();
		ConstraintSetup->RemoveConstraintProfile(ProfileName);
	}
}

bool FPhysicsAssetDetailsCustomization::CanRemoveConstraintFromCurrentConstraintProfile() const
{
	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetEditorPtr.Pin()->GetSharedData();
	TWeakPtr<FPhysicsAssetEditorSharedData> WeakSharedData = SharedData;
	UPhysicsAsset* PhysicsAsset = SharedData->PhysicsAsset;

	auto ConstraintProfileExistsForAny = [WeakSharedData]()
	{
		TSharedPtr<FPhysicsAssetEditorSharedData> LocalSharedData = WeakSharedData.Pin();

		const FName ProfileName = LocalSharedData->PhysicsAsset->CurrentConstraintProfileName;
		for(int32 ConstraintIndex = 0; ConstraintIndex < LocalSharedData->SelectedConstraints.Num(); ++ConstraintIndex)
		{
			UPhysicsConstraintTemplate* ConstraintSetup = LocalSharedData->PhysicsAsset->ConstraintSetup[LocalSharedData->SelectedConstraints[ConstraintIndex].Index];
			if(ConstraintSetup && ConstraintSetup->ContainsConstraintProfile(ProfileName))
			{
				return true;
			}
		}

		return false;
	};

	const bool bSelectedConstraints = SharedData->SelectedConstraints.Num() > 0;
	return (PhysicsAssetEditorPtr.Pin()->IsNotSimulation() && bSelectedConstraints && PhysicsAsset->CurrentConstraintProfileName != NAME_None && ConstraintProfileExistsForAny());
}

#undef LOCTEXT_NAMESPACE