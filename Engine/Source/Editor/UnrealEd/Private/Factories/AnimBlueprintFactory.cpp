// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimBlueprintFactory.cpp: Factory for Anim Blueprints
=============================================================================*/

#include "Factories/AnimBlueprintFactory.h"
#include "InputCoreTypes.h"
#include "UObject/Interface.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstance.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "AssetData.h"
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"


#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"

#define LOCTEXT_NAMESPACE "AnimBlueprintFactory"

static bool CanCreateAnimBlueprint(const FAssetData& Skeleton, UClass const * ParentClass)
{
	if (Skeleton.IsValid() && ParentClass != NULL)
	{
		if (UAnimBlueprintGeneratedClass const * GeneratedParent = Cast<const UAnimBlueprintGeneratedClass>(ParentClass))
		{
			if (Skeleton.GetExportTextName() != FAssetData(GeneratedParent->GetTargetSkeleton()).GetExportTextName())
			{
				return false;
			}
		}
	}
	return true;
}

/*------------------------------------------------------------------------------
	Dialog to configure creation properties
------------------------------------------------------------------------------*/
class SAnimBlueprintCreateDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimBlueprintCreateDialog ){}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		bOkClicked = false;
		ParentClass = UAnimInstance::StaticClass();

		ChildSlot
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
			[
				SNew(SBox)
				.Visibility(EVisibility::Visible)
				.WidthOverride(500.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1)
					[
						SNew(SBorder)
						.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
						.Content()
						[
							SAssignNew(ParentClassContainer, SVerticalBox)
						]
					]
					+SVerticalBox::Slot()
					.FillHeight(1)
					.Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(SBorder)
						.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
						.Content()
						[
							SAssignNew(SkeletonContainer, SVerticalBox)
						]
					]

					// Ok/Cancel buttons
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(8)
					[
						SNew(SUniformGridPanel)
						.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
						.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
						.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
						+SUniformGridPanel::Slot(0,0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding( FEditorStyle::GetMargin("StandardDialog.ContentPadding") )
							.OnClicked(this, &SAnimBlueprintCreateDialog::OkClicked)	
							.Text(LOCTEXT("CreateAnimBlueprintOk", "OK"))
						]
						+SUniformGridPanel::Slot(1,0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding( FEditorStyle::GetMargin("StandardDialog.ContentPadding") )
							.OnClicked(this, &SAnimBlueprintCreateDialog::CancelClicked)
							.Text(LOCTEXT("CreateAnimBlueprintCancel", "Cancel"))
						]
					]
				]
			]
		];

		MakeParentClassPicker();
		MakeSkeletonPicker();
	}
	
	/** Sets properties for the supplied AnimBlueprintFactory */
	bool ConfigureProperties(TWeakObjectPtr<UAnimBlueprintFactory> InAnimBlueprintFactory)
	{
		AnimBlueprintFactory = InAnimBlueprintFactory;

		TSharedRef<SWindow> Window = SNew(SWindow)
		.Title( LOCTEXT("CreateAnimBlueprintOptions", "Create Animation Blueprint") )
		.ClientSize(FVector2D(400, 700))
		.SupportsMinimize(false) .SupportsMaximize(false)
		[
			AsShared()
		];

		PickerWindow = Window;

		GEditor->EditorAddModalWindow(Window);
		AnimBlueprintFactory.Reset();

		return bOkClicked;
	}

