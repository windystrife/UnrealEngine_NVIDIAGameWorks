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

#include "TargetEditor.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmTargetEditor *frmTargetEditor;
//---------------------------------------------------------------------------
__fastcall TfrmTargetEditor::TfrmTargetEditor(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
bool TfrmTargetEditor::DoModal(AnsiString& targetDir){
    txtTarget->Text = targetDir;

    if(ShowModal() == mrOk){
        targetDir = txtTarget->Text;
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------

