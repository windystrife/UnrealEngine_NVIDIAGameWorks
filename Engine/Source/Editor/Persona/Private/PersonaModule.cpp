// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "PersonaModule.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "EditorModeRegistry.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Materials/Material.h"
#include "IPersonaPreviewScene.h"
#include "Logging/TokenizedMessage.h"
#include "ARFilter.h"
#include "AnimGraphDefinitions.h"
#include "Toolkits/ToolkitManager.h"
#include "AssetRegistryModule.h"
#include "SkeletalMeshSocketDetails.h"
#include "AnimNotifyDetails.h"
#include "AnimGraphNodeDetails.h"
#include "AnimInstanceDetails.h"
#include "IEditableSkeleton.h"
#include "IPersonaToolkit.h"
#include "PersonaToolkit.h"
#include "TabSpawners.h"
#include "SSkeletonAnimNotifies.h"
#include "SAssetFamilyShortcutBar.h"
#include "Animation/AnimMontage.h"
#include "SMontageEditor.h"
#include "SSequenceEditor.h"
#include "Animation/AnimComposite.h"
#include "SAnimCompositeEditor.h"
#include "Animation/PoseAsset.h"
#include "SPoseEditor.h"
#include "Animation/BlendSpace.h"
#include "SAnimationBlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "SAnimationBlendSpace1D.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "SAnimationDlgs.h"
#include "FbxMeshUtils.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Logging/MessageLog.h"
#include "AnimationCompressionPanel.h"
#include "DesktopPlatformModule.h"
#include "FbxAnimUtils.h"
#include "PersonaAssetFamilyManager.h"
#include "AnimGraphNode_Slot.h"
#include "Customization/AnimGraphNodeSlotDetails.h"
#include "Customization/BlendSpaceDetails.h"
#include "Customization/BlendParameterDetails.h"
#include "Customization/InterpolationParameterDetails.h"
#include "EditModes/SkeletonSelectionEditMode.h"
#include "PersonaEditorModeManager.h"
#include "PreviewSceneCustomizations.h"
#include "SSkeletonSlotNames.h"
#include "PersonaMeshDetails.h"
#include "Animation/MorphTarget.h"
#include "EditorDirectories.h"
#include "PersonaCommonCommands.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BlueprintEditorUtils.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "PhysicsEngine/PhysicsAsset.h"

IMPLEMENT_MODULE( FPersonaModule, Persona );

const FName PersonaAppName = FName(TEXT("PersonaApp"));

const FEditorModeID FPersonaEditModes::SkeletonSelection(TEXT("PersonaSkeletonSelection"));

#define LOCTEXT_NAMESPACE "PersonaModule"

void FPersonaModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	//Call this to make sure AnimGraph module is setup
	FModuleManager::Get().LoadModuleChecked(TEXT("AnimGraph"));

	// Make sure the advanced preview scene module is loaded 
	FModuleManager::Get().LoadModuleChecked("AdvancedPreviewScene");

	// Load all blueprint animnotifies from asset registry so they are available from drop downs in anim segment detail views
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// Collect a full list of assets with the specified class
		TArray<FAssetData> AssetData;
		AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), AssetData);

		const FName BPParentClassName( TEXT( "ParentClass" ) );
		const FString BPAnimNotify( TEXT("Class'/Script/Engine.AnimNotify'" ));

		for (int32 AssetIndex = 0; AssetIndex < AssetData.Num(); ++AssetIndex)
		{
			FString TagValue = AssetData[ AssetIndex ].GetTagValueRef<FString>(BPParentClassName);
			if (TagValue == BPAnimNotify)
			{
				FString BlueprintPath = AssetData[AssetIndex].ObjectPath.ToString();
				LoadObject<UBlueprint>(NULL, *BlueprintPath, NULL, 0, NULL);
			}
		}
	}
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout( "SkeletalMeshSocket", FOnGetDetailCustomizationInstance::CreateStatic( &FSkeletalMeshSocketDetails::MakeInstance ) );
		PropertyModule.RegisterCustomClassLayout( "EditorNotifyObject", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimNotifyDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout( "AnimGraphNode_Base", FOnGetDetailCustomizationInstance::CreateStatic( &FAnimGraphNodeDetails::MakeInstance ) );
		PropertyModule.RegisterCustomClassLayout( "AnimInstance", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimInstanceDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("BlendSpaceBase", FOnGetDetailCustomizationInstance::CreateStatic(&FBlendSpaceDetails::MakeInstance));	

		PropertyModule.RegisterCustomPropertyTypeLayout( "InputScaleBias", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FInputScaleBiasCustomization::MakeInstance ) );
		PropertyModule.RegisterCustomPropertyTypeLayout( "BoneReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FBoneReferenceCustomization::MakeInstance ) );
		PropertyModule.RegisterCustomPropertyTypeLayout("BoneSocketTarget", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBoneSocketTargetCustomization::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout( "PreviewMeshCollectionEntry", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FPreviewMeshCollectionEntryCustomization::MakeInstance ) );

		PropertyModule.RegisterCustomPropertyTypeLayout("BlendParameter", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlendParameterDetails::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("InterpolationParameter", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInterpolationParameterDetails::MakeInstance));
	}

	// Register the editor modes
	FEditorModeRegistry::Get().RegisterMode<FSkeletonSelectionEditMode>(FPersonaEditModes::SkeletonSelection, LOCTEXT("SkeletonSelectionEditMode", "Skeleton Selection"), FSlateIcon(), false);

	FPersonaCommonCommands::Register();

	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(this, UAnimNotify::StaticClass(), FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &FPersonaModule::HandleNewAnimNotifyBlueprintCreated));
	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(this, UAnimNotifyState::StaticClass(), FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &FPersonaModule::HandleNewAnimNotifyStateBlueprintCreated));
}

