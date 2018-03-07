// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserDefinedEnumEditor.h"
#include "Editor.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"

#include "PropertyCustomizationHelpers.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Docking/SDockTab.h"
#include "IDocumentation.h"
#include "STextPropertyEditableTextBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UserDefinedEnumEditor"

const FName FUserDefinedEnumEditor::EnumeratorsTabId( TEXT( "UserDefinedEnum_EnumeratorEditor" ) );
const FName FUserDefinedEnumEditor::UserDefinedEnumEditorAppIdentifier( TEXT( "UserDefinedEnumEditorApp" ) );

/** Allows STextPropertyEditableTextBox to edit a user defined enum entry */
class FEditableTextUserDefinedEnum : public IEditableTextProperty
{
public:
	FEditableTextUserDefinedEnum(UUserDefinedEnum* InTargetEnum, const int32 InEnumeratorIndex)
		: TargetEnum(InTargetEnum)
		, EnumeratorIndex(InEnumeratorIndex)
		, bCausedChange(false)
	{
	}

	virtual bool IsMultiLineText() const override
	{
		return false;
	}

	virtual bool IsPassword() const override
	{
		return false;
	}

	virtual bool IsReadOnly() const override
	{
		return false;
	}

	virtual bool IsDefaultValue() const override
	{
		return false;
	}

	virtual FText GetToolTipText() const override
	{
		return FText::GetEmpty();
	}

	virtual int32 GetNumTexts() const override
	{
		return 1;
	}

	virtual FText GetText(const int32 InIndex) const override
	{
		check(InIndex == 0);
		return TargetEnum->GetDisplayNameTextByIndex(EnumeratorIndex);
	}

	virtual void SetText(const int32 InIndex, const FText& InText) override
	{
		check(InIndex == 0);
		TGuardValue<bool> CausingChange(bCausedChange, true);
		FEnumEditorUtils::SetEnumeratorDisplayName(TargetEnum, EnumeratorIndex, InText);
	}

	virtual bool IsValidText(const FText& InText, FText& OutErrorMsg) const override
	{
		bool bValidName = true;

		bool bUnchangedName = (InText.ToString() == TargetEnum->GetDisplayNameTextByIndex(EnumeratorIndex).ToString());
		if (InText.IsEmpty())
		{
			OutErrorMsg = LOCTEXT("NameMissingError", "You must provide a name.");
			bValidName = false;
		}
		else if (!FEnumEditorUtils::IsEnumeratorDisplayNameValid(TargetEnum, EnumeratorIndex, InText))
		{
			OutErrorMsg = FText::Format(LOCTEXT("NameInUseError", "'{0}' is already in use."), InText);
			bValidName = false;
		}

		return bValidName && !bUnchangedName;
	}

#if USE_STABLE_LOCALIZATION_KEYS
	virtual void GetStableTextId(const int32 InIndex, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey) const override
	{
		check(InIndex == 0);
		return StaticStableTextId(TargetEnum, InEditAction, InTextSource, InProposedNamespace, InProposedKey, OutStableNamespace, OutStableKey);
	}
#endif // USE_STABLE_LOCALIZATION_KEYS

	virtual void RequestRefresh() override
	{
	}

	bool CausedChange() const
	{
		return bCausedChange;
	}

private:
	/** The user defined enum being edited */
	UUserDefinedEnum* TargetEnum;

	/** Index of enumerator entry */
	int32 EnumeratorIndex;

	/** Set while we are invoking a change to the user defined enum */
	bool bCausedChange;
};



/** Allows STextPropertyEditableTextBox to edit the tooltip metadata for a user defined enum entry */
class FEditableTextUserDefinedEnumTooltip : public IEditableTextProperty
{
public:
	FEditableTextUserDefinedEnumTooltip(UUserDefinedEnum* InTargetEnum, const int32 InEnumeratorIndex)
		: TargetEnum(InTargetEnum)
		, EnumeratorIndex(InEnumeratorIndex)
		, bCausedChange(false)
	{
	}

	virtual bool IsMultiLineText() const override
	{
		return true;
	}

	virtual bool IsPassword() const override
	{
		return false;
	}

