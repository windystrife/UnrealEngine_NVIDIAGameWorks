// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraGraphNodeWriteDataSet.h"
#include "NiagaraNodeInput.h"
#include "NiagaraGraph.h"
#include "Framework/Commands/Commands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "ScopedTransaction.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "NiagaraNodeWriteDataSet.h"
#include "ModuleManager.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "SNiagaraGraphNodeWriteDataSet"


void SNiagaraGraphNodeWriteDataSet::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;

	this->UpdateGraphNode();
}

TSharedRef<SWidget> SNiagaraGraphNodeWriteDataSet::CreateNodeContentArea()
{
	FSinglePropertyParams InitParams;
	InitParams.NamePlacement = EPropertyNamePlacement::Hidden;
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedPtr<ISinglePropertyView> SinglePropView = PropertyEditorModule.CreateSingleProperty(GraphNode, GET_MEMBER_NAME_CHECKED(UNiagaraNodeWriteDataSet, EventName), InitParams);

	TSharedRef<SWidget> ContentAreaWidget = SGraphNode::CreateNodeContentArea();
	TSharedPtr<SVerticalBox> VertContainer = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoWidth()
			.Padding(Settings->GetInputPinPadding())
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EventName","Event Name"))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoWidth()
			[
				SinglePropView.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ContentAreaWidget
		];
	TSharedRef<SWidget> RetWidget = VertContainer.ToSharedRef();
	return RetWidget;
}



#undef LOCTEXT_NAMESPACE