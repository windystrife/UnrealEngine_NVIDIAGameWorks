// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserDefinedStructureEditor.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/CoreMisc.h"
#include "UObject/UnrealType.h"
#include "Modules/ModuleManager.h"
#include "UObject/StructOnScope.h"
#include "Misc/NotifyHook.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Engine/UserDefinedStruct.h"
#include "EdGraphSchema_K2.h"
#include "IDetailCustomization.h"
#include "PropertyEditorModule.h"
#include "IDetailCustomNodeBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Layout/SSplitter.h"

#include "PropertyCustomizationHelpers.h"
#include "SPinTypeSelector.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "StructureEditor"

///////////////////////////////////////////////////////////////////////////////////////
// FDefaultValueDetails

class FDefaultValueDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance(TWeakPtr<class FStructureDefaultValueView> InDefaultValueView, TSharedPtr<FStructOnScope> InStructData)
	{
		return MakeShareable(new FDefaultValueDetails(InDefaultValueView, InStructData));
	}

	FDefaultValueDetails(TWeakPtr<class FStructureDefaultValueView> InDefaultValueView, TSharedPtr<FStructOnScope> InStructData)
		: DefaultValueView(InDefaultValueView)
		, StructData(InStructData)
	{}

	~FDefaultValueDetails()
	{
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailLayout) override;

	/** Callback when finished changing properties to export the default value from the property to where strings are stored */
	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

private:
	TWeakObjectPtr<UUserDefinedStruct> UserDefinedStruct;
	TWeakPtr<class FStructureDefaultValueView> DefaultValueView;
	TSharedPtr<FStructOnScope> StructData;
	IDetailLayoutBuilder* DetailLayoutPtr;
};

void FDefaultValueDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailLayout) 
{
	DetailLayoutPtr = &DetailLayout;
	const TArray<TWeakObjectPtr<UObject>>& Objects = DetailLayout.GetSelectedObjects();
	check(Objects.Num() > 0);

	if (Objects.Num() == 1)
	{
		UserDefinedStruct = CastChecked<UUserDefinedStruct>(Objects[0].Get());

		const IDetailsView* DetailsView = DetailLayout.GetDetailsView();
		DetailsView->OnFinishedChangingProperties().AddSP(this, &FDefaultValueDetails::OnFinishedChangingProperties);

		IDetailCategoryBuilder& StructureCategory = DetailLayout.EditCategory("DefaultValues", LOCTEXT("DefaultValues", "Default Values"));

		for (TFieldIterator<UProperty> PropertyIter(UserDefinedStruct.Get()); PropertyIter; ++PropertyIter)
		{
			StructureCategory.AddExternalStructureProperty(StructData, (*PropertyIter)->GetFName());
		}
	}
}

/////////////////////////////////
// FStructureDefaultValueView

class FStructureDefaultValueView : public FStructureEditorUtils::INotifyOnStructChanged, public TSharedFromThis<FStructureDefaultValueView>, public FNotifyHook
{
public:
	FStructureDefaultValueView(UUserDefinedStruct* EditedStruct) 
		: UserDefinedStruct(EditedStruct)
		, PropertyChangeRecursionGuard(0)
	{
	}