	virtual bool IsReadOnly() const override
	{
		return false;
	}

	virtual bool IsDefaultValue() const override
	{
		return false;
	}

	virtual FText GetToolTipText() const override
	{
		return FText::GetEmpty();
	}

	virtual int32 GetNumTexts() const override
	{
		return 1;
	}

	virtual FText GetText(const int32 InIndex) const override
	{
		check(InIndex == 0);
		return TargetEnum->GetToolTipTextByIndex(EnumeratorIndex);
	}

	virtual void SetText(const int32 InIndex, const FText& InText) override
	{
		check(InIndex == 0);
		TGuardValue<bool> CausingChange(bCausedChange, true);
		//@TODO: Metadata is not transactional right now, so we cannot transact a tooltip edit
		// const FScopedTransaction Transaction(NSLOCTEXT("EnumEditor", "SetEnumeratorTooltip", "Set Description"));
		TargetEnum->Modify();
		TargetEnum->SetMetaData(TEXT("ToolTip"), *InText.ToString(), EnumeratorIndex);
	}

	virtual bool IsValidText(const FText& InText, FText& OutErrorMsg) const override
	{
		return true;
	}

#if USE_STABLE_LOCALIZATION_KEYS
	virtual void GetStableTextId(const int32 InIndex, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey) const override
	{
		check(InIndex == 0);
		return StaticStableTextId(TargetEnum, InEditAction, InTextSource, InProposedNamespace, InProposedKey, OutStableNamespace, OutStableKey);
	}
#endif // USE_STABLE_LOCALIZATION_KEYS

	virtual void RequestRefresh() override
	{
	}

	bool CausedChange() const
	{
		return bCausedChange;
	}

private:
	/** The user defined enum being edited */
	UUserDefinedEnum* TargetEnum;

	/** Index of enumerator entry */
	int32 EnumeratorIndex;

	/** Set while we are invoking a change to the user defined enum */
	bool bCausedChange;
};






void FUserDefinedEnumEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_UserDefinedEnumEditor", "User-Defined Enum Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( EnumeratorsTabId, FOnSpawnTab::CreateSP(this, &FUserDefinedEnumEditor::SpawnEnumeratorsTab) )
		.SetDisplayName( LOCTEXT("EnumeratorEditor", "Enumerators") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "GraphEditor.Enum_16x"));
}

void FUserDefinedEnumEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( EnumeratorsTabId );
}

void FUserDefinedEnumEditor::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UUserDefinedEnum* EnumToEdit)
{
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_UserDefinedEnumEditor_Layout_v1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell( true )
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->Split
			(
				FTabManager::NewStack()
				->AddTab( EnumeratorsTabId, ETabState::OpenedTab )
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, UserDefinedEnumEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, EnumToEdit );

	// @todo toolkit world centric editing
	/*if (IsWorldCentricAssetEditor())
	{
		SpawnToolkitTab(GetToolbarTabId(), FString(), EToolkitTabSpot::ToolBar);
		const FString TabInitializationPayload(TEXT(""));	
		SpawnToolkitTab( EnumeratorsTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/

	//
}

TSharedRef<SDockTab> FUserDefinedEnumEditor::SpawnEnumeratorsTab(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == EnumeratorsTabId );

	UUserDefinedEnum* EditedEnum = NULL;
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	if (EditingObjs.Num())
	{
		EditedEnum = Cast<UUserDefinedEnum>(EditingObjs[ 0 ]);
	}

	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs( /*bUpdateFromSelection=*/ false, /*bLockable=*/ false, /*bAllowSearch=*/ false, FDetailsViewArgs::HideNameArea, /*bHideSelectionTip=*/ true );
	DetailsViewArgs.bShowOptions = false;

	PropertyView = EditModule.CreateDetailView( DetailsViewArgs );

	FOnGetDetailCustomizationInstance LayoutEnumDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FEnumDetails::MakeInstance);
	PropertyView->RegisterInstancedCustomPropertyLayout(UUserDefinedEnum::StaticClass(), LayoutEnumDetails);

	PropertyView->SetObject(EditedEnum);

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("GenericEditor.Tabs.Properties") )
		.Label( LOCTEXT("EnumeratorEditor", "Enumerators") )
		.TabColorScale( GetTabColorScale() )
		[
			PropertyView.ToSharedRef()
		];
}

