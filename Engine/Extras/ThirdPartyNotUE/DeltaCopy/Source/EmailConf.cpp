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

#include "EmailConf.h"
#include "DCConfig.h"
#include <smtpsend.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmEmailConf *frmEmailConf;
//---------------------------------------------------------------------------
__fastcall TfrmEmailConf::TfrmEmailConf(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TfrmEmailConf::btnCancelClick(TObject *Sender)
{
    ModalResult = mrCancel;    
}
//---------------------------------------------------------------------------

void __fastcall TfrmEmailConf::btnOkClick(TObject *Sender)
{
    TDCConfig config;
    config.SetNofityOnSuccess(chkSuccess->Checked);
	config.SetNotifyOnFailure(chkFailure->Checked);
	config.SetSaveLogToDisk(chkSaveLog->Checked);
    config.SetSmtpServer(txtServer->Text);
    config.SetRecipients(txtRecipients->Text);
	config.SetSendersEmail(txtSender->Text);
	config.SetSmtpUser(txtSmtpUser->Text);
	config.SetSmtpPass(txtSmtpPass->Text);

    ModalResult = mrOk;
}
//---------------------------------------------------------------------------

void __fastcall TfrmEmailConf::FormCreate(TObject *Sender)
{
    TDCConfig config(false);
    chkSuccess->Checked = config.IsNotifyOnSuccess();
    chkFailure->Checked = config.IsNotifyOnFailure();
    chkSaveLog->Checked = config.IsSaveLogToDisk();
	txtSender->Text = config.GetSendersEmail();
	txtServer->Text = config.GetSmtpServer();
	txtRecipients->Text = config.GetRecipients();
	txtSmtpUser->Text = config.GetSmtpUser();
	txtSmtpPass->Text = config.GetSmtpPass();

}
//---------------------------------------------------------------------------

void __fastcall TfrmEmailConf::btnTestClick(TObject *Sender)
{
	AnsiString msg;

	msg = "This is a test message generated from DeltaCopy client.";

	AnsiString subject = "Test message";

	TStringList* data = new TStringList();
	data->Text = msg;
	if(SendToEx(txtSender->Text, txtRecipients->Text, subject, txtServer->Text, data, txtSmtpUser->Text, txtSmtpPass->Text)){
		ShowMessage("Email sent successfully");
	}else{
		ShowMessage("Unable to send message. " + data->Text);
    }

	delete data;
}
//---------------------------------------------------------------------------

