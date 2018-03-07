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

#include <vcl.h>
#pragma hdrstop

#include "MainFormClient.h"
#include "NewProfile.h"
#include "RSync.h"
#include "EmailConf.h"
#include "DCConfig.h"
#include "Logger.h"
#include "Registry.hpp"
#include "GenUtils.h"
#include "TargetEditor.h"
#include "AboutDC.h"
#include <smtpsend.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmMainClient *frmMainClient;
//---------------------------------------------------------------------------
__fastcall TfrmMainClient::TfrmMainClient(TComponent* Owner)
    : TForm(Owner)
{
    CreateSubclassProcedures();
}

//---------------------------------------------------------------------------
__fastcall TfrmMainClient::~TfrmMainClient(){

    SetWindowLong(lstFileList->Handle, GWL_WNDPROC, (LONG) FListFileOriginalProc);
    FreeObjectInstance(FListFileObjectInstance);
}

//---------------------------------------------------------------------------
void TfrmMainClient::AddAndFormatFileToList(AnsiString s){
    AnsiString finalAnswer = s + SOURCE_TARGET_DELIMITER + TProfile::GetTargetFolder(s);
    lstFileList->Items->Add(finalAnswer);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::AddData(TMessage& Message){

    int index = Message.WParam;
    AnsiString data = Unix2Dos(results.at(index).c_str());
    statusWindow->AddData(data);
}

//---------------------------------------------------------------------------
void TfrmMainClient::AddDraggedFile(TMessage& Message){
int NumFiles;
char buffer[255];

    if(lstProfiles->ItemIndex < 1){
        return;
    }
    
    NumFiles = DragQueryFile((void*)Message.WParam, -1, NULL, 0)  ;
    //ShowMessage(IntToStr(NumFiles));

    for (int i = 0; i < NumFiles; i++){
        DragQueryFile((void*)Message.WParam, i, buffer, sizeof(buffer));

        if(FileExists(buffer)){
            AddAndFormatFileToList(buffer);
        }else{
            AddAndFormatFileToList(AnsiString(buffer) + "\\");
        }

    }
    SaveCurrentProfile();

}
//---------------------------------------------------------------------------
void TfrmMainClient::AddNewProfile(){
    TfrmAddProfile* dlg = new TfrmAddProfile(this);
    TProfile* aProfile = new TProfile();

    int addSchedule = 0;
    if(dlg->DoModal(aProfile, addSchedule)){
        lstProfiles->Items->Add(aProfile->GetProfileName());
        profileManager.AddProfile(aProfile);
        profileManager.SaveProfiles();
        SaveDummyProfiles(aProfile);

        if(addSchedule == 1){
            AddTask(aProfile);
        }
    }

    delete dlg;
}
//---------------------------------------------------------------------------

void TfrmMainClient::AddTask(TProfile* aProfile){
    TTaskParams tParams;
    AnsiString appName = Application->ExeName; // + " " + aProfile->GetKey();

    tParams.appName = WideString(ExtractFilePath(Application->ExeName)) + aProfile->GetKey() + "." + DEFAULT_PROFILE_EXT; //WideString(appName).c_bstr();
    tParams.comments = WideString("Scheduled task for " + aProfile->GetProfileName()).c_bstr();
    tParams.appParameters = WideString(aProfile->GetKey());
    tParams.workingDirectory = WideString(ExtractFilePath(Application->ExeName));

    if(taskManager.AddNewScheduledTask(WideString(aProfile->GetTaskName()).c_bstr())){
        taskManager.SetTaskProperties(WideString(aProfile->GetTaskName()).c_bstr(), tParams);
    }



}
//---------------------------------------------------------------------------
int  TfrmMainClient::Ask(AnsiString s){
	if(MessageDlg(s, mtConfirmation, TMsgDlgButtons() << mbYes << mbNo, 0) == 6)
    	return 1;
    else
    	return 0;
}


//---------------------------------------------------------------------------
void TfrmMainClient::CheckProgramAssociation(){
    AnsiString program = GetProgramAssociation(DEFAULT_PROFILE_EXT);

    if(program == ""){
        CreateAssociation(DEFAULT_PROFILE_EXT, Application->ExeName);
        return;
    }
    if(program.LowerCase().Pos("deltac.exe")){
        return;
    }

    //This means that the file extension is associated with something else
    AnsiString question = "File extension " + AnsiString(DEFAULT_PROFILE_EXT) + " is associated with another program. (" + program +
        "). Would you like to change that to DeltaCopy? You will not be able to run scheduled tasks " +
        "unless this setting is changed";

    if(Ask(question)){
        CreateAssociation(DEFAULT_PROFILE_EXT, Application->ExeName);
    }

}
//---------------------------------------------------------------------------
void TfrmMainClient::ClearControls(){
 	pgMain->ActivePage = tbFileList;
	txtServer->Text = "";
	cmbModules->Text = "";
	lblKey->Caption = "";
	txtServer->Modified = false;
	pgMain->Enabled = false;
	lblFilesFolders->Enabled = false;
	lblServerName->Enabled = false;
	lblVirtualDir->Enabled = false;
	lblProfileKey->Enabled = false;
	btnAddFolder->Enabled = false;
	btnAddFiles->Enabled = false;
	btnFetchModules->Enabled = false;
	txtSchedule->Text = "";
	txtUID->Text = "";
	txtPWD->Text = "";
}
//---------------------------------------------------------------------------
void TfrmMainClient::CreateAssociation(AnsiString Ext, AnsiString path){
TRegistry* reg;
AnsiString s;

    reg = new TRegistry();

    try{
        reg->RootKey = HKEY_CLASSES_ROOT;
        //reg->RootKey = HKEY_CURRENT_USER;
        reg->LazyWrite = false;
        reg->OpenKey("." + Ext + "\\shell\\open\\command", true);
        reg->WriteString("", path + " \"%1\"");
        reg->CloseKey();

        reg->OpenKey("." + Ext + "\\DefaultIcon", true);
        reg->WriteString("", path + ",1");
        reg->CloseKey();

    }catch(...){

    }
    delete reg;

}
//---------------------------------------------------------------------------
void TfrmMainClient::CreateSubclassProcedures(){
    FListFileOriginalProc = (WNDPROC) GetWindowLong(lstFileList->Handle, GWL_WNDPROC);
    FListFileObjectInstance = MakeObjectInstance(ListProc);
    SetWindowLong(lstFileList->Handle, GWL_WNDPROC, (LONG)FListFileObjectInstance);
}

//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::DisplayError(){
    ShowMessage(errorMessage);
}
//---------------------------------------------------------------------------
void TfrmMainClient::FillInitialControls(){
    FillProfileList();
    lblKey->Caption = "";
}
//---------------------------------------------------------------------------
void TfrmMainClient::FillProfileControl(){

    AnsiString profileName = lstProfiles->Items->Strings[lstProfiles->ItemIndex];
    TProfile* selectedProfile = profileManager.GetProfile(profileName);

    lstFileList->Items->Clear();



	if(selectedProfile == NULL){
		ClearControls();
        return;
    }

    pgMain->Enabled = true;
    lblFilesFolders->Enabled = true;
    lblServerName->Enabled = true;
    lblVirtualDir->Enabled = true;
    lblProfileKey->Enabled = true;
    btnAddFolder->Enabled = true;
    btnAddFiles->Enabled = true;
    btnFetchModules->Enabled = true;
    
    
    txtServer->Text = selectedProfile->GetServerIP();
    cmbModules->Text = selectedProfile->GetModuleName();
    lblKey->Caption = selectedProfile->GetKey();
    chkUseDelete->Checked = selectedProfile->IsDeleteOlderFiles();
    chkUseSkipNewer->Checked = selectedProfile->IsSkipNewerFiles();
    chkUseRecursive->Checked = selectedProfile->IsUseRecursive();
	chkUseCompression->Checked = selectedProfile->IsUseCompression();
	txtAdditionalParams->Text = selectedProfile->GetAdditionalParams();
	chkUseSSH->Checked = selectedProfile->IsUseSSH();
	chkVerbose->Checked = selectedProfile->IsVerboseLogging();
	chkFixPerm->Checked = selectedProfile->IsAssignPermissions();

	txtUID->Text = selectedProfile->GetUserName();
	txtPWD->Text = selectedProfile->GetPassword();


    for(unsigned int i = 0; i < selectedProfile->GetFileList().size(); i++){
        //Do not use AddAndFormatFileToList here. This string is already
        //Formatted
        lstFileList->Items->Add(selectedProfile->GetFileList().at(i).data());
    }

    txtServer->Modified = false;
    FillTaskInfo(selectedProfile);
}
//---------------------------------------------------------------------------
void TfrmMainClient::FillProfileList(){
    unsigned int totalProfiles = profileManager.GetProfileCount();

    lstProfiles->Items->Clear();
    lstProfiles->Items->Add("<Add New Profile>");
    for(unsigned int i = 0; i < totalProfiles; i++){
        TProfile* oneProfile = profileManager.GetProfile(i);
        lstProfiles->Items->Add(oneProfile->GetProfileName());
    }
    theLogger->Log("Total profile count: " + IntToStr(totalProfiles));
}

