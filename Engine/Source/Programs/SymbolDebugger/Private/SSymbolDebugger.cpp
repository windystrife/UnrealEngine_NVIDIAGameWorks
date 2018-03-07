// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSymbolDebugger.h"
#include "SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"


TSharedRef<SWidget> SSymbolDebugger::GenerateActionButton(ESymbolDebuggerActions InAction)
{
	FText ActionName;
	switch (InAction)
	{
	case DebugAction_Inspect:
		ActionName = NSLOCTEXT("SymbolDebugger", "InspectActionName", "Inspect");
		break;
	case DebugAction_Sync:
		ActionName = NSLOCTEXT("SymbolDebugger", "SyncActionName", "Sync");
		break;
	case DebugAction_Debug:
		ActionName = NSLOCTEXT("SymbolDebugger", "DebugActionName", "Debug");
		break;
	}

	return 
		SNew(SButton)
		.Text(ActionName)
		.IsEnabled(this, &SSymbolDebugger::IsActionEnabled, InAction)
		.OnClicked(this, &SSymbolDebugger::OnActionClicked, InAction);
}

TSharedRef<SWidget> SSymbolDebugger::GenerateActionButtons()
{
	return 
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2)
		[
			GenerateActionButton(DebugAction_Inspect)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2)
		[
			GenerateActionButton(DebugAction_Sync)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2)
		[
			GenerateActionButton(DebugAction_Debug)
		];
}

TSharedRef<SWidget> SSymbolDebugger::GenerateLabelTextBoxPair(ESymbolDebuggerTextFields InTextField)
{
	FText LabelText;
	switch (InTextField)
	{
	case TextField_CrashDump:
		LabelText = NSLOCTEXT("SymbolDebugger", "CrashDumpLabel", "Crash Dump:");
		break;
	case TextField_ChangeList:
		LabelText = NSLOCTEXT("SymbolDebugger", "ChangelistLabel", "Changelist #:");
		break;
	case TextField_Label:
		LabelText = NSLOCTEXT("SymbolDebugger", "Label", "Label:");
		break;
	case TextField_Platform:
		LabelText = NSLOCTEXT("SymbolDebugger", "PlatformLabel", "Platform:");
		break;
	case TextField_EngineVersion:
		LabelText = NSLOCTEXT("SymbolDebugger", "EngineVersionLabel", "Engine Ver:");
		break;
	case TextField_SymbolStore:
		LabelText = NSLOCTEXT("SymbolDebugger", "SymbolStoreLabel", "Symbol Store:");
		break;
	case TextField_RemoteDebugIP:
		LabelText = NSLOCTEXT("SymbolDebugger", "RemoteIPLabel", "Remote IP:");
		break;
	case TextField_SourceControlDepot:
		LabelText = NSLOCTEXT("SymbolDebugger", "DepotNameLabel", "Depot Name:");
		break;
	default:
		break;
	}

	check( !LabelText.IsEmpty() );

	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.Padding(2)
		.FillWidth(.3)
		[
			SNew(STextBlock) 
			.Text(LabelText)
			.Visibility(this, &SSymbolDebugger::IsTextVisible, InTextField)
		]
		+ SHorizontalBox::Slot()
		.Padding(2)
		.FillWidth(.7)
		[
			SNew(SEditableTextBox) 
			.Text(this, &SSymbolDebugger::OnGetText, InTextField)
			.OnTextCommitted(this, &SSymbolDebugger::OnTextCommited, InTextField)
			.IsEnabled(this, &SSymbolDebugger::IsTextEnabled, InTextField)
			.Visibility(this, &SSymbolDebugger::IsTextVisible, InTextField)
		];
}

TSharedRef<SWidget> SSymbolDebugger::GenerateMethodButton(const FText& InMethodName, ESymbolDebuggerMethods InMethod)
{
	return
		SNew(SCheckBox)
		.Style(FCoreStyle::Get(), "RadioButton")
		.IsChecked(this, &SSymbolDebugger::IsMethodChecked, InMethod)
		.OnCheckStateChanged(this, &SSymbolDebugger::OnMethodChanged, InMethod)
		[
			SNew(STextBlock).Text(InMethodName)
		];
}

TSharedRef<SWidget> SSymbolDebugger::GenerateMethodButtons()
{
	return 
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2)
		[
			GenerateMethodButton(NSLOCTEXT("SymbolDebugger", "CrashDumpButton", "CrashDump"), DebugMethod_CrashDump)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2)
		[
			GenerateMethodButton(NSLOCTEXT("SymbolDebugger", "EngineVersionButton", "EngineVersion"), DebugMethod_EngineVersion)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2)
		[
			GenerateMethodButton(NSLOCTEXT("SymbolDebugger", "ChangelistButton", "Changelist"), DebugMethod_ChangeList)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2)
		[
			GenerateMethodButton(NSLOCTEXT("SymbolDebugger", "SourceLabelButton", "SourceLabel"), DebugMethod_SourceControlLabel)
		];
}

