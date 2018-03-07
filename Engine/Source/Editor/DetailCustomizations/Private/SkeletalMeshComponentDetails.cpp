// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshComponentDetails.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorStyleSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Editor.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Engine/Selection.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "SImage.h"
#include "PhysicsEngine/PhysicsSettings.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshComponentDetails"

// Filter class for animation blueprint picker
class FAnimBlueprintFilter : public IClassViewerFilter
{
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		if(InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed)
		{
			return true;
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const class IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
	}

	/** Only children of the classes in this set will be unfiltered */
	TSet<const UClass*> AllowedChildrenOfClasses;
};

FSkeletalMeshComponentDetails::FSkeletalMeshComponentDetails()
	: CurrentDetailBuilder(NULL)
	, bAnimPickerEnabled(false)
{

}

FSkeletalMeshComponentDetails::~FSkeletalMeshComponentDetails()
{
	UnregisterAllMeshPropertyChangedCallers();
}

TSharedRef<IDetailCustomization> FSkeletalMeshComponentDetails::MakeInstance()
{
	return MakeShareable(new FSkeletalMeshComponentDetails);
}

void FSkeletalMeshComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if(!CurrentDetailBuilder)
	{
		CurrentDetailBuilder = &DetailBuilder;
	}
	DetailBuilder.EditCategory("SkeletalMesh", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Materials", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Physics", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.HideProperty("bCastStaticShadow", UPrimitiveComponent::StaticClass());
	DetailBuilder.HideProperty("bLightAsIfStatic", UPrimitiveComponent::StaticClass());
	DetailBuilder.EditCategory("Animation", FText::GetEmpty(), ECategoryPriority::Important);

	PerformInitialRegistrationOfSkeletalMeshes(DetailBuilder);

	UpdateAnimationCategory(DetailBuilder);
	UpdatePhysicsCategory(DetailBuilder);
}

void FSkeletalMeshComponentDetails::UpdateAnimationCategory( IDetailLayoutBuilder& DetailBuilder )
{
	UpdateSkeletonNameAndPickerVisibility();

	IDetailCategoryBuilder& AnimationCategory = DetailBuilder.EditCategory("Animation", FText::GetEmpty(), ECategoryPriority::Important);

	// Force the mode switcher to be first
	AnimationModeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMeshComponent, AnimationMode));
	check (AnimationModeHandle->IsValidHandle());

	AnimationBlueprintHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMeshComponent, AnimClass));
	check(AnimationBlueprintHandle->IsValidHandle());

	AnimationCategory.AddProperty(AnimationModeHandle);

	// Place the blueprint property next (which may be hidden, depending on the mode)
	TAttribute<EVisibility> BlueprintVisibility( this, &FSkeletalMeshComponentDetails::VisibilityForBlueprintMode );

	DetailBuilder.HideProperty(AnimationBlueprintHandle);
	AnimationCategory.AddCustomRow(AnimationBlueprintHandle->GetPropertyDisplayName())
		.Visibility(BlueprintVisibility)
		.NameContent()
		[
			AnimationBlueprintHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(125.f)
		.MaxDesiredWidth(250.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(ClassPickerComboButton, SComboButton)
				.OnGetMenuContent(this, &FSkeletalMeshComponentDetails::GetClassPickerMenuContent)
				.ContentPadding(0)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(this, &FSkeletalMeshComponentDetails::GetSelectedAnimBlueprintName)
					.MinDesiredWidth(200.f)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 1.0f)
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FSkeletalMeshComponentDetails::OnBrowseToAnimBlueprint))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 1.0f)
			[
				PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &FSkeletalMeshComponentDetails::UseSelectedAnimBlueprint))
			]
		];

	// Hide the parent AnimationData property, and inline the children with custom visibility delegates
	const FName AnimationDataFName(GET_MEMBER_NAME_CHECKED(USkeletalMeshComponent, AnimationData));

	TSharedPtr<IPropertyHandle> AnimationDataHandle = DetailBuilder.GetProperty(AnimationDataFName);
	check(AnimationDataHandle->IsValidHandle());
	TAttribute<EVisibility> SingleAnimVisibility(this, &FSkeletalMeshComponentDetails::VisibilityForSingleAnimMode);
	DetailBuilder.HideProperty(AnimationDataFName);

	// Process Animation asset selection
	uint32 TotalChildren=0;
	AnimationDataHandle->GetNumChildren(TotalChildren);
	for (uint32 ChildIndex=0; ChildIndex < TotalChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = AnimationDataHandle->GetChildHandle(ChildIndex);
	
		if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FSingleAnimationPlayData, AnimToPlay))
		{
			// Hide the property, as we're about to add it differently
			DetailBuilder.HideProperty(ChildHandle);

			// Add it differently
			TSharedPtr<SWidget> NameWidget = ChildHandle->CreatePropertyNameWidget();

			TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
				.ThumbnailPool(DetailBuilder.GetThumbnailPool())
				.PropertyHandle(ChildHandle)
				.AllowedClass(UAnimationAsset::StaticClass())
				.AllowClear(true)
				.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FSkeletalMeshComponentDetails::OnShouldFilterAnimAsset));

			TAttribute<bool> AnimPickerEnabledAttr(this, &FSkeletalMeshComponentDetails::AnimPickerIsEnabled);

			AnimationCategory.AddCustomRow(ChildHandle->GetPropertyDisplayName())
				.Visibility(SingleAnimVisibility)
				.IsEnabled(AnimPickerEnabledAttr)
				.NameContent()
				[
					NameWidget.ToSharedRef()
				]
				.ValueContent()
				.MinDesiredWidth(600)
				.MaxDesiredWidth(600)
				[
					PropWidget
				];
		}
		else
		{
			AnimationCategory.AddProperty(ChildHandle).Visibility(SingleAnimVisibility);
		}
	}
}

