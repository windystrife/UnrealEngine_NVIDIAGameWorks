#include "BlastImportOptionWindow.h"
#include "IDocumentation.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "HAL/FileManager.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Blast"

class IDetailLayoutBuilder;
class IDetailPropertyRow;
class IPropertyHandle;


void SBlastImportOptionWindow::Construct(const FArguments& InArgs)
{
	ImportUI = InArgs._ImportUI;
	WidgetWindow = InArgs._WidgetWindow;

	check(ImportUI);

	TSharedPtr<SBox> InspectorBoxBlast;
	TSharedPtr<SBox> InspectorBoxFBX;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
					.Text(LOCTEXT("Import_CurrentFileTitle", "Current File: "))
				]
				+SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
					.Text(InArgs._FullPath)
				]
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1)
		.Padding(2)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(InspectorBoxBlast, SBox)
			]
			+ SScrollBox::Slot()
			[
				SAssignNew(InspectorBoxFBX, SBox)
			]
		]
		+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(2)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2)
				+ SUniformGridPanel::Slot(0, 0)
				[
					IDocumentation::Get()->CreateAnchor(FString("Blast/ImportOptions"))
				]
				+ SUniformGridPanel::Slot(1, 0)
					[
						SAssignNew(ImportButton, SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("BlastImportOptionWindow_Import", "Import"))
						.IsEnabled(this, &SBlastImportOptionWindow::CanImport)
						.OnClicked(this, &SBlastImportOptionWindow::OnImport)
					]
				+ SUniformGridPanel::Slot(2, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("BlastImportOptionWindow_Cancel", "Cancel"))
						.ToolTipText(LOCTEXT("BlastImportOptionWindow_Cancel_ToolTip", "Cancels importing this Blast asset file"))
						.OnClicked(this, &SBlastImportOptionWindow::OnCancel)
					]
			]
	];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	
	//Creating a single inspector view seems to now render the UFbxImportUI correctly
	TSharedPtr<IDetailsView> DetailsViewBlast = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	InspectorBoxBlast->SetContent(DetailsViewBlast->AsShared());
	DetailsViewBlast->SetObject(ImportUI);

	TSharedPtr<IDetailsView> DetailsViewFBX = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	InspectorBoxFBX->SetContent(DetailsViewFBX->AsShared());

	DetailsViewFBX->SetObject(ImportUI->FBXImportUI);
}

bool SBlastImportOptionWindow::CanImport() const
{
	FText Unused;
	return !ImportUI->ImportOptions.RootName.IsNone() && ImportUI->ImportOptions.RootName.IsValidObjectName(Unused)
		&& IFileManager::Get().FileExists(*ImportUI->ImportOptions.SkeletalMeshPath.FilePath) && ImportUI->FBXImportUI->MeshTypeToImport == FBXIT_SkeletalMesh;
	
}

#undef LOCTEXT_NAMESPACE