void FPersonaModule::ShutdownModule()
{
	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	// Unregister the editor modes
	FEditorModeRegistry::Get().UnregisterMode(FPersonaEditModes::SkeletonSelection);

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	// Unregister when shut down
	if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("SkeletalMeshSocket");
		PropertyModule.UnregisterCustomClassLayout("EditorNotifyObject");
		PropertyModule.UnregisterCustomClassLayout("AnimGraphNode_Base");
		PropertyModule.UnregisterCustomClassLayout("BlendSpaceBase");

		PropertyModule.UnregisterCustomPropertyTypeLayout("InputScaleBias");
		PropertyModule.UnregisterCustomPropertyTypeLayout("BoneReference");

		PropertyModule.UnregisterCustomPropertyTypeLayout("BlendParameter");
		PropertyModule.UnregisterCustomPropertyTypeLayout("InterpolationParameter");
	}
}

static void SetupPersonaToolkit(const TSharedRef<FPersonaToolkit>& Toolkit, const FPersonaToolkitArgs& PersonaToolkitArgs)
{
	if (PersonaToolkitArgs.bCreatePreviewScene)
	{
		Toolkit->CreatePreviewScene(PersonaToolkitArgs);
	}
}

TSharedRef<IPersonaToolkit> FPersonaModule::CreatePersonaToolkit(USkeleton* InSkeleton, const FPersonaToolkitArgs& PersonaToolkitArgs) const
{
	TSharedRef<FPersonaToolkit> NewPersonaToolkit(new FPersonaToolkit());

	NewPersonaToolkit->Initialize(InSkeleton);

	SetupPersonaToolkit(NewPersonaToolkit, PersonaToolkitArgs);

	return NewPersonaToolkit;
}

TSharedRef<IPersonaToolkit> FPersonaModule::CreatePersonaToolkit(UAnimationAsset* InAnimationAsset, const FPersonaToolkitArgs& PersonaToolkitArgs) const
{
	TSharedRef<FPersonaToolkit> NewPersonaToolkit(new FPersonaToolkit());

	NewPersonaToolkit->Initialize(InAnimationAsset);

	SetupPersonaToolkit(NewPersonaToolkit, PersonaToolkitArgs);

	return NewPersonaToolkit;
}

TSharedRef<IPersonaToolkit> FPersonaModule::CreatePersonaToolkit(USkeletalMesh* InSkeletalMesh, const FPersonaToolkitArgs& PersonaToolkitArgs) const
{
	TSharedRef<FPersonaToolkit> NewPersonaToolkit(new FPersonaToolkit());

	NewPersonaToolkit->Initialize(InSkeletalMesh);

	SetupPersonaToolkit(NewPersonaToolkit, PersonaToolkitArgs);

	return NewPersonaToolkit;
}

TSharedRef<IPersonaToolkit> FPersonaModule::CreatePersonaToolkit(UAnimBlueprint* InAnimBlueprint, const FPersonaToolkitArgs& PersonaToolkitArgs) const
{
	TSharedRef<FPersonaToolkit> NewPersonaToolkit(new FPersonaToolkit());

	NewPersonaToolkit->Initialize(InAnimBlueprint);

	SetupPersonaToolkit(NewPersonaToolkit, PersonaToolkitArgs);

	return NewPersonaToolkit;
}