	void Initialize()
	{
		StructData = MakeShareable(new FStructOnScope(UserDefinedStruct.Get()));
		FStructureEditorUtils::Fill_MakeStructureDefaultValue(UserDefinedStruct.Get(), StructData->GetStructMemory());
		StructData->SetPackage(UserDefinedStruct->GetOutermost());

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs ViewArgs;
		ViewArgs.bAllowSearch = false;
		ViewArgs.bHideSelectionTip = false;
		ViewArgs.bShowActorLabel = false;
		ViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		ViewArgs.NotifyHook = this;

		DetailsView = PropertyModule.CreateDetailView(ViewArgs);
		TWeakPtr< FStructureDefaultValueView > LocalWeakThis = SharedThis(this);
		FOnGetDetailCustomizationInstance LayoutStructDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FDefaultValueDetails::MakeInstance, LocalWeakThis, StructData);
		DetailsView->RegisterInstancedCustomPropertyLayout(UUserDefinedStruct::StaticClass(), LayoutStructDetails);
		DetailsView->SetObject(UserDefinedStruct.Get());
	}

	virtual ~FStructureDefaultValueView()
	{
	}

	UUserDefinedStruct* GetUserDefinedStruct()
	{
		return UserDefinedStruct.Get();
	}

	TSharedPtr<class SWidget> GetWidget()
	{
		return DetailsView;
	}

	virtual void PreChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override
	{
		// No need to destroy the struct data if only the default values are changing
		if (Info != FStructureEditorUtils::DefaultValueChanged)
		{
			StructData->Destroy();
			DetailsView->SetObject(nullptr);
			DetailsView->OnFinishedChangingProperties().Clear();
		}
	}

	virtual void PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override
	{
		// If change is due to default value, then struct data was not destroyed (see PreChange) and therefore does not need to be re-initialized
		if (Info != FStructureEditorUtils::DefaultValueChanged)
		{
			StructData->Initialize(UserDefinedStruct.Get());
			DetailsView->SetObject(UserDefinedStruct.Get());
		}

		FStructureEditorUtils::Fill_MakeStructureDefaultValue(UserDefinedStruct.Get(), StructData->GetStructMemory());
	}

	// FNotifyHook interface
	virtual void NotifyPreChange( UProperty* PropertyAboutToChange ) override
	{
		++PropertyChangeRecursionGuard;
	}

	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged) override
	{
		--PropertyChangeRecursionGuard;
	}
	// End of FNotifyHook interface

	/** Returns TRUE when property changes are complete, according to recursion counts */
	bool IsPropertyChangeComplete() { return PropertyChangeRecursionGuard == 0; }
private:

	/** Struct on scope data that is being viewed in the details panel */
	TSharedPtr<FStructOnScope> StructData;

	/** Details view being used for viewing the struct */
	TSharedPtr<class IDetailsView> DetailsView;

	/** User defined struct that is being represented */
	const TWeakObjectPtr<UUserDefinedStruct> UserDefinedStruct;

	/** Manages recursion in property changing, to ensure we only compile the structure when all properties are done changing */
	int32 PropertyChangeRecursionGuard;
};

///////////////////////////////////////////////////////////////////////////////////////
// FDefaultValueDetails

