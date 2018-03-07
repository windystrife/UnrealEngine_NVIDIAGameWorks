// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PreviewSceneCustomizations.h"
#include "Modules/ModuleManager.h"
#include "AssetData.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PersonaPreviewSceneDescription.h"
#include "Engine/PreviewMeshCollection.h"
#include "Factories/PreviewMeshCollectionFactory.h"
#include "IPropertyUtilities.h"
#include "Preferences/PersonaOptions.h"
#include "SButton.h"
#include "STextBlock.h"
#include "SCheckBox.h"
#include "SImage.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Animation/AnimBlueprint.h"
#include "UObject/UObjectIterator.h"
#include "STextBlock.h"
#include "SCheckBox.h"
#include "PhysicsEngine/PhysicsAsset.h"

#define LOCTEXT_NAMESPACE "PreviewSceneCustomizations"

// static list that contains available classes, so that we can only allow these classes
TArray<FName> FPreviewSceneDescriptionCustomization::AvailableClassNameList;

FPreviewSceneDescriptionCustomization::FPreviewSceneDescriptionCustomization(const FString& InSkeletonName, const TSharedRef<class IPersonaToolkit>& InPersonaToolkit)
	: SkeletonName(InSkeletonName)
	, PersonaToolkit(InPersonaToolkit)
	, PreviewScene(StaticCastSharedRef<FAnimationEditorPreviewScene>(InPersonaToolkit->GetPreviewScene()))
	, EditableSkeleton(InPersonaToolkit->GetEditableSkeleton())
{
	// setup custom factory up-front so we can control its lifetime
	FactoryToUse = NewObject<UPreviewMeshCollectionFactory>();
	FactoryToUse->AddToRoot();

	// only first time
	if (AvailableClassNameList.Num() == 0)
	{
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			if (ClassIt->IsChildOf(UDataAsset::StaticClass()) && ClassIt->ImplementsInterface(UPreviewCollectionInterface::StaticClass()))
			{
				AvailableClassNameList.Add(ClassIt->GetFName());
			}
		}
	}
}

FPreviewSceneDescriptionCustomization::~FPreviewSceneDescriptionCustomization()
{
	if (FactoryToUse)
	{
		FactoryToUse->RemoveFromRoot();
		FactoryToUse = nullptr;
	}
}

void FPreviewSceneDescriptionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	TSharedRef<IPropertyHandle> AnimationModeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPersonaPreviewSceneDescription, AnimationMode));
	AnimationModeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPreviewSceneDescriptionCustomization::HandleAnimationModeChanged));
	TSharedRef<IPropertyHandle> AnimationProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPersonaPreviewSceneDescription, Animation));
	AnimationProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPreviewSceneDescriptionCustomization::HandleAnimationChanged));
	TSharedRef<IPropertyHandle> SkeletalMeshProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPersonaPreviewSceneDescription, PreviewMesh));
	TSharedRef<IPropertyHandle> AdditionalMeshesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPersonaPreviewSceneDescription, AdditionalMeshes));

	DetailBuilder.EditCategory("Animation")
	.AddProperty(AnimationModeProperty);

	DetailBuilder.EditCategory("Animation")
	.AddProperty(AnimationProperty)
	.CustomWidget()
	.NameContent()
	[
		AnimationProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(250.0f)
	.MinDesiredWidth(250.0f)
	[
		SNew(SObjectPropertyEntryBox)
		.AllowedClass(UAnimationAsset::StaticClass())
		.PropertyHandle(AnimationProperty)
		.OnShouldFilterAsset(this, &FPreviewSceneDescriptionCustomization::HandleShouldFilterAsset, false)
		.ThumbnailPool(DetailBuilder.GetThumbnailPool())
	];

	if (PersonaToolkit.Pin()->GetContext() != USkeletalMesh::StaticClass()->GetFName())
	{
		FText PreviewMeshName;
		if (PersonaToolkit.Pin()->GetContext() == UAnimationAsset::StaticClass()->GetFName())
		{
			PreviewMeshName = FText::Format(LOCTEXT("PreviewMeshAnimation", "{0}\n(Animation)"), SkeletalMeshProperty->GetPropertyDisplayName());
		}
		else if(PersonaToolkit.Pin()->GetContext() == UAnimBlueprint::StaticClass()->GetFName())
		{
			PreviewMeshName = FText::Format(LOCTEXT("PreviewMeshAnimBlueprint", "{0}\n(Animation Blueprint)"), SkeletalMeshProperty->GetPropertyDisplayName());
		}
		else if(PersonaToolkit.Pin()->GetContext() == UPhysicsAsset::StaticClass()->GetFName())
		{
			PreviewMeshName = FText::Format(LOCTEXT("PreviewMeshPhysicsAsset", "{0}\n(Physics Asset)"), SkeletalMeshProperty->GetPropertyDisplayName());
		}
		else
		{
			PreviewMeshName = FText::Format(LOCTEXT("PreviewMeshSkeleton", "{0}\n(Skeleton)"), SkeletalMeshProperty->GetPropertyDisplayName());
		}

		DetailBuilder.EditCategory("Mesh")
		.AddProperty(SkeletalMeshProperty)
		.CustomWidget()
		.NameContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SkeletalMeshProperty->CreatePropertyNameWidget(PreviewMeshName)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("ApplyToAsset", "Apply To Asset"))
				.ToolTipText(LOCTEXT("ApplyToAssetToolTip", "The preview mesh has changed, but it will not be able to be saved until it is applied to the asset. Click here to make the change to the preview mesh persistent."))
				.Visibility_Lambda([this]()
				{
					TSharedPtr<IPersonaToolkit> PinnedPersonaToolkit = PersonaToolkit.Pin();
					USkeletalMesh* SkeletalMesh = PinnedPersonaToolkit->GetPreviewMesh();
					if(SkeletalMesh == nullptr)
					{
						SkeletalMesh = EditableSkeleton.Pin()->GetSkeleton().GetPreviewMesh();
					}
					return (SkeletalMesh != PinnedPersonaToolkit->GetPreviewScene()->GetPreviewMesh()) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnClicked_Lambda([this]() 
				{
					TSharedPtr<IPersonaToolkit> PinnedPersonaToolkit = PersonaToolkit.Pin();
					PinnedPersonaToolkit->SetPreviewMesh(PinnedPersonaToolkit->GetPreviewScene()->GetPreviewMesh(), true);
					return FReply::Handled();
				})
			]
		]
		.ValueContent()
		.MaxDesiredWidth(250.0f)
		.MinDesiredWidth(250.0f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.PropertyHandle(SkeletalMeshProperty)
			.OnShouldFilterAsset(this, &FPreviewSceneDescriptionCustomization::HandleShouldFilterAsset, PersonaToolkit.Pin()->GetContext() == UPhysicsAsset::StaticClass()->GetFName())
			.OnObjectChanged(this, &FPreviewSceneDescriptionCustomization::HandleMeshChanged)
			.ThumbnailPool(DetailBuilder.GetThumbnailPool())
		];
	}
	else
	{
		DetailBuilder.HideProperty(SkeletalMeshProperty);
	}

	// set the skeleton to use in our factory as we shouldn't be picking one here
	FactoryToUse->CurrentSkeleton = &EditableSkeleton.Pin()->GetSkeleton();
	TArray<UFactory*> FactoriesToUse({ FactoryToUse });

	FAssetData AdditionalMeshesAsset;
	AdditionalMeshesProperty->GetValue(AdditionalMeshesAsset);

	// bAllowPreviewMeshCollectionsToSelectFromDifferentSkeletons option
	DetailBuilder.EditCategory("Additional Meshes")
	.AddCustomRow(LOCTEXT("AdditvesMeshOption", "Additional Mesh Selection Option"))
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("AdditvesMeshSelectionFromDifferentSkeletons", "Allow Different Skeletons"))
		.ToolTipText(LOCTEXT("AdditvesMeshSelectionFromDifferentSkeletons_ToolTip", "When selecting additional mesh, whether or not filter by the current skeleton."))
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked(this, &FPreviewSceneDescriptionCustomization::HandleAllowDifferentSkeletonsIsChecked)
		.OnCheckStateChanged(this, &FPreviewSceneDescriptionCustomization::HandleAllowDifferentSkeletonsCheckedStateChanged)
	];

	DetailBuilder.EditCategory("Additional Meshes")
	.AddProperty(AdditionalMeshesProperty)
	.CustomWidget()
	.NameContent()
	[
		AdditionalMeshesProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(250.0f)
	.MinDesiredWidth(250.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SObjectPropertyEntryBox)
			// searching uobject is too much for a scale of Fortnite
			// for now we just allow UDataAsset
			.AllowedClass(UDataAsset::StaticClass())
			.PropertyHandle(AdditionalMeshesProperty)
			.OnShouldFilterAsset(this, &FPreviewSceneDescriptionCustomization::HandleShouldFilterAdditionalMesh, true)
			.OnObjectChanged(this, &FPreviewSceneDescriptionCustomization::HandleAdditionalMeshesChanged, &DetailBuilder)
			.ThumbnailPool(DetailBuilder.GetThumbnailPool())
			.NewAssetFactories(FactoriesToUse)
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(SButton)
			.Visibility(this, &FPreviewSceneDescriptionCustomization::GetSaveButtonVisibility, AdditionalMeshesProperty)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FPreviewSceneDescriptionCustomization::OnSaveCollectionClicked, AdditionalMeshesProperty, &DetailBuilder)
			.ContentPadding(4.0f)
			.ForegroundColor(FSlateColor::UseForeground())
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("Persona.SavePreviewMeshCollection"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];

	if (AdditionalMeshesAsset.IsValid())
	{
		TArray<UObject*> Objects;
		Objects.Add(AdditionalMeshesAsset.GetAsset());

		IDetailPropertyRow* PropertyRow = DetailBuilder.EditCategory("Additional Meshes")
		.AddExternalObjectProperty(Objects, "SkeletalMeshes");

		if (PropertyRow)
		{
			PropertyRow->ShouldAutoExpand(true);
		}
	}
}