void FSkeletalMeshComponentDetails::UpdatePhysicsCategory(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& PhysicsCategory = DetailBuilder.EditCategory("Physics", FText::GetEmpty(), ECategoryPriority::TypeSpecific);

	const FName AsyncSceneFName(GET_MEMBER_NAME_CHECKED(USkeletalMeshComponent, UseAsyncScene));
	AsyncSceneHandle = DetailBuilder.GetProperty(AsyncSceneFName);
	check(AsyncSceneHandle->IsValidHandle());

	TAttribute<EVisibility> AsyncSceneWarningVisibilityAttribute(this, &FSkeletalMeshComponentDetails::VisibilityForAsyncSceneWarning);
	TAttribute<bool> AsyncSceneDropdownEnabledState(this, &FSkeletalMeshComponentDetails::ShouldAllowAsyncSceneSettingToBeChanged);

	DetailBuilder.HideProperty(AsyncSceneHandle);
	PhysicsCategory.AddCustomRow(AsyncSceneHandle->GetPropertyDisplayName(), true)
	.Visibility(EVisibility::Visible)
	.NameContent()
	[
		AsyncSceneHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.IsEnabled(AsyncSceneDropdownEnabledState)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				AsyncSceneHandle->CreatePropertyValueWidget()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0,4))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor::Yellow)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(2)
				.HAlign(HAlign_Fill)
				.Visibility(AsyncSceneWarningVisibilityAttribute)
				.Clipping(EWidgetClipping::ClipToBounds)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("Icons.Warning"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(2,0))
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(LOCTEXT("WarningForProjectAsyncSceneNotEnabled", "The project setting \"Enable Async Scene\" must be set."))
						.AutoWrapText(true)
						.ToolTipText(LOCTEXT("WarningForProjectAsyncSceneNotEnabledTooltip",
							"The project setting \"Enable Async Scene\" must be set in order to use an async scene. "
							"Otherwise, this property will be ignored."))
					]
				]
			]
		]
	];
}