void FDefaultValueDetails::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (DefaultValueView.Pin()->IsPropertyChangeComplete())
	{
		UStruct* OwnerStruct = PropertyChangedEvent.MemberProperty->GetOwnerStruct();

		check(PropertyChangedEvent.MemberProperty && OwnerStruct);

		if ( ensure(OwnerStruct == UserDefinedStruct.Get()) )
		{
			const UProperty* DirectProperty = PropertyChangedEvent.MemberProperty;
			while (DirectProperty && !Cast<const UUserDefinedStruct>(DirectProperty->GetOuter()))
			{
				DirectProperty = Cast<const UProperty>(DirectProperty->GetOuter());
			}
			ensure(nullptr != DirectProperty);

			if (DirectProperty)
			{
				FString DefaultValueString;
				bool bDefaultValueSet = false;
				{
					if (StructData.IsValid() && StructData->IsValid())
					{
						bDefaultValueSet = FBlueprintEditorUtils::PropertyValueToString(DirectProperty, StructData->GetStructMemory(), DefaultValueString);
					}
				}

				const FGuid VarGuid = FStructureEditorUtils::GetGuidForProperty(DirectProperty);
				if (bDefaultValueSet && VarGuid.IsValid())
				{
					FStructureEditorUtils::ChangeVariableDefaultValue(UserDefinedStruct.Get(), VarGuid, DefaultValueString);
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// FUserDefinedStructureDetails

class FUserDefinedStructureDetails : public IDetailCustomization, FStructureEditorUtils::INotifyOnStructChanged
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FUserDefinedStructureDetails);
	}

	~FUserDefinedStructureDetails()
	{
	}

	UUserDefinedStruct* GetUserDefinedStruct()
	{
		return UserDefinedStruct.Get();
	}

	struct FStructVariableDescription* FindStructureFieldByGuid(FGuid Guid)
	{
		if (auto Struct = GetUserDefinedStruct())
		{
			return FStructureEditorUtils::GetVarDesc(Struct).FindByPredicate(FStructureEditorUtils::FFindByGuidHelper<FStructVariableDescription>(Guid));
		}
		return NULL;
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailLayout) override;

	/** FStructureEditorUtils::INotifyOnStructChanged */
	virtual void PreChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override {}
	virtual void PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override;

private:
	TWeakObjectPtr<UUserDefinedStruct> UserDefinedStruct;
	TSharedPtr<class FUserDefinedStructureLayout> Layout;
};

///////////////////////////////////////////////////////////////////////////////////////
// FUserDefinedStructureEditor

const FName FUserDefinedStructureEditor::MemberVariablesTabId( TEXT( "UserDefinedStruct_MemberVariablesEditor" ) );
const FName FUserDefinedStructureEditor::UserDefinedStructureEditorAppIdentifier( TEXT( "UserDefinedStructEditorApp" ) );

void FUserDefinedStructureEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_UserDefinedStructureEditor", "User-Defined Structure Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( MemberVariablesTabId, FOnSpawnTab::CreateSP(this, &FUserDefinedStructureEditor::SpawnStructureTab) )
		.SetDisplayName( LOCTEXT("MemberVariablesEditor", "Structure Editor") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Tabs.Variables"));
}

void FUserDefinedStructureEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( MemberVariablesTabId );
}

void FUserDefinedStructureEditor::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UUserDefinedStruct* Struct)
{
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_UserDefinedStructureEditor_Layout_v1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() 
		->SetOrientation(Orient_Vertical)
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
				->SetHideTabWell( true )
				->AddTab( MemberVariablesTabId, ETabState::OpenedTab )
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, UserDefinedStructureEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Struct );
}

FUserDefinedStructureEditor::~FUserDefinedStructureEditor()
{
}

FName FUserDefinedStructureEditor::GetToolkitFName() const
{
	return FName("UserDefinedStructureEditor");
}

FText FUserDefinedStructureEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "Struct Editor" );
}

FText FUserDefinedStructureEditor::GetToolkitName() const
{
	if (1 == GetEditingObjects().Num())
	{
		return FAssetEditorToolkit::GetToolkitName();
	}
	return GetBaseToolkitName();
}

FText FUserDefinedStructureEditor::GetToolkitToolTipText() const
{
	if (1 == GetEditingObjects().Num())
	{
		return FAssetEditorToolkit::GetToolkitToolTipText();
	}
	return GetBaseToolkitName();
}

FString FUserDefinedStructureEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("UDStructWorldCentricTabPrefix", "Struct ").ToString();
}

FLinearColor FUserDefinedStructureEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 1.0f, 0.5f );
}

