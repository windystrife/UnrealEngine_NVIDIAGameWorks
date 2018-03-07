// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Customizations/FileMediaSourceCustomization.h"
#include "Misc/Paths.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SFilePathPicker.h"
#include "Widgets/Images/SImage.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IMediaModule.h"


#define LOCTEXT_NAMESPACE "FFileMediaSourceCustomization"


/* IDetailCustomization interface
 *****************************************************************************/

void FFileMediaSourceCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// customize 'File' category
	IDetailCategoryBuilder& FileCategory = DetailBuilder.EditCategory("File");
	{
		// FilePath
		FilePathProperty = DetailBuilder.GetProperty("FilePath");
		{
			IDetailPropertyRow& FilePathRow = FileCategory.AddProperty(FilePathProperty);

			FilePathRow
				.ShowPropertyButtons(false)
				.CustomWidget()
				.NameContent()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Font(IDetailLayoutBuilder::GetDetailFont())
									.Text(LOCTEXT("FilePathPropertyName", "File Path"))
									.ToolTipText(FilePathProperty->GetToolTipText())
							]

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.Padding(4.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SImage)
									.Image(FCoreStyle::Get().GetBrush("Icons.Warning"))
									.ToolTipText(LOCTEXT("FilePathWarning", "The selected media file will not get packaged, because its path points to a file outside the project's /Content/Movies/ directory."))
									.Visibility(this, &FFileMediaSourceCustomization::HandleFilePathWarningIconVisibility)
							]
					]
				.ValueContent()
					.MaxDesiredWidth(0.0f)
					.MinDesiredWidth(125.0f)
					[
						SNew(SFilePathPicker)
							.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
							.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
							.BrowseButtonToolTip(LOCTEXT("FilePathBrowseButtonToolTip", "Choose a file from this computer"))
							.BrowseDirectory(FPaths::ProjectContentDir() / TEXT("Movies"))
							.FilePath(this, &FFileMediaSourceCustomization::HandleFilePathPickerFilePath)
							.FileTypeFilter(this, &FFileMediaSourceCustomization::HandleFilePathPickerFileTypeFilter)
							.OnPathPicked(this, &FFileMediaSourceCustomization::HandleFilePathPickerPathPicked)
							.ToolTipText(LOCTEXT("FilePathToolTip", "The path to a media file on this computer"))
					];
		}
	}
}


/* FFileMediaSourceCustomization callbacks
 *****************************************************************************/

FString FFileMediaSourceCustomization::HandleFilePathPickerFilePath() const
{
	FString FilePath;
	FilePathProperty->GetValue(FilePath);

	return FilePath;
}


FString FFileMediaSourceCustomization::HandleFilePathPickerFileTypeFilter() const
{
	FString Filter = TEXT("All files (*.*)|*.*");

	auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		return Filter;
	}

/*	FMediaFileTypes FileTypes;
	MediaModule->GetSupportedFileTypes(FileTypes);

	if (FileTypes.Num() == 0)
	{
		return Filter;
	}

	FString AllExtensions;
	FString AllFilters;
			
	for (auto& Format : FileTypes)
	{
		if (!AllExtensions.IsEmpty())
		{
			AllExtensions += TEXT(";");
		}

		AllExtensions += TEXT("*.") + Format.Key;
		AllFilters += TEXT("|") + Format.Value.ToString() + TEXT(" (*.") + Format.Key + TEXT(")|*.") + Format.Key;
	}

	Filter = TEXT("All movie files (") + AllExtensions + TEXT(")|") + AllExtensions + TEXT("|") + Filter + AllFilters;
*/
	return Filter;
}


void FFileMediaSourceCustomization::HandleFilePathPickerPathPicked(const FString& PickedPath)
{
	if (PickedPath.IsEmpty() || PickedPath.StartsWith(TEXT("./")))
	{
		FilePathProperty->SetValue(PickedPath);
	}
	else
	{	
		FString FullPath = FPaths::ConvertRelativePathToFull(PickedPath);
		const FString FullGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

		if (FullPath.StartsWith(FullGameContentDir))
		{
			FPaths::MakePathRelativeTo(FullPath, *FullGameContentDir);
			FullPath = FString(TEXT("./")) + FullPath;
		}

		FilePathProperty->SetValue(FullPath);
	}
}


EVisibility FFileMediaSourceCustomization::HandleFilePathWarningIconVisibility() const
{
	FString FilePath;

	if ((FilePathProperty->GetValue(FilePath) != FPropertyAccess::Success) || FilePath.IsEmpty() || FilePath.Contains(TEXT("://")))
	{
		return EVisibility::Hidden;
	}

	const FString FullMoviesPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies"));
	const FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::IsRelative(FilePath) ? FPaths::ProjectContentDir() / FilePath : FilePath);

	if (FullPath.StartsWith(FullMoviesPath))
	{
		if (FPaths::FileExists(FullPath))
		{
			return EVisibility::Hidden;
		}

		// file doesn't exist
		return EVisibility::Visible;
	}

	// file not inside Movies folder
	return EVisibility::Visible;
}


#undef LOCTEXT_NAMESPACE
