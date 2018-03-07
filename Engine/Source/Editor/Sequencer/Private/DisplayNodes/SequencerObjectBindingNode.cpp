// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "Containers/ArrayBuilder.h"
#include "KeyParams.h"
#include "KeyPropertyParams.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneSection.h"
#include "ISequencerModule.h"
#include "SequencerCommands.h"
#include "MovieScene.h"
#include "Sequencer.h"
#include "SSequencer.h"
#include "SSequencerLabelEditor.h"
#include "MovieSceneSequence.h"
#include "SequencerTrackNode.h"
#include "ObjectEditorUtils.h"
#include "SequencerUtilities.h"
#include "Styling/SlateIconFinder.h"
#include "ScopedTransaction.h"

#include "Tracks/MovieSceneSpawnTrack.h"
#include "Sections/MovieSceneSpawnSection.h"

#define LOCTEXT_NAMESPACE "FObjectBindingNode"


namespace SequencerNodeConstants
{
	extern const float CommonPadding;
}


void GetKeyablePropertyPaths(UClass* Class, void* ValuePtr, UStruct* PropertySource, FPropertyPath PropertyPath, FSequencer& Sequencer, TArray<FPropertyPath>& KeyablePropertyPaths)
{
	//@todo need to resolve this between UMG and the level editor sequencer
	const bool bRecurseAllProperties = Sequencer.IsLevelEditorSequencer();

	for (TFieldIterator<UProperty> PropertyIterator(PropertySource); PropertyIterator; ++PropertyIterator)
	{
		UProperty* Property = *PropertyIterator;

		if (Property && !Property->HasAnyPropertyFlags(CPF_Deprecated))
		{
			PropertyPath.AddProperty(FPropertyInfo(Property));

			bool bIsPropertyKeyable = Sequencer.CanKeyProperty(FCanKeyPropertyParams(Class, PropertyPath));
			if (bIsPropertyKeyable)
			{
				KeyablePropertyPaths.Add(PropertyPath);
			}

			UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
			if (!bIsPropertyKeyable && ArrayProperty)
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(ValuePtr));
				for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
				{
					PropertyPath.AddProperty(FPropertyInfo(ArrayProperty->Inner, Index));

					if (Sequencer.CanKeyProperty(FCanKeyPropertyParams(Class, PropertyPath)))
					{
						KeyablePropertyPaths.Add(PropertyPath);
						bIsPropertyKeyable = true;
					}
					else if (UStructProperty* StructProperty = Cast<UStructProperty>(ArrayProperty->Inner))
					{
						GetKeyablePropertyPaths(Class, ArrayHelper.GetRawPtr(Index), StructProperty->Struct, PropertyPath, Sequencer, KeyablePropertyPaths);
					}

					PropertyPath = *PropertyPath.TrimPath(1);
				}
			}

			if (!bIsPropertyKeyable || bRecurseAllProperties)
			{
				if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
				{
					GetKeyablePropertyPaths(Class, StructProperty->ContainerPtrToValuePtr<void>(ValuePtr), StructProperty->Struct, PropertyPath, Sequencer, KeyablePropertyPaths);
				}
			}

			PropertyPath = *PropertyPath.TrimPath(1);
		}
	}
}


struct PropertyMenuData
{
	FString MenuName;
	FPropertyPath PropertyPath;
};



FSequencerObjectBindingNode::FSequencerObjectBindingNode(FName NodeName, const FText& InDisplayName, const FGuid& InObjectBinding, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree)
	: FSequencerDisplayNode(NodeName, InParentNode, InParentTree)
	, ObjectBinding(InObjectBinding)
	, DefaultDisplayName(InDisplayName)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene->FindPossessable(ObjectBinding))
	{
		BindingType = EObjectBindingType::Possessable;
	}
	else if (MovieScene->FindSpawnable(ObjectBinding))
	{
		BindingType = EObjectBindingType::Spawnable;
	}
	else
	{
		BindingType = EObjectBindingType::Unknown;
	}
}