EVisibility FPreviewSceneDescriptionCustomization::GetSaveButtonVisibility(TSharedRef<IPropertyHandle> AdditionalMeshesProperty) const
{
	FAssetData AdditionalMeshesAsset;
	AdditionalMeshesProperty->GetValue(AdditionalMeshesAsset);
	UObject* Object = AdditionalMeshesAsset.GetAsset();

	return Object == nullptr || !Object->HasAnyFlags(RF_Transient) ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply FPreviewSceneDescriptionCustomization::OnSaveCollectionClicked(TSharedRef<IPropertyHandle> AdditionalMeshesProperty, IDetailLayoutBuilder* DetailLayoutBuilder)
{
	FAssetData AdditionalMeshesAsset;
	AdditionalMeshesProperty->GetValue(AdditionalMeshesAsset);
	UPreviewMeshCollection* DefaultPreviewMeshCollection = CastChecked<UPreviewMeshCollection>(AdditionalMeshesAsset.GetAsset());
	if (DefaultPreviewMeshCollection)
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UPreviewMeshCollection* NewPreviewMeshCollection = Cast<UPreviewMeshCollection>(AssetTools.CreateAssetWithDialog(UPreviewMeshCollection::StaticClass(), FactoryToUse));
		if (NewPreviewMeshCollection)
		{
			NewPreviewMeshCollection->Skeleton = DefaultPreviewMeshCollection->Skeleton;
			NewPreviewMeshCollection->SkeletalMeshes = DefaultPreviewMeshCollection->SkeletalMeshes;
			AdditionalMeshesProperty->SetValue(FAssetData(NewPreviewMeshCollection));
			PreviewScene.Pin()->SetAdditionalMeshes(NewPreviewMeshCollection);

			DetailLayoutBuilder->ForceRefreshDetails();
		}
	}

	return FReply::Handled();
}

bool FPreviewSceneDescriptionCustomization::HandleShouldFilterAdditionalMesh(const FAssetData& InAssetData, bool bCanUseDifferentSkeleton)
{
	// see if it's in valid class set
	bool bValidClass = false;

	// first to see if it's allowed class
	for (FName& ClassName: AvailableClassNameList)
	{
		if (ClassName == InAssetData.AssetClass)
		{
			bValidClass = true;
			break;
		}
	}

	// not valid class, filter it
	if (!bValidClass)
	{
		return true;
	}

	return HandleShouldFilterAsset(InAssetData, bCanUseDifferentSkeleton);
}

bool FPreviewSceneDescriptionCustomization::HandleShouldFilterAsset(const FAssetData& InAssetData, bool bCanUseDifferentSkeleton)
{
	if (bCanUseDifferentSkeleton && GetDefault<UPersonaOptions>()->bAllowPreviewMeshCollectionsToSelectFromDifferentSkeletons)
	{
		return false;
	}

	FString SkeletonTag = InAssetData.GetTagValueRef<FString>("Skeleton");
	if (SkeletonTag == SkeletonName)
	{
		return false;
	}

	return true;
}

void FPreviewSceneDescriptionCustomization::HandleAnimationModeChanged()
{
	UPersonaPreviewSceneDescription* PersonaPreviewSceneDescription = PreviewScene.Pin()->GetPreviewSceneDescription();
	switch (PersonaPreviewSceneDescription->AnimationMode)
	{
	case EPreviewAnimationMode::Default:
		PreviewScene.Pin()->ShowDefaultMode();
		break;
	case EPreviewAnimationMode::ReferencePose:
		PreviewScene.Pin()->ShowReferencePose(true);
		break;
	case EPreviewAnimationMode::UseSpecificAnimation:
		PreviewScene.Pin()->SetPreviewAnimationAsset(PersonaPreviewSceneDescription->Animation.LoadSynchronous());
		break;
	}
}

