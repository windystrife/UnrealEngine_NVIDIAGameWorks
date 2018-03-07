// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

class SModalDialogWithCheckbox;

/**
 * Opens a modal/blocking message box dialog (with an additional 'copy message text' button), and returns the result immediately
 *
 * @param InMessageType		The type of message box to display (e.g. 'ok', or 'yes'/'no' etc.)
 * @param InMessage			The message to display in the message box
 * @param InTitle			The title to display for the message box
 * @return					Returns the result of the user input
 */
EAppReturnType::Type UNREALED_API OpenMsgDlgInt(EAppMsgType::Type InMessageType, const FText& InMessage, const FText& InTitle);

DECLARE_DELEGATE_TwoParams(FOnMsgDlgResult, const TSharedRef<SWindow>&, EAppReturnType::Type);

/**
 * Opens a non-modal/non-blocking message box, which returns its result through a delegate/callback,
 * using a reference to the created window, to identify which dialog has returned a result (in case there are multiple dialog windows)
 *
 * @param InMessageType		The type of message box to display (e.g. 'ok', or 'yes'/'no' etc.)
 * @param InMessage			The message to display in the message box
 * @param InTitle			The title to display for the message box
 * @param ResultCallback	The delegate/callback instance, where results should be returned
 * @return					Returns the dialog window reference, which the calling code should store, to identify which dialog returned
 */
TSharedRef<SWindow> UNREALED_API OpenMsgDlgInt_NonModal(EAppMsgType::Type InMessageType, const FText& InMessage, const FText& InTitle,
											FOnMsgDlgResult ResultCallback);

/*-----------------------------------------------------------------------------
	FDragDropConfirmationDialog
-----------------------------------------------------------------------------*/
class FDragDropConfirmation
{
public:
	enum EResult
	{
		Folder,
		Contents,
		Cancel
	};

	UNREALED_API static EResult OpenDialog(const FString& ConfirmationTitle, const FString& Message, const FString& FolderOption, const FString& ContentsOption, const FString& CancelOption);
};

/*-----------------------------------------------------------------------------
	FSuppressableWarningDialog
-----------------------------------------------------------------------------*/
/**
 * A Dialog that displays a warning message to the user and provides the option to not display it in the future
 */
class UNREALED_API FSuppressableWarningDialog 
{
public:

	/** 
	 * Struct used to initialize FSuppressableWarningDialog
	 * 
	 * User must provide confirm text, and cancel text (if using cancel button)
	 */
	struct FSetupInfo
	{
		/** Warning message displayed on the dialog */
		FText Message;

		/** Title shown at the top of the warning message window */
		FText Title;	

		/** The name of the setting which stores whether to display the warning in future */
		FString IniSettingName;

		/** The name of the file which stores the IniSettingName flag result */
		FString IniSettingFileName;

		/** If true the suppress checkbox defaults to true*/
		bool bDefaultToSuppressInTheFuture;

		/** Text used on the button which will return FSuppressableWarningDialog::Confirm */
		FText ConfirmText;

		/** Text used on the button which will return FSuppressableWarningDialog::Cancel */
		FText CancelText;

		/** Test displayed next to the checkbox, defaulted to "Don't show this again" */
		FText CheckBoxText;

		/** Image used on the side of the warning, a default is provided. */
		struct FSlateBrush* Image;

		/**
		* Constructs a warning dialog setup object, used to initialize a warning dialog.
		*
		* @param Prompt					The message that appears to the user
		* @param Title					The title of the dialog
		* @param InIniSettingName		The name of the entry in the INI where the state of the "Disable this warning" check box is stored
		* @param InIniSettingFileName	The name of the INI where the state of the InIniSettingName flag is stored (defaults to GEditorPerProjectIni)
		*/
		FSetupInfo(const FText& InMessage, const FText& InTitle, const FString& InIniSettingName, const FString& InIniSettingFileName=GEditorPerProjectIni )
			: Message(InMessage)
			, Title(InTitle)
			, IniSettingName(InIniSettingName)
			, IniSettingFileName(InIniSettingFileName)
			, bDefaultToSuppressInTheFuture(false)
			, ConfirmText()
			, CancelText()
			, CheckBoxText(NSLOCTEXT("ModalDialogs", "DefaultCheckBoxMessage", "Don't show this again"))
			, Image(NULL)
		{
		}
	};

	/** Custom return type used by ShowModal */
	enum EResult
	{
		Suppressed = -1,	// User previously suppressed dialog, in most cases this should be treated as confirm
		Cancel = 0,			// No/Cancel, normal usage would stop the current action
		Confirm = 1,		// Yes/Ok/Etc, normal usage would continue with action
	};

	/**
	 * Constructs FSuppressableWarningDialog
	 * 
	 * @param FSetupInfo Info - struct used to initialize the dialog. 
	 */
	FSuppressableWarningDialog ( const FSuppressableWarningDialog::FSetupInfo& info );
	
	/** Launches warning window, returns user response or suppressed */
	EResult ShowModal() const;

private:

	/** Name of the flag which controls whether to launch the warning */
	FString IniSettingName;

	/** Name of the file which stores the IniSettingName flag result */
	FString IniSettingFileName;

	/** Cached warning text to output to the log if the warning is suppressed */
	FText Prompt;

	/** Cached pointer to the modal window */
	TSharedPtr<SWindow> ModalWindow;

	/** Cached pointer to the message box held within the window */
	TSharedPtr<class SModalDialogWithCheckbox> MessageBox;

};


class SGenericDialogWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SGenericDialogWidget ){}
		SLATE_DEFAULT_SLOT( FArguments, Content )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/** Sets the window of this dialog. */
	void SetWindow( TSharedPtr<SWindow> InWindow )
	{
		MyWindow = InWindow;
	}

	UNREALED_API static void OpenDialog(const FText& InDialogTitle, const TSharedRef< SWidget >& DisplayContent);

private:
	FReply OnOK_Clicked(void);

private:
	/** Pointer to the containing window. */
	TWeakPtr< SWindow > MyWindow;
};

UNREALED_API bool PromptUserIfExistingObject(const FString& Name, const FString& Package, const FString& Group, class UPackage* &Pkg );
/**
* Helper method for popping up a directory dialog for the user.  OutDirectory will be 
* set to the empty string if the user did not select the OK button.
*
* @param	OutDirectory	[out] The resulting path.
* @param	Message			A message to display in the directory dialog.
* @param	DefaultPath		An optional default path.
* @return					true if the user selected the OK button, false otherwise.
*/
UNREALED_API bool PromptUserForDirectory(FString& OutDirectory, const FString& Message, const FString& DefaultPath);
