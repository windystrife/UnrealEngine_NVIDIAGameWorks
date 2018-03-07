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

#include "MainFormServer.h"
#include "GenUtils.h"
#include "ServiceStatus.h"
#include "InstallService.h"
#include "DCConfig.h"
#include "AboutDC.h"
#include "ConsoleRunner.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"

TfrmMainFormServer *frmMainFormServer;
//---------------------------------------------------------------------------
__fastcall TfrmMainFormServer::TfrmMainFormServer(TComponent* Owner)
    : TForm(Owner)
{
}

//---------------------------------------------------------------------------
void TfrmMainFormServer::AddANewModule(){
    AnsiString name = InputBox("New Virtual Directory",
        "Enter an alias WITHOUT spaces",
        "ANewName");

    if(name.Length() > 0){
        adapter.AddANewModule(name);
        LoadModuleNames();
    }


}
//---------------------------------------------------------------------------
void TfrmMainFormServer::FillModuleValues(){

    if(lstModules->ItemIndex < 1){
        txtPath->Text = "";
        txtComment->Text = "";
        chkReadonly->Checked = false;
        txtPath->Enabled = false;
        txtComment->Enabled = false;
        lblPath->Enabled = false;
        lblComment->Enabled = false;
        btnBrowse->Enabled = false;
		chkReadonly->Enabled = false;
		chkUseAuth->Checked = false;
		chkUseAuth->Enabled = false;
		txtUID->Enabled = false;
		lblUID->Enabled = false;
		lblTIP->Enabled = false;
		txtPWD->Enabled = false;
		lblPWD->Enabled = false;

        return;
    }

    txtPath->Enabled = true;
    txtComment->Enabled = true;
    lblPath->Enabled = true;
    lblComment->Enabled = true;
    btnBrowse->Enabled = true;
    chkReadonly->Enabled = true;
    chkUseAuth->Enabled = true;

    AnsiString selectedModule = lstModules->Items->Strings[lstModules->ItemIndex];

	txtPath->Text = GenericUtils::ConvertPathCygwinToWindows(adapter.GetParamValue(selectedModule, "path"));
	txtComment->Text = adapter.GetParamValue(selectedModule, "comment");
	txtUID->Text = adapter.GetParamValue(selectedModule, "auth users");
    txtPWD->Text = "";


	if(txtUID->Text.Trim().Length() > 0){
		//The value for secrets file is actually a file, not the password. Therefore, I have to read the
		//value from that file.
		AnsiString secretFile = adapter.GetParamValue(selectedModule, "secrets file");

		if(secretFile.Trim().Length() == 0){
			txtPWD->Text = "";
		}else{

			txtPWD->Text = "";  //Assume there was an error reading the password.
			secretFile = GenericUtils::ConvertPathCygwinToWindows(secretFile);

			if(FileExists(secretFile)){
				TStringList* sList = new TStringList();
				sList->LoadFromFile(secretFile);

				for(int i = 0; i < sList->Count; i++){
					AnsiString userName = txtUID->Text + ":";
					if(sList->Strings[i].Trim().Pos(userName) == 1){
						//Found the appropriate user name.
						txtPWD->Text = sList->Strings[i].Trim().SubString(userName.Length() + 1, 100);
					}
				}
				delete sList;
			}
		}

		chkUseAuth->Checked = true;
		txtUID->Enabled = true;
		lblUID->Enabled = true;
		lblTIP->Enabled = true;
		txtPWD->Enabled = true;
		lblPWD->Enabled = true;
	}else{
		//If User Name is blank, I assume no authentication is used.
		chkUseAuth->Checked = false;
		txtUID->Enabled = false;
		lblUID->Enabled = false;
		lblTIP->Enabled = false;
		txtPWD->Enabled = false;
		lblPWD->Enabled = false;
	}
    chkReadonly->Checked = adapter.GetParamValue(selectedModule, "read only").LowerCase() == "true";

    txtComment->Modified = false;
}


//---------------------------------------------------------------------------
void TfrmMainFormServer::InstallNTService(){


    AnsiString path = "\"" + ExtractFilePath(Application->ExeName) + "DCServce.exe\"";


    TfrmServiceInstallParams* dlg = new TfrmServiceInstallParams(this);
    AnsiString uid;
    AnsiString pwd;
    if(dlg->DoModal(uid, pwd)){
        TServiceInfo service(true);
        if(service.CreateNewService(NT_SERVICE_NAME,
                "DeltaCopy Server", path,
                uid, pwd)){

            ShowMessage("Service created successfully");
        }else{
            ShowMessage("Service creation failed. " + service.GetLastErrorStr());
        }

    }
    delete dlg;



}
//---------------------------------------------------------------------------
void TfrmMainFormServer::LoadModuleNames(){

    lstModules->Items->Clear();
    lstModules->Items->Add("<Add New Directory>");

    adapter.GetModuleNames(lstModules->Items);
}

