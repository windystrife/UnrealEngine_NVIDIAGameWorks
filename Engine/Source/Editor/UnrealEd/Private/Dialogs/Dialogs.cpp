// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Dialogs/Dialogs.h"
#include "Misc/MessageDialog.h"
#include "Misc/ConfigCacheIni.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "ObjectTools.h"
#include "DesktopPlatformModule.h"
#include "Widgets/Input/SHyperlink.h"
#include "HAL/PlatformApplicationMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogDialogs, Log, All);

#define LOCTEXT_NAMESPACE "Dialogs"

///////////////////////////////////////////////////////////////////////////////
//
// Local classes.
//
///////////////////////////////////////////////////////////////////////////////

class SChoiceDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SChoiceDialog )	{}
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ATTRIBUTE(FText, Message)	
		SLATE_ATTRIBUTE(float, WrapMessageAt)
		SLATE_ATTRIBUTE(EAppMsgType::Type, MessageType)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		ParentWindow = InArgs._ParentWindow.Get();
		ParentWindow->SetWidgetToFocusOnActivate(SharedThis(this));
		Response = EAppReturnType::Cancel;

		FSlateFontInfo MessageFont( FEditorStyle::GetFontStyle("StandardDialog.LargeFont"));
		MyMessage = InArgs._Message;

		TSharedPtr<SUniformGridPanel> ButtonBox;

		this->ChildSlot
			[	
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							.FillHeight(1.0f)
							.MaxHeight(550)
							.Padding(12.0f)
							[
								SNew(SScrollBox)

								+ SScrollBox::Slot()
									[
										SNew(STextBlock)
											.Text(MyMessage)
											.Font(MessageFont)
											.WrapTextAt(InArgs._WrapMessageAt)
									]
							]

						+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Bottom)
									.Padding(12.0f)
									[
										SNew(SHyperlink)
											.OnNavigate(this, &SChoiceDialog::HandleCopyMessageHyperlinkNavigate)
											.Text( NSLOCTEXT("SChoiceDialog", "CopyMessageHyperlink", "Copy Message") )
											.ToolTipText( NSLOCTEXT("SChoiceDialog", "CopyMessageTooltip", "Copy the text in this message to the clipboard (CTRL+C)") )
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Bottom)
									.Padding(2.f)
									[
										SAssignNew( ButtonBox, SUniformGridPanel )
											.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
											.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
											.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
									]
							]
					]
			];

		int32 SlotIndex = 0;

#define ADD_SLOT(Button)\
		ButtonBox->AddSlot(SlotIndex++,0)\
		[\
		SNew( SButton )\
		.Text( EAppReturnTypeToText(EAppReturnType::Button) )\
		.OnClicked( this, &SChoiceDialog::HandleButtonClicked, EAppReturnType::Button )\
		.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))\
		.HAlign(HAlign_Center)\
		];

		switch ( InArgs._MessageType.Get() )
		{	
		case EAppMsgType::Ok:
			ADD_SLOT(Ok)
			break;
		case EAppMsgType::YesNo:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			break;
		case EAppMsgType::OkCancel:
			ADD_SLOT(Ok)
			ADD_SLOT(Cancel)
			break;
		case EAppMsgType::YesNoCancel:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(Cancel)
			break;
		case EAppMsgType::CancelRetryContinue:
			ADD_SLOT(Cancel)
			ADD_SLOT(Retry)
			ADD_SLOT(Continue)
			break;
		case EAppMsgType::YesNoYesAllNoAll:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(YesAll)
			ADD_SLOT(NoAll)
			break;
		case EAppMsgType::YesNoYesAllNoAllCancel:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(YesAll)
			ADD_SLOT(NoAll)
			ADD_SLOT(Cancel)
			break;
		case EAppMsgType::YesNoYesAll:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(YesAll)
			break;
		default:
			UE_LOG(LogDialogs, Fatal, TEXT("Invalid Message Type"));
		}

