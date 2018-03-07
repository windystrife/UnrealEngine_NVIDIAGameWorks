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

#ifndef NewProfileH
#define NewProfileH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <ExtCtrls.hpp>

#include "TProfile.h"
//---------------------------------------------------------------------------
class TfrmAddProfile : public TForm
{
__published:	// IDE-managed Components
    TPanel *Panel1;
    TPanel *Panel2;
    TPanel *Panel3;
    TLabel *Label1;
    TEdit *txtProfileName;
    TEdit *txtServer;
    TLabel *Label2;
    TComboBox *cmbModules;
    TBitBtn *btnFetchModules;
    TLabel *Label3;
    TCheckBox *chkAddSchedule;
    TButton *btnAddProfile;
    TButton *btnTest;
    TButton *btnCancel;
    void __fastcall btnFetchModulesClick(TObject *Sender);
    void __fastcall btnAddProfileClick(TObject *Sender);
    void __fastcall btnTestClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TfrmAddProfile(TComponent* Owner);

    bool DoModal(TProfile*, int& addSchedule);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmAddProfile *frmAddProfile;
//---------------------------------------------------------------------------
#endif