//---------------------------------------------------------------------------
void TfrmMainClient::FillTaskInfo(TProfile* aProfile){
    if(aProfile == NULL){
        txtSchedule->Text = "";
        btnModify->Enabled = false;
        btnDeleteAddTask->Enabled = false;
        btnDeleteAddTask->Caption = LABEL_ADD_SCHEDULE;
    }

    WideString wStr(aProfile->GetTaskName());
    if(taskManager.IsAvailable(wStr.c_bstr())){
        TTaskParams taskInfo;
        if(taskManager.GetTaskInfo(wStr.c_bstr(), taskInfo)){
            txtSchedule->Lines->Clear();

            char buffer[20];

            taskInfo.getStatusStr(buffer, taskInfo.status);

            txtSchedule->Lines->Add("Task name      : " + aProfile->GetTaskName());
            txtSchedule->Lines->Add("NT Account name: " + WideString(taskInfo.accountName.data()));
            txtSchedule->Lines->Add("Created by     : " + WideString(taskInfo.creator.data()));
            txtSchedule->Lines->Add("Status         : " + AnsiString(buffer));

            if(taskInfo.lastRun.wYear == 0 && taskInfo.lastRun.wMonth == 0){
                txtSchedule->Lines->Add("Last ran at    : Never");
            }else{
                txtSchedule->Lines->Add("Last ran at    : " + SystemTimeToDateTime(taskInfo.lastRun).DateTimeString());
            }
            if(taskInfo.nextRun.wYear == 0 && taskInfo.nextRun.wMonth == 0){
                txtSchedule->Lines->Add("Next ran at    : Not specified");
            }else{
                txtSchedule->Lines->Add("Next run at    : " + SystemTimeToDateTime(taskInfo.nextRun).DateTimeString());
            }
            txtSchedule->Lines->Add("Max run time   : " + IntToStr(taskInfo.maxRuntime) + " (ms)");
            txtSchedule->Lines->Add("Comments       : " + WideString(taskInfo.comments.data()));

            btnDeleteAddTask->Caption = LABEL_DELETE_SCHEDULE;
        }
    }else{
        btnDeleteAddTask->Caption = LABEL_ADD_SCHEDULE;
        txtSchedule->Text = "No schedule has been assigned to this profile. Click the Add Schedule button to assign a schedule.";
    }

}