TSharedRef<IPersonaToolkit> FPersonaModule::CreatePersonaToolkit(UPhysicsAsset* InPhysicsAsset, const FPersonaToolkitArgs& PersonaToolkitArgs) const
{
	TSharedRef<FPersonaToolkit> NewPersonaToolkit(new FPersonaToolkit());

	NewPersonaToolkit->Initialize(InPhysicsAsset);

	SetupPersonaToolkit(NewPersonaToolkit, PersonaToolkitArgs);

	return NewPersonaToolkit;
}

TSharedRef<class IAssetFamily> FPersonaModule::CreatePersonaAssetFamily(const UObject* InAsset) const
{
	return FPersonaAssetFamilyManager::Get().CreatePersonaAssetFamily(InAsset);
}

TSharedRef<SWidget> FPersonaModule::CreateAssetFamilyShortcutWidget(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily) const
{
	return SNew(SAssetFamilyShortcutBar, InHostingApp, InAssetFamily);
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateDetailsTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, FOnDetailsCreated InOnDetailsCreated) const
{
	return MakeShareable(new FPersonaDetailsTabSummoner(InHostingApp, InOnDetailsCreated));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreatePersonaViewportTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const FPersonaViewportArgs& InArgs) const
{
	return MakeShareable(new FPreviewViewportSummoner(InHostingApp, InArgs));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateAnimNotifiesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnChangeAnimNotifies, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectsSelected InOnObjectsSelected) const
{
	return MakeShareable(new FSkeletonAnimNotifiesSummoner(InHostingApp, InEditableSkeleton, InOnChangeAnimNotifies, InOnPostUndo, InOnObjectsSelected));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateCurveViewerTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectsSelected InOnObjectsSelected) const
{
	return MakeShareable(new FAnimCurveViewerTabSummoner(InHostingApp, InEditableSkeleton, InPreviewScene, InOnPostUndo, InOnObjectsSelected));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateRetargetManagerTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo) const
{
	return MakeShareable(new FRetargetManagerTabSummoner(InHostingApp, InEditableSkeleton, InPreviewScene, InOnPostUndo));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateAdvancedPreviewSceneTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene) const
{
	return MakeShareable(new FAdvancedPreviewSceneTabSummoner(InHostingApp, InPreviewScene));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateAnimationAssetBrowserTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaToolkit>& InPersonaToolkit, FOnOpenNewAsset InOnOpenNewAsset, FOnAnimationSequenceBrowserCreated InOnAnimationSequenceBrowserCreated, bool bInShowHistory) const
{
	return MakeShareable(new FAnimationAssetBrowserSummoner(InHostingApp, InPersonaToolkit, InOnOpenNewAsset, InOnAnimationSequenceBrowserCreated, bInShowHistory));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateAssetDetailsTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, FOnGetAsset InOnGetAsset, FOnDetailsCreated InOnDetailsCreated) const
{
	return MakeShareable(new FAssetPropertiesSummoner(InHostingApp, InOnGetAsset, InOnDetailsCreated));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateMorphTargetTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo) const
{
	return MakeShareable(new FMorphTargetTabSummoner(InHostingApp, InPreviewScene, OnPostUndo));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateAnimBlueprintPreviewTabFactory(const TSharedRef<class FBlueprintEditor>& InBlueprintEditor, const TSharedRef<IPersonaPreviewScene>& InPreviewScene) const
{
	return MakeShareable(new FAnimBlueprintPreviewEditorSummoner(InBlueprintEditor, InPreviewScene));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateAnimBlueprintAssetOverridesTabFactory(const TSharedRef<class FBlueprintEditor>& InBlueprintEditor, UAnimBlueprint* InAnimBlueprint, FSimpleMulticastDelegate& InOnPostUndo) const
{
	return MakeShareable(new FAnimBlueprintParentPlayerEditorSummoner(InBlueprintEditor, InOnPostUndo));
}

TSharedRef<class FWorkflowTabFactory> FPersonaModule::CreateSkeletonSlotNamesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectSelected InOnObjectSelected) const
{
	return MakeShareable(new FSkeletonSlotNamesSummoner(InHostingApp, InEditableSkeleton, InOnPostUndo, InOnObjectSelected));
}

TSharedRef<SWidget> FPersonaModule::CreateEditorWidgetForAnimDocument(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, UObject* InAnimAsset, const FAnimDocumentArgs& InArgs, FString& OutDocumentLink)
{
	TSharedPtr<SWidget> Result = SNullWidget::NullWidget;
	if (InAnimAsset)
	{
		if (UAnimSequence* Sequence = Cast<UAnimSequence>(InAnimAsset))
		{
			Result = SNew(SSequenceEditor, InArgs.PreviewScene.Pin().ToSharedRef(), InArgs.EditableSkeleton.Pin().ToSharedRef(), InArgs.OnPostUndo)
				.Sequence(Sequence)
				.OnObjectsSelected(InArgs.OnDespatchObjectsSelected)
				.OnAnimNotifiesChanged(InArgs.OnDespatchAnimNotifiesChanged)
				.OnInvokeTab(InArgs.OnDespatchInvokeTab);

			OutDocumentLink = TEXT("Engine/Animation/Sequences");
		}
		else if (UAnimComposite* Composite = Cast<UAnimComposite>(InAnimAsset))
		{
			Result = SNew(SAnimCompositeEditor, InArgs.PreviewScene.Pin().ToSharedRef(), InArgs.EditableSkeleton.Pin().ToSharedRef(), InArgs.OnPostUndo)
				.Composite(Composite)
				.OnObjectsSelected(InArgs.OnDespatchObjectsSelected)
				.OnAnimNotifiesChanged(InArgs.OnDespatchAnimNotifiesChanged)
				.OnInvokeTab(InArgs.OnDespatchInvokeTab);

			OutDocumentLink = TEXT("Engine/Animation/AnimationComposite");
		}
		else if (UAnimMontage* Montage = Cast<UAnimMontage>(InAnimAsset))
		{
			FMontageEditorRequiredArgs RequiredArgs(InArgs.PreviewScene.Pin().ToSharedRef(), InArgs.EditableSkeleton.Pin().ToSharedRef(), InArgs.OnPostUndo, InArgs.OnAnimNotifiesChanged, InArgs.OnSectionsChanged);

			Result = SNew(SMontageEditor, RequiredArgs)
				.Montage(Montage)
				.OnSectionsChanged(InArgs.OnDespatchSectionsChanged)
				.OnInvokeTab(InArgs.OnDespatchInvokeTab)
				.OnObjectsSelected(InArgs.OnDespatchObjectsSelected)
				.OnAnimNotifiesChanged(InArgs.OnDespatchAnimNotifiesChanged);

			OutDocumentLink = TEXT("Engine/Animation/AnimMontage");
		}
		else if (UPoseAsset* PoseAsset = Cast<UPoseAsset>(InAnimAsset))
		{
			Result = SNew(SPoseEditor, InArgs.PersonaToolkit.Pin().ToSharedRef(), InArgs.EditableSkeleton.Pin().ToSharedRef(), InArgs.PreviewScene.Pin().ToSharedRef())
				.PoseAsset(PoseAsset);

			OutDocumentLink = TEXT("Engine/Animation/Sequences");
		}
		else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(InAnimAsset))
		{
			Result = SNew(SBlendSpaceEditor, InArgs.PreviewScene.Pin().ToSharedRef(), InArgs.OnPostUndo)
				.BlendSpace(BlendSpace);

			if (Cast<UAimOffsetBlendSpace>(InAnimAsset))
			{
				OutDocumentLink = TEXT("Engine/Animation/AimOffset");
			}
			else
			{
				OutDocumentLink = TEXT("Engine/Animation/Blendspaces");
			}
		}
		else if (UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(InAnimAsset))
		{
			Result = SNew(SBlendSpaceEditor1D, InArgs.PreviewScene.Pin().ToSharedRef(), InArgs.OnPostUndo)
				.BlendSpace1D(BlendSpace1D);

			if (Cast<UAimOffsetBlendSpace1D>(InAnimAsset))
			{
				OutDocumentLink = TEXT("Engine/Animation/AimOffset");
			}
			else
			{
				OutDocumentLink = TEXT("Engine/Animation/Blendspaces");
			}
		}
	}

	if (Result.IsValid())
	{
		InAnimAsset->SetFlags(RF_Transactional);
	}

	return Result.ToSharedRef();
}

