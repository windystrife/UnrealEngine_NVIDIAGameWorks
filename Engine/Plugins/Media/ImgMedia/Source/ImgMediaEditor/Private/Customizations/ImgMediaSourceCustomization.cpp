// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ImgMediaSourceCustomization.h"

#include "IMediaModule.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"

#include "Styling/CoreStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SDirectoryPicker.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SFilePathPicker.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"


#define LOCTEXT_NAMESPACE "FImgMediaSourceCustomization"


/* IDetailCustomization interface
 *****************************************************************************/

void FImgMediaSourceCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// customize 'File' category
	IDetailCategoryBuilder& FileCategory = DetailBuilder.EditCategory("Sequence");
	{
		// FilePath
		SequencePathProperty = DetailBuilder.GetProperty("SequencePath");
		{
			IDetailPropertyRow& SequencePathRow = FileCategory.AddProperty(SequencePathProperty);

			SequencePathRow
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
									.Text(LOCTEXT("SequencePathPropertyName", "Sequence Path"))
									.ToolTipText(SequencePathProperty->GetToolTipText())
							]

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.Padding(4.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SImage)
									.Image(FCoreStyle::Get().GetBrush("Icons.Warning"))
									.ToolTipText(LOCTEXT("SequencePathWarning", "The selected image sequence will not get packaged, because its path points to a directory outside the project's /Content/Movies/ directory."))
									.Visibility(this, &FImgMediaSourceCustomization::HandleSequencePathWarningIconVisibility)
							]
					]
				.ValueContent()
					.MaxDesiredWidth(0.0f)
					.MinDesiredWidth(125.0f)
					[
						SNew(SFilePathPicker)
							.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
							.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
							.BrowseButtonToolTip(LOCTEXT("SequencePathBrowseButtonToolTip", "Choose a file from this computer"))
							.BrowseDirectory_Lambda([this]() -> FString
							{
								const FString SequencePath = GetSequencePath();
								return !SequencePath.IsEmpty() ? SequencePath : (FPaths::ProjectContentDir() / TEXT("Movies"));
							})
							.FilePath_Lambda([this]() -> FString
							{
								return GetSequencePath();
							})
							.FileTypeFilter_Lambda([]() -> FString
							{
								return TEXT("All files (*.*)|*.*|EXR files (*.exr)|*.exr");
							})
							.OnPathPicked(this, &FImgMediaSourceCustomization::HandleSequencePathPickerPathPicked)
							.ToolTipText(LOCTEXT("SequencePathToolTip", "The path to an image sequence file on this computer"))
					];
		}
	}

	// add 'Proxies' category
	IDetailCategoryBuilder& ProxiesCategory = DetailBuilder.EditCategory("Proxies", LOCTEXT("ProxiesCategoryName", "Proxies"));
	{
		// add 'Proxy Directories' row
		FDetailWidgetRow& PreviewRow = ProxiesCategory.AddCustomRow(LOCTEXT("ProxiesRowFilterString", "Proxy Directories"));

		PreviewRow.WholeRowContent()
			[
				SAssignNew(ProxiesTextBlock, SEditableTextBox)
					.IsReadOnly(true)
			];
	}
}


/* FImgMediaSourceCustomization implementation
 *****************************************************************************/

FString FImgMediaSourceCustomization::GetSequencePath() const
{
	FString FilePath;
	SequencePathProperty->GetChildHandle("Path")->GetValue(FilePath);

	return FilePath;
}


/* FImgMediaSourceCustomization callbacks
 *****************************************************************************/

void FImgMediaSourceCustomization::HandleSequencePathPickerPathPicked(const FString& PickedPath)
{
	// fully expand path & strip optional file name
	const FString FullPath = PickedPath.StartsWith(TEXT("./"))
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir(), PickedPath.RightChop(2))
		: PickedPath;

	const FString FullDir = FPaths::ConvertRelativePathToFull(FPaths::FileExists(FullPath) ? FPaths::GetPath(FullPath) : FullPath);
	const FString FullGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

	FString PickedDir = FullDir;

	if (PickedDir.StartsWith(FullGameContentDir))
	{
		FPaths::MakePathRelativeTo(PickedDir, *FullGameContentDir);
		PickedDir = FString(TEXT("./")) + PickedDir;
	}

	// update property
	TSharedPtr<IPropertyHandle> SequencePathPathProperty = SequencePathProperty->GetChildHandle("Path");
	SequencePathPathProperty->SetValue(PickedDir);
}


EVisibility FImgMediaSourceCustomization::HandleSequencePathWarningIconVisibility() const
{
	FString FilePath;

	if ((SequencePathProperty->GetValue(FilePath) != FPropertyAccess::Success) || FilePath.IsEmpty() || FilePath.Contains(TEXT("://")))
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