//---------------------------------------------------------------------------
AnsiString TfrmMainClient::GetProgramAssociation(AnsiString Ext){
TRegistry* reg;
AnsiString s;
    reg = new TRegistry();
    reg->RootKey = HKEY_CLASSES_ROOT;
    //reg->RootKey = HKEY_CURRENT_USER;

    if ( reg->OpenKey("." + Ext + "\\shell\\open\\command", false)){
        s = reg->ReadString("");
        reg->CloseKey();
    }else{
        //Perhaps there is a system file pointer
        if (reg->OpenKey("." + Ext, false)){
            s = reg->ReadString("");
            reg->CloseKey();
        }

        if (s != ""){
            if(reg->OpenKey(s + "\\shell\\open\\command", false)){
                s = reg->ReadString("");
                reg->CloseKey();
            }
        }
    }


    //Now delete any command line, quotes and spaces

    if( s.Pos("%") > 0)
        s.Delete(s.Pos("%"), s.Length());


    if ( (s.Length() > 0) && (s[1] == '\"')) s.Delete(1, 1);

    if( (s.Length() > 0) && ( s[s.Length()] == '\"' )) s.Delete(s.Length(), 1);


    while( (s.Length() > 0) && ( s[s.Length()] == 32 || s[s.Length()] == '\n' || s[s.Length()] == '\"'))
        s.Delete(s.Length(), 1);

    delete reg;
    return s;

}