TSharedRef<SDockTab> FUserDefinedStructureEditor::SpawnStructureTab(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == MemberVariablesTabId );

	UUserDefinedStruct* EditedStruct = NULL;
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	if (EditingObjs.Num())
	{
		EditedStruct = Cast<UUserDefinedStruct>(EditingObjs[ 0 ]);
	}

	TSharedRef<SSplitter> Splitter = SNew(SSplitter)
		.Orientation(Orient_Vertical)
		.PhysicalSplitterHandleSize(10.0f)
		.ResizeMode(ESplitterResizeMode::FixedPosition);

	{
		// Create a property view
		FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs DetailsViewArgs( /*bUpdateFromSelection=*/ false, /*bLockable=*/ false, /*bAllowSearch=*/ false, FDetailsViewArgs::HideNameArea, /*bHideSelectionTip=*/ true);
		DetailsViewArgs.bShowOptions = false;
		PropertyView = EditModule.CreateDetailView(DetailsViewArgs);
		FOnGetDetailCustomizationInstance LayoutStructDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FUserDefinedStructureDetails::MakeInstance);
		PropertyView->RegisterInstancedCustomPropertyLayout(UUserDefinedStruct::StaticClass(), LayoutStructDetails);
		PropertyView->SetObject(EditedStruct);
		Splitter->AddSlot()
		.Value(0.25f)
		[
			PropertyView.ToSharedRef()
		];
	}

	DefaultValueView = NULL;

	static FBoolConfigValueHelper ShowDefaultValuePropertyEditor(TEXT("UserDefinedStructure"), TEXT("bShowDefaultValuePropertyEditor"));
	if (ShowDefaultValuePropertyEditor)
	{
		DefaultValueView = MakeShareable(new FStructureDefaultValueView(EditedStruct));
		DefaultValueView->Initialize();
		auto DefaultValueWidget = DefaultValueView->GetWidget();
		if (DefaultValueWidget.IsValid())
		{
			Splitter->AddSlot()
			[
				DefaultValueWidget.ToSharedRef()
			];
		}
	}

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("GenericEditor.Tabs.Properties") )
		.Label( LOCTEXT("UserDefinedStructureEditor", "Structure Editor") )
		.TabColorScale( GetTabColorScale() )
		[
			Splitter
		];
}

///////////////////////////////////////////////////////////////////////////////////////
// FUserDefinedStructureLayout

//Represents single structure (List of fields)
class FUserDefinedStructureLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FUserDefinedStructureLayout>
{
public:
	FUserDefinedStructureLayout(TWeakPtr<class FUserDefinedStructureDetails> InStructureDetails)
		: StructureDetails(InStructureDetails)
		, InitialPinType(GetDefault<UEdGraphSchema_K2>()->PC_Boolean, FString(), nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
	{}

	void OnChanged()
	{
		OnRegenerateChildren.ExecuteIfBound();
	}

	FReply OnAddNewField()
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid())
		{
			FStructureEditorUtils::AddVariable(StructureDetailsSP->GetUserDefinedStruct(), InitialPinType);
		}

		return FReply::Handled();
	}

	const FSlateBrush* OnGetStructureStatus() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid())
		{
			if(auto Struct = StructureDetailsSP->GetUserDefinedStruct())
			{
				switch(Struct->Status.GetValue())
				{
				case EUserDefinedStructureStatus::UDSS_Error:
					return FEditorStyle::GetBrush("Kismet.Status.Error.Small");
				case EUserDefinedStructureStatus::UDSS_UpToDate:
					return FEditorStyle::GetBrush("Kismet.Status.Good.Small");
				default:
					return FEditorStyle::GetBrush("Kismet.Status.Unknown.Small");
				}
				
			}
		}
		return NULL;
	}

	FText GetStatusTooltip() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			if (auto Struct = StructureDetailsSP->GetUserDefinedStruct())
			{
				switch (Struct->Status.GetValue())
				{
				case EUserDefinedStructureStatus::UDSS_Error:
					return FText::FromString(Struct->ErrorMessage);
				}
			}
		}
		return FText::GetEmpty();
	}

	FText OnGetTooltipText() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			if (auto Struct = StructureDetailsSP->GetUserDefinedStruct())
			{
				return FText::FromString(FStructureEditorUtils::GetTooltip(Struct));
			}
		}
		return FText();
	}

	void OnTooltipCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			if (auto Struct = StructureDetailsSP->GetUserDefinedStruct())
			{
				FStructureEditorUtils::ChangeTooltip(Struct, NewText.ToString());
			}
		}
	}

	/** Callback when a pin type is selected to cache the value so new variables in the struct will be set to the cached type */
	void OnPinTypeSelected(const FEdGraphPinType& InPinType)
	{
		InitialPinType = InPinType;
	}

	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) override 
	{
		OnRegenerateChildren = InOnRegenerateChildren;
	}
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override;

	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override {}

	virtual void Tick( float DeltaTime ) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override 
	{ 
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid())
		{
			if(auto Struct = StructureDetailsSP->GetUserDefinedStruct())
			{
				return Struct->GetFName();
			}
		}
		return NAME_None; 
	}
	virtual bool InitiallyCollapsed() const override { return false; }

