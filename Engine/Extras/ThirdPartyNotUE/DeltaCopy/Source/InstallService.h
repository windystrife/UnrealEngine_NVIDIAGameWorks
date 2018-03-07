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

#ifndef InstallServiceH
#define InstallServiceH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
class TfrmServiceInstallParams : public TForm
{
__published:	// IDE-managed Components
    TPanel *Panel1;
    TPanel *Panel2;
    TButton *btnOk;
    TButton *btnCancel;
    TPanel *Panel3;
    TLabel *Label1;
    TLabel *Label2;
    TEdit *txtUser;
    TLabel *Label3;
    TEdit *txtPwd;
    TLabel *Label4;
    TLabel *Label5;
	TCheckBox *chkLocalService;
	void __fastcall chkLocalServiceClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TfrmServiceInstallParams(TComponent* Owner);
    bool DoModal(AnsiString&, AnsiString&);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmServiceInstallParams *frmServiceInstallParams;
//---------------------------------------------------------------------------
#endif