private:
	class FAnimBlueprintParentFilter : public IClassViewerFilter
	{
	public:
		/** All children of these classes will be included unless filtered out by another setting. */
		TSet< const UClass* > AllowedChildrenOfClasses;
		const FAssetData& ShouldBeCompatibleWithSkeleton;

		FAnimBlueprintParentFilter(const FAssetData& Skeleton) : ShouldBeCompatibleWithSkeleton(Skeleton) {}

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			if (InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed)
			{
				return CanCreateAnimBlueprint(ShouldBeCompatibleWithSkeleton, InClass);
			}
			return false;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			return InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
		}
	};

	/** Creates the combo menu for the parent class */
	void MakeParentClassPicker()
	{
		// Load the classviewer module to display a class picker
		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

		// Fill in options
		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;
		
		// Only allow parenting to base blueprints.
		Options.bIsBlueprintBaseOnly = true;

		TSharedPtr<FAnimBlueprintParentFilter> Filter = MakeShareable(new FAnimBlueprintParentFilter(TargetSkeleton));
		Options.ClassFilter = Filter;

		// All child child classes of UAnimInstance are valid.
		Filter->AllowedChildrenOfClasses.Add(UAnimInstance::StaticClass());

		ParentClassContainer->ClearChildren();
		ParentClassContainer->AddSlot()
		.AutoHeight()
		[
			SNew( STextBlock )
			.Text( LOCTEXT("ParentClass", "Parent Class:") )
			.ShadowOffset( FVector2D(1.0f, 1.0f) )
		];

		ParentClassContainer->AddSlot()
		[
			ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SAnimBlueprintCreateDialog::OnClassPicked))
		];
	}

	/** Handler for when a parent class is selected */
	void OnClassPicked(UClass* ChosenClass)
	{
		ParentClass = ChosenClass;
		MakeSkeletonPicker();
	}

	/** Creates the combo menu for the target skeleton */
	void MakeSkeletonPicker()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAnimBlueprintCreateDialog::OnSkeletonSelected);
		AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SAnimBlueprintCreateDialog::FilterSkeletonBasedOnParentClass);
		AssetPickerConfig.bAllowNullSelection = true;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
		AssetPickerConfig.InitialAssetSelection = TargetSkeleton;

		SkeletonContainer->ClearChildren();
		SkeletonContainer->AddSlot()
		.AutoHeight()
		[
			SNew( STextBlock )
			.Text( LOCTEXT("TargetSkeleton", "Target Skeleton:") )
			.ShadowOffset( FVector2D(1.0f, 1.0f) )
		];

		SkeletonContainer->AddSlot()
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
	}

	bool FilterSkeletonBasedOnParentClass(const FAssetData& AssetData)
	{
		return ! CanCreateAnimBlueprint(AssetData, ParentClass.Get());
	}

	/** Handler for when a skeleton is selected */
	void OnSkeletonSelected(const FAssetData& AssetData)
	{
		TargetSkeleton = AssetData;
	}

	/** Handler for when ok is clicked */
	FReply OkClicked()
	{
		if ( AnimBlueprintFactory.IsValid() )
		{
			AnimBlueprintFactory->BlueprintType = BPTYPE_Normal;
			AnimBlueprintFactory->ParentClass = ParentClass.Get();
			AnimBlueprintFactory->TargetSkeleton = Cast<USkeleton>(TargetSkeleton.GetAsset());
		}

		if ( !TargetSkeleton.IsValid() )
		{
			// if TargetSkeleton is not valid
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("NeedValidSkeleton", "Must specify a valid skeleton for the Anim Blueprint to target."));			
			return FReply::Handled();
		}

		if (! CanCreateAnimBlueprint(TargetSkeleton, ParentClass.Get()))
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("NeedCompatibleSkeleton", "Selected skeleton has to be compatible with selected parent class."));
			return FReply::Handled();
		}
		CloseDialog(true);

		return FReply::Handled();
	}

	void CloseDialog(bool bWasPicked=false)
	{
		bOkClicked = bWasPicked;
		if ( PickerWindow.IsValid() )
		{
			PickerWindow.Pin()->RequestDestroyWindow();
		}
	}

	/** Handler for when cancel is clicked */
	FReply CancelClicked()
	{
		CloseDialog();
		return FReply::Handled();
	}

	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			CloseDialog();
			return FReply::Handled();
		}
		return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	/** The factory for which we are setting up properties */
	TWeakObjectPtr<UAnimBlueprintFactory> AnimBlueprintFactory;

	/** A pointer to the window that is asking the user to select a parent class */
	TWeakPtr<SWindow> PickerWindow;

	/** The container for the Parent Class picker */
	TSharedPtr<SVerticalBox> ParentClassContainer;

	/** The container for the target skeleton picker*/
	TSharedPtr<SVerticalBox> SkeletonContainer;

	/** The selected class */
	TWeakObjectPtr<UClass> ParentClass;

	/** The selected skeleton */
	FAssetData TargetSkeleton;

	/** True if Ok was clicked */
	bool bOkClicked;
};


/*------------------------------------------------------------------------------
	UAnimBlueprintFactory implementation.
------------------------------------------------------------------------------*/

UAnimBlueprintFactory::UAnimBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAnimBlueprint::StaticClass();
	ParentClass = UAnimInstance::StaticClass();
}

bool UAnimBlueprintFactory::ConfigureProperties()
{
	TSharedRef<SAnimBlueprintCreateDialog> Dialog = SNew(SAnimBlueprintCreateDialog);
	return Dialog->ConfigureProperties(this);
};

UObject* UAnimBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a Anim Blueprint, then create and init one
	check(Class->IsChildOf(UAnimBlueprint::StaticClass()));

	// If they selected an interface, force the parent class to be UInterface
	if (BlueprintType == BPTYPE_Interface)
	{
		ParentClass = UInterface::StaticClass();
	}

	if (TargetSkeleton == NULL)
	{
		FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("NeedValidSkeleton", "Must specify a valid skeleton for the Anim Blueprint to target."));
		return NULL;
	}

	if ((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(UAnimInstance::StaticClass()))
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateAnimBlueprint", "Cannot create an Anim Blueprint based on the class '{0}'."), Args ) );
		return NULL;
	}
	else
	{
		UAnimBlueprint* NewBP = CastChecked<UAnimBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UAnimBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));
		
		// Inherit any existing overrides in parent class
		if (NewBP->ParentAssetOverrides.Num() > 0)
		{
			// We've inherited some overrides from the parent graph and need to recompile the blueprint.
			FKismetEditorUtilities::CompileBlueprint(NewBP);
		}
		
		NewBP->TargetSkeleton = TargetSkeleton;

		// Because the BP itself didn't have the skeleton set when the initial compile occured, it's not set on the generated classes either
		if (UAnimBlueprintGeneratedClass* TypedNewClass = Cast<UAnimBlueprintGeneratedClass>(NewBP->GeneratedClass))
		{
			TypedNewClass->TargetSkeleton = TargetSkeleton;
		}
		if (UAnimBlueprintGeneratedClass* TypedNewClass_SKEL = Cast<UAnimBlueprintGeneratedClass>(NewBP->SkeletonGeneratedClass))
		{
			TypedNewClass_SKEL->TargetSkeleton = TargetSkeleton;
		}

		if (PreviewSkeletalMesh)
		{
			NewBP->SetPreviewMesh(PreviewSkeletalMesh);
		}

		return NewBP;
	}
}

UObject* UAnimBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

#undef LOCTEXT_NAMESPACE
