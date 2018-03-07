// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorModule.h"
#include "NiagaraEditorTickables.h"
#include "Modules/ModuleManager.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "Misc/ConfigCacheIni.h"
#include "ISequencerModule.h"
#include "ISettingsModule.h"
#include "SequencerSettings.h"

#include "AssetTypeActions/AssetTypeActions_NiagaraSystem.h"
#include "AssetTypeActions/AssetTypeActions_NiagaraEmitter.h"
#include "AssetTypeActions/AssetTypeActions_NiagaraScript.h"
#include "AssetTypeActions/AssetTypeActions_NiagaraParameterCollection.h"

#include "EdGraphUtilities.h"
#include "SGraphPin.h"
#include "SGraphPinVector4.h"
#include "SGraphPinNum.h"
#include "SGraphPinInteger.h"
#include "SGraphPinVector.h"
#include "SGraphPinVector2D.h"
#include "SGraphPinObject.h"
#include "SGraphPinColor.h"
#include "SGraphPinBool.h"
#include "Private/KismetPins/SGraphPinEnum.h"
#include "SNiagaraGraphPinNumeric.h"
#include "SNiagaraGraphPinAdd.h"
#include "NiagaraNodeConvert.h"
#include "EdGraphSchema_Niagara.h"
#include "TypeEditorUtilities/NiagaraFloatTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraIntegerTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraBoolTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraFloatTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraVectorTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraColorTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraDataInterfaceCurveTypeEditorUtilities.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorCommands.h"
#include "NiagaraEmitterTrackEditor.h"
#include "PropertyEditorModule.h"
#include "NiagaraSettings.h"
#include "NiagaraModule.h"
#include "NiagaraShaderModule.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraDataInterfaceVector2DCurve.h"
#include "NiagaraDataInterfaceVectorCurve.h"
#include "NiagaraDataInterfaceVector4Curve.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraSystemViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "TNiagaraGraphPinEditableName.h"
#include "Class.h"

#include "Customizations/NiagaraComponentDetails.h"
#include "Customizations/NiagaraTypeCustomizations.h"
#include "Customizations/NiagaraEventScriptPropertiesCustomization.h"

IMPLEMENT_MODULE( FNiagaraEditorModule, NiagaraEditor );

#define LOCTEXT_NAMESPACE "NiagaraEditorModule"

const FName FNiagaraEditorModule::NiagaraEditorAppIdentifier( TEXT( "NiagaraEditorApp" ) );
const FLinearColor FNiagaraEditorModule::WorldCentricTabColorScale(0.0f, 0.0f, 0.2f, 0.5f);

EAssetTypeCategories::Type FNiagaraEditorModule::NiagaraAssetCategory;

//////////////////////////////////////////////////////////////////////////

class FNiagaraScriptGraphPanelPinFactory : public FGraphPanelPinFactory
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<class SGraphPin>, FCreateGraphPin, UEdGraphPin*);

	/** Registers a delegate for creating a pin for a specific type. */
	void RegisterTypePin(const UScriptStruct* Type, FCreateGraphPin CreateGraphPin)
	{
		TypeToCreatePinDelegateMap.Add(Type, CreateGraphPin);
	}

	/** Registers a delegate for creating a pin for for a specific miscellaneous sub category. */
	void RegisterMiscSubCategoryPin(FString SubCategory, FCreateGraphPin CreateGraphPin)
	{
		MiscSubCategoryToCreatePinDelegateMap.Add(SubCategory, CreateGraphPin);
	}

	//~ FGraphPanelPinFactory interface
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		if (const UEdGraphSchema_Niagara* NSchema = Cast<UEdGraphSchema_Niagara>(InPin->GetSchema()))
		{
			if (InPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryType)
			{
				const UScriptStruct* Struct = CastChecked<const UScriptStruct>(InPin->PinType.PinSubCategoryObject.Get());
				const FCreateGraphPin* CreateGraphPin = TypeToCreatePinDelegateMap.Find(Struct);
				if (CreateGraphPin != nullptr)
				{
					return (*CreateGraphPin).Execute(InPin);
				}
			}
			else if (InPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryEnum)
			{
				const UEnum* Enum = Cast<const UEnum>(InPin->PinType.PinSubCategoryObject.Get());
				if (Enum == nullptr)
				{
					UE_LOG(LogNiagaraEditor, Error, TEXT("Pin states that it is of Enum type, but is missing its Enum! Pin Name '%s' Owning Node '%s'. Turning into standard int definition!"), *InPin->PinName,
						*InPin->GetOwningNode()->GetName());
					InPin->PinType.PinCategory = UEdGraphSchema_Niagara::PinCategoryType;
					InPin->PinType.PinSubCategoryObject = FNiagaraTypeDefinition::GetIntStruct();
					InPin->DefaultValue.Empty();
					return CreatePin(InPin);
				}
				return SNew(TNiagaraGraphPinEditableName<SGraphPinEnum>, InPin);
			}
			else if (InPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc)
			{
				const FCreateGraphPin* CreateGraphPin = MiscSubCategoryToCreatePinDelegateMap.Find(InPin->PinType.PinSubCategory);
				if (CreateGraphPin != nullptr)
				{
					return (*CreateGraphPin).Execute(InPin);
				}
			}

			return SNew(TNiagaraGraphPinEditableName<SGraphPin>, InPin);
		}
		return nullptr;
	}