void FSequencerObjectBindingNode::AddTrackNode( TSharedRef<FSequencerTrackNode> NewChild )
{
	AddChildAndSetParent( NewChild );
}

/* FSequencerDisplayNode interface
 *****************************************************************************/

void FSequencerObjectBindingNode::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");

	UObject* BoundObject = GetSequencer().FindSpawnedObjectOrTemplate(ObjectBinding);

	TSharedRef<FUICommandList> CommandList(new FUICommandList);
	TSharedPtr<FExtender> Extender = SequencerModule.GetObjectBindingContextMenuExtensibilityManager()->GetAllExtenders(CommandList, TArrayBuilder<UObject*>().Add(BoundObject));
	if (Extender.IsValid())
	{
		MenuBuilder.PushExtender(Extender.ToSharedRef());
	}

	if (GetSequencer().IsLevelEditorSequencer())
	{
		UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);

		if (Spawnable)
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("OwnerLabel", "Spawned Object Owner"),
				LOCTEXT("OwnerTooltip", "Specifies how the spawned object is to be owned"),
				FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::AddSpawnOwnershipMenu)
			);

			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().SaveCurrentSpawnableState );

			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ConvertToPossessable );
		}
		else
		{
			const UClass* ObjectClass = GetClassForObjectBinding();
			
			if (ObjectClass->IsChildOf(AActor::StaticClass()))
			{
				FFormatNamedArguments Args;

				MenuBuilder.AddSubMenu(
					FText::Format( LOCTEXT("Assign Actor ", "Assign Actor"), Args),
					FText::Format( LOCTEXT("AssignActorTooltip", "Assign an actor to this track"), Args ),
					FNewMenuDelegate::CreateRaw(&GetSequencer(), &FSequencer::AssignActor, ObjectBinding));
			}

			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ConvertToSpawnable );
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Import FBX", "Import..."),
			LOCTEXT("ImportFBXTooltip", "Import FBX animation to this object"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ GetSequencer().ImportFBX(); })
			));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Export FBX", "Export..."),
			LOCTEXT("ExportFBXTooltip", "Export FBX animation from this object"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ GetSequencer().ExportFBX(); })
			));
	}

	MenuBuilder.BeginSection("Organize", LOCTEXT("OrganizeContextMenuSectionName", "Organize"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("LabelsSubMenuText", "Labels"),
			LOCTEXT("LabelsSubMenuTip", "Add or remove labels on this track"),
			FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::HandleLabelsSubMenuCreate)
		);
	}
	MenuBuilder.EndSection();

	FSequencerDisplayNode::BuildContextMenu(MenuBuilder);
}

void FSequencerObjectBindingNode::AddSpawnOwnershipMenu(FMenuBuilder& MenuBuilder)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);
	if (!Spawnable)
	{
		return;
	}
	auto Callback = [=](ESpawnOwnership NewOwnership){

		FScopedTransaction Transaction(LOCTEXT("SetSpawnOwnership", "Set Spawnable Ownership"));

		Spawnable->SetSpawnOwnership(NewOwnership);

		// Overwrite the completion state for all spawn sections to ensure the expected behaviour.
		EMovieSceneCompletionMode NewCompletionMode = NewOwnership == ESpawnOwnership::InnerSequence ? EMovieSceneCompletionMode::RestoreState : EMovieSceneCompletionMode::KeepState;

		// Make all spawn sections retain state
		UMovieSceneSpawnTrack* SpawnTrack = MovieScene->FindTrack<UMovieSceneSpawnTrack>(ObjectBinding);
		if (SpawnTrack)
		{
			for (UMovieSceneSection* Section : SpawnTrack->GetAllSections())
			{
				Section->Modify();
				Section->EvalOptions.CompletionMode = NewCompletionMode;
			}
		}
	};

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ThisSequence_Label", "This Sequence"),
		LOCTEXT("ThisSequence_Tooltip", "Indicates that this sequence will own the spawned object. The object will be destroyed at the end of the sequence."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::InnerSequence),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::InnerSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MasterSequence_Label", "Master Sequence"),
		LOCTEXT("MasterSequence_Tooltip", "Indicates that the outermost sequence will own the spawned object. The object will be destroyed when the outermost sequence stops playing."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::MasterSequence),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::MasterSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("External_Label", "External"),
		LOCTEXT("External_Tooltip", "Indicates this object's lifetime is managed externally once spawned. It will not be destroyed by sequencer."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::External),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::External; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);
}