void FPersonaModule::CustomizeMeshDetails(const TSharedRef<IDetailsView>& InDetailsView, const TSharedRef<IPersonaToolkit>& InPersonaToolkit)
{
	InDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FPersonaMeshDetails::MakeInstance, TWeakPtr<IPersonaToolkit>(InPersonaToolkit)));
}

void FPersonaModule::ImportNewAsset(USkeleton* InSkeleton, EFBXImportType DefaultImportType)
{
	TSharedRef<SImportPathDialog> NewAnimDlg =
		SNew(SImportPathDialog);

	if (NewAnimDlg->ShowModal() != EAppReturnType::Cancel)
	{
		FString AssetPath = NewAnimDlg->GetAssetPath();

		UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();
		ImportUI->Skeleton = InSkeleton;
		ImportUI->MeshTypeToImport = DefaultImportType;

		FbxMeshUtils::SetImportOption(ImportUI);

		// now I have to set skeleton on it. 
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().ImportAssetsWithDialog(AssetPath);
	}
}

void PopulateWithAssets(FName ClassName, FName SkeletonMemberName, const FString& SkeletonString, TArray<FAssetData>& OutAssets)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.ClassNames.Add(ClassName);
	Filter.TagsAndValues.Add(SkeletonMemberName, SkeletonString);

	AssetRegistryModule.Get().GetAssets(Filter, OutAssets);
}