FUserDefinedEnumEditor::~FUserDefinedEnumEditor()
{
}

FName FUserDefinedEnumEditor::GetToolkitFName() const
{
	return FName("EnumEditor");
}

FText FUserDefinedEnumEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "Enum Editor" );
}

FText FUserDefinedEnumEditor::GetToolkitName() const
{
	if (1 == GetEditingObjects().Num())
	{
		return FAssetEditorToolkit::GetToolkitName();
	}
	return GetBaseToolkitName();
}

FText FUserDefinedEnumEditor::GetToolkitToolTipText() const
{
	if (1 == GetEditingObjects().Num())
	{
		return FAssetEditorToolkit::GetToolkitToolTipText();
	}
	return GetBaseToolkitName();
}

FString FUserDefinedEnumEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("UDEnumWorldCentricTabPrefix", "Enum ").ToString();
}

FLinearColor FUserDefinedEnumEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.0f, 0.0f, 0.5f );
}

void FEnumDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	const TArray<TWeakObjectPtr<UObject>>& Objects = DetailLayout.GetSelectedObjects();
	check(Objects.Num() > 0);

	if (Objects.Num() == 1)
	{
		TargetEnum = CastChecked<UUserDefinedEnum>(Objects[0].Get());
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(FName("Names"), UEnum::StaticClass());

		const FString DocLink = TEXT("Shared/Editors/BlueprintEditor/EnumDetails");

		IDetailCategoryBuilder& InputsCategory = DetailLayout.EditCategory("Enumerators", LOCTEXT("EnumDetailsEnumerators", "Enumerators"));

		InputsCategory.AddCustomRow( LOCTEXT("FunctionNewInputArg", "New") )
			[
				SNew(SBox)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.Text(LOCTEXT("FunctionNewInputArg", "New"))
					.OnClicked(this, &FEnumDetails::OnAddNewEnumerator)
				]
			];

		Layout = MakeShareable( new FUserDefinedEnumLayout(TargetEnum.Get()) );
		InputsCategory.AddCustomBuilder( Layout.ToSharedRef() );

		TSharedPtr<SToolTip> BitmaskFlagsTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("BitmaskFlagsTooltip", "When enabled, this enumeration can be used as a set of explicitly-named bitmask flags. Each enumerator's value will correspond to the index of the bit (flag) in the mask."), nullptr, DocLink, TEXT("Bitmask Flags"));

		InputsCategory.AddCustomRow(LOCTEXT("BitmaskFlagsAttributeLabel", "Bitmask Flags"), true)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BitmaskFlagsAttributeLabel", "Bitmask Flags"))
			.ToolTip(BitmaskFlagsTooltip)
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FEnumDetails::OnGetBitmaskFlagsAttributeState)
			.OnCheckStateChanged(this, &FEnumDetails::OnBitmaskFlagsAttributeStateChanged)
			.ToolTip(BitmaskFlagsTooltip)
		];
	}
}

FEnumDetails::FEnumDetails()
	: TargetEnum(nullptr)
{
	GEditor->RegisterForUndo(this);
}

FEnumDetails::~FEnumDetails()
{
	GEditor->UnregisterForUndo( this );
}

void FEnumDetails::OnForceRefresh()
{
	if (Layout.IsValid())
	{
		Layout->Refresh();
	}
}

void FEnumDetails::PostUndo(bool bSuccess)
{
	OnForceRefresh();
}

void FEnumDetails::PreChange(const class UUserDefinedEnum* Enum, FEnumEditorUtils::EEnumEditorChangeInfo Info)
{
}

void FEnumDetails::PostChange(const class UUserDefinedEnum* Enum, FEnumEditorUtils::EEnumEditorChangeInfo Info)
{
	if (Enum && (TargetEnum.Get() == Enum))
	{
		OnForceRefresh();
	}
}

FReply FEnumDetails::OnAddNewEnumerator()
{
	FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(TargetEnum.Get());
	return FReply::Handled();
}