//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::ListProc(TMessage &msg){
    msg.Result = 0;
    switch(msg.Msg)    {
        case WM_DROPFILES:

            LockWindowUpdate(lstFileList->Handle);
            msg.Result = CallWindowProc ((FARPROC)FListFileOriginalProc,
                                         lstFileList->Handle,
                                         msg.Msg,
                                         msg.WParam,
                                         msg.LParam);
            //InvalidateRect (lstFileList->Handle, 0, true);
            AddDraggedFile(msg);
            LockWindowUpdate(NULL);
            break;
        default:

            msg.Result = CallWindowProc ((FARPROC)FListFileOriginalProc,
                                         lstFileList->Handle,
                                         msg.Msg,
                                         msg.WParam,
                                         msg.LParam);
                                         break;

        }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::ProcessDropedFiles(TMessage &Message){
    int NumFiles;
    char buffer[255];

    NumFiles = DragQueryFile((void*)Message.WParam, -1, NULL, 0)  ;
    //ShowMessage(IntToStr(NumFiles));

    for (int i = 0; i < NumFiles; i++){
        DragQueryFile((void*)Message.WParam, i, buffer, sizeof(buffer));
        //createMDIChild(AnsiString(buffer));
        ShowMessage(buffer);
    }

}
//---------------------------------------------------------------------------
void TfrmMainClient::RemoveProfile(){
    AnsiString profileName = lstProfiles->Items->Strings[lstProfiles->ItemIndex];
    TProfile* selectedProfile = profileManager.GetProfile(profileName);

    if(selectedProfile == NULL){
        errorMessage = "Please select a profile";
        ShowMessage(errorMessage);
        return;
    }

    if(Ask("Are you sure you want to remove this profile?")){
        theLogger->Log("Removing profile " + profileName);
        AnsiString dummyFile = ExtractFilePath(Application->ExeName) + selectedProfile->GetKey() + "." + DEFAULT_PROFILE_EXT;

        WideString wStr(selectedProfile->GetTaskName());
        taskManager.DeleteTask(wStr.c_bstr());
        profileManager.RemoveProfile(selectedProfile);  //This method will DELECT the selectedProfile object.
        profileManager.SaveProfiles();
        FillProfileList();

        if(!DeleteFile(dummyFile)){
            theLogger->Log("Unable to delete the profile file. " + dummyFile);
        }
    }

}
//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::RSyncStarted(TMessage& Message){
    currentChildPid = Message.WParam;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::RSyncTerminated(TMessage& Message){
    currentChildPid = 0;
}
//---------------------------------------------------------------------------
void TfrmMainClient::RunFromCommandLine(){


    AnsiString profileName = ParamStr(1);

    if(profileName.Pos("." + AnsiString(DEFAULT_PROFILE_EXT)) > 0){
        profileName = ExtractFileName(profileName);
        profileName.SetLength(profileName.Length() - 4);
    }

    TProfile* selectedProfile = profileManager.GetProfileByKey(profileName);

    if(selectedProfile == NULL){
        errorMessage = "Invalid profile name '" + profileName + "' provided";
        theLogger->Log(errorMessage);
        return;
    }

    theLogger->Log("Running '" + selectedProfile->GetProfileName() + "' from command line");
    int runStat = 1;
    bool result = selectedProfile->Run(NULL, "", &results, false, false, &runStat);

	SendNotificationEmail(result, selectedProfile->GetProfileName());

    exit(0);
}
//---------------------------------------------------------------------------
void TfrmMainClient::RunManually(bool showCommand, bool restore){
    AnsiString profileName = lstProfiles->Items->Strings[lstProfiles->ItemIndex];
    TProfile* selectedProfile = profileManager.GetProfile(profileName);

    if(selectedProfile == NULL){
        errorMessage = "Please select a profile";
        ShowMessage(errorMessage);
        return;
    }

    keepRunning = 1;
	bool success = selectedProfile->Run(Handle, "", &results, showCommand, restore, &keepRunning);

    TDCConfig config(false);
	if(config.IsSaveLogToDisk() && !showCommand){
		AnsiString msg;

		if(success){
			msg = "Profile " + profileName + " ran successfully.\r\n\r\n";
		}else{
			msg = "Profile " + profileName + " failed to execute.\r\n\r\n";
		}

		msg += "Execution log\r\n-------------\r\n";
		for(unsigned int i = 0; i < results.size(); i++){
			msg += results.at(i).data();
		}
		SaveLogToDisk(profileName, msg);
		results.clear();
	}
}
//---------------------------------------------------------------------------
void TfrmMainClient::SaveCurrentProfile(){
    AnsiString profileName = lstProfiles->Items->Strings[lstProfiles->ItemIndex];
    TProfile* selectedProfile = profileManager.GetProfile(profileName);

    if(selectedProfile == NULL){
        return;
    }

    selectedProfile->SetModuleName(cmbModules->Text);
    selectedProfile->SetServerIP(txtServer->Text);
    selectedProfile->SetAdditionalParams(txtAdditionalParams->Text);
    selectedProfile->SetDeleteOlderFiles(chkUseDelete->Checked);
    selectedProfile->SetSkipNewerFiles(chkUseSkipNewer->Checked);
	selectedProfile->SetUseRecursive(chkUseRecursive->Checked);
	selectedProfile->SetUseCompression(chkUseCompression->Checked);
	selectedProfile->SetUseSSH(chkUseSSH->Checked);
	selectedProfile->SetVerboseLogging(chkVerbose->Checked);
	selectedProfile->SetAssignPermissions(chkFixPerm->Checked);

	selectedProfile->SetUserName(txtUID->Text);
	selectedProfile->SetPassword(txtPWD->Text);

    selectedProfile->ClearFiles();

    for(int i = 0; i < lstFileList->Count; i++){
        selectedProfile->AddFile(lstFileList->Items->Strings[i].c_str());
    }

    profileManager.SaveProfiles();
    txtServer->Modified = false;
}
//---------------------------------------------------------------------------
void TfrmMainClient::SaveDummyProfiles(TProfile* aProfile){
    TStringList* list = new TStringList();
    list->Add(aProfile->GetProfileName());
    list->SaveToFile(ExtractFilePath(Application->ExeName) + aProfile->GetKey() + "." + DEFAULT_PROFILE_EXT);
    delete list;
}

