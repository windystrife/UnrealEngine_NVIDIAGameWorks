// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "MovieScene.h"
#include "Animation/WidgetAnimation.h"
#include "Serialization/TextReferenceCollector.h"
#include "Engine/UserInterfaceSettings.h"
#include "UMGPrivate.h"
#include "Interfaces/ITargetPlatform.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/ObjectResource.h"
#include "UObject/LinkerLoad.h"
#include "Engine/StreamableManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

#define LOCTEXT_NAMESPACE "UMG"

#if WITH_EDITOR

int32 TemplatePreviewInEditor = 0;
static FAutoConsoleVariableRef CVarTemplatePreviewInEditor(TEXT("Widget.TemplatePreviewInEditor"), TemplatePreviewInEditor, TEXT("Should a dynamic template be generated at runtime for the editor for widgets?  Useful for debugging templates."), ECVF_Default);

#endif

#if WITH_EDITORONLY_DATA
namespace
{
	void CollectWidgetBlueprintGeneratedClassTextReferences(UObject* Object, FArchive& Ar)
	{
		// In an editor build, both UWidgetBlueprint and UWidgetBlueprintGeneratedClass reference an identical WidgetTree.
		// So we ignore the UWidgetBlueprintGeneratedClass when looking for persistent text references since it will be overwritten by the UWidgetBlueprint version.
	}
}
#endif

/////////////////////////////////////////////////////
// UWidgetBlueprintGeneratedClass

UWidgetBlueprintGeneratedClass::UWidgetBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bTemplateInitialized(false)
{
#if WITH_EDITORONLY_DATA
	{ static const FAutoRegisterTextReferenceCollectorCallback AutomaticRegistrationOfTextReferenceCollector(UWidgetBlueprintGeneratedClass::StaticClass(), &CollectWidgetBlueprintGeneratedClassTextReferences); }
#endif
}

void UWidgetBlueprintGeneratedClass::InitializeBindingsStatic(UUserWidget* UserWidget, const TArray< FDelegateRuntimeBinding >& InBindings)
{
	ensure(!UserWidget->HasAnyFlags(RF_ArchetypeObject));

	// For each property binding that we're given, find the corresponding field, and setup the delegate binding on the widget.
	for ( const FDelegateRuntimeBinding& Binding : InBindings )
	{
		UObjectProperty* WidgetProperty = FindField<UObjectProperty>(UserWidget->GetClass(), *Binding.ObjectName);
		if ( WidgetProperty == nullptr )
		{
			continue;
		}

		UWidget* Widget = Cast<UWidget>(WidgetProperty->GetObjectPropertyValue_InContainer(UserWidget));

		if ( Widget )
		{
			UDelegateProperty* DelegateProperty = FindField<UDelegateProperty>(Widget->GetClass(), FName(*( Binding.PropertyName.ToString() + TEXT("Delegate") )));
			if ( !DelegateProperty )
			{
				DelegateProperty = FindField<UDelegateProperty>(Widget->GetClass(), Binding.PropertyName);
			}

			if ( DelegateProperty )
			{
				bool bSourcePathBound = false;

				if ( Binding.SourcePath.IsValid() )
				{
					bSourcePathBound = Widget->AddBinding(DelegateProperty, UserWidget, Binding.SourcePath);
				}

				// If no native binder is found then the only possibility is that the binding is for
				// a delegate that doesn't match the known native binders available and so we
				// fallback to just attempting to bind to the function directly.
				if ( bSourcePathBound == false )
				{
					FScriptDelegate* ScriptDelegate = DelegateProperty->GetPropertyValuePtr_InContainer(Widget);
					if ( ScriptDelegate )
					{
						ScriptDelegate->BindUFunction(UserWidget, Binding.FunctionName);
					}
				}
			}
		}
	}
}