void FPreviewSceneDescriptionCustomization::HandleAnimationChanged()
{
	UPersonaPreviewSceneDescription* PersonaPreviewSceneDescription = PreviewScene.Pin()->GetPreviewSceneDescription();
	UAnimationAsset* AnimationAsset = PersonaPreviewSceneDescription->Animation.LoadSynchronous();
	PreviewScene.Pin()->SetPreviewAnimationAsset(AnimationAsset);

	if (AnimationAsset != nullptr)
	{
		PreviewScene.Pin()->GetPreviewSceneDescription()->AnimationMode = EPreviewAnimationMode::UseSpecificAnimation;
	}
}

void FPreviewSceneDescriptionCustomization::HandleMeshChanged(const FAssetData& InAssetData)   
{
	USkeletalMesh* NewPreviewMesh = Cast<USkeletalMesh>(InAssetData.GetAsset());
	PersonaToolkit.Pin()->SetPreviewMesh(NewPreviewMesh, false);
}

void FPreviewSceneDescriptionCustomization::HandleAdditionalMeshesChanged(const FAssetData& InAssetData, IDetailLayoutBuilder* DetailLayoutBuilder)
{
	UDataAsset* MeshCollection = Cast<UDataAsset>(InAssetData.GetAsset());
	if (!MeshCollection || MeshCollection->GetClass()->ImplementsInterface(UPreviewCollectionInterface::StaticClass()))
	{
		PreviewScene.Pin()->SetAdditionalMeshes(MeshCollection);
	}

	DetailLayoutBuilder->ForceRefreshDetails();
}

void FPreviewSceneDescriptionCustomization::HandleAllowDifferentSkeletonsCheckedStateChanged(ECheckBoxState CheckState)
{
	GetMutableDefault<UPersonaOptions>()->bAllowPreviewMeshCollectionsToSelectFromDifferentSkeletons = (CheckState == ECheckBoxState::Checked);
}

ECheckBoxState FPreviewSceneDescriptionCustomization::HandleAllowDifferentSkeletonsIsChecked() const
{
	return GetDefault<UPersonaOptions>()->bAllowPreviewMeshCollectionsToSelectFromDifferentSkeletons? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// FPreviewMeshCollectionEntryCustomization
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FPreviewMeshCollectionEntryCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// get the enclosing preview mesh collection to determine the skeleton we want
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	check(OuterObjects.Num() > 0);
		
	if (OuterObjects[0] != nullptr)
	{
		FString SkeletonName = FAssetData(CastChecked<UPreviewMeshCollection>(OuterObjects[0])->Skeleton).GetExportTextName();

		PropertyHandle->GetParentHandle()->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPreviewMeshCollectionEntryCustomization::HandleMeshesArrayChanged, CustomizationUtils.GetPropertyUtilities()));

		TSharedPtr<IPropertyHandle> SkeletalMeshProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPreviewMeshCollectionEntry, SkeletalMesh));
		if (SkeletalMeshProperty.IsValid())
		{
			HeaderRow.NameContent()
			[
				SkeletalMeshProperty->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MaxDesiredWidth(250.0f)
			.MinDesiredWidth(250.0f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(USkeletalMesh::StaticClass())
				.PropertyHandle(SkeletalMeshProperty)
				.OnShouldFilterAsset(this, &FPreviewMeshCollectionEntryCustomization::HandleShouldFilterAsset, SkeletonName)
				.OnObjectChanged(this, &FPreviewMeshCollectionEntryCustomization::HandleMeshChanged)
				.ThumbnailPool(CustomizationUtils.GetThumbnailPool())
			];
		}
	}
}

bool FPreviewMeshCollectionEntryCustomization::HandleShouldFilterAsset(const FAssetData& InAssetData, FString SkeletonName)
{
	if (GetDefault<UPersonaOptions>()->bAllowPreviewMeshCollectionsToSelectFromDifferentSkeletons)
	{
		return false;
	}

	FString SkeletonTag = InAssetData.GetTagValueRef<FString>("Skeleton");
	if (SkeletonTag == SkeletonName)
	{
		return false;
	}

	return true;
}

void FPreviewMeshCollectionEntryCustomization::HandleMeshChanged(const FAssetData& InAssetData)
{
	if (PreviewScene.IsValid())
	{
		PreviewScene.Pin()->RefreshAdditionalMeshes();
	}
}

void FPreviewMeshCollectionEntryCustomization::HandleMeshesArrayChanged(TSharedPtr<IPropertyUtilities> PropertyUtilities)
{
	if (PreviewScene.IsValid())
	{
		PreviewScene.Pin()->RefreshAdditionalMeshes();
		if (PropertyUtilities.IsValid())
		{
			PropertyUtilities->ForceRefresh();
		}
	}
}

#undef LOCTEXT_NAMESPACE