#undef ADD_SLOT
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION


	EAppReturnType::Type GetResponse()
	{
		return Response;
	}

	virtual	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		//see if we pressed the Enter or Spacebar keys
		if( InKeyEvent.GetKey() == EKeys::Escape )
		{
			return HandleButtonClicked(EAppReturnType::Cancel);
		}

		if (InKeyEvent.GetKey() == EKeys::C && InKeyEvent.IsControlDown())
		{
			CopyMessageToClipboard();

			return FReply::Handled();
		}

		//if it was some other button, ignore it
		return FReply::Unhandled();
	}

	/** Override the base method to allow for keyboard focus */
	virtual bool SupportsKeyboardFocus() const
	{
		return true;
	}

	/** Converts an EAppReturnType into a localized FText */
	static FText EAppReturnTypeToText(EAppReturnType::Type ReturnType)
	{
		switch(ReturnType)
		{
		case EAppReturnType::No:
			return LOCTEXT("EAppReturnTypeNo", "No");
		case EAppReturnType::Yes:
			return LOCTEXT("EAppReturnTypeYes", "Yes");
		case EAppReturnType::YesAll:
			return LOCTEXT("EAppReturnTypeYesAll", "Yes All");
		case EAppReturnType::NoAll:
			return LOCTEXT("EAppReturnTypeNoAll", "No All");
		case EAppReturnType::Cancel:
			return LOCTEXT("EAppReturnTypeCancel", "Cancel");
		case EAppReturnType::Ok:
			return LOCTEXT("EAppReturnTypeOk", "OK");
		case EAppReturnType::Retry:
			return LOCTEXT("EAppReturnTypeRetry", "Retry");
		case EAppReturnType::Continue:
			return LOCTEXT("EAppReturnTypeContinue", "Continue");
		default:
			return LOCTEXT("MissingType", "MISSING RETURN TYPE");
		}
	}

protected:

	/**
	 * Copies the message text to the clipboard.
	 */
	void CopyMessageToClipboard( )
	{
		FPlatformApplicationMisc::ClipboardCopy( *MyMessage.Get().ToString() );
	}


private:

	// Handles clicking a message box button.
	FReply HandleButtonClicked( EAppReturnType::Type InResponse )
	{
		Response = InResponse;

		ResultCallback.ExecuteIfBound(ParentWindow.ToSharedRef(), Response);


		ParentWindow->RequestDestroyWindow();

		return FReply::Handled();
	}

	// Handles clicking the 'Copy Message' hyper link.
	void HandleCopyMessageHyperlinkNavigate( )
	{
		CopyMessageToClipboard();
	}
		
public:
	/** Callback delegate that is triggered, when the dialog is run in non-modal mode */
	FOnMsgDlgResult ResultCallback;

private:

	EAppReturnType::Type Response;
	TSharedPtr<SWindow> ParentWindow;
	TAttribute<FText> MyMessage;
};


void CreateMsgDlgWindow(TSharedPtr<SWindow>& OutWindow, TSharedPtr<SChoiceDialog>& OutDialog, EAppMsgType::Type InMessageType,
						const FText& InMessage, const FText& InTitle, FOnMsgDlgResult ResultCallback=NULL)
{
	OutWindow = SNew(SWindow)
		.Title(InTitle)
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false).SupportsMaximize(false);

	OutDialog = SNew(SChoiceDialog)
		.ParentWindow(OutWindow)
		.Message(InMessage)
		.WrapMessageAt(512.0f)
		.MessageType(InMessageType);

	OutDialog->ResultCallback = ResultCallback;

	OutWindow->SetContent(OutDialog.ToSharedRef());
}

EAppReturnType::Type OpenMsgDlgInt(EAppMsgType::Type InMessageType, const FText& InMessage, const FText& InTitle)
{
	TSharedPtr<SWindow> MsgWindow = NULL;
	TSharedPtr<SChoiceDialog> MsgDialog = NULL;

	CreateMsgDlgWindow(MsgWindow, MsgDialog, InMessageType, InMessage, InTitle);

	GEditor->EditorAddModalWindow(MsgWindow.ToSharedRef());

	EAppReturnType::Type Response = MsgDialog->GetResponse();

	return Response;
}