//---------------------------------------------------------------------------
void TfrmMainFormServer::SaveModuleValues(){
    if(lstModules->ItemIndex < 1){
        return;
    }

   
    AnsiString selectedModule = lstModules->Items->Strings[lstModules->ItemIndex];

	adapter.SetParamValue(selectedModule, "path", GenericUtils::ConvertPathWindowsToCygwin(txtPath->Text));
    adapter.SetParamValue(selectedModule, "comment", txtComment->Text);
    adapter.SetParamValue(selectedModule, "read only", chkReadonly->Checked ? "true" : "false");

	if(txtUID->Text.Trim().Length() > 0 && txtPWD->Text.Trim().Length() > 0 && chkUseAuth->Checked){
		adapter.SetParamValue(selectedModule, "auth users", txtUID->Text.Trim());


		//Now save the secret file
		AnsiString absoluteSecret = ExtractFilePath(Application->ExeName) + SECRET_DIR;
		if(!DirectoryExists(absoluteSecret)){
			ForceDirectories(absoluteSecret);
		}


		AnsiString secretFileName = absoluteSecret + selectedModule + ".secret";
		TStringList* sList = new TStringList();
		sList->Add(txtUID->Text.Trim() + ":" + txtPWD->Text.Trim());
		sList->SaveToFile(secretFileName);
		delete sList;

		adapter.SetParamValue(selectedModule, "secrets file", GenericUtils::ConvertPathWindowsToCygwin(secretFileName));
	}else{
		adapter.SetParamValue(selectedModule, "auth users", "");
		adapter.SetParamValue(selectedModule, "secrets file", "");
	}
	adapter.SaveConfig();



	txtComment->Modified = false;
	txtUID->Modified = false;
	txtPWD->Modified = false;
}

//---------------------------------------------------------------------------
void TfrmMainFormServer::UninstallNTService(){
    TServiceInfo service(true);
    if(service.DeleteExistingService(NT_SERVICE_NAME)){
        ShowMessage("Successfully removed");
        return;
    }else{
        ShowMessage("Error: " + service.GetLastErrorStr());
    }
}

//---------------------------------------------------------------------------
void TfrmMainFormServer::UpdateServiceStatus(){
    TServiceInfo service;

    int status = service.CheckStatus("DeltaCopyService");


    btnInstallService->Visible = (status == 0);
    imgMain->Visible = (status != 0);
    lblInstallService->Visible = (status == 0);

    switch(status){
        case SERVICE_STOPPED:
            if(imgMain->Tag != imgStopped->Tag){
                imgMain->Picture = imgStopped->Picture;
                imgMain->Tag = imgStopped->Tag;
            }
            btnStop->Enabled = false;
            btnStart->Enabled = true;

            return;

        case SERVICE_RUNNING:
            if(imgMain->Tag != imgRunning->Tag){
                imgMain->Picture = imgRunning->Picture;
                imgMain->Tag = imgRunning->Tag;
            }
            btnStop->Enabled = true;
            btnStart->Enabled = false;
            return;
        default:
            if(imgMain->Tag != imgStarting->Tag){
                imgMain->Picture = imgStarting->Picture;
                imgMain->Tag = imgStarting->Tag;
            }
            btnStop->Enabled = false;
            btnStart->Enabled = false;
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMainFormServer::FormShow(TObject *Sender)
{
    UpdateServiceStatus();
    LoadModuleNames();
}
//---------------------------------------------------------------------------
void __fastcall TfrmMainFormServer::btnStartClick(TObject *Sender)
{
    TServiceInfo service;
    if(!service.RunService(NT_SERVICE_NAME)){
        ShowMessage("Could not start the service. " + service.GetLastErrorStr());
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMainFormServer::tmrMainTimer(TObject *Sender)
{
    UpdateServiceStatus();
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::btnStopClick(TObject *Sender)
{
    TServiceInfo service;
    if(!service.StopService(NT_SERVICE_NAME)){
        ShowMessage("Could not stop the service. " + service.GetLastErrorStr());
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::btnCloseClick(TObject *Sender)
{
    Close();    
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::btnBrowseClick(TObject *Sender)
{
    AnsiString folder = GenericUtils::BrowseForFolder("Virtual Directory Folder", Handle);
    if(folder.Length() > 0){
        txtPath->Text = folder;
        SaveModuleValues();
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::lstModulesClick(TObject *Sender)
{
    FillModuleValues();    
}
//---------------------------------------------------------------------------


void __fastcall TfrmMainFormServer::txtCommentExit(TObject *Sender)
{
	if(txtComment->Modified || txtUID->Modified || txtPWD->Modified){
        SaveModuleValues();
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::chkReadonlyClick(TObject *Sender)
{
    if(chkReadonly->Focused()){
        SaveModuleValues();
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::lstModulesDblClick(TObject *Sender)
{
    if(lstModules->ItemIndex == 0){
        AddANewModule();
    }

}


//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::lstModulesDrawItem(
      TWinControl *Control, int Index, TRect &Rect, TOwnerDrawState State)
{
    TRect cRect(0, 0, lstModules->Width, lstModules->Height);


    TCanvas*        pCanvas     = ((TListBox*)Control)->Canvas;

    pCanvas->FillRect(Rect);  //This clears the rect

    Graphics::TBitmap* bitmap = new Graphics::TBitmap();
    AnsiString moduleName = lstModules->Items->Strings[Index];

    if(Index == 0){
        imgList->GetBitmap(1, bitmap);    //table image
    }else{
        imgList->GetBitmap(0, bitmap);    //table image
    }

    
    pCanvas->Draw(Rect.Left + 1, Rect.Top, bitmap);
    delete bitmap;

    pCanvas->TextOut(Rect.Left + 22, Rect.Top + 2, moduleName);


}

//---------------------------------------------------------------------------



void __fastcall TfrmMainFormServer::btnInstallServiceClick(TObject *Sender)
{
    InstallNTService();
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::FormCreate(TObject *Sender)
{
    TDCConfig config(true);
    config.SetAppPath(ExtractFilePath(Application->ExeName));
    pgMain->ActivePage = tbStatus; 
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::AddNewDirectory1Click(TObject *Sender)
{
    AddANewModule();
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::RenameDirectory1Click(TObject *Sender)
{
    if(lstModules->ItemIndex < 1){
        return;
    }

	AnsiString oldName = lstModules->Items->Strings[lstModules->ItemIndex];

    AnsiString newName = InputBox("Rename Virtual Directory",
        "Enter an alias WITHOUT spaces",
        oldName);

    if(newName.Length() > 0){
        adapter.RenameModule(oldName, newName);
        LoadModuleNames();
        adapter.SaveConfig();
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::DeleteDirectory1Click(TObject *Sender)
{
    if(lstModules->ItemIndex < 1){
        return;
    }

	AnsiString name = lstModules->Items->Strings[lstModules->ItemIndex];

    AnsiString question = "Are you sure you want to delete the " + name;
    if(MessageDlg(question, mtConfirmation, TMsgDlgButtons() << mbYes << mbNo, 0) == 6){
        adapter.RemoveModule(name);
        LoadModuleNames();
        adapter.SaveConfig();
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::Exit1Click(TObject *Sender)
{
    Close();    
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::DeltaCopyHelp1Click(TObject *Sender)
{
    AnsiString helpFileName = ExtractFilePath(Application->ExeName) + "\\DeltaCopy.chm";

    ShellExecute(Handle,
                 "open",
                 helpFileName.c_str(),
                 NULL, NULL, SW_SHOWDEFAULT);    
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::About1Click(TObject *Sender)
{
    AboutBox->ShowModal();
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::chkUseAuthClick(TObject *Sender)
{

	if(chkUseAuth->Focused()){
		if(chkUseAuth->Checked){
			ShowMessage("IMPORTANT\r\n\r\nIf you decide to use authentication on DeltaCopy server, make sure to \r\n"
						"specify a user id and password in DeltaCopy client as well. Failure to do so\r\n"
						"will cause the DeltaCopy client to hang at runtime.");
		}

		txtUID->Enabled = chkUseAuth->Checked;
		lblUID->Enabled = chkUseAuth->Checked;
		lblTIP->Enabled = chkUseAuth->Checked;
		txtPWD->Enabled = chkUseAuth->Checked;
		lblPWD->Enabled = chkUseAuth->Checked;


		SaveModuleValues();
	}
}
//---------------------------------------------------------------------------
bool TfrmMainFormServer::FixPermissions(AnsiString path){
	TConsoleRunner cRunner;

	AnsiString cmdLine = "chmod -Rv a+rwX " + path;
	vector<string> results;

	cRunner.Run(cmdLine, &results, NULL);


	AnsiString msg;

	for(unsigned int i = 0; i < results.size(); i++){
		msg += results.at(i).data();
	}

	DWORD exitCode = cRunner.GetExitCode();

	if(exitCode > 0){

		if(msg.Length()){
			ShowMessage(msg);
		}


		msg = cRunner.GetLastError();

		if(msg.Length()){
			ShowMessage(msg);
		}

		return false;
	}

	return true;
}
//---------------------------------------------------------------------------

void __fastcall TfrmMainFormServer::FixFilePermissions1Click(TObject *Sender)
{
	//
	AnsiString name = lstModules->Items->Strings[lstModules->ItemIndex];
	AnsiString path = adapter.GetParamValue(name, "path");

	if(FixPermissions(path)){
     	ShowMessage("Permissions successfully updated.");
	}
}
//---------------------------------------------------------------------------