private:
	TWeakPtr<class FUserDefinedStructureDetails> StructureDetails;
	FSimpleDelegate OnRegenerateChildren;

	/** Cached value of the last pin type the user selected, used as the initial value for new struct members */
	FEdGraphPinType InitialPinType;
};

enum EMemberFieldPosition
{
	MFP_First	=	0x1,
	MFP_Last	=	0x2,
};

///////////////////////////////////////////////////////////////////////////////////////
// FUserDefinedStructureFieldLayout

//Represents single field
class FUserDefinedStructureFieldLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FUserDefinedStructureFieldLayout>
{
public:
	FUserDefinedStructureFieldLayout(TWeakPtr<class FUserDefinedStructureDetails> InStructureDetails, TWeakPtr<class FUserDefinedStructureLayout> InStructureLayout, FGuid InFieldGuid, uint32 InPositionFlags)
		: StructureDetails(InStructureDetails)
		, StructureLayout(InStructureLayout)
		, FieldGuid(InFieldGuid)
		, PositionFlags(InPositionFlags) {}

	void OnChanged()
	{
		OnRegenerateChildren.ExecuteIfBound();
	}

	FText OnGetNameText() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid())
		{
			return FText::FromString(FStructureEditorUtils::GetVariableDisplayName(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid));
		}
		return FText::GetEmpty();
	}

	void OnNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid())
		{
			const FString NewNameStr = NewText.ToString();
			FStructureEditorUtils::RenameVariable(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid, NewNameStr);
		}
	}

	FEdGraphPinType OnGetPinInfo() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid())
		{
			if(const FStructVariableDescription* FieldDesc = StructureDetailsSP->FindStructureFieldByGuid(FieldGuid))
			{
				return FieldDesc->ToPinType();
			}
		}
		return FEdGraphPinType();
	}

	void PinInfoChanged(const FEdGraphPinType& PinType)
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid())
		{
			FStructureEditorUtils::ChangeVariableType(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid, PinType);
			auto StructureLayoutPin = StructureLayout.Pin();
			if (StructureLayoutPin.IsValid())
			{
				StructureLayoutPin->OnPinTypeSelected(PinType);
			}
		}
	}

	void OnPrePinInfoChange(const FEdGraphPinType& PinType)
	{

	}

	void OnRemovField()
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid())
		{
			FStructureEditorUtils::RemoveVariable(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid);
		}
	}

	bool IsRemoveButtonEnabled()
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			if (auto UDStruct = StructureDetailsSP->GetUserDefinedStruct())
			{
				return (FStructureEditorUtils::GetVarDesc(UDStruct).Num() > 1);
			}
		}
		return false;
	}

	FText OnGetTooltipText() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			if (const FStructVariableDescription* FieldDesc = StructureDetailsSP->FindStructureFieldByGuid(FieldGuid))
			{
				return FText::FromString(FieldDesc->ToolTip);
			}
		}
		return FText();
	}

	void OnTooltipCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			FStructureEditorUtils::ChangeVariableTooltip(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid, NewText.ToString());
		}
	}

	ECheckBoxState OnGetEditableOnBPInstanceState() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			if (const FStructVariableDescription* FieldDesc = StructureDetailsSP->FindStructureFieldByGuid(FieldGuid))
			{
				return !FieldDesc->bDontEditoOnInstance ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
		}
		return ECheckBoxState::Undetermined;
	}

	void OnEditableOnBPInstanceCommitted(ECheckBoxState InNewState)
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			FStructureEditorUtils::ChangeEditableOnBPInstance(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid, ECheckBoxState::Unchecked != InNewState);
		}
	}

	// Multi-line text
	EVisibility IsMultiLineTextOptionVisible() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			return FStructureEditorUtils::CanEnableMultiLineText(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid) ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	}

	ECheckBoxState OnGetMultiLineTextEnabled() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			return FStructureEditorUtils::IsMultiLineTextEnabled(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	void OnMultiLineTextEnabledCommitted(ECheckBoxState InNewState)
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid() && (ECheckBoxState::Undetermined != InNewState))
		{
			FStructureEditorUtils::ChangeMultiLineTextEnabled(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid, ECheckBoxState::Checked == InNewState);
		}
	}

	// 3D widget
	EVisibility Is3dWidgetOptionVisible() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			return FStructureEditorUtils::CanEnable3dWidget(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid) ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	}

	ECheckBoxState OnGet3dWidgetEnabled() const
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid())
		{
			return FStructureEditorUtils::Is3dWidgetEnabled(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	void On3dWidgetEnabledCommitted(ECheckBoxState InNewState)
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid() && (ECheckBoxState::Undetermined != InNewState))
		{
			FStructureEditorUtils::Change3dWidgetEnabled(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid, ECheckBoxState::Checked == InNewState);
		}
	}

	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) override 
	{
		OnRegenerateChildren = InOnRegenerateChildren;
	}

	EVisibility GetErrorIconVisibility()
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid())
		{
			auto FieldDesc = StructureDetailsSP->FindStructureFieldByGuid(FieldGuid);
			if (FieldDesc && FieldDesc->bInvalidMember)
			{
				return EVisibility::Visible;
			}
		}

		return EVisibility::Collapsed;
	}

	void RemoveInvalidSubTypes(TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> PinTypeNode, const UUserDefinedStruct* Parent) const
	{
		if (!PinTypeNode.IsValid() || !Parent)
		{
			return;
		}

		for (int32 ChildIndex = 0; ChildIndex < PinTypeNode->Children.Num();)
		{
			const auto Child = PinTypeNode->Children[ChildIndex];
			if(Child.IsValid())
			{
				const bool bCanCheckSubObjectWithoutLoading = Child->GetPinType(false).PinSubCategoryObject.IsValid();
				if (bCanCheckSubObjectWithoutLoading && !FStructureEditorUtils::CanHaveAMemberVariableOfType(Parent, Child->GetPinType(false)))
				{
					PinTypeNode->Children.RemoveAt(ChildIndex);
					continue;
				}
			}
			++ChildIndex;
		}
	}

	void GetFilteredVariableTypeTree( TArray< TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> >& TypeTree, ETypeTreeFilter TypeTreeFilter) const
	{
		auto K2Schema = GetDefault<UEdGraphSchema_K2>();
		auto StructureDetailsSP = StructureDetails.Pin();
		if(StructureDetailsSP.IsValid() && K2Schema)
		{
			K2Schema->GetVariableTypeTree(TypeTree, TypeTreeFilter);
			const auto Parent = StructureDetailsSP->GetUserDefinedStruct();
			// THE TREE HAS ONLY 2 LEVELS
			for (auto PinTypePtr : TypeTree)
			{
				RemoveInvalidSubTypes(PinTypePtr, Parent);
			}
		}
	}

	FReply OnMoveUp()
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid() && !(PositionFlags & EMemberFieldPosition::MFP_First))
		{
			FStructureEditorUtils::MoveVariable(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid, FStructureEditorUtils::MD_Up);
		}
		return FReply::Handled();
	}

	FReply OnMoveDown()
	{
		auto StructureDetailsSP = StructureDetails.Pin();
		if (StructureDetailsSP.IsValid() && !(PositionFlags & EMemberFieldPosition::MFP_Last))
		{
			FStructureEditorUtils::MoveVariable(StructureDetailsSP->GetUserDefinedStruct(), FieldGuid, FStructureEditorUtils::MD_Down);
		}
		return FReply::Handled();
	}

	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override 
	{
		auto K2Schema = GetDefault<UEdGraphSchema_K2>();

		TSharedPtr<SImage> ErrorIcon;

		const float ValueContentWidth = 250.0f;

		NodeRow
		.NameContent()
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SAssignNew(ErrorIcon, SImage)
				.Image( FEditorStyle::GetBrush("Icons.Error") )
			]

			+SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign(VAlign_Center)
			[
				SNew(SEditableTextBox)
				.Text( this, &FUserDefinedStructureFieldLayout::OnGetNameText )
				.OnTextCommitted( this, &FUserDefinedStructureFieldLayout::OnNameTextCommitted )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		]
		.ValueContent()
		.MaxDesiredWidth(ValueContentWidth)
		.MinDesiredWidth(ValueContentWidth)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SPinTypeSelector, FGetPinTypeTree::CreateSP(this, &FUserDefinedStructureFieldLayout::GetFilteredVariableTypeTree))
				.TargetPinType(this, &FUserDefinedStructureFieldLayout::OnGetPinInfo)
				.OnPinTypePreChanged(this, &FUserDefinedStructureFieldLayout::OnPrePinInfoChange)
				.OnPinTypeChanged(this, &FUserDefinedStructureFieldLayout::PinInfoChanged)
				.Schema(K2Schema)
				.TypeTreeFilter(ETypeTreeFilter::None)
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FUserDefinedStructureFieldLayout::OnMoveUp)
				.IsEnabled(!(EMemberFieldPosition::MFP_First & PositionFlags))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("BlueprintEditor.Details.ArgUpButton"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FUserDefinedStructureFieldLayout::OnMoveDown)
				.IsEnabled(!(EMemberFieldPosition::MFP_Last & PositionFlags))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("BlueprintEditor.Details.ArgDownButton"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeClearButton(
					FSimpleDelegate::CreateSP(this, &FUserDefinedStructureFieldLayout::OnRemovField),
					LOCTEXT("RemoveVariable", "Remove member variable"),
					TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FUserDefinedStructureFieldLayout::IsRemoveButtonEnabled)))
			]
		];

		if (ErrorIcon.IsValid())
		{
			ErrorIcon->SetVisibility(
				TAttribute<EVisibility>::Create(
					TAttribute<EVisibility>::FGetter::CreateSP(
						this, &FUserDefinedStructureFieldLayout::GetErrorIconVisibility)));
		}
	}

	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override 
	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("Tooltip", "Tooltip"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Tooltip", "Tooltip"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
			.Text(this, &FUserDefinedStructureFieldLayout::OnGetTooltipText)
			.OnTextCommitted(this, &FUserDefinedStructureFieldLayout::OnTooltipCommitted)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

		ChildrenBuilder.AddCustomRow(LOCTEXT("EditableOnInstance", "EditableOnInstance"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Editable", "Editable"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.ToolTipText(LOCTEXT("EditableOnBPInstance", "Variable can be edited on an instance of a Blueprint."))
			.OnCheckStateChanged(this, &FUserDefinedStructureFieldLayout::OnEditableOnBPInstanceCommitted)
			.IsChecked(this, &FUserDefinedStructureFieldLayout::OnGetEditableOnBPInstanceState)
		];

		ChildrenBuilder.AddCustomRow(LOCTEXT("MultiLineText", "Multi-line Text"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MultiLineText", "Multi-line Text"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.ToolTipText(LOCTEXT("MultiLineTextToolTip", "Should this property allow multiple lines of text to be entered?"))
			.OnCheckStateChanged(this, &FUserDefinedStructureFieldLayout::OnMultiLineTextEnabledCommitted)
			.IsChecked(this, &FUserDefinedStructureFieldLayout::OnGetMultiLineTextEnabled)
		]
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FUserDefinedStructureFieldLayout::IsMultiLineTextOptionVisible)));

		ChildrenBuilder.AddCustomRow(LOCTEXT("3dWidget", "3D Widget"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("3dWidget", "3D Widget"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &FUserDefinedStructureFieldLayout::On3dWidgetEnabledCommitted)
			.IsChecked(this, &FUserDefinedStructureFieldLayout::OnGet3dWidgetEnabled)
		]
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FUserDefinedStructureFieldLayout::Is3dWidgetOptionVisible)));
	}

	virtual void Tick( float DeltaTime ) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { return FName(*FieldGuid.ToString()); }
	virtual bool InitiallyCollapsed() const override { return true; }