TSharedRef<SWidget> SSymbolDebugger::GenerateMethodInputWidgets()
{
	return 
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(this, &SSymbolDebugger::OnGetMethodText)
			.OnTextCommitted(this, &SSymbolDebugger::OnMethodTextCommited)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2)
		[
			SNew(SButton)
			.Text(NSLOCTEXT("SymbolDebugger", "OpenFileButtonLabel", "..."))
			.Visibility(this, &SSymbolDebugger::IsFileOpenVisible)
			.OnClicked(this, &SSymbolDebugger::FileOpenClicked)
		];
}

TSharedRef<SWidget> SSymbolDebugger::GenerateStatusWidgets()
{
	return
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(.25)
		.HAlign(HAlign_Right)
		[
			SNew(SCircularThrobber)
			.Visibility( this, &SSymbolDebugger::AreStatusWidgetsVisible)
		]
		+ SHorizontalBox::Slot()
		.Padding(2)
		.FillWidth(.75)
		[
			SNew(SEditableTextBox) 
			.Text(this, &SSymbolDebugger::OnGetStatusText)
			.IsEnabled(this, &SSymbolDebugger::IsStatusTextEnabled)
			.Visibility(this, &SSymbolDebugger::AreStatusWidgetsVisible)
		];
}

void SSymbolDebugger::Construct(const FArguments& InArgs)
{
	// Default to crash dump
	Delegate_OnGetCurrentMethod = InArgs._OnGetCurrentMethod;
	Delegate_OnSetCurrentMethod = InArgs._OnSetCurrentMethod;

	Delegate_OnFileOpen = InArgs._OnFileOpen;

	Delegate_OnGetTextField = InArgs._OnGetTextField;
	Delegate_OnSetTextField = InArgs._OnSetTextField;

	Delegate_OnGetMethodText = InArgs._OnGetMethodText;
	Delegate_OnSetMethodText = InArgs._OnSetMethodText;

	Delegate_OnGetCurrentAction = InArgs._OnGetCurrentAction;
	Delegate_IsActionEnabled = InArgs._IsActionEnabled;
	Delegate_OnAction = InArgs._OnAction;

	Delegate_OnGetStatusText = InArgs._OnGetStatusText;

	Delegate_HasActionCompleted = InArgs._HasActionCompleted;

	// All of these delegates are required for proper operation...
	checkf(Delegate_OnGetCurrentMethod.IsBound(), TEXT("OnGetCurrentMethod must be bound!"));
	checkf(Delegate_OnSetCurrentMethod.IsBound(), TEXT("OnSetCurrentMethod must be bound!"));
	checkf(Delegate_OnFileOpen.IsBound(), TEXT("OnFileOpen must be bound!"));
	checkf(Delegate_OnGetTextField.IsBound(), TEXT("OnGetTextField must be bound!"));
	checkf(Delegate_OnSetTextField.IsBound(), TEXT("OnSetTextField must be bound!"));
	checkf(Delegate_OnGetMethodText.IsBound(), TEXT("OnGetMethodText must be bound!"));
	checkf(Delegate_OnSetMethodText.IsBound(), TEXT("OnSetMethodText must be bound!"));
	checkf(Delegate_OnGetCurrentAction.IsBound(), TEXT("OnGetCurrentAction must be bound!"));
	checkf(Delegate_IsActionEnabled.IsBound(), TEXT("IsActionEnabled must be bound!"));
	checkf(Delegate_OnAction.IsBound(), TEXT("OnAction must be bound!"));
	checkf(Delegate_OnGetStatusText.IsBound(), TEXT("OnGetStatusText must be bound!"));
	checkf(Delegate_HasActionCompleted.IsBound(), TEXT("HasActionCompleted must be bound!"));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				GenerateMethodButtons()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				GenerateMethodInputWidgets()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
	 		[
	 			GenerateLabelTextBoxPair(TextField_SymbolStore)
	 		]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				GenerateLabelTextBoxPair(TextField_SourceControlDepot)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				GenerateLabelTextBoxPair(TextField_RemoteDebugIP)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				GenerateLabelTextBoxPair(TextField_Platform)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				GenerateLabelTextBoxPair(TextField_EngineVersion)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				GenerateLabelTextBoxPair(TextField_ChangeList)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				GenerateLabelTextBoxPair(TextField_Label)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			.HAlign(HAlign_Center)
			[
				GenerateActionButtons()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			.HAlign(HAlign_Center)
			[
				GenerateStatusWidgets()
			]
		]
	];
}

SSymbolDebugger::ESymbolDebuggerMethods SSymbolDebugger::GetSymbolDebuggertMethod() const
{
	return Delegate_OnGetCurrentMethod.Execute();
}

SSymbolDebugger::ESymbolDebuggerActions SSymbolDebugger::GetSymbolDebuggerAction() const
{
	return Delegate_OnGetCurrentAction.Execute();
}