void UWidgetBlueprintGeneratedClass::InitializeWidgetStatic(UUserWidget* UserWidget
	, const UClass* InClass
	, bool InCanTemplate
	, UWidgetTree* InWidgetTree
	, const TArray< UWidgetAnimation* >& InAnimations
	, const TArray< FDelegateRuntimeBinding >& InBindings)
{
	check(InClass);

	if ( UserWidget->HasAllFlags(RF_ArchetypeObject) )
	{
		UE_LOG(LogUMG, Error, TEXT("Widget Class %s - Running Initialize On Archetype, %s."), *InClass->GetName(), *UserWidget->GetName());
		return;
	}

	UWidgetTree* ClonedTree = UserWidget->WidgetTree;

	if ( UserWidget->bCookedWidgetTree )
	{
#if WITH_EDITOR

		// TODO This can get called at editor time when PostLoad runs and we attempt to initialize the tree.
		// Perhaps we shouldn't call init in post load if it's a cooked tree?

		//UE_LOG(LogUMG, Fatal, TEXT("Initializing a cooked widget tree at editor time! %s."), *InClass->GetName());

#else
		// If we can be templated, we need to go ahead and initialize all the user widgets under us, since we're
		// an already expanded tree.
		check(InCanTemplate);
		check(ClonedTree);

		// TODO NDarnell This initialization can be made faster if part of storing the template data is some kind of
		// acceleration structure that could be the all the userwidgets we need to initialize bindings for...etc.

		// If there's an existing widget tree, then we need to initialize all userwidgets in the tree.
		ClonedTree->ForEachWidget([&] (UWidget* Widget) {
			check(Widget);

			if ( UUserWidget* SubUserWidget = Cast<UUserWidget>(Widget) )
			{
				SubUserWidget->Initialize();
			}
		});

		InitializeBindingsStatic(UserWidget, InBindings);

		UBlueprintGeneratedClass::BindDynamicDelegates(InClass, UserWidget);

#endif
		// We don't need any more initialization for template widgets.
		return;
	}
	else
	{
		// Normally the ClonedTree should be null - we do in the case of design time with the widget, actually
		// clone the widget tree directly from the WidgetBlueprint so that the rebuilt preview matches the newest
		// widget tree, without a full blueprint compile being required.  In that case, the WidgetTree on the UserWidget
		// will have already been initialized to some value.  When that's the case, we'll avoid duplicating it from the class
		// similar to how we use to use the DesignerWidgetTree.
		if ( ClonedTree == nullptr )
		{
			UserWidget->DuplicateAndInitializeFromWidgetTree(InWidgetTree);
			ClonedTree = UserWidget->WidgetTree;
		}
	}

#if !WITH_EDITOR && UE_BUILD_DEBUG
	UE_LOG(LogUMG, Warning, TEXT("Widget Class %s - Slow Static Duplicate Object."), *InClass->GetName());
#endif

	UserWidget->WidgetGeneratedByClass = InClass;

#if WITH_EDITOR
	UserWidget->WidgetGeneratedBy = InClass->ClassGeneratedBy;
#endif

	if (ClonedTree)
	{
		UClass* WidgetBlueprintClass = UserWidget->GetClass();

		for (UWidgetAnimation* Animation : InAnimations)
		{
			UWidgetAnimation* Anim = DuplicateObject<UWidgetAnimation>(Animation, UserWidget);

			if ( Anim->GetMovieScene() )
			{
				// Find property with the same name as the template and assign the new widget to it.
				UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(WidgetBlueprintClass, Anim->GetMovieScene()->GetFName());
				if ( Prop )
				{
					Prop->SetObjectPropertyValue_InContainer(UserWidget, Anim);
				}
			}
		}

		ClonedTree->ForEachWidget([&](UWidget* Widget) {
			// Not fatal if NULL, but shouldn't happen
			if (!ensure(Widget != nullptr))
			{
				return;
			}

			Widget->WidgetGeneratedByClass = InClass;

#if WITH_EDITOR
			Widget->WidgetGeneratedBy = InClass->ClassGeneratedBy;
#endif

			// TODO UMG Make this an FName
			FString VariableName = Widget->GetName();

			// Find property with the same name as the template and assign the new widget to it.
			UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(WidgetBlueprintClass, *VariableName);
			if (Prop)
			{
				Prop->SetObjectPropertyValue_InContainer(UserWidget, Widget);
				UObject* Value = Prop->GetObjectPropertyValue_InContainer(UserWidget);
				check(Value == Widget);
			}

			// Initialize Navigation Data
			if (Widget->Navigation)
			{
				Widget->Navigation->ResolveExplictRules(ClonedTree);
			}

#if WITH_EDITOR
			Widget->ConnectEditorData();
#endif
		});

		InitializeBindingsStatic(UserWidget, InBindings);

		// Bind any delegates on widgets
		UBlueprintGeneratedClass::BindDynamicDelegates(InClass, UserWidget);

		//TODO UMG Add OnWidgetInitialized?
	}
}

