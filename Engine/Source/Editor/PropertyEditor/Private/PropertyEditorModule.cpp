// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "PropertyEditorModule.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "SSingleProperty.h"
#include "IDetailsView.h"
#include "SDetailsView.h"
#include "IPropertyTableWidgetHandle.h"
#include "IPropertyTable.h"
#include "UserInterface/PropertyTable/SPropertyTable.h"
#include "UserInterface/PropertyTable/PropertyTableWidgetHandle.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "SPropertyTreeViewImpl.h"
#include "Interfaces/IMainFrameModule.h"
#include "IPropertyChangeListener.h"
#include "PropertyChangeListener.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "PropertyEditorToolkit.h"

#include "Presentation/PropertyTable/PropertyTable.h"
#include "IPropertyTableCellPresenter.h"
#include "UserInterface/PropertyTable/TextPropertyTableCellPresenter.h"

#include "SStructureDetailsView.h"
#include "Widgets/Colors/SColorPicker.h"
#include "PropertyRowGenerator.h"


IMPLEMENT_MODULE( FPropertyEditorModule, PropertyEditor );

TSharedRef<IPropertyTypeCustomization> FPropertyTypeLayoutCallback::GetCustomizationInstance() const
{
	return PropertyTypeLayoutDelegate.Execute();
}

void FPropertyTypeLayoutCallbackList::Add( const FPropertyTypeLayoutCallback& NewCallback )
{
	if( !NewCallback.PropertyTypeIdentifier.IsValid() )
	{
		BaseCallback = NewCallback;
	}
	else
	{
		IdentifierList.Add( NewCallback );
	}
}

void FPropertyTypeLayoutCallbackList::Remove( const TSharedPtr<IPropertyTypeIdentifier>& InIdentifier )
{
	if( !InIdentifier.IsValid() )
	{
		BaseCallback = FPropertyTypeLayoutCallback();
	}
	else
	{
		IdentifierList.RemoveAllSwap( [&InIdentifier]( FPropertyTypeLayoutCallback& Callback) { return Callback.PropertyTypeIdentifier == InIdentifier; } );
	}
}

const FPropertyTypeLayoutCallback& FPropertyTypeLayoutCallbackList::Find( const IPropertyHandle& PropertyHandle ) const 
{
	if( IdentifierList.Num() > 0 )
	{
		const FPropertyTypeLayoutCallback* Callback =
			IdentifierList.FindByPredicate
			(
				[&]( const FPropertyTypeLayoutCallback& InCallback )
				{
					return InCallback.PropertyTypeIdentifier->IsPropertyTypeCustomized( PropertyHandle );
				}
			);

		if( Callback )
		{
			return *Callback;
		}
	}

	return BaseCallback;
}

void FPropertyEditorModule::StartupModule()
{
}

void FPropertyEditorModule::ShutdownModule()
{
	// NOTE: It's vital that we clean up everything created by this DLL here!  We need to make sure there
	//       are no outstanding references to objects as the compiled code for this module's class will
	//       literally be unloaded from memory after this function exits.  This even includes instantiated
	//       templates, such as delegate wrapper objects that are allocated by the module!
	DestroyColorPicker();

	AllDetailViews.Empty();
	AllSinglePropertyViews.Empty();
}

void FPropertyEditorModule::NotifyCustomizationModuleChanged()
{
	if (FSlateApplication::IsInitialized())
	{
		// The module was changed (loaded or unloaded), force a refresh.  Note it is assumed the module unregisters all customization delegates before this
		for (int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex)
		{
			if (AllDetailViews[ViewIndex].IsValid())
			{
				TSharedPtr<SDetailsView> DetailViewPin = AllDetailViews[ViewIndex].Pin();

				DetailViewPin->ForceRefresh();
			}
		}
	}
}

static bool ShouldShowProperty(const FPropertyAndParent& PropertyAndParent, bool bHaveTemplate)
{
	const UProperty& Property = PropertyAndParent.Property;

	if ( bHaveTemplate )
	{
		const UClass* PropertyOwnerClass = Cast<const UClass>(Property.GetOuter());
		const bool bDisableEditOnTemplate = PropertyOwnerClass 
			&& PropertyOwnerClass->IsNative()
			&& Property.HasAnyPropertyFlags(CPF_DisableEditOnTemplate);
		if(bDisableEditOnTemplate)
		{
			return false;
		}
	}
	return true;
}

TSharedRef<SWindow> FPropertyEditorModule::CreateFloatingDetailsView( const TArray< UObject* >& InObjects, bool bIsLockable )
{

	TSharedRef<SWindow> NewSlateWindow = SNew(SWindow)
		.Title( NSLOCTEXT("PropertyEditor", "WindowTitle", "Property Editor") )
		.ClientSize( FVector2D(400, 550) );

	// If the main frame exists parent the window to it
	TSharedPtr< SWindow > ParentWindow;
	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	if( ParentWindow.IsValid() )
	{
		// Parent the window to the main frame 
		FSlateApplication::Get().AddWindowAsNativeChild( NewSlateWindow, ParentWindow.ToSharedRef() );
	}
	else
	{
		FSlateApplication::Get().AddWindow( NewSlateWindow );
	}
	
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.bLockable = bIsLockable;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedRef<IDetailsView> DetailView = PropertyEditorModule.CreateDetailView( Args );

	bool bHaveTemplate = false;
	for(int32 i=0; i<InObjects.Num(); i++)
	{
		if(InObjects[i] != NULL && InObjects[i]->IsTemplate())
		{
			bHaveTemplate = true;
			break;
		}
	}

	DetailView->SetIsPropertyVisibleDelegate( FIsPropertyVisible::CreateStatic(&ShouldShowProperty, bHaveTemplate) );

	DetailView->SetObjects( InObjects );

	NewSlateWindow->SetContent(
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush(TEXT("PropertyWindow.WindowBorder")) )
		[
			DetailView
		]
	);

	return NewSlateWindow;
}

TSharedRef<SPropertyTreeViewImpl> FPropertyEditorModule::CreatePropertyView( 
	UObject* InObject,
	bool bAllowFavorites, 
	bool bIsLockable, 
	bool bHiddenPropertyVisibility, 
	bool bAllowSearch, 
	bool ShowTopLevelNodes,
	FNotifyHook* InNotifyHook, 
	float InNameColumnWidth,
	FOnPropertySelectionChanged OnPropertySelectionChanged,
	FOnPropertyClicked OnPropertyMiddleClicked,
	FConstructExternalColumnHeaders ConstructExternalColumnHeaders,
	FConstructExternalColumnCell ConstructExternalColumnCell)
{
	TSharedRef<SPropertyTreeViewImpl> PropertyView = 
		SNew( SPropertyTreeViewImpl )
		.IsLockable( bIsLockable )
		.AllowFavorites( bAllowFavorites )
		.HiddenPropertyVis( bHiddenPropertyVisibility )
		.NotifyHook( InNotifyHook )
		.AllowSearch( bAllowSearch )
		.ShowTopLevelNodes( ShowTopLevelNodes )
		.NameColumnWidth( InNameColumnWidth )
		.OnPropertySelectionChanged( OnPropertySelectionChanged )
		.OnPropertyMiddleClicked( OnPropertyMiddleClicked )
		.ConstructExternalColumnHeaders( ConstructExternalColumnHeaders )
		.ConstructExternalColumnCell( ConstructExternalColumnCell );

	if( InObject )
	{
		TArray< TWeakObjectPtr< UObject > > Objects;
		Objects.Add( InObject );
		PropertyView->SetObjectArray( Objects );
	}

	return PropertyView;
}

TSharedPtr<FAssetThumbnailPool> FPropertyEditorModule::GetThumbnailPool()
{
	if (!GlobalThumbnailPool.IsValid())
	{
		// Create a thumbnail pool for the view if it doesn't exist.  This does not use resources of no thumbnails are used
		GlobalThumbnailPool = MakeShareable(new FAssetThumbnailPool(50, false));
	}

	return GlobalThumbnailPool;
}

TSharedRef<IDetailsView> FPropertyEditorModule::CreateDetailView( const FDetailsViewArgs& DetailsViewArgs )
{
	// Compact the list of detail view instances
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		if ( !AllDetailViews[ViewIndex].IsValid() )
		{
			AllDetailViews.RemoveAtSwap( ViewIndex );
			--ViewIndex;
		}
	}

	TSharedRef<SDetailsView> DetailView = 
		SNew( SDetailsView )
		.DetailsViewArgs( DetailsViewArgs );

	AllDetailViews.Add( DetailView );

	PropertyEditorOpened.Broadcast();
	return DetailView;
}

TSharedPtr<IDetailsView> FPropertyEditorModule::FindDetailView( const FName ViewIdentifier ) const
{
	if(ViewIdentifier.IsNone())
	{
		return nullptr;
	}

	for(auto It = AllDetailViews.CreateConstIterator(); It; ++It)
	{
		TSharedPtr<SDetailsView> DetailsView = It->Pin();
		if(DetailsView.IsValid() && DetailsView->GetIdentifier() == ViewIdentifier)
		{
			return DetailsView;
		}
	}

	return nullptr;
}

TSharedPtr<ISinglePropertyView> FPropertyEditorModule::CreateSingleProperty( UObject* InObject, FName InPropertyName, const FSinglePropertyParams& InitParams )
{
	// Compact the list of detail view instances
	for( int32 ViewIndex = 0; ViewIndex < AllSinglePropertyViews.Num(); ++ViewIndex )
	{
		if ( !AllSinglePropertyViews[ViewIndex].IsValid() )
		{
			AllSinglePropertyViews.RemoveAtSwap( ViewIndex );
			--ViewIndex;
		}
	}

	TSharedRef<SSingleProperty> Property = 
		SNew( SSingleProperty )
		.Object( InObject )
		.PropertyName( InPropertyName )
		.NamePlacement( InitParams.NamePlacement )
		.NameOverride( InitParams.NameOverride )
		.NotifyHook( InitParams.NotifyHook )
		.PropertyFont( InitParams.Font );

	if( Property->HasValidProperty() )
	{
		AllSinglePropertyViews.Add( Property );

		return Property;
	}

	return NULL;
}

TSharedRef< IPropertyTable > FPropertyEditorModule::CreatePropertyTable()
{
	return MakeShareable( new FPropertyTable() );
}

TSharedRef< SWidget > FPropertyEditorModule::CreatePropertyTableWidget( const TSharedRef< class IPropertyTable >& PropertyTable )
{
	return SNew( SPropertyTable, PropertyTable );
}

TSharedRef< SWidget > FPropertyEditorModule::CreatePropertyTableWidget( const TSharedRef< IPropertyTable >& PropertyTable, const TArray< TSharedRef< class IPropertyTableCustomColumn > >& Customizations )
{
	return SNew( SPropertyTable, PropertyTable )
		.ColumnCustomizations( Customizations );
}

TSharedRef< IPropertyTableWidgetHandle > FPropertyEditorModule::CreatePropertyTableWidgetHandle( const TSharedRef< IPropertyTable >& PropertyTable )
{
	TSharedRef< FPropertyTableWidgetHandle > FWidgetHandle = MakeShareable( new FPropertyTableWidgetHandle( SNew( SPropertyTable, PropertyTable ) ) );

	TSharedRef< IPropertyTableWidgetHandle > IWidgetHandle = StaticCastSharedRef<IPropertyTableWidgetHandle>(FWidgetHandle);

	 return IWidgetHandle;
}

TSharedRef< IPropertyTableWidgetHandle > FPropertyEditorModule::CreatePropertyTableWidgetHandle( const TSharedRef< IPropertyTable >& PropertyTable, const TArray< TSharedRef< class IPropertyTableCustomColumn > >& Customizations )
{
	TSharedRef< FPropertyTableWidgetHandle > FWidgetHandle = MakeShareable( new FPropertyTableWidgetHandle( SNew( SPropertyTable, PropertyTable )
		.ColumnCustomizations( Customizations )));

	TSharedRef< IPropertyTableWidgetHandle > IWidgetHandle = StaticCastSharedRef<IPropertyTableWidgetHandle>(FWidgetHandle);

	 return IWidgetHandle;
}

TSharedRef< IPropertyTableCellPresenter > FPropertyEditorModule::CreateTextPropertyCellPresenter(const TSharedRef< class FPropertyNode >& InPropertyNode, const TSharedRef< class IPropertyTableUtilities >& InPropertyUtilities, 
																								 const FSlateFontInfo* InFontPtr /* = NULL */)
{
	FSlateFontInfo InFont;

	if (InFontPtr == NULL)
	{
		// Encapsulating reference to Private file PropertyTableConstants.h
		InFont = FEditorStyle::GetFontStyle( PropertyTableConstants::NormalFontStyle );
	}
	else
	{
		InFont = *InFontPtr;
	}

	TSharedRef< FPropertyEditor > PropertyEditor = FPropertyEditor::Create( InPropertyNode, InPropertyUtilities );
	return MakeShareable( new FTextPropertyTableCellPresenter( PropertyEditor, InPropertyUtilities, InFont) );
}

UStructProperty* FPropertyEditorModule::RegisterStructOnScopeProperty(TSharedRef<FStructOnScope> StructOnScope)
{
	const FName StructName = StructOnScope->GetStruct()->GetFName();
	UStructProperty* StructProperty = RegisteredStructToProxyMap.FindRef(StructName);

	if(!StructProperty)
	{
		UScriptStruct* InnerStruct = Cast<UScriptStruct>(const_cast<UStruct*>(StructOnScope->GetStruct()));
		StructProperty = NewObject<UStructProperty>(InnerStruct, MakeUniqueObjectName(InnerStruct, UStructProperty::StaticClass(), InnerStruct->GetFName()));
		StructProperty->Struct = InnerStruct;
		StructProperty->ElementSize = StructOnScope->GetStruct()->GetStructureSize();

		RegisteredStructToProxyMap.Add(StructName, StructProperty);
		StructProperty->AddToRoot();
	}

	return StructProperty;
}

TSharedRef< FAssetEditorToolkit > FPropertyEditorModule::CreatePropertyEditorToolkit( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit )
{
	return FPropertyEditorToolkit::CreateEditor( Mode, InitToolkitHost, ObjectToEdit );
}


TSharedRef< FAssetEditorToolkit > FPropertyEditorModule::CreatePropertyEditorToolkit( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray< UObject* >& ObjectsToEdit )
{
	return FPropertyEditorToolkit::CreateEditor( Mode, InitToolkitHost, ObjectsToEdit );
}

TSharedRef< FAssetEditorToolkit > FPropertyEditorModule::CreatePropertyEditorToolkit( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray< TWeakObjectPtr< UObject > >& ObjectsToEdit )
{
	TArray< UObject* > RawObjectsToEdit;
	for( auto ObjectIter = ObjectsToEdit.CreateConstIterator(); ObjectIter; ++ObjectIter )
	{
		RawObjectsToEdit.Add( ObjectIter->Get() );
	}

	return FPropertyEditorToolkit::CreateEditor( Mode, InitToolkitHost, RawObjectsToEdit );
}

TSharedRef<IPropertyChangeListener> FPropertyEditorModule::CreatePropertyChangeListener()
{
	return MakeShareable( new FPropertyChangeListener );
}

void FPropertyEditorModule::RegisterCustomClassLayout( FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate )
{
	if (ClassName != NAME_None)
	{
		FDetailLayoutCallback Callback;
		Callback.DetailLayoutDelegate = DetailLayoutDelegate;
		// @todo: DetailsView: Fix me: this specifies the order in which detail layouts should be queried
		Callback.Order = ClassNameToDetailLayoutNameMap.Num();

		ClassNameToDetailLayoutNameMap.Add(ClassName, Callback);
	}
}


void FPropertyEditorModule::UnregisterCustomClassLayout( FName ClassName )
{
	if (ClassName.IsValid() && (ClassName != NAME_None))
	{
		ClassNameToDetailLayoutNameMap.Remove(ClassName);
	}
}


void FPropertyEditorModule::RegisterCustomPropertyTypeLayout( FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier)
{
	if( PropertyTypeName != NAME_None )
	{
		FPropertyTypeLayoutCallback Callback;
		Callback.PropertyTypeLayoutDelegate = PropertyTypeLayoutDelegate;
		Callback.PropertyTypeIdentifier = Identifier;

		FPropertyTypeLayoutCallbackList* LayoutCallbacks = GlobalPropertyTypeToLayoutMap.Find(PropertyTypeName);
		if (LayoutCallbacks)
		{
			LayoutCallbacks->Add(Callback);
		}
		else
		{
			FPropertyTypeLayoutCallbackList NewLayoutCallbacks;
			NewLayoutCallbacks.Add(Callback);
			GlobalPropertyTypeToLayoutMap.Add(PropertyTypeName, NewLayoutCallbacks);
		}
	}
}

void FPropertyEditorModule::RegisterCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier, TSharedPtr<IDetailsView> ForSpecificInstance)
{
	if (ForSpecificInstance.IsValid())
	{
		ForSpecificInstance->RegisterInstancedCustomPropertyTypeLayout(PropertyTypeName, PropertyTypeLayoutDelegate, Identifier);
	}
	else
	{
		RegisterCustomPropertyTypeLayout(PropertyTypeName, PropertyTypeLayoutDelegate, Identifier);
	}
}

void FPropertyEditorModule::UnregisterCustomPropertyTypeLayout(FName PropertyTypeName, TSharedPtr<IPropertyTypeIdentifier> InIdentifier, TSharedPtr<IDetailsView> ForSpecificInstance)
{
	if (ForSpecificInstance.IsValid())
	{
		ForSpecificInstance->UnregisterInstancedCustomPropertyTypeLayout(PropertyTypeName, InIdentifier);
	}
	else
	{
		UnregisterCustomPropertyTypeLayout(PropertyTypeName, InIdentifier);
	}
}


void FPropertyEditorModule::UnregisterCustomPropertyTypeLayout( FName PropertyTypeName, TSharedPtr<IPropertyTypeIdentifier> Identifier)
{
	if (!PropertyTypeName.IsValid() || (PropertyTypeName == NAME_None))
	{
		return;
	}

	FPropertyTypeLayoutCallbackList* LayoutCallbacks = GlobalPropertyTypeToLayoutMap.Find(PropertyTypeName);

	if (LayoutCallbacks)
	{
		LayoutCallbacks->Remove(Identifier);
	}
}



bool FPropertyEditorModule::HasUnlockedDetailViews() const
{
	uint32 NumUnlockedViews = 0;
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		const TWeakPtr<SDetailsView>& DetailView = AllDetailViews[ ViewIndex ];
		if( DetailView.IsValid() )
		{
			TSharedPtr<SDetailsView> DetailViewPin = DetailView.Pin();
			if( DetailViewPin->IsUpdatable() &&  !DetailViewPin->IsLocked() )
			{
				return true;
			}
		}
	}

	return false;
}

/**
 * Refreshes property windows with a new list of objects to view
 * 
 * @param NewObjectList	The list of objects each property window should view
 */
void FPropertyEditorModule::UpdatePropertyViews( const TArray<UObject*>& NewObjectList )
{
	DestroyColorPicker();

	TSet<AActor*> ValidObjects;
	
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		if( AllDetailViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SDetailsView> DetailViewPin = AllDetailViews[ ViewIndex ].Pin();
			if( DetailViewPin->IsUpdatable() )
			{
				if( !DetailViewPin->IsLocked() )
				{
					DetailViewPin->SetObjects(NewObjectList, true);
				}
				else
				{
					DetailViewPin->RemoveInvalidObjects();
				}
			}
		}
	}
}

void FPropertyEditorModule::ReplaceViewedObjects( const TMap<UObject*, UObject*>& OldToNewObjectMap )
{
	// Replace objects from detail views
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		if( AllDetailViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SDetailsView> DetailViewPin = AllDetailViews[ ViewIndex ].Pin();

			DetailViewPin->ReplaceObjects( OldToNewObjectMap );
		}
	}

	// Replace objects from single views
	for( int32 ViewIndex = 0; ViewIndex < AllSinglePropertyViews.Num(); ++ViewIndex )
	{
		if( AllSinglePropertyViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SSingleProperty> SinglePropPin = AllSinglePropertyViews[ ViewIndex ].Pin();

			SinglePropPin->ReplaceObjects( OldToNewObjectMap );
		}
	}
}

void FPropertyEditorModule::RemoveDeletedObjects( TArray<UObject*>& DeletedObjects )
{
	DestroyColorPicker();

	// remove deleted objects from detail views
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		if( AllDetailViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SDetailsView> DetailViewPin = AllDetailViews[ ViewIndex ].Pin();

			DetailViewPin->RemoveDeletedObjects( DeletedObjects );
		}
	}

	// remove deleted object from single property views
	for( int32 ViewIndex = 0; ViewIndex < AllSinglePropertyViews.Num(); ++ViewIndex )
	{
		if( AllSinglePropertyViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SSingleProperty> SinglePropPin = AllSinglePropertyViews[ ViewIndex ].Pin();

			SinglePropPin->RemoveDeletedObjects( DeletedObjects );
		}
	}
}

bool FPropertyEditorModule::IsCustomizedStruct(const UStruct* Struct, const FCustomPropertyTypeLayoutMap& InstancePropertyTypeLayoutMap) const
{
	bool bFound = false;
	if (Struct && !Struct->IsA<UUserDefinedStruct>())
	{
		bFound = InstancePropertyTypeLayoutMap.Contains( Struct->GetFName() );
		if( !bFound )
		{
			bFound = GlobalPropertyTypeToLayoutMap.Contains( Struct->GetFName() );
		}
	}
	
	return bFound;
}

FPropertyTypeLayoutCallback FPropertyEditorModule::GetPropertyTypeCustomization(const UProperty* Property, const IPropertyHandle& PropertyHandle, const FCustomPropertyTypeLayoutMap& InstancedPropertyTypeLayoutMap)
{
	if( Property )
	{
		const UStructProperty* StructProperty = Cast<UStructProperty>(Property);
		bool bStructProperty = StructProperty && StructProperty->Struct;
		const bool bUserDefinedStruct = bStructProperty && StructProperty->Struct->IsA<UUserDefinedStruct>();
		bStructProperty &= !bUserDefinedStruct;

		const UEnum* Enum = nullptr;

		if (const UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
		{
			Enum = ByteProperty->Enum;
		}
		else if (const UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
		{
			Enum = EnumProperty->GetEnum();
		}

		if (Enum && Enum->IsA<UUserDefinedEnum>())
		{
			Enum = nullptr;
		}

		const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
		const bool bObjectProperty = ObjectProperty != NULL && ObjectProperty->PropertyClass != NULL;

		FName PropertyTypeName;
		if( bStructProperty )
		{
			PropertyTypeName = StructProperty->Struct->GetFName();
		}
		else if( Enum )
		{
			PropertyTypeName = Enum->GetFName();
		}
		else if ( bObjectProperty )
		{
			PropertyTypeName = ObjectProperty->PropertyClass->GetFName();
		}
		else
		{
			PropertyTypeName = Property->GetClass()->GetFName();
		}


		if (PropertyTypeName != NAME_None)
		{
			const FPropertyTypeLayoutCallbackList* LayoutCallbacks = InstancedPropertyTypeLayoutMap.Find( PropertyTypeName );
	
			if( !LayoutCallbacks )
			{
				LayoutCallbacks = GlobalPropertyTypeToLayoutMap.Find(PropertyTypeName);
			}

			if ( LayoutCallbacks )
			{
				const FPropertyTypeLayoutCallback& Callback = LayoutCallbacks->Find(PropertyHandle);
				return Callback;
			}
		}
	}

	return FPropertyTypeLayoutCallback();
}

TSharedRef<class IStructureDetailsView> FPropertyEditorModule::CreateStructureDetailView(const FDetailsViewArgs& DetailsViewArgs, const FStructureDetailsViewArgs& StructureDetailsViewArgs, TSharedPtr<FStructOnScope> StructData, const FText& CustomName)
{
	TSharedRef<SStructureDetailsView> DetailView =
		SNew(SStructureDetailsView)
		.DetailsViewArgs(DetailsViewArgs)
		.CustomName(CustomName);

	struct FStructureDetailsViewFilter
	{
		static bool HasFilter( const FStructureDetailsViewArgs InStructureDetailsViewArgs )
		{
			const bool bShowEverything = InStructureDetailsViewArgs.bShowObjects 
				&& InStructureDetailsViewArgs.bShowAssets 
				&& InStructureDetailsViewArgs.bShowClasses 
				&& InStructureDetailsViewArgs.bShowInterfaces;
			return !bShowEverything;
		}

		static bool PassesFilter( const FPropertyAndParent& PropertyAndParent, const FStructureDetailsViewArgs InStructureDetailsViewArgs )
		{
			const auto ArrayProperty = Cast<UArrayProperty>(&PropertyAndParent.Property);
			const auto SetProperty = Cast<USetProperty>(&PropertyAndParent.Property);
			const auto MapProperty = Cast<UMapProperty>(&PropertyAndParent.Property);

			// If the property is a container type, the filter should test against the type of the container's contents
			const UProperty* PropertyToTest = ArrayProperty ? ArrayProperty->Inner : &PropertyAndParent.Property;
			PropertyToTest = SetProperty ? SetProperty->ElementProp : PropertyToTest;
			PropertyToTest = MapProperty ? MapProperty->ValueProp : PropertyToTest;

			if( InStructureDetailsViewArgs.bShowClasses && (PropertyToTest->IsA<UClassProperty>() || PropertyToTest->IsA<USoftClassProperty>()) )
			{
				return true;
			}

			if( InStructureDetailsViewArgs.bShowInterfaces && PropertyToTest->IsA<UInterfaceProperty>() )
			{
				return true;
			}

			const auto ObjectProperty = Cast<UObjectPropertyBase>(PropertyToTest);
			if( ObjectProperty )
			{
				if( InStructureDetailsViewArgs.bShowAssets )
				{
					// Is this an "asset" property?
					if( PropertyToTest->IsA<USoftObjectProperty>())
					{
						return true;
					}

					// Not an "asset" property, but it may still be a property using an asset class type (such as a raw pointer)
					if( ObjectProperty->PropertyClass )
					{
						// We can use the asset tools module to see whether this type has asset actions (which likely means it's an asset class type)
						FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
						return AssetToolsModule.Get().GetAssetTypeActionsForClass(ObjectProperty->PropertyClass).IsValid();
					}
				}

				return InStructureDetailsViewArgs.bShowObjects;
			}

			return true;
		}
	};

	// Only add the filter if we need to exclude things
	if (FStructureDetailsViewFilter::HasFilter(StructureDetailsViewArgs))
	{
		DetailView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateStatic(&FStructureDetailsViewFilter::PassesFilter, StructureDetailsViewArgs));
	}
	DetailView->SetStructureData(StructData);

	return DetailView;
}

TSharedRef<class IPropertyRowGenerator> FPropertyEditorModule::CreatePropertyRowGenerator(const struct FPropertyRowGeneratorArgs& InArgs)
{
	return MakeShared<FPropertyRowGenerator>(InArgs, GetThumbnailPool());
}
