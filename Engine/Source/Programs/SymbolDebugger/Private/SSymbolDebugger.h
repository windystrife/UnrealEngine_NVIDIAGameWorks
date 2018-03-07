// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"

class SSymbolDebugger : public SCompoundWidget
{
public:
	/**
	 *	Text fields for the label/text box pairs
	 */
	enum ESymbolDebuggerTextFields
	{
		/** The crash dump file */
		TextField_CrashDump,
		/** The engine version */
		TextField_EngineVersion,
		/** The changelist */
		TextField_ChangeList,
		/** The source control label */
		TextField_Label,
		/** The platform */
		TextField_Platform,
		/** The locaion of the symbol store */
		TextField_SymbolStore,
		/** The IP of the machine to remote debug on */
		TextField_RemoteDebugIP,
		/** The source control depot to utilize */
		TextField_SourceControlDepot,
		TextField_MAX
	};

	/**
	 *	The different methods of symbol debugging 
	 */
	enum ESymbolDebuggerMethods
	{
		/** A minidump file */
		DebugMethod_CrashDump,
		/** The engine version */
		DebugMethod_EngineVersion,
		/** The changelist number */
		DebugMethod_ChangeList,
		/** The source control label */
		DebugMethod_SourceControlLabel
	};

	/**
	 *	Actions the symbol debugger can perform
	 */
	enum ESymbolDebuggerActions
	{
		/** None - no action */
		DebugAction_None,
		/** Inspect - using the current method, gather any other information */
		DebugAction_Inspect,
		/** Sync - sync the files required for debugging to the symbol store */
		DebugAction_Sync,
		/** Debug - launch the debugger; only valid for CrashDump at this time */
		DebugAction_Debug,
		/** Process - inspect, sync and debug the given CrashDump */
		DebugAction_Process
	};

	/**
	 *	The results of an action
	 */
	enum ESymbolDebuggerActionResults
	{
		/** Task is still going */
		DebugResult_InProgress,
		/** Task completed successfully */
		DebugResult_Success,
		/** Task completed but failed */
		DebugResult_Failure
	};