TSharedRef<SWindow> OpenMsgDlgInt_NonModal(EAppMsgType::Type InMessageType, const FText& InMessage, const FText& InTitle,
							FOnMsgDlgResult ResultCallback)
{
	TSharedPtr<SWindow> MsgWindow = NULL;
	TSharedPtr<SChoiceDialog> MsgDialog = NULL;

	CreateMsgDlgWindow(MsgWindow, MsgDialog, InMessageType, InMessage, InTitle, ResultCallback);

	FSlateApplication::Get().AddWindow(MsgWindow.ToSharedRef());

	return MsgWindow.ToSharedRef();
}

class SModalDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SModalDialog ){}
		SLATE_ARGUMENT(FText,Message)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		MyMessage = InArgs._Message;
		this->ChildSlot
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			.Padding(5)
			[
				SNew( STextBlock )
				.WrapTextAt(615.0f)	// 400.0f
				.Text( MyMessage )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SButton )
					.Text( NSLOCTEXT("UnrealEd", "Yes", "Yes") )
					.OnClicked( this, &SModalDialog::OnYesClicked )
					.ContentPadding(7)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SButton )
					.Text( NSLOCTEXT("UnrealEd", "No", "No") )
					.OnClicked( this, &SModalDialog::OnNoClicked )
					.ContentPadding(7)
				]
			]
		];
	}

	
	SModalDialog()
	: bUserResponse( false )
	{
	}

	void SetWindow( TSharedPtr<SWindow> InWindow )
	{
		MyWindow = InWindow;
	}

	bool GetResponse() const { return bUserResponse; }
	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		if (InKeyEvent.GetKey() == EKeys::C && InKeyEvent.IsControlDown())
		{
			FPlatformApplicationMisc::ClipboardCopy( *MyMessage.Get().ToString() );
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

private:
	
	FReply OnYesClicked()
	{
		bUserResponse = true;
		MyWindow->RequestDestroyWindow();
		return FReply::Handled();
	}

	FReply OnNoClicked()
	{
		bUserResponse = false;
		MyWindow->RequestDestroyWindow();
		return FReply::Handled();
	}

	TSharedPtr<SWindow> MyWindow;
	bool bUserResponse;
	TAttribute<FText> MyMessage;
};

/**
 * SModalDialogWithCheckbox 
 * Modal dialog with one or two buttons and a checkbox. 
 * All text and images contained are customizable.
 * Setup so Escape acts as cancel.
 */
