// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FontFaceDetailsCustomization.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/Paths.h"
#include "FontEditorModule.h"
#include "DesktopPlatformModule.h"
#include "Engine/FontFace.h"
#include "EditorStyleSet.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "ScopedTransaction.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "EditorFontGlyphs.h"
#include "EditorDirectories.h"
#include "Misc/FileHelper.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "FontFaceDetailsCustomization"

void FFontFaceDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingEdited);

	TSharedPtr<IPropertyHandle> SourceFilenamePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFontFace, SourceFilename));
	SourceFilenamePropertyHandle->MarkHiddenByCustomization();

	IDetailCategoryBuilder& FontFaceCategory = DetailBuilder.EditCategory("FontFace");

	// Source Filename
	{
		FontFaceCategory.AddCustomRow(SourceFilenamePropertyHandle->GetPropertyDisplayName())
			.NameContent()
			[
				SourceFilenamePropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.VAlign(VAlign_Center)
			.MinDesiredWidth(TOptional<float>())
			.MaxDesiredWidth(TOptional<float>())
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(DetailBuilder.GetDetailFont())
					.Text(this, &FFontFaceDetailsCustomization::GetFontDisplayName)
					.ToolTipText(this, &FFontFaceDetailsCustomization::GetFontDisplayToolTip)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("FontFilePathPickerToolTip", "Choose a font file from this computer"))
					.OnClicked(this, &FFontFaceDetailsCustomization::OnBrowseFontPath)
					.ContentPadding(2.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
						.Text(FEditorFontGlyphs::Folder_Open)
					]
				]
			];
	}
}

FText FFontFaceDetailsCustomization::GetFontDisplayName() const
{
	if (ObjectsBeingEdited.Num() == 1)
	{
		const UFontFace* FontFace = CastChecked<UFontFace>(ObjectsBeingEdited[0].Get());
		if (FontFace && !FontFace->SourceFilename.IsEmpty())
		{
			return FText::FromString(FPaths::GetCleanFilename(FontFace->SourceFilename));
		}
	}

	return FText::GetEmpty();
}

FText FFontFaceDetailsCustomization::GetFontDisplayToolTip() const
{
	if (ObjectsBeingEdited.Num() == 1)
	{
		const UFontFace* FontFace = CastChecked<UFontFace>(ObjectsBeingEdited[0].Get());
		if (FontFace && !FontFace->SourceFilename.IsEmpty())
		{
			return FText::FromString(FontFace->SourceFilename);
		}
	}

	return FText::GetEmpty();
}

FReply FFontFaceDetailsCustomization::OnBrowseFontPath()
{
	IDesktopPlatform* const DesktopPlatform = FDesktopPlatformModule::Get();

	if (DesktopPlatform)
	{
		const FString DefaultPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);

		TArray<FString> OutFiles;
		if (DesktopPlatform->OpenFileDialog(
			nullptr,
			LOCTEXT("FontPickerTitle", "Choose a font file...").ToString(),
			DefaultPath,
			TEXT(""),
			TEXT("All Font Files (*.ttf, *.otf)|*.ttf;*.otf|TrueType fonts (*.ttf)|*.ttf|OpenType fonts (*.otf)|*.otf"),
			EFileDialogFlags::None,
			OutFiles
			))
		{
			OnFontPathPicked(OutFiles[0]);
		}
	}

	return FReply::Handled();
}

void FFontFaceDetailsCustomization::OnFontPathPicked(const FString& InNewFontFilename)
{
	TArray<uint8> FontData;
	FFileHelper::LoadFileToArray(FontData, *InNewFontFilename);

	const FScopedTransaction Transaction(LOCTEXT("SetFontFile", "Set Font File"));

	for (const TWeakObjectPtr<UObject>& ObjectBeingEdited : ObjectsBeingEdited)
	{
		UFontFace* FontFace = CastChecked<UFontFace>(ObjectBeingEdited.Get());
		if (FontFace)
		{
			FontFace->Modify();
			FontFace->SourceFilename = InNewFontFilename;
			FontFace->FontFaceData = FFontFaceData::MakeFontFaceData(MoveTemp(FontData)); // Make a new instance as the existing one may be being used by the font cache
		}
	}

	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InNewFontFilename));

	FSlateApplication::Get().GetRenderer()->FlushFontCache();
}

#undef LOCTEXT_NAMESPACE