bool FSequencerObjectBindingNode::CanRenameNode() const
{
	return true;
}

TSharedRef<SWidget> FSequencerObjectBindingNode::GetCustomOutlinerContent()
{
	if (GetSequencer().IsReadOnly())
	{
		return SNullWidget::NullWidget;
	}
	
	// Create a container edit box
	TSharedRef<SHorizontalBox> BoxPanel = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SSpacer)
		];


	TAttribute<bool> HoverState = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FSequencerDisplayNode::IsHovered));

	BoxPanel->AddSlot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(LOCTEXT("TrackText", "Track"), FOnGetContent::CreateSP(this, &FSequencerObjectBindingNode::HandleAddTrackComboButtonGetMenuContent), HoverState)
	];

	const UClass* ObjectClass = GetClassForObjectBinding();
	GetSequencer().BuildObjectBindingEditButtons(BoxPanel, ObjectBinding, ObjectClass);

	return BoxPanel;
}


FText FSequencerObjectBindingNode::GetDisplayName() const
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene != nullptr)
	{
		return MovieScene->GetObjectDisplayName(ObjectBinding);
	}

	return DefaultDisplayName;
}

FLinearColor FSequencerObjectBindingNode::GetDisplayNameColor() const
{
	FSequencer& Sequencer = ParentTree.GetSequencer();
	
	for (TWeakObjectPtr<> BoundObject : Sequencer.FindBoundObjects(ObjectBinding, Sequencer.GetFocusedTemplateID()))
	{
		if (BoundObject.IsValid())
		{
			return FSequencerDisplayNode::GetDisplayNameColor();
		}
	}

	return FLinearColor::Red;
}

FText FSequencerObjectBindingNode::GetDisplayNameToolTipText() const
{
	FSequencer& Sequencer = ParentTree.GetSequencer();
	if ( Sequencer.FindObjectsInCurrentSequence(ObjectBinding).Num() == 0 )
	{
		return LOCTEXT("InvalidBoundObjectToolTip", "The object bound to this track is missing.");
	}
	else
	{
		return FText();
	}
}

const FSlateBrush* FSequencerObjectBindingNode::GetIconBrush() const
{
	return FSlateIconFinder::FindIconBrushForClass(GetClassForObjectBinding());
}

const FSlateBrush* FSequencerObjectBindingNode::GetIconOverlayBrush() const
{
	if (BindingType == EObjectBindingType::Spawnable)
	{
		return FEditorStyle::GetBrush("Sequencer.SpawnableIconOverlay");
	}
	return nullptr;
}

FText FSequencerObjectBindingNode::GetIconToolTipText() const
{
	if (BindingType == EObjectBindingType::Spawnable)
	{
		return LOCTEXT("SpawnableToolTip", "This item is spawned by sequencer according to this object's spawn track.");
	}
	else if (BindingType == EObjectBindingType::Possessable)
	{
		return LOCTEXT("PossessableToolTip", "This item is a possessable reference to an existing object.");
	}

	return FText();
}

float FSequencerObjectBindingNode::GetNodeHeight() const
{
	return SequencerLayoutConstants::ObjectNodeHeight + SequencerNodeConstants::CommonPadding*2;
}