class SModalDialogWithCheckbox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SModalDialogWithCheckbox )
	:_bHasCancelButton(false) 
	{}
		/** Warning message displayed on the dialog */
		SLATE_ATTRIBUTE(FText, Message)

		/** Message Displayed next to the checkbox */
		SLATE_ATTRIBUTE(FText, CheckboxMessage)
		
		/** Text to display on the confirm button */
		SLATE_ATTRIBUTE(FText, ConfirmText)

		/** Text to display on the cancel button */
		SLATE_ATTRIBUTE(FText, CancelText)

		/** If true an extra button is displayed to be used as a cancel button */
		SLATE_ARGUMENT(bool, bHasCancelButton)

		/** Default value of the checkbox */
		SLATE_ARGUMENT(bool, bDefaultCheckValue)

		/** Typically an icon to help the user more easily identify the nature of the issue */
		SLATE_ATTRIBUTE( const FSlateBrush*, Image )

		/** Window in which this widget resides */
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)

	SLATE_END_ARGS()

	/** Used to construct widgets */
	void Construct( const FArguments& InArgs )
	{
		bCheckboxResult = InArgs._bDefaultCheckValue;
		// Set this widget as focused, to allow users to hit ESC to cancel.
		MyWindow = InArgs._ParentWindow.Get();
		MyWindow.Pin()->SetWidgetToFocusOnActivate(SharedThis(this));
		MyMessage = InArgs._Message;
		MyCheckboxMessage = InArgs._CheckboxMessage;

		FSlateFontInfo MessageFont( FEditorStyle::GetFontStyle("StandardDialog.LargeFont"));

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillHeight(1.0f)
				.Padding(0,5,0,5)
				.MaxHeight(550)
				[
					SNew( SScrollBox )
					+ SScrollBox::Slot()
					[
						SNew(SHorizontalBox)
				
						// warning image
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew( SImage )
							.Image(InArgs._Image)
						]

						// main warning message
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(5,0,5,0)
						[
							SNew( STextBlock )
							.WrapTextAt(512.0f)
							.Text( MyMessage )
							.Font(MessageFont)
						]
					]
				]
			
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				[
					ConstructConditionalInternals(InArgs)
				]
			]
		];
	}
	
	/** 
	 * Constructs the widget components which require conditional checks.
	 *
	 * @param InArgs - classes custom arguments passed though from construct
	 *
	 * @returns A Shared reference to a SHorizontalBox containing all newly construct widgets.
	 */
	TSharedRef<SHorizontalBox> ConstructConditionalInternals( const FArguments& InArgs ) 
	{
		TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
		TSharedPtr<SUniformGridPanel> UniformGridPanel;
		
		// checkbox with user specified text
		HorizontalBox->AddSlot()
		.HAlign(HAlign_Left)
		.Padding(5,0,15,0)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.IsChecked(InArgs._bDefaultCheckValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged(this, &SModalDialogWithCheckbox::OnCheckboxClicked)
			.Visibility(this, &SModalDialogWithCheckbox::GetCheckboxVisibility)
			[
				SNew( STextBlock )
				.WrapTextAt(615.0f)
				.Text( MyCheckboxMessage )
			]
		];
		HorizontalBox->AddSlot()
		.HAlign(HAlign_Right)
		.Padding(2.f)
		[
			SAssignNew(UniformGridPanel, SUniformGridPanel)
			.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
		];

		// yes/ok/confirm button
		UniformGridPanel->AddSlot(0,0)
		.HAlign(HAlign_Fill)
		[
			SNew( SButton )
			.Text( InArgs._ConfirmText )
			.OnClicked( this, &SModalDialogWithCheckbox::OnConfirmClicked )
			.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
			.HAlign(HAlign_Center)
		];

		// Only add a cancel button if required
		if (InArgs._bHasCancelButton)
		{
			// cancel/stop/abort button
			UniformGridPanel->AddSlot(1,0)
			.HAlign(HAlign_Fill)
			[
				SNew( SButton )
				.Text( InArgs._CancelText )
				.OnClicked( this, &SModalDialogWithCheckbox::OnCancelClicked )
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.HAlign(HAlign_Center)
			];
		}
	
		return HorizontalBox;
	}

	SModalDialogWithCheckbox()
	: bUserResponse(false)
	, bCheckboxResult(false)
	{
	}
	
	/** Returns true if the user pressed the confirm button, otherwise false. */
	bool GetResponse() const { return bUserResponse; }

	/** Returns true if the user activated the checkbox */
	bool GetCheckBoxState() const { return bCheckboxResult; }
	
	/** Override the base method to allow for keyboard focus */
	virtual bool SupportsKeyboardFocus() const
	{
		return true;
	}

	/** Used to intercept Escape key presses, then interprets them as cancel */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		// Pressing escape returns as if the user canceled
		if ( InKeyEvent.GetKey() == EKeys::Escape )
		{
			return OnCancelClicked();
		}

		if (InKeyEvent.GetKey() == EKeys::C && InKeyEvent.IsControlDown())
		{
			FPlatformApplicationMisc::ClipboardCopy( *MyMessage.Get().ToString() );
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

private:

	/** Used as a delegate for the confirm buttons OnClicked method */
	FReply OnConfirmClicked()
	{
		bUserResponse = true;
		MyWindow.Pin()->RequestDestroyWindow();
		return FReply::Handled();
	}
	
	/** Used as a delegate for the Cancel buttons OnClicked method */
	FReply OnCancelClicked()
	{
		bUserResponse = false;
		MyWindow.Pin()->RequestDestroyWindow();
		return FReply::Handled();
	}

	/** Used as a delegate for the Checkboxs OnClicked method */
	void OnCheckboxClicked(ECheckBoxState InNewState)
	{
		bCheckboxResult = InNewState == ECheckBoxState::Checked;
	}

	/** Used as a delegate for the Checkboxs Visibile method */
	EVisibility GetCheckboxVisibility() const
	{
		return (MyCheckboxMessage.Get().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible);
	}

	/** Used to cache the users response to the warning */
	bool bUserResponse;

	/**  Used to cache whether the user activated the checkbox*/
	bool bCheckboxResult;

	/** Pointer to the window which holds this Widget, required for modal control */
	TWeakPtr<SWindow> MyWindow;

	TAttribute<FText> MyMessage;
	TAttribute<FText> MyCheckboxMessage;
};