void FPersonaModule::TestSkeletonCurveNamesForUse(const TSharedRef<IEditableSkeleton>& InEditableSkeleton) const
{
	const USkeleton& Skeleton = InEditableSkeleton->GetSkeleton();

	if (const FSmartNameMapping* Mapping = Skeleton.GetSmartNameContainer(USkeleton::AnimCurveMappingName))
	{
		const FString SkeletonString = FAssetData(&Skeleton).GetExportTextName();

		TArray<FAssetData> SkeletalMeshes;
		PopulateWithAssets(USkeletalMesh::StaticClass()->GetFName(), GET_MEMBER_NAME_CHECKED(USkeletalMesh, Skeleton), SkeletonString, SkeletalMeshes);
		TArray<FAssetData> Animations;
		PopulateWithAssets(UAnimSequence::StaticClass()->GetFName(), FName("Skeleton"), SkeletonString, Animations);

		FText TimeTakenMessage = FText::Format(LOCTEXT("TimeTakenWarning", "In order to verify curve usage all Skeletal Meshes and Animations that use this skeleton will be loaded, this may take some time.\n\nProceed?\n\nNumber of Meshes: {0}\nNumber of Animations: {1}"), FText::AsNumber(SkeletalMeshes.Num()), FText::AsNumber(Animations.Num()));

		if (FMessageDialog::Open(EAppMsgType::YesNo, TimeTakenMessage) == EAppReturnType::Yes)
		{
			const FText LoadingStatusUpdate = FText::Format(LOCTEXT("VerifyCurves_LoadingAllAnimations", "Loading all animations for skeleton '{0}'"), FText::FromString(Skeleton.GetName()));
			{
				FScopedSlowTask LoadingAnimSlowTask(Animations.Num(), LoadingStatusUpdate);
				LoadingAnimSlowTask.MakeDialog();

				// Loop through all animations to load then, this makes sure smart names are all up to date
				for (const FAssetData& Anim : Animations)
				{
					LoadingAnimSlowTask.EnterProgressFrame();
					UAnimSequence* Seq = Cast<UAnimSequence>(Anim.GetAsset());
				}
			}

			// Grab all curve names for this skeleton
			TArray<FName> UnusedNames;
			Mapping->FillNameArray(UnusedNames);

			const FText ProcessingStatusUpdate = FText::Format(LOCTEXT("VerifyCurves_ProcessingCurveUsage", "Looking at curve useage for each skeletal mesh of skeleton '{0}'"), FText::FromString(Skeleton.GetName()));
			{
				FScopedSlowTask LoadingSkelMeshSlowTask(SkeletalMeshes.Num(), ProcessingStatusUpdate);
				LoadingSkelMeshSlowTask.MakeDialog();

				for (int32 MeshIdx = 0; MeshIdx < SkeletalMeshes.Num(); ++MeshIdx)
				{
					LoadingSkelMeshSlowTask.EnterProgressFrame();

					const USkeletalMesh* Mesh = Cast<USkeletalMesh>(SkeletalMeshes[MeshIdx].GetAsset());

					// Filter morph targets from curves
					const TArray<UMorphTarget*>& MorphTargets = Mesh->MorphTargets;
					for (int32 I = 0; I < MorphTargets.Num(); ++I)
					{
						const int32 CurveIndex = UnusedNames.RemoveSingleSwap(MorphTargets[I]->GetFName(), false);
					}

					// Filter material params from curves
					for (const FSkeletalMaterial& Mat : Mesh->Materials)
					{
						if (UnusedNames.Num() == 0)
						{
							break; // Done
						}

						UMaterial* Material = (Mat.MaterialInterface != nullptr) ? Mat.MaterialInterface->GetMaterial() : nullptr;
						if (Material)
						{
							TArray<FName> OutParameterNames;
							TArray<FGuid> OutParameterIds;

							// Retrieve all scalar parameter names from the material
							Material->GetAllScalarParameterNames(OutParameterNames, OutParameterIds);

							for (FName SPName : OutParameterNames)
							{
								UnusedNames.RemoveSingleSwap(SPName);
							}
						}
					}
				}
			}

			FMessageLog CurveOutput("Persona");
			CurveOutput.NewPage(LOCTEXT("PersonaMessageLogName", "Persona"));

			bool bFoundIssue = false;

			const FText ProcessingAnimStatusUpdate = FText::Format(LOCTEXT("FindUnusedCurves_ProcessingSkeletalMeshes", "Finding animations that reference unused curves on skeleton '{0}'"), FText::FromString(Skeleton.GetName()));
			{
				FScopedSlowTask ProcessingAnimationsSlowTask(Animations.Num(), ProcessingAnimStatusUpdate);
				ProcessingAnimationsSlowTask.MakeDialog();

				for (const FAssetData& Anim : Animations)
				{
					ProcessingAnimationsSlowTask.EnterProgressFrame();
					UAnimSequence* Seq = Cast<UAnimSequence>(Anim.GetAsset());

					TSharedPtr<FTokenizedMessage> Message;
					for (FFloatCurve& Curve : Seq->RawCurveData.FloatCurves)
					{
						if (UnusedNames.Contains(Curve.Name.DisplayName))
						{
							bFoundIssue = true;
							if (!Message.IsValid())
							{
								Message = CurveOutput.Warning();
								Message->AddToken(FAssetNameToken::Create(Anim.ObjectPath.ToString(), FText::FromName(Anim.AssetName)));
								Message->AddToken(FTextToken::Create(LOCTEXT("VerifyCurves_FoundAnimationsWithUnusedReferences", "References the following curves that are not used for either morph targets or material parameters and so may be unneeded")));
							}
							CurveOutput.Info(FText::FromName(Curve.Name.DisplayName));
						}
					}
				}
			}

			if (bFoundIssue)
			{
				CurveOutput.Notify();
			}
		}
	}
}