private:
	TMap<const UScriptStruct*, FCreateGraphPin> TypeToCreatePinDelegateMap;
	TMap<FString, FCreateGraphPin> MiscSubCategoryToCreatePinDelegateMap;
};

FNiagaraEditorModule::FNiagaraEditorModule() 
	: SequencerSettings(nullptr)
{
}

void FNiagaraEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	NiagaraAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("FX")), LOCTEXT("NiagaraAssetsCategory", "FX"));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraSystem()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraEmitter()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraScript()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraParameterCollection()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraParameterCollectionInstance()));

	UNiagaraSettings::OnSettingsChanged().AddRaw(this, &FNiagaraEditorModule::OnNiagaraSettingsChangedEvent);

	// register details customization
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("NiagaraComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraComponentDetails::MakeInstance));
	
	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraFloat",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraNumericCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraInt32",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraNumericCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraNumeric",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraNumericCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraParameterMap",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraNumericCustomization::MakeInstance)
	);


	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraBool",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraBoolCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraMatrix",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraMatrixCustomization::MakeInstance)
	);

	FNiagaraEditorStyle::Initialize();
	FNiagaraEditorCommands::Register();

	TSharedPtr<FNiagaraScriptGraphPanelPinFactory> GraphPanelPinFactory = MakeShareable(new FNiagaraScriptGraphPanelPinFactory());

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetFloatStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinNum>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetIntStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinInteger>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetVec2Struct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinVector2D>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetVec3Struct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinVector>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetVec4Struct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinVector4>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetColorStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinColor>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetBoolStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinBool>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetGenericNumericStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SNiagaraGraphPinNumeric>, GraphPin); }));

	// TODO: Don't register this here.
	GraphPanelPinFactory->RegisterMiscSubCategoryPin(UNiagaraNodeWithDynamicPins::AddPinSubCategory, FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(SNiagaraGraphPinAdd, GraphPin); }));

	RegisterTypeUtilities(FNiagaraTypeDefinition::GetFloatDef(), MakeShareable(new FNiagaraEditorFloatTypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetIntDef(), MakeShareable(new FNiagaraEditorIntegerTypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetBoolDef(), MakeShareable(new FNiagaraEditorBoolTypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetVec2Def(), MakeShareable(new FNiagaraEditorVector2TypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetVec3Def(), MakeShareable(new FNiagaraEditorVector3TypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetVec4Def(), MakeShareable(new FNiagaraEditorVector4TypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetColorDef(), MakeShareable(new FNiagaraEditorColorTypeUtilities()));

	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceCurve::StaticClass()), MakeShared<FNiagaraDataInterfaceCurveTypeEditorUtilities>());
	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceVector2DCurve::StaticClass()), MakeShared<FNiagaraDataInterfaceCurveTypeEditorUtilities>());
	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceVectorCurve::StaticClass()), MakeShared<FNiagaraDataInterfaceVectorCurveTypeEditorUtilities>());
	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceVector4Curve::StaticClass()), MakeShared<FNiagaraDataInterfaceVectorCurveTypeEditorUtilities>());
	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceColorCurve::StaticClass()), MakeShared<FNiagaraDataInterfaceColorCurveTypeEditorUtilities>());

	FEdGraphUtilities::RegisterVisualPinFactory(GraphPanelPinFactory);

	FNiagaraOpInfo::Init();

	RegisterSettings();

	// Register sequencer track editor
	ISequencerModule &SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	CreateEmitterTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FNiagaraEmitterTrackEditor::CreateTrackEditor));

	// Register the shader queue processor (for cooking)
	INiagaraModule& NiagaraModule = FModuleManager::LoadModuleChecked<INiagaraModule>("Niagara");
	NiagaraModule.SetOnProcessShaderCompilationQueue(INiagaraModule::FOnProcessQueue::CreateLambda([]()
	{
		FNiagaraShaderQueueTickable::ProcessQueue();
	}));

	INiagaraShaderModule& NiagaraShaderModule = FModuleManager::LoadModuleChecked<INiagaraShaderModule>("NiagaraShader");
	NiagaraShaderModule.SetOnProcessShaderCompilationQueue(INiagaraShaderModule::FOnProcessQueue::CreateLambda([]()
	{
		FNiagaraShaderQueueTickable::ProcessQueue();
	}));

}


void FNiagaraEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (auto CreatedAssetTypeAction : CreatedAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeAction.ToSharedRef());
		}
	}
	CreatedAssetTypeActions.Empty();

	UNiagaraSettings::OnSettingsChanged().RemoveAll(this);

	
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("NiagaraComponent");
	}

	FNiagaraEditorStyle::Shutdown();

	UnregisterSettings();

	ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModule != nullptr)
	{
		SequencerModule->UnRegisterTrackEditor(CreateEmitterTrackEditorHandle);
	}

	// Verify that we've cleaned up all the view models in the world.
	FNiagaraScriptViewModel::CleanAll();
	FNiagaraSystemViewModel::CleanAll();
	FNiagaraEmitterViewModel::CleanAll();
}

void FNiagaraEditorModule::OnNiagaraSettingsChangedEvent(const FString& PropertyName, const UNiagaraSettings* Settings)
{
	if (PropertyName == "AdditionalParameterTypes" || PropertyName == "AdditionalPayloadTypes")
	{
		FNiagaraTypeDefinition::RecreateUserDefinedTypeRegistry();
	}
}

void FNiagaraEditorModule::RegisterTypeUtilities(FNiagaraTypeDefinition Type, TSharedRef<INiagaraEditorTypeUtilities> EditorUtilities)
{
	TypeToEditorUtilitiesMap.Add(Type, EditorUtilities);
}


TSharedPtr<INiagaraEditorTypeUtilities> FNiagaraEditorModule::GetTypeUtilities(const FNiagaraTypeDefinition& Type)
{
	TSharedRef<INiagaraEditorTypeUtilities>* EditorUtilities = TypeToEditorUtilitiesMap.Find(Type);
	if (EditorUtilities != nullptr)
	{
		return *EditorUtilities;
	}
	return TSharedPtr<INiagaraEditorTypeUtilities>();
}

TSharedRef<SWidget> FNiagaraEditorModule::CreateStackWidget(UNiagaraStackViewModel* StackViewModel) const
{
	checkf(OnCreateStackWidget.IsBound(), TEXT("Can not create stack widget.  Stack creation delegate was never set."));
	return OnCreateStackWidget.Execute(StackViewModel);
}

FDelegateHandle FNiagaraEditorModule::SetOnCreateStackWidget(FOnCreateStackWidget InOnCreateStackWidget)
{
	checkf(OnCreateStackWidget.IsBound() == false, TEXT("Stack creation delegate already set."));
	OnCreateStackWidget = InOnCreateStackWidget;
	return OnCreateStackWidget.GetHandle();
}

void FNiagaraEditorModule::ResetOnCreateStackWidget(FDelegateHandle Handle)
{
	checkf(OnCreateStackWidget.GetHandle() == Handle, TEXT("Can only reset the stack creation module with the handle it was created with."));
	OnCreateStackWidget.Unbind();
}




void FNiagaraEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

void FNiagaraEditorModule::RegisterSettings()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SequencerSettings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("NiagaraSequenceEditor"));

		SettingsModule->RegisterSettings("Editor", "ContentEditors", "NiagaraSequenceEditor",
			LOCTEXT("NiagaraSequenceEditorSettingsName", "Niagara Sequence Editor"),
			LOCTEXT("NiagaraSequenceEditorSettingsDescription", "Configure the look and feel of the Niagara Sequence Editor."),
			SequencerSettings);	
	}
}

void FNiagaraEditorModule::UnregisterSettings()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Editor", "ContentEditors", "NiagaraSequenceEditor");
	}
}

void FNiagaraEditorModule::AddReferencedObjects( FReferenceCollector& Collector )
{
	if (SequencerSettings)
	{
		Collector.AddReferencedObject(SequencerSettings);
	}
}

#undef LOCTEXT_NAMESPACE