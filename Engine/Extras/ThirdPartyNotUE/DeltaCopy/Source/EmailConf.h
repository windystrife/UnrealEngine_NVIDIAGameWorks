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

#ifndef EmailConfH
#define EmailConfH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
class TfrmEmailConf : public TForm
{
__published:	// IDE-managed Components
    TPanel *Panel1;
    TPanel *Panel2;
    TPanel *Panel3;
    TLabel *Label1;
    TLabel *Label2;
    TEdit *txtServer;
    TLabel *Label3;
    TEdit *txtRecipients;
    TCheckBox *chkSuccess;
    TCheckBox *chkFailure;
    TButton *btnOk;
    TButton *btnCancel;
    TEdit *txtSender;
    TLabel *Label4;
	TButton *btnTest;
	TLabel *Label5;
	TEdit *txtSmtpUser;
	TEdit *txtSmtpPass;
	TLabel *Label6;
	TCheckBox *chkSaveLog;
    void __fastcall btnCancelClick(TObject *Sender);
    void __fastcall btnOkClick(TObject *Sender);
    void __fastcall FormCreate(TObject *Sender);
	void __fastcall btnTestClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TfrmEmailConf(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmEmailConf *frmEmailConf;
//---------------------------------------------------------------------------
#endif