void UWidgetBlueprintGeneratedClass::InitializeWidget(UUserWidget* UserWidget) const
{
	InitializeWidgetStatic(UserWidget, this, HasTemplate(), WidgetTree, Animations, Bindings);
}

UObject* UWidgetBlueprintGeneratedClass::CreateDefaultObject()
{
#if WITH_EDITOR
	return Super::CreateDefaultObject();
#else
	return Super::CreateDefaultObject();
#endif
}

void UWidgetBlueprintGeneratedClass::PostLoad()
{
	Super::PostLoad();

	// Clear CDO flag on tree
	if (WidgetTree)
	{
		WidgetTree->ClearFlags(RF_DefaultSubObject);
	}

	if ( GetLinkerUE4Version() < VER_UE4_RENAME_WIDGET_VISIBILITY )
	{
		static const FName Visiblity(TEXT("Visiblity"));
		static const FName Visibility(TEXT("Visibility"));

		for ( FDelegateRuntimeBinding& Binding : Bindings )
		{
			if ( Binding.PropertyName == Visiblity )
			{
				Binding.PropertyName = Visibility;
			}
		}
	}
}

void UWidgetBlueprintGeneratedClass::PurgeClass(bool bRecompilingOnLoad)
{
	Super::PurgeClass(bRecompilingOnLoad);

	const ERenameFlags RenFlags = REN_DontCreateRedirectors | ( ( bRecompilingOnLoad ) ? REN_ForceNoResetLoaders : 0 ) | REN_NonTransactional | REN_DoNotDirty;

	// Remove the old widdget tree.
	if ( WidgetTree )
	{
		WidgetTree->Rename(nullptr, GetTransientPackage(), RenFlags);
		FLinkerLoad::InvalidateExport(WidgetTree);
		WidgetTree = nullptr;
	}

	// Remove all animations.
	for ( UWidgetAnimation* Animation : Animations )
	{
		Animation->Rename(nullptr, GetTransientPackage(), RenFlags);
		FLinkerLoad::InvalidateExport(Animation);
	}
	Animations.Empty();

	bValidTemplate = false;

	Template = nullptr;
	TemplateAsset.Reset();

#if WITH_EDITOR
	EditorTemplate = nullptr;
#endif

	Bindings.Empty();
}

bool UWidgetBlueprintGeneratedClass::NeedsLoadForServer() const
{
	const UUserInterfaceSettings* UISettings = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
	check(UISettings);
	return UISettings->bLoadWidgetsOnDedicatedServer;
}

bool UWidgetBlueprintGeneratedClass::HasTemplate() const
{
	return bValidTemplate;
}

void UWidgetBlueprintGeneratedClass::SetTemplate(UUserWidget* InTemplate)
{
	Template = InTemplate;
	TemplateAsset = InTemplate;

	bValidTemplate = TemplateAsset.IsNull() ? false : true;
}

UUserWidget* UWidgetBlueprintGeneratedClass::GetTemplate()
{
#if WITH_EDITOR

	if ( TemplatePreviewInEditor )
	{

		if ( EditorTemplate == nullptr && HasTemplate() )
		{
			EditorTemplate = NewObject<UUserWidget>(this, this, NAME_None, EObjectFlags(RF_ArchetypeObject | RF_Transient));
			EditorTemplate->TemplateInit();

#if UE_BUILD_DEBUG
			TArray<FText> OutErrors;
			if ( EditorTemplate->VerifyTemplateIntegrity(OutErrors) == false )
			{
				UE_LOG(LogUMG, Error, TEXT("Widget Class %s - Template Failed Verification"), *GetName());
			}
#endif
		}

		return EditorTemplate;
	}
	else
	{
		return nullptr;
	}

#else

	if ( bTemplateInitialized == false && HasTemplate() )
	{
		// This shouldn't be possible with the EDL loader, so only attempt to do it then.
		if (GEventDrivenLoaderEnabled == false && Template == nullptr)
		{
			Template = TemplateAsset.LoadSynchronous();
		}

		// If you hit this ensure, it's possible there's a problem with the loader, or the cooker and the template
		// widget did not end up in the cooked package.
		if ( ensureMsgf(Template, TEXT("No Template Found!  Could not load a Widget Archetype for %s."), *GetName()) )
		{
			bTemplateInitialized = true;

			// This should only ever happen if the EDL is disabled, where you're not guaranteed every object in the package
			// has been loaded at this point.
			if (GEventDrivenLoaderEnabled == false)
			{
				if (Template->HasAllFlags(RF_NeedLoad))
				{
					if (FLinkerLoad* Linker = Template->GetLinker())
					{
						Linker->Preload(Template);
					}
				}
			}

#if !UE_BUILD_SHIPPING
			UE_LOG(LogUMG, Display, TEXT("Widget Class %s - Loaded Fast Template."), *GetName());
#endif

#if UE_BUILD_DEBUG
			TArray<FText> OutErrors;
			if ( Template->VerifyTemplateIntegrity(OutErrors) == false )
			{
				UE_LOG(LogUMG, Error, TEXT("Widget Class %s - Template Failed Verification"), *GetName());
			}
#endif
		}
		else
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogUMG, Error, TEXT("Widget Class %s - Failed To Load Template."), *GetName());
#endif
		}
	}