void FPersonaModule::ApplyCompression(TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences)
{
	FDlgAnimCompression AnimCompressionDialog(AnimSequences);
	AnimCompressionDialog.ShowModal();
}

void FPersonaModule::ExportToFBX(TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences, USkeletalMesh* SkeletalMesh)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if (DesktopPlatform)
	{
		if (SkeletalMesh == NULL)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ExportToFBXExportMissingSkeletalMesh", "ERROR: Missing skeletal mesh"));
			return;
		}

		if (AnimSequences.Num() > 0)
		{
			//Get parent window for dialogs
			TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();

			void* ParentWindowWindowHandle = NULL;

			if (RootWindow.IsValid() && RootWindow->GetNativeWindow().IsValid())
			{
				ParentWindowWindowHandle = RootWindow->GetNativeWindow()->GetOSWindowHandle();
			}

			//Cache anim file names
			TArray<FString> AnimFileNames;
			for (auto Iter = AnimSequences.CreateIterator(); Iter; ++Iter)
			{
				AnimFileNames.Add(Iter->Get()->GetName() + TEXT(".fbx"));
			}

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			FString DestinationFolder;

			const FString Title = LOCTEXT("ExportFBXsToFolderTitle", "Choose a destination folder for the FBX file(s)").ToString();

			if (AnimSequences.Num() > 1)
			{
				bool bFolderValid = false;
				// More than one file, just ask for directory
				while (!bFolderValid)
				{
					const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
						ParentWindowWindowHandle,
						Title,
						FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
						DestinationFolder
						);

					if (!bFolderSelected)
					{
						// User canceled, return
						return;
					}

					FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, DestinationFolder);
					FPaths::NormalizeFilename(DestinationFolder);

					//Check whether there are any fbx filename conflicts in this folder
					for (auto Iter = AnimFileNames.CreateIterator(); Iter; ++Iter)
					{
						FString& AnimFileName = *Iter;
						FString FullPath = DestinationFolder + "/" + AnimFileName;

						bFolderValid = true;
						if (PlatformFile.FileExists(*FullPath))
						{
							FFormatNamedArguments Args;
							Args.Add(TEXT("DestinationFolder"), FText::FromString(DestinationFolder));
							const FText DialogMessage = FText::Format(LOCTEXT("ExportToFBXFileOverwriteMessage", "Exporting to '{DestinationFolder}' will cause one or more existing FBX files to be overwritten. Would you like to continue?"), Args);
							EAppReturnType::Type DialogReturn = FMessageDialog::Open(EAppMsgType::YesNo, DialogMessage);
							bFolderValid = (EAppReturnType::Yes == DialogReturn);
							break;
						}
					}
				}
			}
			else
			{
				// One file only, ask for full filename.
				// Can set bFolderValid from the SaveFileDialog call as the window will handle 
				// duplicate files for us.
				TArray<FString> TempDestinationNames;
				bool bSave = DesktopPlatform->SaveFileDialog(
					ParentWindowWindowHandle,
					Title,
					FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
					AnimSequences[0]->GetName(),
					"FBX  |*.fbx",
					EFileDialogFlags::None,
					TempDestinationNames
					);

				if (!bSave)
				{
					// Canceled
					return;
				}
				check(TempDestinationNames.Num() == 1);
				check(AnimFileNames.Num() == 1);

				DestinationFolder = FPaths::GetPath(TempDestinationNames[0]);
				AnimFileNames[0] = FPaths::GetCleanFilename(TempDestinationNames[0]);

				FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, DestinationFolder);
			}

			EAppReturnType::Type DialogReturn = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ExportToFBXExportSkeletalMeshToo", "Would you like to export the current skeletal mesh with the animation(s)?"));
			bool bSaveSkeletalMesh = EAppReturnType::Yes == DialogReturn;

			const bool bShowCancel = false;
			const bool bShowProgressDialog = true;
			GWarn->BeginSlowTask(LOCTEXT("ExportToFBXProgress", "Exporting Animation(s) to FBX"), bShowProgressDialog, bShowCancel);

			// make sure to use SkeletalMesh, when export inside of Persona
			const int32 NumberOfAnimations = AnimSequences.Num();
			bool ExportBatch = (NumberOfAnimations > 1);
			bool ExportAll = false;
			bool ExportCancel = false;
			for (int32 i = 0; i < NumberOfAnimations; ++i)
			{
				GWarn->UpdateProgress(i, NumberOfAnimations);

				UAnimSequence* AnimSequence = AnimSequences[i].Get();

				FString FileName = FString::Printf(TEXT("%s/%s"), *DestinationFolder, *AnimFileNames[i]);

				FbxAnimUtils::ExportAnimFbx(*FileName, AnimSequence, SkeletalMesh, bSaveSkeletalMesh, ExportBatch, ExportAll, ExportCancel);
				if (ExportBatch && ExportCancel)
				{
					//The user cancel the batch export
					break;
				}
			}

			GWarn->EndSlowTask();
		}
	}
}

