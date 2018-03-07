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

#include "InstallService.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmServiceInstallParams *frmServiceInstallParams;
//---------------------------------------------------------------------------
__fastcall TfrmServiceInstallParams::TfrmServiceInstallParams(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
bool TfrmServiceInstallParams::DoModal(AnsiString& uid, AnsiString& pwd){

    if(ShowModal() == mrOk){
        uid = txtUser->Text;
        pwd = txtPwd->Text;


        if(uid.Pos("\\") == 0){
            uid = ".\\" + uid;
		}

		if(chkLocalService->Checked){
			uid = NULL;
			pwd = "";
        }
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------

void __fastcall TfrmServiceInstallParams::chkLocalServiceClick(TObject *Sender)
{
	txtUser->Enabled = !chkLocalService->Checked;
	txtPwd->Enabled = !chkLocalService->Checked;
	Label2->Enabled = !chkLocalService->Checked;
	Label3->Enabled = !chkLocalService->Checked;
}
//---------------------------------------------------------------------------