FSuppressableWarningDialog::FSuppressableWarningDialog(const FSetupInfo& Info)
{
	// Ensure proper usage of the suppression warning.
	checkf(!Info.ConfirmText.IsEmpty(), TEXT("All warnings should have ConfirmText set!"));

	const static FString ConfigSection = TEXT("SuppressableDialogs");
	
	bool bShouldSuppressDialog = false;

	// Cache the value suppression value to be check and possible reset in ShowModal.
	IniSettingName = Info.IniSettingName;
	IniSettingFileName = Info.IniSettingFileName;
	Prompt = Info.Message;

	GConfig->GetBool( *ConfigSection, *IniSettingName, bShouldSuppressDialog, IniSettingFileName );
	
	if (!bShouldSuppressDialog && FSlateApplication::IsInitialized())
	{
		ModalWindow = SNew(SWindow)
		.Title(Info.Title)
		.SizingRule( ESizingRule::Autosized )
		.SupportsMaximize(false) .SupportsMinimize(false);

		// Cache a default image to be used as most cases will not provide their own.
		static const FSlateBrush* DefaultImage = FEditorStyle::GetBrush("NotificationList.DefaultMessage");

		MessageBox = SNew(SModalDialogWithCheckbox)
			.Message(Prompt)
			.bHasCancelButton(!Info.CancelText.IsEmpty())
			.ConfirmText(Info.ConfirmText)
			.CancelText(Info.CancelText)
			.bDefaultCheckValue(Info.bDefaultToSuppressInTheFuture)
			.CheckboxMessage(Info.CheckBoxText)
			.ParentWindow(ModalWindow)
			.Image((Info.Image != NULL) ? Info.Image : DefaultImage);

		ModalWindow->SetContent( MessageBox.ToSharedRef() );
	}
}

FSuppressableWarningDialog::EResult FSuppressableWarningDialog::ShowModal() const
{
	const FString ConfigSection = TEXT("SuppressableDialogs");
	// Assume we should not suppress the dialog
	bool bShouldSuppressDialog = false;

	// Get the setting from the config file.
	GConfig->GetBool( *ConfigSection, *IniSettingName, bShouldSuppressDialog, IniSettingFileName );

	EResult RetCode = Suppressed;
	if( !bShouldSuppressDialog )
	{
		GEditor->EditorAddModalWindow(ModalWindow.ToSharedRef());
		RetCode = (MessageBox->GetResponse()) ? Confirm : Cancel;

		if( RetCode == Confirm )
		{
			// Set the ini variable to the state of the disable check box
			bShouldSuppressDialog = MessageBox->GetCheckBoxState();
			GConfig->SetBool( *ConfigSection, *IniSettingName, bShouldSuppressDialog, IniSettingFileName );
		}
	}
	else
	{
		// If the dialog is suppressed, log the warning
		UE_LOG(LogDialogs, Warning, TEXT("Suppressed: %s"), *Prompt.ToString());
	}

	return RetCode;
}

