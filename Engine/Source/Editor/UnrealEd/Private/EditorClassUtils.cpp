// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditorClassUtils.h"
#include "HAL/FileManager.h"
#include "Widgets/Layout/SSpacer.h"
#include "EditorStyleSet.h"
#include "Engine/Blueprint.h"
#include "Editor.h"

#include "IDocumentationPage.h"
#include "IDocumentation.h"
#include "SourceCodeNavigation.h"
#include "Widgets/Input/SHyperlink.h"

FString FEditorClassUtils::GetDocumentationPage(const UClass* Class)
{
	return (Class ? FString::Printf( TEXT("Shared/Types/%s%s"), Class->GetPrefixCPP(), *Class->GetName() ) : FString());
}

FString FEditorClassUtils::GetDocumentationExcerpt(const UClass* Class)
{
	return (Class ? FString::Printf( TEXT("%s%s"), Class->GetPrefixCPP(), *Class->GetName() ) : FString());
}

TSharedRef<SToolTip> FEditorClassUtils::GetTooltip(const UClass* Class)
{
	return (Class ? GetTooltip(Class, Class->GetToolTipText()) : SNew(SToolTip));
}

TSharedRef<SToolTip> FEditorClassUtils::GetTooltip(const UClass* Class, const TAttribute<FText>& OverrideText)
{
	return (Class ? IDocumentation::Get()->CreateToolTip(OverrideText, nullptr, GetDocumentationPage(Class), GetDocumentationExcerpt(Class)) : SNew(SToolTip));
}

FString FEditorClassUtils::GetDocumentationLinkFromExcerpt(const FString& DocLink, const FString DocExcerpt)
{
	FString DocumentationLink;
	TSharedRef<IDocumentation> Documentation = IDocumentation::Get();
	if (Documentation->PageExists(DocLink))
	{
		TSharedRef<IDocumentationPage> ClassDocs = Documentation->GetPage(DocLink, NULL);

		FExcerpt Excerpt;
		if (ClassDocs->GetExcerpt(DocExcerpt, Excerpt))
		{
			FString* FullDocumentationLink = Excerpt.Variables.Find(TEXT("ToolTipFullLink"));
			if (FullDocumentationLink)
			{
				DocumentationLink = *FullDocumentationLink;
			}
		}
	}

	return DocumentationLink;
}


FString FEditorClassUtils::GetDocumentationLink(const UClass* Class, const FString& OverrideExcerpt)
{
	const FString ClassDocsPage = GetDocumentationPage(Class);
	const FString ExcerptSection = (OverrideExcerpt.IsEmpty() ? GetDocumentationExcerpt(Class) : OverrideExcerpt);

	return GetDocumentationLinkFromExcerpt(ClassDocsPage, ExcerptSection);
}


TSharedRef<SWidget> FEditorClassUtils::GetDocumentationLinkWidget(const UClass* Class)
{
	TSharedRef<SWidget> DocLinkWidget = SNullWidget::NullWidget;
	const FString DocumentationLink = GetDocumentationLink(Class);

	if (!DocumentationLink.IsEmpty())
	{
		DocLinkWidget = IDocumentation::Get()->CreateAnchor(DocumentationLink);
	}

	return DocLinkWidget;
}

TSharedRef<SWidget> FEditorClassUtils::GetSourceLink(const UClass* Class, const TWeakObjectPtr<UObject> ObjectWeakPtr)
{
	const FText BlueprintFormat = NSLOCTEXT("SourceHyperlink", "EditBlueprint", "Edit {0}");
	const FText CodeFormat = NSLOCTEXT("SourceHyperlink", "GoToCode", "Open {0}");

	return GetSourceLinkFormatted(Class, ObjectWeakPtr, BlueprintFormat, CodeFormat);
}

TSharedRef<SWidget> FEditorClassUtils::GetSourceLinkFormatted(const UClass* Class, const TWeakObjectPtr<UObject> ObjectWeakPtr, const FText& BlueprintFormat, const FText& CodeFormat)
{
	TSharedRef<SWidget> SourceHyperlink = SNew( SSpacer );
	UBlueprint* Blueprint = (Class ? Cast<UBlueprint>(Class->ClassGeneratedBy) : nullptr);

	if (Blueprint)
	{
		struct Local
		{
			static void OnEditBlueprintClicked( TWeakObjectPtr<UBlueprint> InBlueprint, TWeakObjectPtr<UObject> InAsset )
			{
				if (UBlueprint* BlueprintToEdit = InBlueprint.Get())
				{
					// Set the object being debugged if given an actor reference (if we don't do this before we edit the object the editor wont know we are debugging something)
					if (UObject* Asset = InAsset.Get())
					{
						check(Asset->GetClass()->ClassGeneratedBy == BlueprintToEdit);
						BlueprintToEdit->SetObjectBeingDebugged(Asset);
					}
					// Open the blueprint
					GEditor->EditObject( BlueprintToEdit );
				}
			}
		};

		TWeakObjectPtr<UBlueprint> BlueprintPtr = Blueprint;

		SourceHyperlink = SNew(SHyperlink)
			.Style(FEditorStyle::Get(), "Common.GotoBlueprintHyperlink")
			.OnNavigate_Static(&Local::OnEditBlueprintClicked, BlueprintPtr, ObjectWeakPtr)
			.Text(FText::Format(BlueprintFormat, FText::FromString(Blueprint->GetName())))
			.ToolTipText(NSLOCTEXT("SourceHyperlink", "EditBlueprint_ToolTip", "Click to edit the blueprint"));
	}
	else if (Class && FSourceCodeNavigation::CanNavigateToClass(Class))
	{
		struct Local
		{
			static void OnEditCodeClicked(const UClass* TargetClass)
			{
				FSourceCodeNavigation::NavigateToClass(TargetClass);
			}
		};

		SourceHyperlink = SNew(SHyperlink)
			.Style(FEditorStyle::Get(), "Common.GotoNativeCodeHyperlink")
			.OnNavigate_Static(&Local::OnEditCodeClicked, Class)
			.Text(FText::Format(CodeFormat, FText::FromString((Class->GetName()))))
			.ToolTipText(FText::Format(NSLOCTEXT("SourceHyperlink", "GoToCode_ToolTip", "Click to open this source file in {0}"), FSourceCodeNavigation::GetSuggestedSourceCodeIDE()));
	}

	return SourceHyperlink;
}

UClass* FEditorClassUtils::GetClassFromString(const FString& ClassName)
{
	if(ClassName.IsEmpty() || ClassName == "None")
	{
		return nullptr;
	}

	UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);
	if(!Class)
	{
		Class = LoadObject<UClass>(nullptr, *ClassName);
	}
	return Class;
}