FNodePadding FSequencerObjectBindingNode::GetNodePadding() const
{
	return FNodePadding(0.f);//SequencerNodeConstants::CommonPadding);
}


ESequencerNode::Type FSequencerObjectBindingNode::GetType() const
{
	return ESequencerNode::Object;
}


void FSequencerObjectBindingNode::SetDisplayName(const FText& NewDisplayName)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene != nullptr)
	{
		MovieScene->SetObjectDisplayName(ObjectBinding, NewDisplayName);
	}
}


bool FSequencerObjectBindingNode::CanDrag() const
{
	TSharedPtr<FSequencerDisplayNode> ParentSeqNode = GetParent();
	return ParentSeqNode.IsValid() == false || ParentSeqNode->GetType() != ESequencerNode::Object;
}


/* FSequencerObjectBindingNode implementation
 *****************************************************************************/

void FSequencerObjectBindingNode::AddPropertyMenuItems(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyableProperties, int32 PropertyNameIndexStart, int32 PropertyNameIndexEnd)
{
	TArray<PropertyMenuData> KeyablePropertyMenuData;

	for (auto KeyableProperty : KeyableProperties)
	{
		TArray<FString> PropertyNames;
		if (PropertyNameIndexEnd == -1)
		{
			PropertyNameIndexEnd = KeyableProperty.GetNumProperties();
		}

		//@todo
		if (PropertyNameIndexStart >= KeyableProperty.GetNumProperties())
		{
			continue;
		}

		for (int32 PropertyNameIndex = PropertyNameIndexStart; PropertyNameIndex < PropertyNameIndexEnd; ++PropertyNameIndex)
		{
			PropertyNames.Add(KeyableProperty.GetPropertyInfo(PropertyNameIndex).Property.Get()->GetDisplayNameText().ToString());
		}

		PropertyMenuData KeyableMenuData;
		{
			KeyableMenuData.PropertyPath = KeyableProperty;
			KeyableMenuData.MenuName = FString::Join( PropertyNames, TEXT( "." ) );
		}

		KeyablePropertyMenuData.Add(KeyableMenuData);
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});

	// Add menu items
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); ++MenuDataIndex)
	{
		FUIAction AddTrackMenuAction(FExecuteAction::CreateSP(this, &FSequencerObjectBindingNode::HandlePropertyMenuItemExecute, KeyablePropertyMenuData[MenuDataIndex].PropertyPath));
		AddTrackMenuBuilder.AddMenuEntry(FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName), FText(), FSlateIcon(), AddTrackMenuAction);
	}
}


const UClass* FSequencerObjectBindingNode::GetClassForObjectBinding() const
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);
	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBinding);
	
	// should exist, but also shouldn't be both a spawnable and a possessable
	check((Spawnable != nullptr) ^ (Possessable != nullptr));
	const UClass* ObjectClass = Spawnable ? Spawnable->GetObjectTemplate()->GetClass() : Possessable->GetPossessedObjectClass();

	return ObjectClass;
}

/* FSequencerObjectBindingNode callbacks
 *****************************************************************************/

TSharedRef<SWidget> FSequencerObjectBindingNode::HandleAddTrackComboButtonGetMenuContent()
{
	FSequencer& Sequencer = GetSequencer();

	//@todo need to resolve this between UMG and the level editor sequencer
	const bool bUseSubMenus = Sequencer.IsLevelEditorSequencer();

	UObject* BoundObject = GetSequencer().FindSpawnedObjectOrTemplate(ObjectBinding);

	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>( "Sequencer" );
	TSharedRef<FUICommandList> CommandList(new FUICommandList);
	FMenuBuilder AddTrackMenuBuilder(true, nullptr, SequencerModule.GetAddTrackMenuExtensibilityManager()->GetAllExtenders(CommandList, TArrayBuilder<UObject*>().Add(BoundObject)));

	const UClass* ObjectClass = GetClassForObjectBinding();
	AddTrackMenuBuilder.BeginSection(NAME_None, LOCTEXT("TracksMenuHeader" , "Tracks"));
	GetSequencer().BuildObjectBindingTrackMenu(AddTrackMenuBuilder, ObjectBinding, ObjectClass);
	AddTrackMenuBuilder.EndSection();

	TArray<FPropertyPath> KeyablePropertyPaths;

	if (BoundObject != nullptr)
	{
		FPropertyPath PropertyPath;
		GetKeyablePropertyPaths(BoundObject->GetClass(), BoundObject, BoundObject->GetClass(), PropertyPath, Sequencer, KeyablePropertyPaths);
	}

	// [Aspect Ratio]
	// [PostProcess Settings] [Bloom1Tint] [X]
	// [PostProcess Settings] [Bloom1Tint] [Y]
	// [PostProcess Settings] [ColorGrading]
	// [Ortho View]

	// Create property menu data based on keyable property paths
	TArray<PropertyMenuData> KeyablePropertyMenuData;
	for (const FPropertyPath& KeyablePropertyPath : KeyablePropertyPaths)
	{
		UProperty* Property = KeyablePropertyPath.GetRootProperty().Property.Get();
		if (Property)
		{
			PropertyMenuData KeyableMenuData;
			KeyableMenuData.PropertyPath = KeyablePropertyPath;
			if (KeyablePropertyPath.GetRootProperty().ArrayIndex != INDEX_NONE)
			{
				KeyableMenuData.MenuName = FText::Format(LOCTEXT("PropertyMenuTextFormat", "{0} [{1}]"), Property->GetDisplayNameText(), FText::AsNumber(KeyablePropertyPath.GetRootProperty().ArrayIndex)).ToString();
			}
			else
			{
				KeyableMenuData.MenuName = Property->GetDisplayNameText().ToString();
			}
			KeyablePropertyMenuData.Add(KeyableMenuData);
		}
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});
	

	// Add menu items
	AddTrackMenuBuilder.BeginSection( SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, LOCTEXT("PropertiesMenuHeader" , "Properties"));
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); )
	{
		TArray<FPropertyPath> KeyableSubMenuPropertyPaths;

		KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);

		// If this menu data only has one property name, add the menu item
		if (KeyablePropertyMenuData[MenuDataIndex].PropertyPath.GetNumProperties() == 1 || !bUseSubMenus)
		{
			AddPropertyMenuItems(AddTrackMenuBuilder, KeyableSubMenuPropertyPaths);
			++MenuDataIndex;
		}
		// Otherwise, look to the next menu data to gather up new data
		else
		{
			for (; MenuDataIndex < KeyablePropertyMenuData.Num()-1; )
			{
				if (KeyablePropertyMenuData[MenuDataIndex].MenuName == KeyablePropertyMenuData[MenuDataIndex+1].MenuName)
				{	
					++MenuDataIndex;
					KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);
				}
				else
				{
					break;
				}
			}

			AddTrackMenuBuilder.AddSubMenu(
				FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName),
				FText::GetEmpty(), 
				FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::HandleAddTrackSubMenuNew, KeyableSubMenuPropertyPaths, 0));

			++MenuDataIndex;
		}
	}
	AddTrackMenuBuilder.EndSection();

	return AddTrackMenuBuilder.MakeWidget();
}


