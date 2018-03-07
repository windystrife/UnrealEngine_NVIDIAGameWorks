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

#ifndef StatusWindowH
#define StatusWindowH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <Buttons.hpp>

#define TERMINATE_RSYNC_TASK WM_USER + 100

//---------------------------------------------------------------------------
class TfrmStatus : public TForm
{
__published:	// IDE-managed Components
    TPanel *Panel1;
    TMemo *txtStatus;
    TBitBtn *btnClose;
    TBitBtn *btnTerminate;
    void __fastcall btnCloseClick(TObject *Sender);
    void __fastcall btnTerminateClick(TObject *Sender);
private:	// User declarations
    HWND parentHandle;
public:		// User declarations
    __fastcall TfrmStatus(TComponent* Owner);
    void AddData(AnsiString string);
    void ClearData();

    void SetParentHandle(HWND h) { parentHandle = h;}
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmStatus *frmStatus;
//---------------------------------------------------------------------------
#endif