void FPersonaModule::AddLoopingInterpolation(TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences)
{
	FText WarningMessage = LOCTEXT("AddLoopiingInterpolation", "This will add an extra first frame at the end of the animation to create a better looping interpolation. This action cannot be undone. Would you like to proceed?");

	if (FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage) == EAppReturnType::Yes)
	{
		for (auto Animation : AnimSequences)
		{
			// get first frame and add to the last frame and go through track
			// now calculating old animated space bases
			Animation->AddLoopingInterpolation();
		}
	}
}

void FPersonaModule::CustomizeSlotNodeDetails(const TSharedRef<class IDetailsView>& InDetailsView, FOnInvokeTab InOnInvokeTab)
{
	InDetailsView->RegisterInstancedCustomPropertyLayout(UAnimGraphNode_Slot::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FAnimGraphNodeSlotDetails::MakeInstance, InOnInvokeTab));
}

IPersonaEditorModeManager* FPersonaModule::CreatePersonaEditorModeManager()
{
	return new FPersonaEditorModeManager();
}

void FPersonaModule::AddCommonToolbarExtensions(FToolBarBuilder& InToolbarBuilder, TSharedRef<IPersonaToolkit> PersonaToolkit)
{
	TWeakPtr<IPersonaToolkit> WeakPersonaToolkit = PersonaToolkit;

	// Handler to hang notifications on
	struct FNotificationHandler : public TSharedFromThis<FNotificationHandler>
	{
		static void HandleApplyPreviewMesh(TSharedPtr<FNotificationHandler> InNotificationHandler, TWeakPtr<IPersonaToolkit> InWeakPersonaToolkit)
		{
			TSharedPtr<IPersonaToolkit> PinnedPersonaToolkit = InWeakPersonaToolkit.Pin();
			if(PinnedPersonaToolkit.IsValid())	// Toolkit can become invalid while the toast is open
			{
				PinnedPersonaToolkit->SetPreviewMesh(PinnedPersonaToolkit->GetPreviewScene()->GetPreviewMesh(), true);
				if(InNotificationHandler->Notification.IsValid())
				{
					InNotificationHandler->Notification->Fadeout();
				}
			}
		}

		TSharedPtr<SNotificationItem> Notification;
	};

	auto CreatePreviewMeshComboButtonContents = [WeakPersonaToolkit]()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.BeginSection(TEXT("ChoosePreviewMesh"), LOCTEXT("ChoosePreviewMesh", "Choose Preview Mesh"));
		{
			FAssetPickerConfig AssetPickerConfig;
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([WeakPersonaToolkit](const FAssetData& AssetData)
			{
				if (WeakPersonaToolkit.IsValid())
				{
					WeakPersonaToolkit.Pin()->SetPreviewMesh(Cast<USkeletalMesh>(AssetData.GetAsset()), false);
				}

				if(WeakPersonaToolkit.IsValid())	// SetPreviewMesh can invalidate the persona toolkit, so check it here before displaying toast
				{
					TSharedPtr<FNotificationHandler> NotificationHandler = MakeShared<FNotificationHandler>();

					FNotificationInfo Info(LOCTEXT("PreviewMeshSetTemporarily", "Preview mesh set temporarily"));
					Info.ExpireDuration = 10.0f;
					Info.bUseLargeFont = true;
					Info.ButtonDetails.Add(
						FNotificationButtonInfo(
							LOCTEXT("ApplyToAsset", "Apply To Asset"), 
							LOCTEXT("ApplyToAssetToolTip", "The preview mesh has changed, but it will not be able to be saved until it is applied to the asset. Click here to make the change to the preview mesh persistent."),
							FSimpleDelegate::CreateStatic(&FNotificationHandler::HandleApplyPreviewMesh, NotificationHandler, WeakPersonaToolkit),
							SNotificationItem::CS_Success));

					NotificationHandler->Notification = FSlateNotificationManager::Get().AddNotification(Info);
					if (NotificationHandler->Notification.IsValid())
					{
						NotificationHandler->Notification->SetCompletionState(SNotificationItem::CS_Success);
					}

					FSlateApplication::Get().DismissAllMenus();
				}
			});
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
			AssetPickerConfig.Filter.bRecursiveClasses = false;
			AssetPickerConfig.Filter.ClassNames.Add(USkeletalMesh::StaticClass()->GetFName());
			AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda([WeakPersonaToolkit](const FAssetData& AssetData)
			{
				if (WeakPersonaToolkit.IsValid())
				{
					if(WeakPersonaToolkit.Pin()->GetContext() == UPhysicsAsset::StaticClass()->GetFName())
					{
						return false;
					}

					FString TagValue;
					if (AssetData.GetTagValue("Skeleton", TagValue))
					{
						return TagValue != FAssetData(WeakPersonaToolkit.Pin()->GetSkeleton()).GetExportTextName();
					}
				}
				return true;
			});
			if (WeakPersonaToolkit.IsValid())
			{
				AssetPickerConfig.InitialAssetSelection = FAssetData(WeakPersonaToolkit.Pin()->GetPreviewMesh());
			}

			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

			TSharedPtr<SBox> MenuEntry = SNew(SBox)
				.WidthOverride(300.0f)
				.HeightOverride(300.0f)
				[
					ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
				];

			MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	};

	InToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateLambda(CreatePreviewMeshComboButtonContents),
		LOCTEXT("SetPreviewMesh", "Preview Mesh"),
		LOCTEXT("SetPreviewMeshTooltip", "Set a new preview skeletal mesh for the current asset (stored per-animation or per-skeleton)"),
		FSlateIcon("EditorStyle", "Persona.TogglePreviewAsset", "Persona.TogglePreviewAsset.Small")
		);
}

void FPersonaModule::HandleNewAnimNotifyBlueprintCreated(UBlueprint* InBlueprint)
{
	if (InBlueprint->BlueprintType == BPTYPE_Normal)
	{
		UEdGraph* const NewGraph = FBlueprintEditorUtils::CreateNewGraph(InBlueprint, "Received_Notify", UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph(InBlueprint, NewGraph, /*bIsUserCreated=*/ false, UAnimNotify::StaticClass());
		InBlueprint->LastEditedDocuments.Add(NewGraph);
	}
}

void FPersonaModule::HandleNewAnimNotifyStateBlueprintCreated(UBlueprint* InBlueprint)
{
	if (InBlueprint->BlueprintType == BPTYPE_Normal)
	{
		UEdGraph* const NewGraph = FBlueprintEditorUtils::CreateNewGraph(InBlueprint, "Received_NotifyTick", UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph(InBlueprint, NewGraph, /*bIsUserCreated=*/ false, UAnimNotifyState::StaticClass());
		InBlueprint->LastEditedDocuments.Add(NewGraph);
	}
}

#undef LOCTEXT_NAMESPACE