EVisibility FSkeletalMeshComponentDetails::VisibilityForAnimationMode(EAnimationMode::Type AnimationMode) const
{
	uint8 AnimationModeValue=0;
	FPropertyAccess::Result Ret = AnimationModeHandle.Get()->GetValue(AnimationModeValue);
	if (Ret == FPropertyAccess::Result::Success)
	{
		return (AnimationModeValue == AnimationMode) ? EVisibility::Visible : EVisibility::Hidden;
	}

	return EVisibility::Hidden; //Hidden if we get fail or MultipleValues from the property
}

bool FSkeletalMeshComponentDetails::OnShouldFilterAnimAsset( const FAssetData& AssetData )
{
	const FString SkeletonName = AssetData.GetTagValueRef<FString>("Skeleton");
	return SkeletonName != SelectedSkeletonName;
}

void FSkeletalMeshComponentDetails::SkeletalMeshPropertyChanged()
{
	UpdateSkeletonNameAndPickerVisibility();
}

void FSkeletalMeshComponentDetails::UpdateSkeletonNameAndPickerVisibility()
{
	// Update the selected skeleton name and the picker visibility
	USkeleton* Skeleton = GetValidSkeletonFromRegisteredMeshes();

	if (Skeleton)
	{
		bAnimPickerEnabled = true;
		SelectedSkeletonName = FString::Printf(TEXT("%s'%s'"), *Skeleton->GetClass()->GetName(), *Skeleton->GetPathName());
	}
	else
	{
		bAnimPickerEnabled = false;
		SelectedSkeletonName = "";
	}
}

void FSkeletalMeshComponentDetails::RegisterSkeletalMeshPropertyChanged(TWeakObjectPtr<USkeletalMeshComponent> Mesh)
{
	if(Mesh.IsValid() && OnSkeletalMeshPropertyChanged.IsBound())
	{
		OnSkeletalMeshPropertyChangedDelegateHandles.Add(Mesh.Get(), Mesh->RegisterOnSkeletalMeshPropertyChanged(OnSkeletalMeshPropertyChanged));
	}
}

void FSkeletalMeshComponentDetails::UnregisterSkeletalMeshPropertyChanged(TWeakObjectPtr<USkeletalMeshComponent> Mesh)
{
	if(Mesh.IsValid())
	{
		Mesh->UnregisterOnSkeletalMeshPropertyChanged(OnSkeletalMeshPropertyChangedDelegateHandles.FindRef(Mesh.Get()));
		OnSkeletalMeshPropertyChangedDelegateHandles.Remove(Mesh.Get());
	}
}

void FSkeletalMeshComponentDetails::UnregisterAllMeshPropertyChangedCallers()
{
	for(auto MeshIter = SelectedObjects.CreateIterator() ; MeshIter ; ++MeshIter)
	{
		if(USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(MeshIter->Get()))
		{
			Mesh->UnregisterOnSkeletalMeshPropertyChanged(OnSkeletalMeshPropertyChangedDelegateHandles.FindRef(Mesh));
			OnSkeletalMeshPropertyChangedDelegateHandles.Remove(Mesh);
		}
	}
}

bool FSkeletalMeshComponentDetails::AnimPickerIsEnabled() const
{
	return bAnimPickerEnabled;
}

TSharedRef<SWidget> FSkeletalMeshComponentDetails::GetClassPickerMenuContent()
{
	TSharedPtr<FAnimBlueprintFilter> Filter = MakeShareable(new FAnimBlueprintFilter);
	Filter->AllowedChildrenOfClasses.Add(UAnimInstance::StaticClass());

	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	FClassViewerInitializationOptions InitOptions;
	InitOptions.Mode = EClassViewerMode::ClassPicker;
	InitOptions.ClassFilter = Filter;
	InitOptions.bShowNoneOption = true;

	return SNew(SBorder)
		.Padding(3)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		.ForegroundColor(FEditorStyle::GetColor("DefaultForeground"))
		[
			SNew(SBox)
			.WidthOverride(280)
			[
				ClassViewerModule.CreateClassViewer(InitOptions, FOnClassPicked::CreateSP(this, &FSkeletalMeshComponentDetails::OnClassPicked))
			]
		];
}