	/** Delegates used to trigger actions, check for completion */
	DECLARE_DELEGATE_RetVal(ESymbolDebuggerMethods, FSymbolDebuggerGetCurrentMethod);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FSymbolDebuggerSetCurrentMethod, ESymbolDebuggerMethods);
	DECLARE_DELEGATE_RetVal(FString, FSymbolDebuggerGetMethodText)
	DECLARE_DELEGATE_RetVal_OneParam(bool, FSymbolDebuggerSetMethodText, const FString&);

	DECLARE_DELEGATE_RetVal_OneParam(bool, FSymbolDebuggerOnFileOpen, TSharedRef<SWidget>);

	DECLARE_DELEGATE_RetVal_OneParam(FString, FSymbolDebuggerGetTextField, ESymbolDebuggerTextFields);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FSymbolDebuggerSetTextField, ESymbolDebuggerTextFields, const FString&);

	DECLARE_DELEGATE_RetVal(ESymbolDebuggerActions, FSymbolDebuggerGetCurrentAction);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FSymbolDebuggerIsActionEnabled, ESymbolDebuggerActions);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FSymbolDebuggerOnAction, ESymbolDebuggerActions);

	DECLARE_DELEGATE_RetVal(FString, FSymbolDebuggerOnGetStatusText)

	DECLARE_DELEGATE_RetVal_OneParam(ESymbolDebuggerActionResults, FSymbolDebuggerHasActionCompleted, ESymbolDebuggerActions);

	SLATE_BEGIN_ARGS(SSymbolDebugger)
	{
	}

	SLATE_EVENT(FSymbolDebuggerGetCurrentMethod, OnGetCurrentMethod)
	SLATE_EVENT(FSymbolDebuggerSetCurrentMethod, OnSetCurrentMethod)
	SLATE_EVENT(FSymbolDebuggerOnFileOpen, OnFileOpen)
	SLATE_EVENT(FSymbolDebuggerGetTextField, OnGetTextField)
	SLATE_EVENT(FSymbolDebuggerSetTextField, OnSetTextField)
	SLATE_EVENT(FSymbolDebuggerGetMethodText, OnGetMethodText)
	SLATE_EVENT(FSymbolDebuggerSetMethodText, OnSetMethodText)
	SLATE_EVENT(FSymbolDebuggerGetCurrentAction, OnGetCurrentAction)
	SLATE_EVENT(FSymbolDebuggerIsActionEnabled, IsActionEnabled)
	SLATE_EVENT(FSymbolDebuggerOnAction, OnAction)
	SLATE_EVENT(FSymbolDebuggerOnGetStatusText, OnGetStatusText)
	SLATE_EVENT(FSymbolDebuggerHasActionCompleted, HasActionCompleted)

	SLATE_END_ARGS()

	/**
	 *	Generate the button for the given action
	 *
	 *	@param	InAction		The debugger action button to generate
	 *
	 *	@return	SWidget			The button...
	 */
	TSharedRef<SWidget> GenerateActionButton(ESymbolDebuggerActions InAction);

	/**
	 *	Generate the action buttons
	 *
	 *	@return	SWidget			The buttons
	 */
	TSharedRef<SWidget> GenerateActionButtons();

	/**
	 *	Generate a label/text box pair for the given Debugger text field
	 *
	 *	@param	InTextField		The text field to generate the label/textbox pair for
	 *
	 *	@return	SWidget			The label/textbox pair...
	 */
	TSharedRef<SWidget> GenerateLabelTextBoxPair(ESymbolDebuggerTextFields InTextField);

	/**
	 *	Generate the radio button for the given method
	 *
	 *	@param	InMethodName	The name of the method to display on the radio button
	 *	@param	InMethod		The debugger method associated with the radio button
	 *
	 *	@return	SWidget			The radio button
	 */
	TSharedRef<SWidget> GenerateMethodButton(const FText& InMethodName, ESymbolDebuggerMethods InMethod);

	/**
	 *	Generate the radio buttons for selecting the current method
	 *
	 *	@return	SWidget			The radio buttons
	 */
	TSharedRef<SWidget> GenerateMethodButtons();

	/**
	 *	Generate the method input widgets
	 *
	 *	@return	SWidget			The widgets
	 */
	TSharedRef<SWidget> GenerateMethodInputWidgets();

	/**
	 *	Generate the status widgets
	 *
	 *	@return	SWidget		The widgets
	 */
	TSharedRef<SWidget> GenerateStatusWidgets();

	/**
	 *	Construct this Slate ui
	 */
	void Construct(const FArguments& InArgs);

	/**
	 *	Get the current method 
	 *
	 *	@return	ESymbolDebuggerMethods	The current method
	 */
	ESymbolDebuggerMethods GetSymbolDebuggertMethod() const;

	/**
	 *	Get the current action being handled
	 *
	 *	@return	ESymbolDebuggerActions		The current action
	 */
	ESymbolDebuggerActions GetSymbolDebuggerAction() const;

	/**
	 *	Is the FileOpen button visible?
	 *
	 *	@return	EVisibility		The visibility of the button at this time
	 */
	EVisibility IsFileOpenVisible() const;

	/**
	 *	Handle the FileOpen button being clicked
	 *
	 *	@return	FReply			How the function handled the click
	 */
	FReply FileOpenClicked();

	/**
	 *	Is the given method selected in the Method radio buttons?
	 *
	 *	@param	InMethod				The debugger method to check
	 *
	 *	@return	ECheckBoxState			The state of the button for the given method
	 */
	ECheckBoxState IsMethodChecked(ESymbolDebuggerMethods InMethod) const;

	/**
	 *	Handle the Method radio buttons changing
	 *
	 *	@param	InNewRadioState			The new state of the radio button
	 *	@param	InMethodThatChanged		The method radio button that changed
	 */
	void OnMethodChanged(ECheckBoxState InNewRadioState, ESymbolDebuggerMethods InMethodThatChanged);

	/**
	 *	Is the textbox enabled for the given TextField?
	 *
	 *	@param	InTextField		The textfield of interest
	 *
	 *	@return	bool			true if enabled, false if not
	 */
	bool IsTextEnabled(ESymbolDebuggerTextFields InTextField) const;

	/** 
	 *	Is the given text field visible?
	 *
	 *	@param	InTextField		The textfield of interest
	 *
	 *	@return	EVisibility		The visibility of the status widgets at this time
	 */
	EVisibility IsTextVisible(ESymbolDebuggerTextFields InTextField) const;

	/**
	 *	Get the text for the given TextField textbox
	 *
	 *	@param	InTextField		The textfield of interest
	 *
	 *	@return	FString			The contents of the text box, "" if not found
	 */
	FText OnGetText(ESymbolDebuggerTextFields InTextField) const;

	/**
	 *	Callback for a TextField textbox edit to be committed
	 *
	 *	@param	InNewString		The edited string
	 *	@param	InCommitInfo	The TextCommit type that occurred (unused at this time)
	 *	@param	InTextField		The textfield of interest
	 */
	void OnTextCommited(const FText& InNewString, ETextCommit::Type /*InCommitInfo*/, ESymbolDebuggerTextFields InTextField);

	/**
	 *	Get the shared 'Method' text
	 *
	 *	@return	FString		The method text
	 */
	FText OnGetMethodText() const;

	/**
	 *	Callback for the method text edit to be committed
	 *
	 *	@param	InNewString		The edited string
	 *	@param	InCommitInfo	The TextCommit type that occurred (unused at this time)
	 */
	void OnMethodTextCommited(const FText& InNewString, ETextCommit::Type InCommitInfo);

	/**
	 *	Is the given Action button enabled?
	 *
	 *	@param	InAction		The action of interest
	 *
	 *	@return	bool			true if enabled, false if not
	 */
	bool IsActionEnabled(ESymbolDebuggerActions InAction) const;

	/**
	 *	Handle for an action button being clicked
	 *
	 *	@param	InAction		The action that was clicked
	 *
	 *	@return	FReply			How the function handled the click
	 */
	FReply OnActionClicked(ESymbolDebuggerActions InAction);

	/** 
	 *	Handler for DragOver of an item on this window
	 */
	FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

	/** 
	 *	Handler for Drop of an item on this window
	 */
	FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

	/** 
	 *	Are the status widgets visible?
	 *
	 *	@return	EVisibility		The visibility of the status widgets at this time
	 */
	EVisibility AreStatusWidgetsVisible() const;

	/**
	 *	Get the status text
	 *
	 *	@return	FString		The status text
	 */
	FText OnGetStatusText() const;

	/**
	 *	Is the status text enabled?
	 *
	 *	@return	bool		true if enabled, false if not
	 */
	bool IsStatusTextEnabled() const;

protected:
	/** Method Delegates */
	FSymbolDebuggerGetCurrentMethod Delegate_OnGetCurrentMethod;
	FSymbolDebuggerSetCurrentMethod Delegate_OnSetCurrentMethod;
	FSymbolDebuggerGetMethodText Delegate_OnGetMethodText;
	FSymbolDebuggerSetMethodText Delegate_OnSetMethodText;

	/** FileOpen Delegates */
	FSymbolDebuggerOnFileOpen Delegate_OnFileOpen;

	/** TextField Delegates */
	FSymbolDebuggerGetTextField Delegate_OnGetTextField;
	FSymbolDebuggerSetTextField Delegate_OnSetTextField;

	/** Action Delegates */
	FSymbolDebuggerGetCurrentAction Delegate_OnGetCurrentAction;
	FSymbolDebuggerIsActionEnabled Delegate_IsActionEnabled;
	FSymbolDebuggerOnAction Delegate_OnAction;

	/** Status Delegates */
	FSymbolDebuggerOnGetStatusText Delegate_OnGetStatusText;
	FSymbolDebuggerHasActionCompleted Delegate_HasActionCompleted;
};