void FSequencerObjectBindingNode::HandleAddTrackSubMenuNew(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyablePropertyPaths, int32 PropertyNameIndexStart)
{
	// [PostProcessSettings] [Bloom1Tint] [X]
	// [PostProcessSettings] [Bloom1Tint] [Y]
	// [PostProcessSettings] [ColorGrading]

	// Create property menu data based on keyable property paths
	TSet<UProperty*> PropertiesTraversed;
	TArray<PropertyMenuData> KeyablePropertyMenuData;
	for (const FPropertyPath& KeyablePropertyPath : KeyablePropertyPaths)
	{
		PropertyMenuData KeyableMenuData;
		KeyableMenuData.PropertyPath = KeyablePropertyPath;

		// If the path is greater than 1, keep track of the actual properties (not channels) and only add these properties once since we can't do single channel keying of a property yet.
		if (KeyablePropertyPath.GetNumProperties() > 1) //@todo
		{
			const FPropertyInfo& PropertyInfo = KeyablePropertyPath.GetPropertyInfo(1);
			UProperty* Property = PropertyInfo.Property.Get();
			if (PropertiesTraversed.Find(Property) != nullptr)
			{
				continue;
			}

			if (PropertyInfo.ArrayIndex != INDEX_NONE)
			{
				KeyableMenuData.MenuName = FText::Format(LOCTEXT("ArrayElementFormat", "Element {0}"), FText::AsNumber(PropertyInfo.ArrayIndex)).ToString();
			}
			else
			{
				KeyableMenuData.MenuName = FObjectEditorUtils::GetCategoryFName(Property).ToString();
			}

			PropertiesTraversed.Add(Property);
		}
		else
		{
			// No sub menus items, so skip
			continue; 
		}
		KeyablePropertyMenuData.Add(KeyableMenuData);
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});

	// Add menu items
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); )
	{
		TArray<FPropertyPath> KeyableSubMenuPropertyPaths;
		KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);

		for (; MenuDataIndex < KeyablePropertyMenuData.Num()-1; )
		{
			if (KeyablePropertyMenuData[MenuDataIndex].MenuName == KeyablePropertyMenuData[MenuDataIndex+1].MenuName)
			{
				++MenuDataIndex;
				KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);
			}
			else
			{
				break;
			}
		}

		AddTrackMenuBuilder.AddSubMenu(
			FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName),
			FText::GetEmpty(), 
			FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::AddPropertyMenuItems, KeyableSubMenuPropertyPaths, PropertyNameIndexStart + 1, PropertyNameIndexStart + 2));

		++MenuDataIndex;
	}
}


void FSequencerObjectBindingNode::HandleLabelsSubMenuCreate(FMenuBuilder& MenuBuilder)
{
	const TSet< TSharedRef<FSequencerDisplayNode> >& SelectedNodes = GetSequencer().GetSelection().GetSelectedOutlinerNodes();
	TArray<FGuid> ObjectBindingIds;
	for (TSharedRef<const FSequencerDisplayNode> SelectedNode : SelectedNodes )
	{
		if (SelectedNode->GetType() == ESequencerNode::Object)
		{
			TSharedRef<const FSequencerObjectBindingNode> ObjectBindingNode = StaticCastSharedRef<const FSequencerObjectBindingNode>(SelectedNode);
			FGuid ObjectBindingId = ObjectBindingNode->GetObjectBinding();
			if (ObjectBindingId.IsValid())
			{
				ObjectBindingIds.Add(ObjectBindingId);
			}
		}
	}

	MenuBuilder.AddWidget(SNew(SSequencerLabelEditor, GetSequencer(), ObjectBindingIds), FText::GetEmpty(), true);
}


void FSequencerObjectBindingNode::HandlePropertyMenuItemExecute(FPropertyPath PropertyPath)
{
	FSequencer& Sequencer = GetSequencer();
	UObject* BoundObject = GetSequencer().FindSpawnedObjectOrTemplate(ObjectBinding);

	TArray<UObject*> KeyableBoundObjects;
	if (BoundObject != nullptr)
	{
		if (Sequencer.CanKeyProperty(FCanKeyPropertyParams(BoundObject->GetClass(), PropertyPath)))
		{
			KeyableBoundObjects.Add(BoundObject);
		}
	}

	FKeyPropertyParams KeyPropertyParams(KeyableBoundObjects, PropertyPath, ESequencerKeyMode::ManualKeyForced);

	Sequencer.KeyProperty(KeyPropertyParams);
}


#undef LOCTEXT_NAMESPACE