bool PromptUserForDirectory(FString& OutDirectory, const FString& Message, const FString& DefaultPath)
{
	bool bFolderSelected = false;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if ( DesktopPlatform )
	{
		FString FolderName;
		bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			Message,
			DefaultPath,
			FolderName
			);

		if ( bFolderSelected )
		{
			OutDirectory = FolderName;
		}
	}
	return bFolderSelected;
}

bool PromptUserIfExistingObject( const FString& Name, const FString& Package, const FString& Group, UPackage* &Pkg )
{
	FString	QualifiedName = Package + TEXT(".");
	if( Group.Len() > 0 )
	{
		QualifiedName += Group + TEXT(".");
	}
	QualifiedName += Name;

	// Check for an existing object
	UObject* ExistingObject = StaticFindObject( UObject::StaticClass(), ANY_PACKAGE, *QualifiedName );
	if( ExistingObject != NULL )
	{
		// Object already exists in either the specified package or another package.  Check to see if the user wants
		// to replace the object.
		bool bWantReplace =
			EAppReturnType::Yes == FMessageDialog::Open(
									EAppMsgType::YesNo,
									FText::Format(
									NSLOCTEXT("UnrealEd", "ReplaceExistingObjectInPackage_F", "An object [{0}] of class [{1}] already exists in file [{2}].  Do you want to replace the existing object?  If you click 'Yes', the existing object will be deleted.  Otherwise, click 'No' and choose a unique name for your new object." ),
									FText::FromString(Name),
									FText::FromString(ExistingObject->GetClass()->GetName()),
									FText::FromString(Package) ) );

		if( bWantReplace )
		{
			// Replacing an object.  Here we go!
			//Try to Delete the existing object
			if (ObjectTools::DeleteSingleObject( ExistingObject ))
			{
				// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
				CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

				// Old package will be GC'ed... create a new one here
				Pkg = CreatePackage(NULL,*Package);
				if( Group.Len() )
				{
					Pkg = CreatePackage(Pkg,*Group);
				}
			}
			else //failed to delete
			{
				// Notify the user that the operation failed b/c the existing asset couldn't be deleted
				FMessageDialog::Open( EAppMsgType::Ok, FText::Format( NSLOCTEXT("DlgNewGeneric", "ContentBrowser_CannotDeleteExistingAsset", "The new asset wasn't created due to a problem while attempting\nto delete the existing '{0}' asset."), FText::FromString( Name ) ) );
				return false;
			}
		}
		else
		{
			// User chose not to replace the object; they'll need to enter a new name
			return false;
		}
	}
	return true;
}

void SGenericDialogWidget::Construct( const FArguments& InArgs )
{
	TSharedPtr< SScrollBox > ScrollBox;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300.0f)
		[
			SAssignNew(ScrollBox, SScrollBox)
		]

		+SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.Text( NSLOCTEXT("UnrealEd", "OK", "OK") )
					.OnClicked(this, &SGenericDialogWidget::OnOK_Clicked)
			]
	];

	ScrollBox->AddSlot()
	[
		InArgs._Content.Widget
	];
}

void SGenericDialogWidget::OpenDialog (const FText& InDialogTitle, const TSharedRef< SWidget >& DisplayContent)
{
	TSharedPtr< SWindow > Window;
	TSharedPtr< SGenericDialogWidget > GenericDialogWidget;

	Window = SNew(SWindow)
		.Title(InDialogTitle)
		.SizingRule( ESizingRule::Autosized )
		.SupportsMaximize(false) .SupportsMinimize(false)
		[
			SNew( SBorder )
			.Padding( 4.f )
			.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
			[
				SAssignNew(GenericDialogWidget, SGenericDialogWidget)
				[
					DisplayContent
				]
			]
		];

	GenericDialogWidget->SetWindow(Window);

	FSlateApplication::Get().AddWindow( Window.ToSharedRef() );
}

FReply SGenericDialogWidget::OnOK_Clicked(void)
{
	MyWindow.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE 