ECheckBoxState FEnumDetails::OnGetBitmaskFlagsAttributeState() const
{
	return FEnumEditorUtils::IsEnumeratorBitflagsType(TargetEnum.Get()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FEnumDetails::OnBitmaskFlagsAttributeStateChanged(ECheckBoxState InNewState)
{
	FEnumEditorUtils::SetEnumeratorBitflagsTypeState(TargetEnum.Get(), InNewState == ECheckBoxState::Checked);
}

bool FUserDefinedEnumLayout::CausedChange() const
{
	for (const TWeakPtr<class FUserDefinedEnumIndexLayout>& Child : Children)
	{
		if (Child.IsValid() && Child.Pin()->CausedChange())
		{
			return true;
		}
	}
	return false;
}

void FUserDefinedEnumLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	const int32 EnumToShowNum = FMath::Max<int32>(0, TargetEnum->NumEnums() - 1);
	Children.Reset(EnumToShowNum);
	for (int32 EnumIdx = 0; EnumIdx < EnumToShowNum; ++EnumIdx)
	{
		TSharedRef<class FUserDefinedEnumIndexLayout> EnumIndexLayout = MakeShareable(new FUserDefinedEnumIndexLayout(TargetEnum.Get(), EnumIdx) );
		ChildrenBuilder.AddCustomBuilder(EnumIndexLayout);
		Children.Add(EnumIndexLayout);
	}
}


bool FUserDefinedEnumIndexLayout::CausedChange() const
{
	return (DisplayNameEditor.IsValid() && DisplayNameEditor->CausedChange()) || (TooltipEditor.IsValid() && TooltipEditor->CausedChange());
}

void FUserDefinedEnumIndexLayout::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	DisplayNameEditor = MakeShared<FEditableTextUserDefinedEnum>(TargetEnum, EnumeratorIndex);

	TooltipEditor = MakeShared<FEditableTextUserDefinedEnumTooltip>(TargetEnum, EnumeratorIndex);

	const bool bIsEditable = !DisplayNameEditor->IsReadOnly();
	const bool bIsMoveUpEnabled = (TargetEnum->NumEnums() != 1) && (EnumeratorIndex != 0) && bIsEditable;
	const bool bIsMoveDownEnabled = (TargetEnum->NumEnums() != 1) && (EnumeratorIndex < TargetEnum->NumEnums() - 2) && bIsEditable;

	TSharedRef< SWidget > ClearButton = PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FUserDefinedEnumIndexLayout::OnEnumeratorRemove));
	ClearButton->SetEnabled(bIsEditable);

	NodeRow
		.WholeRowWidget
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EnumDisplayNameLabel", "Display Name"))
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
				SNew(STextPropertyEditableTextBox, DisplayNameEditor.ToSharedRef())
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(4, 0, 4, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EnumTooltipLabel", "Description"))
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
				SNew(STextPropertyEditableTextBox, TooltipEditor.ToSharedRef())
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FUserDefinedEnumIndexLayout::OnMoveEnumeratorUp)
				.IsEnabled( bIsMoveUpEnabled )
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("BlueprintEditor.Details.ArgUpButton"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FUserDefinedEnumIndexLayout::OnMoveEnumeratorDown)
				.IsEnabled( bIsMoveDownEnabled )
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("BlueprintEditor.Details.ArgDownButton"))
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0)
			.VAlign(VAlign_Center)
			[
				ClearButton
			]
		];
}

void FUserDefinedEnumIndexLayout::OnEnumeratorRemove()
{
	FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(TargetEnum, EnumeratorIndex);
}

FReply FUserDefinedEnumIndexLayout::OnMoveEnumeratorUp()
{
	FEnumEditorUtils::MoveEnumeratorInUserDefinedEnum(TargetEnum, EnumeratorIndex, true);
	return FReply::Handled();
}

FReply FUserDefinedEnumIndexLayout::OnMoveEnumeratorDown()
{
	FEnumEditorUtils::MoveEnumeratorInUserDefinedEnum(TargetEnum, EnumeratorIndex, false);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
