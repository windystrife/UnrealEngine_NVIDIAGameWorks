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

#include "NewProfile.h"
#include "RSync.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmAddProfile *frmAddProfile;
//---------------------------------------------------------------------------
__fastcall TfrmAddProfile::TfrmAddProfile(TComponent* Owner)
    : TForm(Owner)
{
}

//---------------------------------------------------------------------------
bool TfrmAddProfile::DoModal(TProfile* aProfile, int& addSchedule){

    if(ShowModal() == mrOk){
        aProfile->SetProfileName(txtProfileName->Text);
        aProfile->SetServerIP(txtServer->Text);
        aProfile->SetModuleName(cmbModules->Text);
        if(chkAddSchedule->Checked){
            addSchedule = 1;
        }
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------
void __fastcall TfrmAddProfile::btnFetchModulesClick(TObject *Sender)
{

    if(txtServer->Text.Trim().Length() == 0){
        ShowMessage("Please provide a server name");
        return;
    }
    TRsync rsync;
    Screen->Cursor = crHourGlass;
    int total = rsync.FetchModules(txtServer->Text, cmbModules->Items);
    Screen->Cursor = crDefault;

    if(total == -1){
        ShowMessage("Unable to fetch directory names. " + rsync.GetLastError());
    }else if(total > 0){
        cmbModules->ItemIndex = 0;
    }

}
//---------------------------------------------------------------------------

void __fastcall TfrmAddProfile::btnAddProfileClick(TObject *Sender)
{
    if(txtProfileName->Text == ""){
        ShowMessage("You must specify a profile name. This is an arbitrary value used to identify this profile");
        return;
    }

    if(txtServer->Text == ""){
        ShowMessage("You must specify the server name. This is the host name or IP address of the the machine running rsycn server.");
        return;
    }

    if(cmbModules->Text == ""){
        ShowMessage("You must specify the virutal directory. Click on the button next to the combo box to fetch available directory names.");
        return;
    }
    ModalResult = mrOk;
}
//---------------------------------------------------------------------------

void __fastcall TfrmAddProfile::btnTestClick(TObject *Sender)
{
    if(txtServer->Text.Trim().Length() == 0){
        ShowMessage("Please provide a server name");
        return;
    }
    TRsync rsync;
    Screen->Cursor = crHourGlass;
    TStringList* list = new TStringList();
    int total = rsync.FetchModules(txtServer->Text, list);
    Screen->Cursor = crDefault;

    if(total == -1){
        ShowMessage("Unable to establish connection. " + rsync.GetLastError());
    }else if(total > 0){
        ShowMessage("Connection successfull");
    }
    delete list;
}
//---------------------------------------------------------------------------