//---------------------------------------------------------------------------
void TfrmMainClient::SaveLogToDisk(AnsiString profileName, AnsiString msg){
	AnsiString logFolder = ExtractFilePath(Application->ExeName) + "\\logs\\";
	if(!DirectoryExists(logFolder)){
		CreateDir(logFolder);
	}


	TDateTime now = TDateTime::CurrentDateTime();
	unsigned short year, month, day, hour, min, sec, msec;

	now.DecodeDate(&year, &month, &day);
	now.DecodeTime(&hour, &min, &sec, &msec);
	
	AnsiString fileName = profileName + "_" + IntToStr(year);

	fileName += (month < 10) ? "0" + IntToStr(month) : IntToStr(month);
	fileName += (day < 10) ? "0" + IntToStr(day) : IntToStr(day);
	fileName += "_";
	fileName += IntToStr(hour) + "-" + IntToStr(min) + "-" + IntToStr(sec) + ".log";

    TStringList* data = new TStringList();
	data->Text = msg;
    data->SaveToFile(logFolder + fileName);

	delete data;
}
//---------------------------------------------------------------------------
void TfrmMainClient::SendNotificationEmail(bool success, AnsiString profileName){
    TDCConfig config(false);


	AnsiString msg;

	if(success){
		msg = "Profile " + profileName + " ran successfully.\r\n\r\n";
	}else{
		msg = "Profile " + profileName + " failed to execute.\r\n\r\n";
	}

	msg += "Execution log\r\n-------------\r\n";
	for(unsigned int i = 0; i < results.size(); i++){
		msg += results.at(i).data();
	}

	if(config.IsSaveLogToDisk()){
		SaveLogToDisk(profileName, msg);
	}


    if(success && !config.IsNotifyOnSuccess()){
		theLogger->Log("Task ran successfully, but notification email won't be sent.");
        return;
    }

    if(!success && !config.IsNotifyOnFailure()){
        theLogger->Log("Task failed to run, but notification email won't be sent.");
        return;
    }

    if(config.GetSmtpServer() == "" || config.GetRecipients() == ""){
        theLogger->Log("Unable to send email notification. Either SmtpServer is null or no recipient specified.");
        return;
    }



	AnsiString subject = "Profile " + profileName + (success ? " ran successfully" : " failed to execute");

	TStringList* data = new TStringList();
	data->Text = msg;
	if(SendToEx(config.GetSendersEmail(), config.GetRecipients(), subject, config.GetSmtpServer(), data, config.GetSmtpUser(), config.GetSmtpPass())){
		theLogger->Log("Successfully sent notification email to " + config.GetRecipients());
	}else{
		theLogger->Log("Error occurred while sending message. " + data->Text);
    }

	delete data;

	/*

    smMain->Host = config.GetSmtpServer();

    try{
        smMain->Connect();
        if(smMain->Connected){
            smMain->PostMessage->FromAddress        = config.GetSendersEmail(); //"DCNotification@yourdomain.com";
            smMain->PostMessage->FromName           = "DeltaCopy Notifier";
			smMain->PostMessage->Subject            = "Profile " + profileName + (success ? " ran successfully" : " failed to execute");
            smMain->PostMessage->ToAddress->Text    = config.GetRecipients();
            smMain->PostMessage->ToBlindCarbonCopy->Clear();
            smMain->PostMessage->ToCarbonCopy->Clear();



			AnsiString msg;

			if(success){
				msg = "Profile " + profileName + " ran successfully.\r\n\r\n";
			}else{
				msg = "Profile " + profileName + " failed to execute.\r\n\r\n";
			}

			msg += "Execution log\r\n-------------\r\n";
			for(unsigned int i = 0; i < results.size(); i++){
				msg += results.at(i).data();
			}

            smMain->PostMessage->Body->Text = msg;
            smMain->SendMail();

			theLogger->Log("Successfully sent notification email to " + config.GetRecipients());
            smMain->Disconnect();

        }
    }catch(Exception& e){
		theLogger->Log("Error occurred while sending message. " + e.Message);
        if(smMain->Connected){
            smMain->Disconnect();
        }
	}
	*/
}
//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::TerminateCurrentlyRunningRsync(TMessage& Message){
    if(currentChildPid == 0) return;

    keepRunning = false;
    TConsoleRunner console;
    console.TerminateApp(currentChildPid, 5000, true);
    statusWindow->AddData("Terminating running task upon user's request...");

}
//---------------------------------------------------------------------------
AnsiString TfrmMainClient::Unix2Dos(AnsiString input){
    AnsiString result;

    for(int i = 1; i <= input.Length(); i++){
        if(input[i] == '\n'){

            if((i > 1 && input[i - 1] != '\r') || i == 0){
                result += "\r\n";
            }
        }else{
            result += input[i];
        }
    }
    return result;

}
//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::btnExitClick(TObject *Sender)
{
    Close();
}
//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::btnAddFilesClick(TObject *Sender)
{
    if(dlgOpen->Execute()){
        AddAndFormatFileToList(dlgOpen->FileName);
        SaveCurrentProfile();
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::FormCreate(TObject *Sender)
{
    theLogger = new TLogger(ExtractFilePath(Application->ExeName) + "deltac.log");


    if(ParamCount() > 0){
        //a Command line argument is provided.
        theLogger->Log("Executing task from command line");
        RunFromCommandLine();
    }

    TDCConfig config(true);
    config.SetAppPath(ExtractFilePath(Application->ExeName));

    CheckProgramAssociation();
    statusWindow = new TfrmStatus(this);
    statusWindow->SetParentHandle(Handle);
	FillInitialControls();
	ClearControls();

    DragAcceptFiles(lstFileList->Handle, True);

	pgMain->ActivePage = tbFileList;

	lblAuthHeader->Caption = "Specifying a user id and password is optional. You must configure a virtual directory in DeltaCopy server to accept user id/password before specifying it here.";

}
//---------------------------------------------------------------------------
void __fastcall TfrmMainClient::btnFetchModulesClick(TObject *Sender)
{

    TRsync rsync;
    Screen->Cursor = crHourGlass;
    int total = rsync.FetchModules(txtServer->Text, cmbModules->Items);
    Screen->Cursor = crDefault;

    if(total == -1){
        ShowMessage("Unable to fetch directory names. " + rsync.GetLastError());
    }else if(total > 0 && cmbModules->Text.Trim().Length() == 0){
        cmbModules->ItemIndex = 0;
    }

}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::btnNewProfileClick(TObject *Sender)
{
    AddNewProfile();    
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::lstProfilesDblClick(TObject *Sender)
{
    if(lstProfiles->ItemIndex == -1){
        return;
    }
    if(lstProfiles->Items->Strings[lstProfiles->ItemIndex] == "<Add New Profile>"){
        AddNewProfile();
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::lstProfilesClick(TObject *Sender)
{
    FillProfileControl();
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::txtServerExit(TObject *Sender)
{
    if(txtServer->Modified){
        SaveCurrentProfile();
    }
}
//---------------------------------------------------------------------------


void __fastcall TfrmMainClient::lstFileListKeyUp(TObject *Sender,
      WORD &Key, TShiftState Shift)
{
    if(Key == VK_DELETE){
        lstFileList->Items->Delete(lstFileList->ItemIndex);
        SaveCurrentProfile();
    }
}
//---------------------------------------------------------------------------


void __fastcall TfrmMainClient::btnAddFolderClick(TObject *Sender)
{
    AnsiString folderName = GenericUtils::BrowseForFolder("Add a folder", Handle);
    if(folderName.Length() > 0){
        AddAndFormatFileToList(folderName + "\\");
        SaveCurrentProfile();
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::chkUseRecursiveClick(TObject *Sender)
{
	if((pgMain->ActivePage == tbCopyOptions || pgMain->ActivePage == tbAuth) && !lstProfiles->Focused()){
        SaveCurrentProfile();
    }
}
//---------------------------------------------------------------------------


void __fastcall TfrmMainClient::btnDeleteAddTaskClick(TObject *Sender)
{
    AnsiString profileName = lstProfiles->Items->Strings[lstProfiles->ItemIndex];
    TProfile* selectedProfile = profileManager.GetProfile(profileName);

    if(selectedProfile == NULL){
        return;
    }

    if(btnDeleteAddTask->Caption == LABEL_ADD_SCHEDULE){
        AddTask(selectedProfile);
        taskManager.EditExistingTask(WideString(selectedProfile->GetTaskName()).c_bstr());
        FillTaskInfo(selectedProfile);
    }else{
        if(Ask("Are you sure you want to delete the schedule?")){
            WideString wStr(selectedProfile->GetTaskName());
            taskManager.DeleteTask(wStr.c_bstr());
            FillTaskInfo(selectedProfile);
        }
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::btnModifyClick(TObject *Sender)
{
    AnsiString profileName = lstProfiles->Items->Strings[lstProfiles->ItemIndex];
    TProfile* selectedProfile = profileManager.GetProfile(profileName);

    if(selectedProfile == NULL){
        return;
    }

    taskManager.EditExistingTask(WideString(selectedProfile->GetTaskName()).c_bstr());
}
//---------------------------------------------------------------------------


void __fastcall TfrmMainClient::RunNow1Click(TObject *Sender)
{

    if(lstProfiles->ItemIndex >= 1){
        statusWindow->ClearData();
        new TManualRunThread(this, false, false);
        statusWindow->ShowModal();
    }

}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::DeleteProfile1Click(TObject *Sender)
{
    RemoveProfile();    
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::btnEmailClick(TObject *Sender)
{
    TfrmEmailConf* cfg = new TfrmEmailConf(this);
    cfg->ShowModal();
    delete cfg;
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::DeleteEntry1Click(TObject *Sender)
{
    if(lstFileList->ItemIndex >= 0){
        lstFileList->Items->Delete(lstFileList->ItemIndex);
        SaveCurrentProfile();
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::ModifySelection1Click(TObject *Sender)
{

    if(lstFileList->ItemIndex >= 0){
        AnsiString sourceAndTarget = lstFileList->Items->Strings[lstFileList->ItemIndex];
        AnsiString source = TProfile::StripSource(sourceAndTarget);
        AnsiString target = TProfile::StripTarget(sourceAndTarget);

        AnsiString answer = InputBox("Modify file name",
                "Modify the value. You can put wild cards like *.txt",
                source);

        lstFileList->Items->Strings[lstFileList->ItemIndex] = answer + SOURCE_TARGET_DELIMITER + target;
        SaveCurrentProfile();
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::lstProfilesDrawItem(TWinControl *Control,
      int Index, TRect &Rect, TOwnerDrawState State)
{
    TRect cRect(0, 0, lstProfiles->Width, lstProfiles->Height);


    TCanvas*        pCanvas     = ((TListBox*)Control)->Canvas;

    pCanvas->FillRect(Rect);  //This clears the rect

    Graphics::TBitmap* bitmap = new Graphics::TBitmap();
    AnsiString moduleName = lstProfiles->Items->Strings[Index];

    TFontStyles oldStyle = pCanvas->Font->Style;
    if(Index == 0){
        pCanvas->Font->Style = TFontStyles() << fsBold;
        imgList->GetBitmap(1, bitmap);    //table image
    }else{
        imgList->GetBitmap(0, bitmap);    //table image
    }


    pCanvas->Draw(Rect.Left + 1, Rect.Top, bitmap);
    delete bitmap;

    pCanvas->TextOut(Rect.Left + 22, Rect.Top + 2, moduleName);
    pCanvas->Font->Style = oldStyle;

}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::lstFileListDrawItem(TWinControl *Control,
      int Index, TRect &Rect, TOwnerDrawState State)
{
    TRect cRect(0, 0, lstFileList->Width, lstFileList->Height);


    TCanvas*        pCanvas     = ((TListBox*)Control)->Canvas;

    pCanvas->FillRect(Rect);  //This clears the rect

    Graphics::TBitmap* bitmap = new Graphics::TBitmap();
    AnsiString moduleName = lstFileList->Items->Strings[Index];

    moduleName = TProfile::StripSource(moduleName);
    imgList->GetBitmap(2, bitmap);    //table image


    
    pCanvas->Draw(Rect.Left + 1, Rect.Top, bitmap);
    delete bitmap;

    pCanvas->TextOut(Rect.Left + 22, Rect.Top + 2, moduleName);
}
//---------------------------------------------------------------------------


void __fastcall TfrmMainClient::cmbModulesChange(TObject *Sender)
{
    SaveCurrentProfile();  
}
//---------------------------------------------------------------------------


void __fastcall TfrmMainClient::DisplayRunCommand1Click(TObject *Sender)
{
    statusWindow->ClearData();
    RunManually(true, false);
    statusWindow->ShowModal();
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::Restore1Click(TObject *Sender)
{
    if(Ask("This will restore files on your machine with the copy on the server. Are you sure?")){
        if(lstProfiles->ItemIndex >= 1){
            statusWindow->ClearData();
            new TManualRunThread(this, false, true);
            statusWindow->ShowModal();
        }
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::DisplayRestoreCommand1Click(
      TObject *Sender)
{
    statusWindow->ClearData();
    RunManually(true, true);
    statusWindow->ShowModal();
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::ModifyTargetPath1Click(TObject *Sender){
    //Target editor
    if(lstFileList->ItemIndex >= 0){
        AnsiString sourceAndTarget = lstFileList->Items->Strings[lstFileList->ItemIndex];
        AnsiString source = TProfile::StripSource(sourceAndTarget);
        AnsiString target = TProfile::StripTarget(sourceAndTarget);

        TfrmTargetEditor* dlg = new TfrmTargetEditor(this);
        if(dlg->DoModal(target)){
            lstFileList->Items->Strings[lstFileList->ItemIndex] =
                    source + SOURCE_TARGET_DELIMITER + target;
            SaveCurrentProfile();
        }
        delete dlg;
    }
}
//---------------------------------------------------------------------------


void __fastcall TfrmMainClient::ppmProfilesPopup(TObject *Sender)
{
    ShowStatusWindow1->Enabled = currentChildPid > 0;
    RunNow1->Enabled = currentChildPid == 0;
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::ShowStatusWindow1Click(TObject *Sender)
{
    statusWindow->ShowModal();
}
//---------------------------------------------------------------------------



void __fastcall TfrmMainClient::Content1Click(TObject *Sender)
{

    AnsiString helpFileName = ExtractFilePath(Application->ExeName) + "\\DeltaCopy.chm";

    ShellExecute(Handle,
                 "open",
                 helpFileName.c_str(),
                 NULL, NULL, SW_SHOWDEFAULT);

}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::AboutDeltaCopy1Click(TObject *Sender)
{
    AboutBox->ShowModal();
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainClient::SetRetryCount1Click(TObject *Sender)
{
    TDCConfig config(true);
    AnsiString answer = InputBox("Retry Count",
                "Specify a retry count.",
                config.GetRetryCount());
    if(answer.Length() > 0){
        config.SetRetryCount(answer);
    }

}
//---------------------------------------------------------------------------