#endif

	return Template;
}

void UWidgetBlueprintGeneratedClass::PreSave(const class ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR
	if ( TargetPlatform && TargetPlatform->RequiresCookedData() )
	{
		if ( WidgetTree )
		{
			if (bCookSlowConstructionWidgetTree)
			{
				WidgetTree->ClearFlags(RF_Transient);
			}
			else
			{
				WidgetTree->SetFlags(RF_Transient);
			}
		}

		InitializeTemplate(TargetPlatform);
	}
	else
	{
		// If we're saving the generated class in the editor, should we allow it to preserve a shadow copy of the one in the
		// blueprint?  Seems dangerous to have this potentially stale copy around, when really it should be the latest version
		// that's compiled on load.
		if ( WidgetTree )
		{
			WidgetTree->SetFlags(RF_Transient);
		}
	}
#endif

	Super::PreSave(TargetPlatform);
}

void UWidgetBlueprintGeneratedClass::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
}

void UWidgetBlueprintGeneratedClass::InitializeTemplate(const ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR

	if ( TargetPlatform && TargetPlatform->RequiresCookedData() )
	{
		bool bCanTemplate = bAllowTemplate;

		if ( bCanTemplate )
		{
			UUserWidget* WidgetTemplate = NewObject<UUserWidget>(GetTransientPackage(), this);
			WidgetTemplate->TemplateInit();

			// Determine if we can generate a template for this widget to speed up CreateWidget time.
			TArray<FText> OutErrors;
			bCanTemplate = WidgetTemplate->VerifyTemplateIntegrity(OutErrors);
			for ( FText Error : OutErrors )
			{
				UE_LOG(LogUMG, Warning, TEXT("Widget Class %s Template Error - %s."), *GetName(), *Error.ToString());
			}
		}

		UPackage* WidgetTemplatePackage = GetOutermost();

		// Remove the old archetype.
		{
			UUserWidget* OldArchetype = FindObject<UUserWidget>(WidgetTemplatePackage, TEXT("WidgetArchetype"));
			if ( OldArchetype )
			{
				const ERenameFlags RenFlags = REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty | REN_ForceNoResetLoaders;

				FString TransientArchetypeString = FString::Printf(TEXT("OLD_TEMPLATE_%s"), *OldArchetype->GetName());
				FName TransientArchetypeName = MakeUniqueObjectName(GetTransientPackage(), OldArchetype->GetClass(), FName(*TransientArchetypeString));
				OldArchetype->Rename(*TransientArchetypeName.ToString(), GetTransientPackage(), RenFlags);
				OldArchetype->SetFlags(RF_Transient);
				OldArchetype->ClearFlags(RF_Public | RF_Standalone | RF_ArchetypeObject);
			}
		}

		if ( bCanTemplate )
		{
			UUserWidget* WidgetTemplate = NewObject<UUserWidget>(WidgetTemplatePackage, this, TEXT("WidgetArchetype"), EObjectFlags(RF_Public | RF_Standalone | RF_ArchetypeObject));
			WidgetTemplate->TemplateInit();

			this->SetTemplate(WidgetTemplate);

			UE_LOG(LogUMG, Display, TEXT("Widget Class %s - Template Initialized."), *GetName());
		}
		else if ( bAllowTemplate == false )
		{
			UE_LOG(LogUMG, Display, TEXT("Widget Class %s - Not Allowed To Create Template"), *GetName());

			this->SetTemplate(nullptr);
		}
		else
		{
			UE_LOG(LogUMG, Warning, TEXT("Widget Class %s - Failed To Create Template"), *GetName());

			this->SetTemplate(nullptr);
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE
