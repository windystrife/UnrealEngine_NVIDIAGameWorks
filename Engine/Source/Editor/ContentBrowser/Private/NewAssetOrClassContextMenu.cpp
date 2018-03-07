// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NewAssetOrClassContextMenu.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Settings/ContentBrowserSettings.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "ContentBrowserUtils.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "ClassIconFinder.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

struct FFactoryItem
{
	UFactory* Factory;
	FText DisplayName;

	FFactoryItem(UFactory* InFactory, const FText& InDisplayName)
		: Factory(InFactory), DisplayName(InDisplayName)
	{}
};

TArray<FFactoryItem> FindFactoriesInCategory(EAssetTypeCategories::Type AssetTypeCategory)
{
	TArray<FFactoryItem> FactoriesInThisCategory;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UFactory::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			UFactory* Factory = Class->GetDefaultObject<UFactory>();
			if (Factory->ShouldShowInNewMenu() && ensure(!Factory->GetDisplayName().IsEmpty()))
			{
				uint32 FactoryCategories = Factory->GetMenuCategories();

				if (FactoryCategories & AssetTypeCategory)
				{
					new(FactoriesInThisCategory)FFactoryItem(Factory, Factory->GetDisplayName());
				}
			}
		}
	}

	return FactoriesInThisCategory;
}

class SFactoryMenuEntry : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SFactoryMenuEntry )
		: _Width(32)
		, _Height(32)
		{}
		SLATE_ARGUMENT( uint32, Width )
		SLATE_ARGUMENT( uint32, Height )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	Factory				The factory this menu entry represents
	 */
	void Construct( const FArguments& InArgs, UFactory* Factory )
	{
		const FName ClassThumbnailBrushOverride = Factory->GetNewAssetThumbnailOverride();
		const FSlateBrush* ClassThumbnail = nullptr;
		if (ClassThumbnailBrushOverride.IsNone())
		{
			ClassThumbnail = FClassIconFinder::FindThumbnailForClass(Factory->GetSupportedClass());
		}
		else
		{
			// Instead of getting the override thumbnail directly from the editor style here get it from the
			// ClassIconFinder since it may have additional styles registered which can be searched by passing
			// it as a default with no class to search for.
			ClassThumbnail = FClassIconFinder::FindThumbnailForClass(nullptr, ClassThumbnailBrushOverride);
		}

		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Factory->GetSupportedClass());

		FLinearColor AssetColor = FLinearColor::White;
		if ( AssetTypeActions.IsValid() )
		{
			AssetColor = AssetTypeActions.Pin()->GetTypeColor();
		}

		ChildSlot
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 4, 0, 0, 0 )
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew( SOverlay )

				+SOverlay::Slot()
				[
					SNew( SBox )
					.WidthOverride( InArgs._Width + 4 )
					.HeightOverride( InArgs._Height + 4 )
					[
						SNew( SBorder )
						.BorderImage( FEditorStyle::GetBrush("AssetThumbnail.AssetBackground") )
						.BorderBackgroundColor(AssetColor.CopyWithNewOpacity(0.3f))
						.Padding( 2.0f )
						.VAlign( VAlign_Center )
						.HAlign( HAlign_Center )
						[
							SNew( SImage )
							.Image( ClassThumbnail )
						]
					]
				]

				+SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Bottom)
				[
					SNew( SBorder )
					.BorderImage( FEditorStyle::GetBrush("WhiteBrush") )
					.BorderBackgroundColor( AssetColor )
					.Padding( FMargin(0, FMath::Max(FMath::CeilToFloat(InArgs._Width*0.025f), 3.0f), 0, 0) )
				]
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 4, 0)
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.Padding(0, 0, 0, 1)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font( FEditorStyle::GetFontStyle("LevelViewportContextMenu.AssetLabel.Text.Font") )
					.Text( Factory->GetDisplayName() )
				]
			]
		];

		
		SetToolTip(IDocumentation::Get()->CreateToolTip(Factory->GetToolTip(), nullptr, Factory->GetToolTipDocumentationPage(), Factory->GetToolTipDocumentationExcerpt()));
	}
};

void FNewAssetOrClassContextMenu::MakeContextMenu(
	FMenuBuilder& MenuBuilder, 
	const TArray<FName>& InSelectedPaths,
	const FOnNewAssetRequested& InOnNewAssetRequested, 
	const FOnNewClassRequested& InOnNewClassRequested, 
	const FOnNewFolderRequested& InOnNewFolderRequested, 
	const FOnImportAssetRequested& InOnImportAssetRequested, 
	const FOnGetContentRequested& InOnGetContentRequested
	)
{
	TArray<FString> SelectedStringPaths;
	SelectedStringPaths.Reserve(InSelectedPaths.Num());
	for(const FName& Path : InSelectedPaths)
	{
		SelectedStringPaths.Add(Path.ToString());
	}

	MakeContextMenu(MenuBuilder, SelectedStringPaths, InOnNewAssetRequested, InOnNewClassRequested, InOnNewFolderRequested, InOnImportAssetRequested, InOnGetContentRequested);
}

void FNewAssetOrClassContextMenu::MakeContextMenu(
	FMenuBuilder& MenuBuilder, 
	const TArray<FString>& InSelectedPaths,
	const FOnNewAssetRequested& InOnNewAssetRequested, 
	const FOnNewClassRequested& InOnNewClassRequested, 
	const FOnNewFolderRequested& InOnNewFolderRequested, 
	const FOnImportAssetRequested& InOnImportAssetRequested, 
	const FOnGetContentRequested& InOnGetContentRequested
	)
{
	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(InSelectedPaths, NumAssetPaths, NumClassPaths);

	const FString FirstSelectedPath = (InSelectedPaths.Num() > 0) ? InSelectedPaths[0] : FString();
	const bool bIsValidNewClassPath = ContentBrowserUtils::IsValidPathToCreateNewClass(FirstSelectedPath);
	const bool bIsValidNewFolderPath = ContentBrowserUtils::IsValidPathToCreateNewFolder(FirstSelectedPath);
	const bool bHasSinglePathSelected = InSelectedPaths.Num() == 1;

	auto CanExecuteFolderActions = [NumAssetPaths, NumClassPaths, bIsValidNewFolderPath]() -> bool
	{
		// We can execute folder actions when we only have a single path selected, and that path is a valid path for creating a folder
		return (NumAssetPaths + NumClassPaths) == 1 && bIsValidNewFolderPath;
	};
	const FCanExecuteAction CanExecuteFolderActionsDelegate = FCanExecuteAction::CreateLambda(CanExecuteFolderActions);

	auto CanExecuteAssetActions = [NumAssetPaths, NumClassPaths]() -> bool
	{
		// We can execute asset actions when we only have a single asset path selected
		return NumAssetPaths == 1 && NumClassPaths == 0;
	};
	const FCanExecuteAction CanExecuteAssetActionsDelegate = FCanExecuteAction::CreateLambda(CanExecuteAssetActions);

	auto CanExecuteClassActions = [NumAssetPaths, NumClassPaths]() -> bool
	{
		// We can execute class actions when we only have a single path selected
		// This menu always lets you create classes, but uses your default project source folder if the selected path is invalid for creating classes
		return (NumAssetPaths + NumClassPaths) == 1;
	};
	const FCanExecuteAction CanExecuteClassActionsDelegate = FCanExecuteAction::CreateLambda(CanExecuteClassActions);

	// Get Content
	if ( InOnGetContentRequested.IsBound() )
	{
		MenuBuilder.BeginSection( "ContentBrowserGetContent", LOCTEXT( "GetContentMenuHeading", "Content" ) );
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT( "GetContentText", "Add Feature or Content Pack..." ),
				LOCTEXT( "GetContentTooltip", "Add features and content packs to the project." ),
				FSlateIcon( FEditorStyle::GetStyleSetName(), "ContentBrowser.AddContent" ),
				FUIAction( FExecuteAction::CreateStatic( &FNewAssetOrClassContextMenu::ExecuteGetContent, InOnGetContentRequested ) )
				);
		}
		MenuBuilder.EndSection();
	}

	// New Folder
	if(InOnNewFolderRequested.IsBound() && GetDefault<UContentBrowserSettings>()->DisplayFolders)
	{
		MenuBuilder.BeginSection("ContentBrowserNewFolder", LOCTEXT("FolderMenuHeading", "Folder") );
		{
			FText NewFolderToolTip;
			if(bHasSinglePathSelected)
			{
				if(bIsValidNewFolderPath)
				{
					NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_CreateIn", "Create a new folder in {0}."), FText::FromString(FirstSelectedPath));
				}
				else
				{
					NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_InvalidPath", "Cannot create new folders in {0}."), FText::FromString(FirstSelectedPath));
				}
			}
			else
			{
				NewFolderToolTip = LOCTEXT("NewFolderTooltip_InvalidNumberOfPaths", "Can only create folders when there is a single path selected.");
			}

			MenuBuilder.AddMenuEntry(
				LOCTEXT("NewFolderLabel", "New Folder"),
				NewFolderToolTip,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.NewFolderIcon"),
				FUIAction(
					FExecuteAction::CreateStatic(&FNewAssetOrClassContextMenu::ExecuteNewFolder, FirstSelectedPath, InOnNewFolderRequested),
					CanExecuteFolderActionsDelegate
					)
				);
		}
		MenuBuilder.EndSection(); //ContentBrowserNewFolder
	}

	// Add Class
	if(InOnNewClassRequested.IsBound())
	{
		FString ClassCreationPath = FirstSelectedPath;
		FText NewClassToolTip;
		if(bHasSinglePathSelected)
		{
			if(bIsValidNewClassPath)
			{
				NewClassToolTip = FText::Format(LOCTEXT("NewClassTooltip_CreateIn", "Create a new class in {0}."), FText::FromString(ClassCreationPath));
			}
			else
			{
				NewClassToolTip = LOCTEXT("NewClassTooltip_CreateInDefault", "Create a new class in your project's source folder.");
				ClassCreationPath.Empty(); // An empty path override will cause the class wizard to use the default project path
			}
		}
		else
		{
			NewClassToolTip = LOCTEXT("NewClassTooltip_InvalidNumberOfPaths", "Can only create classes when there is a single path selected.");
		}

		MenuBuilder.BeginSection("ContentBrowserNewClass", LOCTEXT("ClassMenuHeading", "C++ Class") );
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("NewClassLabel", "New C++ Class..."),
				NewClassToolTip,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.AddCodeToProject"),
				FUIAction(
					FExecuteAction::CreateStatic(&FNewAssetOrClassContextMenu::ExecuteNewClass, ClassCreationPath, InOnNewClassRequested),
					CanExecuteClassActionsDelegate
					)
				);
		}
		MenuBuilder.EndSection(); //ContentBrowserNewClass
	}

	// Import
	if (InOnImportAssetRequested.IsBound() && !FirstSelectedPath.IsEmpty())
	{
		MenuBuilder.BeginSection( "ContentBrowserImportAsset", LOCTEXT( "ImportAssetMenuHeading", "Import Asset" ) );
		{
			MenuBuilder.AddMenuEntry(
				FText::Format( LOCTEXT( "ImportAsset", "Import to {0}..." ), FText::FromString( FirstSelectedPath ) ),
				LOCTEXT( "ImportAssetTooltip_NewAssetOrClass", "Imports an asset from file to this folder." ),
				FSlateIcon( FEditorStyle::GetStyleSetName(), "ContentBrowser.ImportIcon" ),
				FUIAction(
					FExecuteAction::CreateStatic( &FNewAssetOrClassContextMenu::ExecuteImportAsset, InOnImportAssetRequested, FirstSelectedPath ),
					CanExecuteAssetActionsDelegate
					)
				);
		}
		MenuBuilder.EndSection();
	}

	
	if (InOnNewAssetRequested.IsBound())
	{
		// Add Basic Asset
		MenuBuilder.BeginSection("ContentBrowserNewBasicAsset", LOCTEXT("CreateBasicAssetsMenuHeading", "Create Basic Asset") );
		{
			CreateNewAssetMenuCategory(
				MenuBuilder, 
				EAssetTypeCategories::Basic, 
				FirstSelectedPath, 
				InOnNewAssetRequested, 
				CanExecuteAssetActionsDelegate
				);
		}
		MenuBuilder.EndSection(); //ContentBrowserNewBasicAsset

		// Add Advanced Asset
		MenuBuilder.BeginSection("ContentBrowserNewAdvancedAsset", LOCTEXT("CreateAdvancedAssetsMenuHeading", "Create Advanced Asset"));
		{
			FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();

			TArray<FAdvancedAssetCategory> AdvancedAssetCategories;
			AssetToolsModule.Get().GetAllAdvancedAssetCategories(/*out*/ AdvancedAssetCategories);
			AdvancedAssetCategories.Sort([](const FAdvancedAssetCategory& A, const FAdvancedAssetCategory& B) {
				return (A.CategoryName.CompareToCaseIgnored(B.CategoryName) < 0);
			});

			for (const FAdvancedAssetCategory& AdvancedAssetCategory : AdvancedAssetCategories)
			{
				TArray<FFactoryItem> Factories = FindFactoriesInCategory(AdvancedAssetCategory.CategoryType);
				if (Factories.Num() > 0)
				{
					MenuBuilder.AddSubMenu(
						AdvancedAssetCategory.CategoryName,
						FText::GetEmpty(),
						FNewMenuDelegate::CreateStatic(
							&FNewAssetOrClassContextMenu::CreateNewAssetMenuCategory, 
							AdvancedAssetCategory.CategoryType, 
							FirstSelectedPath, 
							InOnNewAssetRequested, 
							FCanExecuteAction() // We handle this at this level, rather than at the sub-menu item level
						),
						FUIAction(
							FExecuteAction(),
							CanExecuteAssetActionsDelegate
						),
						NAME_None,
						EUserInterfaceActionType::Button
						);
				}
			}
		}
		MenuBuilder.EndSection(); //ContentBrowserNewAdvancedAsset
	}
}

void FNewAssetOrClassContextMenu::CreateNewAssetMenuCategory(FMenuBuilder& MenuBuilder, EAssetTypeCategories::Type AssetTypeCategory, FString InPath, FOnNewAssetRequested InOnNewAssetRequested, FCanExecuteAction InCanExecuteAction)
{
	// Find UFactory classes that can create new objects in this category.
	TArray<FFactoryItem> FactoriesInThisCategory = FindFactoriesInCategory(AssetTypeCategory);

	// Sort the list
	struct FCompareFactoryDisplayNames
	{
		FORCEINLINE bool operator()( const FFactoryItem& A, const FFactoryItem& B ) const
		{
			return A.DisplayName.CompareToCaseIgnored(B.DisplayName) < 0;
		}
	};
	FactoriesInThisCategory.Sort( FCompareFactoryDisplayNames() );

	// Add menu entries for each one
	for ( auto FactoryIt = FactoriesInThisCategory.CreateConstIterator(); FactoryIt; ++FactoryIt )
	{
		UFactory* Factory = (*FactoryIt).Factory;
		TWeakObjectPtr<UClass> WeakFactoryClass = Factory->GetClass();

		MenuBuilder.AddMenuEntry(
			FUIAction(
				FExecuteAction::CreateStatic( &FNewAssetOrClassContextMenu::ExecuteNewAsset, InPath, WeakFactoryClass, InOnNewAssetRequested ),
				InCanExecuteAction
				),
			SNew( SFactoryMenuEntry, Factory )
			);
	}
}