EVisibility SSymbolDebugger::IsFileOpenVisible() const
{
	if (GetSymbolDebuggertMethod() == DebugMethod_CrashDump)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FReply SSymbolDebugger::FileOpenClicked()
{
	if (Delegate_OnFileOpen.Execute(AsShared()) == true)
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

ECheckBoxState SSymbolDebugger::IsMethodChecked(ESymbolDebuggerMethods InMethod) const
{
	if (GetSymbolDebuggertMethod() == InMethod)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

void SSymbolDebugger::OnMethodChanged(ECheckBoxState InNewRadioState, ESymbolDebuggerMethods InMethodThatChanged)
{
	if (InNewRadioState == ECheckBoxState::Checked)
	{
		Delegate_OnSetCurrentMethod.Execute(InMethodThatChanged);
	}
}

bool SSymbolDebugger::IsTextEnabled(ESymbolDebuggerTextFields InTextField) const
{
	bool bIsEnabled = false;
	switch (InTextField)
	{
	case SSymbolDebugger::TextField_CrashDump:
	case SSymbolDebugger::TextField_SymbolStore:
	case SSymbolDebugger::TextField_Label:
	case SSymbolDebugger::TextField_ChangeList:
	case SSymbolDebugger::TextField_EngineVersion:
		// Never enabled
		break;
	case SSymbolDebugger::TextField_SourceControlDepot:
	case SSymbolDebugger::TextField_RemoteDebugIP:
		// Always enabled
		bIsEnabled = true;
		break;
	case SSymbolDebugger::TextField_Platform:
		{
			// Only enabled for EngineVersion, Changelist, and Label
			if (GetSymbolDebuggertMethod() != SSymbolDebugger::DebugMethod_CrashDump)
			{
				bIsEnabled = true;
			}
		}
		break;
	}

	return bIsEnabled;
}

EVisibility SSymbolDebugger::IsTextVisible(ESymbolDebuggerTextFields InTextField) const
{
	// For now, just hide the remote IP.
	if (InTextField == TextField_RemoteDebugIP)
	{
		return EVisibility::Collapsed;
	}
	return EVisibility::Visible;
}

FText SSymbolDebugger::OnGetText(ESymbolDebuggerTextFields InTextField) const
{
	return FText::FromString( Delegate_OnGetTextField.Execute(InTextField) );
}

void SSymbolDebugger::OnTextCommited(const FText& InNewString, ETextCommit::Type /*InCommitInfo*/, ESymbolDebuggerTextFields InTextField)
{
	Delegate_OnSetTextField.Execute(InTextField, InNewString.ToString());
}

FText SSymbolDebugger::OnGetMethodText() const
{
	return FText::FromString( Delegate_OnGetMethodText.Execute() );
}

void SSymbolDebugger::OnMethodTextCommited(const FText& InNewString, ETextCommit::Type InCommitInfo)
{
	Delegate_OnSetMethodText.Execute(InNewString.ToString());
}

bool SSymbolDebugger::IsActionEnabled(ESymbolDebuggerActions InAction) const
{
	return Delegate_IsActionEnabled.Execute(InAction);
}

FReply SSymbolDebugger::OnActionClicked(ESymbolDebuggerActions InAction)
{
	if (Delegate_OnAction.Execute(InAction) == true)
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SSymbolDebugger::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (GetSymbolDebuggerAction() == DebugAction_None)
	{
		TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
		if (DragDropOp.IsValid())
		{
			if (DragDropOp->HasFiles())
			{
				if (DragDropOp->GetFiles().Num() == 1)
				{
					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SSymbolDebugger::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (GetSymbolDebuggerAction() == DebugAction_None)
	{
		// Handle drag drop for import
		TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
		if (DragDropOp.IsValid())
		{
			if (DragDropOp->HasFiles())
			{
				// Set the current method to CrashDump as that is the only thing we support dropping for
				Delegate_OnSetCurrentMethod.Execute(DebugMethod_CrashDump);

				// Get the file
				const TArray<FString>& DroppedFiles = DragDropOp->GetFiles();
				// For now, only allow a single file.
				for (int32 FileIdx = 0; FileIdx < 1/*DroppedFiles.Num()*/; FileIdx++)
				{
					FString DroppedFile = DroppedFiles[FileIdx];

					// Set the crash dump name
					Delegate_OnSetMethodText.Execute(DroppedFile);

					// Launch the process action
					if (Delegate_OnAction.Execute(DebugAction_Process) == true)
					{
						return FReply::Handled();
					}
				}
			}
		}
	}

	return FReply::Unhandled();
}

EVisibility SSymbolDebugger::AreStatusWidgetsVisible() const
{
	if (GetSymbolDebuggerAction() != DebugAction_None)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

FText SSymbolDebugger::OnGetStatusText() const
{
	return FText::FromString( *Delegate_OnGetStatusText.Execute() );
}

bool SSymbolDebugger::IsStatusTextEnabled() const
{
	return false;
}
