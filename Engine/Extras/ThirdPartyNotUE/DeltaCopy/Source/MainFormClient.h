/*
   Copyright (C) Synametrics Technologies, Inc 2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
//---------------------------------------------------------------------------

#ifndef MainFormClientH
#define MainFormClientH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ActnCtrls.hpp>
#include <ActnMan.hpp>
#include <ExtCtrls.hpp>
#include <ToolWin.hpp>
#include <ComCtrls.hpp>
#include <Buttons.hpp>
#include <Dialogs.hpp>

#include <vector.h>
#include <Menus.hpp>
#include <ImgList.hpp>
#include <string>

#include "ConsoleRunner.h"
#include "TProfile.h"
#include "TTaskScheduler.h"
#include "StatusWindow.h"


#define LABEL_ADD_SCHEDULE "Add Schedule"
#define LABEL_DELETE_SCHEDULE "Delete Schedule"
#define DEFAULT_PROFILE_EXT "dcp"


//---------------------------------------------------------------------------
class TfrmMainClient : public TForm
{
__published:	// IDE-managed Components
    TCoolBar *CoolBar1;
    TPanel *Panel1;
    TSpeedButton *btnExit;
    TPanel *Panel2;
    TPanel *Panel3;
    TPanel *Panel4;
    TLabel *Label1;
    TListBox *lstProfiles;
    TPageControl *pgMain;
    TTabSheet *tbFileList;
    TTabSheet *tbCopyOptions;
    TSpeedButton *btnNewProfile;
    TPanel *Panel5;
    TLabel *Label2;
    TLabel *lblFilesFolders;
    TListBox *lstFileList;
    TButton *btnAddFiles;
    TOpenDialog *dlgOpen;
    TLabel *lblServerName;
    TEdit *txtServer;
    TComboBox *cmbModules;
    TLabel *lblVirtualDir;
    TBitBtn *btnFetchModules;
    TMemo *txtSchedule;
    TLabel *lblProfileKey;
    TLabel *lblKey;
    TButton *btnModify;
    TGroupBox *GroupBox1;
    TCheckBox *chkUseRecursive;
    TCheckBox *chkUseDelete;
    TCheckBox *chkUseCompression;
    TCheckBox *chkUseSkipNewer;
    TLabel *Label7;
    TEdit *txtAdditionalParams;
    TButton *btnAddFolder;
    TButton *btnDeleteAddTask;
    TPopupMenu *ppmProfiles;
    TMenuItem *RunNow1;
    TMenuItem *N1;
    TMenuItem *DeleteProfile1;
    TSpeedButton *btnEmail;
    TMenuItem *AddProfile1;
    TPopupMenu *ppmFileList;
    TMenuItem *DeleteEntry1;
	TMenuItem *ModifySelection1;
    TImageList *imgList;
    TMenuItem *DisplayRunCommand1;
    TMenuItem *N2;
    TMenuItem *Restore1;
    TMenuItem *DisplayRestoreCommand1;
    TMenuItem *ModifyTargetPath1;
    TMenuItem *N3;
    TMenuItem *ShowStatusWindow1;
    TMainMenu *mnuMain;
    TMenuItem *File1;
    TMenuItem *NewProfile1;
    TMenuItem *N4;
    TMenuItem *Exit1;
    TMenuItem *Edit1;
    TMenuItem *ModifyEmailConfiguration1;
    TMenuItem *Help1;
    TMenuItem *Content1;
    TMenuItem *N5;
    TMenuItem *AboutDeltaCopy1;
    TMenuItem *N6;
    TMenuItem *SetRetryCount1;
	TCheckBox *chkUseSSH;
	TTabSheet *tbAuth;
	TGroupBox *GroupBox2;
	TLabel *lblAuthHeader;
	TEdit *txtUID;
	TEdit *txtPWD;
	TLabel *Label3;
	TLabel *Label4;
	TCheckBox *chkFixPerm;
	TCheckBox *chkVerbose;
    void __fastcall btnExitClick(TObject *Sender);
    void __fastcall btnAddFilesClick(TObject *Sender);
    void __fastcall FormCreate(TObject *Sender);
    void __fastcall btnFetchModulesClick(TObject *Sender);
    void __fastcall btnNewProfileClick(TObject *Sender);
    void __fastcall lstProfilesDblClick(TObject *Sender);
    void __fastcall lstProfilesClick(TObject *Sender);
    void __fastcall txtServerExit(TObject *Sender);
    void __fastcall lstFileListKeyUp(TObject *Sender, WORD &Key,
          TShiftState Shift);
    void __fastcall btnAddFolderClick(TObject *Sender);
    void __fastcall chkUseRecursiveClick(TObject *Sender);
    void __fastcall btnDeleteAddTaskClick(TObject *Sender);
    void __fastcall btnModifyClick(TObject *Sender);
    void __fastcall RunNow1Click(TObject *Sender);
    void __fastcall DeleteProfile1Click(TObject *Sender);
    void __fastcall btnEmailClick(TObject *Sender);
    void __fastcall DeleteEntry1Click(TObject *Sender);
    void __fastcall ModifySelection1Click(TObject *Sender);
    void __fastcall lstProfilesDrawItem(TWinControl *Control, int Index,
          TRect &Rect, TOwnerDrawState State);
    void __fastcall lstFileListDrawItem(TWinControl *Control, int Index,
          TRect &Rect, TOwnerDrawState State);
    void __fastcall cmbModulesChange(TObject *Sender);
    void __fastcall DisplayRunCommand1Click(TObject *Sender);
    void __fastcall Restore1Click(TObject *Sender);
    void __fastcall DisplayRestoreCommand1Click(TObject *Sender);
    void __fastcall ModifyTargetPath1Click(TObject *Sender);
    void __fastcall ppmProfilesPopup(TObject *Sender);
    void __fastcall ShowStatusWindow1Click(TObject *Sender);
    void __fastcall Content1Click(TObject *Sender);
    void __fastcall AboutDeltaCopy1Click(TObject *Sender);
    void __fastcall SetRetryCount1Click(TObject *Sender);
private:	// User declarations

    TProfileManager profileManager;
    AnsiString errorMessage;
    TTaskScheduler taskManager;
    TfrmStatus* statusWindow;
    vector<string> results;
    DWORD  currentChildPid;
    int keepRunning;

    WNDPROC FListFileOriginalProc;
    void *  FListFileObjectInstance;



    void AddAndFormatFileToList(AnsiString s);
    void AddNewProfile();
    void AddTask(TProfile*);
    int  Ask(AnsiString s);

	void CheckProgramAssociation();
	void ClearControls();
    void CreateAssociation(AnsiString Ext, AnsiString path);
    void CreateSubclassProcedures();
    void __fastcall DisplayError();
    void FillInitialControls();
    void FillProfileControl();
    void FillProfileList();
    void FillTaskInfo(TProfile*);
    AnsiString GetProgramAssociation(AnsiString Ext);
    void RemoveProfile();
    void SaveDummyProfiles(TProfile* profileName);
	void SaveCurrentProfile();
	void SaveLogToDisk(AnsiString, AnsiString);
    void SendNotificationEmail(bool, AnsiString);
    AnsiString Unix2Dos(AnsiString);



public:		// User declarations
    __fastcall TfrmMainClient(TComponent* Owner);
    __fastcall ~TfrmMainClient();

    void __fastcall AddData(TMessage& Message);
    void AddDraggedFile(TMessage& );
    static AnsiString BrowseForFolder(AnsiString title, HWND ownerHandle);
    void __fastcall ListProc(TMessage &msg);
    void __fastcall ProcessDropedFiles(TMessage &Message);
    void __fastcall RSyncStarted(TMessage& Message);
    void __fastcall RSyncTerminated(TMessage& Message);
    void RunFromCommandLine();
    void RunManually(bool showCommand, bool restore);
    void __fastcall TerminateCurrentlyRunningRsync(TMessage& Message);

    BEGIN_MESSAGE_MAP        
        MESSAGE_HANDLER(MSG_TO_STDOUT, TMessage, AddData)
        MESSAGE_HANDLER(PROCESS_STARTED, TMessage, RSyncStarted)
        MESSAGE_HANDLER(PROCESS_TERMINATED, TMessage, RSyncTerminated)
        MESSAGE_HANDLER(TERMINATE_RSYNC_TASK, TMessage, TerminateCurrentlyRunningRsync)
        MESSAGE_HANDLER(WM_DROPFILES , TMessage, ProcessDropedFiles)
    END_MESSAGE_MAP(TForm)

};

class TManualRunThread : public TThread{
    TfrmMainClient* worker;
    bool showCmdOnly;
    bool restore;
public:
    TManualRunThread(TfrmMainClient* worker, bool showCmdOnly, bool restore)
        : TThread(false) {
            this->worker = worker;
            this->showCmdOnly = showCmdOnly;
            this->restore = restore;
        }
    void __fastcall Execute(){ worker->RunManually(showCmdOnly, restore);}

};
//---------------------------------------------------------------------------
extern PACKAGE TfrmMainClient *frmMainClient;
//---------------------------------------------------------------------------
#endif