void FNewAssetOrClassContextMenu::ExecuteImportAsset( FOnImportAssetRequested InOnInportAssetRequested, FString InPath )
{
	InOnInportAssetRequested.ExecuteIfBound( InPath );
}

void FNewAssetOrClassContextMenu::ExecuteNewAsset(FString InPath, TWeakObjectPtr<UClass> FactoryClass, FOnNewAssetRequested InOnNewAssetRequested)
{
	if ( ensure(FactoryClass.IsValid()) && ensure(!InPath.IsEmpty()) )
	{
		InOnNewAssetRequested.ExecuteIfBound(InPath, FactoryClass);
	}
}

void FNewAssetOrClassContextMenu::ExecuteNewClass(FString InPath, FOnNewClassRequested InOnNewClassRequested)
{
	// An empty path override will cause the class wizard to use the default project path
	InOnNewClassRequested.ExecuteIfBound(InPath);
}

void FNewAssetOrClassContextMenu::ExecuteNewFolder(FString InPath, FOnNewFolderRequested InOnNewFolderRequested)
{
	if(ensure(!InPath.IsEmpty()) )
	{
		InOnNewFolderRequested.ExecuteIfBound(InPath);
	}
}

void FNewAssetOrClassContextMenu::ExecuteGetContent( FOnGetContentRequested InOnGetContentRequested )
{
	InOnGetContentRequested.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