private:
	TWeakPtr<class FUserDefinedStructureDetails> StructureDetails;

	TWeakPtr<class FUserDefinedStructureLayout> StructureLayout;

	FGuid FieldGuid;

	FSimpleDelegate OnRegenerateChildren;

	uint32 PositionFlags;
};

///////////////////////////////////////////////////////////////////////////////////////
// FUserDefinedStructureLayout
void FUserDefinedStructureLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) 
{
	const float NameWidth = 80.0f;
	const float ContentWidth = 130.0f;

	ChildrenBuilder.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.MaxWidth(NameWidth)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(this, &FUserDefinedStructureLayout::OnGetStructureStatus)
			.ToolTipText(this, &FUserDefinedStructureLayout::GetStatusTooltip)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		[
			SNew(SBox)
			.WidthOverride(ContentWidth)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("NewStructureField", "New Variable"))
				.OnClicked(this, &FUserDefinedStructureLayout::OnAddNewField)
			]
		]
	];

	ChildrenBuilder.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.MaxWidth(NameWidth)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Tooltip", "Tooltip"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		[
			SNew(SBox)
			.WidthOverride(ContentWidth)
			[
				SNew(SEditableTextBox)
				.Text(this, &FUserDefinedStructureLayout::OnGetTooltipText)
				.OnTextCommitted(this, &FUserDefinedStructureLayout::OnTooltipCommitted)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
	];

	auto StructureDetailsSP = StructureDetails.Pin();
	if(StructureDetailsSP.IsValid())
	{
		if(auto Struct = StructureDetailsSP->GetUserDefinedStruct())
		{
			auto& VarDescArrayRef = FStructureEditorUtils::GetVarDesc(Struct);
			for (int32 Index = 0; Index < VarDescArrayRef.Num(); ++Index)
			{
				auto& VarDesc = VarDescArrayRef[Index];
				uint32 PositionFlag = 0;
				PositionFlag |= (0 == Index) ? EMemberFieldPosition::MFP_First : 0;
				PositionFlag |= ((VarDescArrayRef.Num() - 1) == Index) ? EMemberFieldPosition::MFP_Last : 0;
				TSharedRef<class FUserDefinedStructureFieldLayout> VarLayout = MakeShareable(new FUserDefinedStructureFieldLayout(StructureDetails,  SharedThis(this), VarDesc.VarGuid, PositionFlag));
				ChildrenBuilder.AddCustomBuilder(VarLayout);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// FUserDefinedStructureLayout

/** IDetailCustomization interface */
void FUserDefinedStructureDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailLayout) 
{
	const TArray<TWeakObjectPtr<UObject>>& Objects = DetailLayout.GetSelectedObjects();
	check(Objects.Num() > 0);

	if (Objects.Num() == 1)
	{
		UserDefinedStruct = CastChecked<UUserDefinedStruct>(Objects[0].Get());

		IDetailCategoryBuilder& StructureCategory = DetailLayout.EditCategory("Structure", LOCTEXT("StructureCategory", "Structure"));
		Layout = MakeShareable(new FUserDefinedStructureLayout(SharedThis(this)));
		StructureCategory.AddCustomBuilder(Layout.ToSharedRef());
	}
}

/** FStructureEditorUtils::INotifyOnStructChanged */
void FUserDefinedStructureDetails::PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	if (Struct && (GetUserDefinedStruct() == Struct))
	{
		if (Layout.IsValid())
		{
			Layout->OnChanged();
		}
	}
}

#undef LOCTEXT_NAMESPACE