FText FSkeletalMeshComponentDetails::GetSelectedAnimBlueprintName() const
{
	check(AnimationBlueprintHandle->IsValidHandle());

	UObject* Object = NULL;
	AnimationBlueprintHandle->GetValue(Object);
	if(Object)
	{
		return FText::FromString(Object->GetName());
	}
	else
	{
		return LOCTEXT("None", "None");
	}
}

void FSkeletalMeshComponentDetails::OnClassPicked( UClass* PickedClass )
{
	check(AnimationBlueprintHandle->IsValidHandle());

	ClassPickerComboButton->SetIsOpen(false);

	AnimationBlueprintHandle->SetValue(PickedClass);
}

void FSkeletalMeshComponentDetails::OnBrowseToAnimBlueprint()
{
	check(AnimationBlueprintHandle->IsValidHandle());

	UObject* Object = NULL;
	AnimationBlueprintHandle->GetValue(Object);

	TArray<UObject*> Objects;
	Objects.Add(Object);
	GEditor->SyncBrowserToObjects(Objects);
}

void FSkeletalMeshComponentDetails::UseSelectedAnimBlueprint()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	USelection* AssetSelection = GEditor->GetSelectedObjects();
	if (AssetSelection && AssetSelection->Num() == 1)
	{
		UAnimBlueprint* AnimBlueprintToAssign = AssetSelection->GetTop<UAnimBlueprint>();
		if (AnimBlueprintToAssign)
		{
			if(USkeleton* AnimBlueprintSkeleton = AnimBlueprintToAssign->TargetSkeleton)
			{
				FString BlueprintSkeletonName = FString::Printf(TEXT("%s'%s'"), *AnimBlueprintSkeleton->GetClass()->GetName(), *AnimBlueprintSkeleton->GetPathName());
				if (BlueprintSkeletonName == SelectedSkeletonName)
				{
					OnClassPicked(AnimBlueprintToAssign->GetAnimBlueprintGeneratedClass());
				}
			}
		}
	}
}

EVisibility FSkeletalMeshComponentDetails::VisibilityForAsyncSceneWarning() const
{
	return UPhysicsSettings::Get()->bEnableAsyncScene ? EVisibility::Collapsed : EVisibility::Visible;
}

bool FSkeletalMeshComponentDetails::ShouldAllowAsyncSceneSettingToBeChanged() const
{
	return UPhysicsSettings::Get()->bEnableAsyncScene;
}

void FSkeletalMeshComponentDetails::PerformInitialRegistrationOfSkeletalMeshes(IDetailLayoutBuilder& DetailBuilder)
{
	OnSkeletalMeshPropertyChanged = USkeletalMeshComponent::FOnSkeletalMeshPropertyChanged::CreateSP(this, &FSkeletalMeshComponentDetails::SkeletalMeshPropertyChanged);

	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);

	check(SelectedObjects.Num() > 0);

	for (auto ObjectIter = SelectedObjects.CreateIterator(); ObjectIter; ++ObjectIter)
	{
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(ObjectIter->Get()))
		{
			RegisterSkeletalMeshPropertyChanged(Mesh);
		}
	}
}

USkeleton* FSkeletalMeshComponentDetails::GetValidSkeletonFromRegisteredMeshes() const
{
	USkeleton* Skeleton = NULL;

	for (auto ObjectIter = SelectedObjects.CreateConstIterator(); ObjectIter; ++ObjectIter)
	{
		USkeletalMeshComponent* const Mesh = Cast<USkeletalMeshComponent>(ObjectIter->Get());
		if ( !Mesh || !Mesh->SkeletalMesh )
		{
			continue;
		}

		// If we've not come across a valid skeleton yet, store this one.
		if (!Skeleton)
		{
			Skeleton = Mesh->SkeletalMesh->Skeleton;
			continue;
		}

		// We've encountered a valid skeleton before.
		// If this skeleton is not the same one, that means there are multiple
		// skeletons selected, so we don't want to take any action.
		if (Mesh->SkeletalMesh->Skeleton != Skeleton)
		{
			return NULL;
		}
	}

	return Skeleton;
}

#undef LOCTEXT_NAMESPACE
