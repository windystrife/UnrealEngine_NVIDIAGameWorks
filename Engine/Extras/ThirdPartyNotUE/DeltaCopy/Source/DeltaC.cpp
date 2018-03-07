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
//---------------------------------------------------------------------------
USEFORM("MainFormClient.cpp", frmMainClient);
USEFORM("NewProfile.cpp", frmAddProfile);
USEFORM("StatusWindow.cpp", frmStatus);
USEFORM("EmailConf.cpp", frmEmailConf);
USEFORM("TargetEditor.cpp", frmTargetEditor);
USEFORM("AboutDC.cpp", AboutBox);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    try
    {
         Application->Initialize();
         Application->Title = "DeltaCopy Client";
         Application->CreateForm(__classid(TfrmMainClient), &frmMainClient);
		Application->CreateForm(__classid(TfrmStatus), &frmStatus);
		Application->CreateForm(__classid(TfrmTargetEditor), &frmTargetEditor);
		Application->CreateForm(__classid(TAboutBox), &AboutBox);
		Application->Run();
    }
    catch (Exception &exception)
    {
         Application->ShowException(&exception);
    }
    catch (...)
    {
         try
         {
             throw Exception("");
         }
         catch (Exception &exception)
         {
             Application->ShowException(&exception);
         }
    }
    return 0;
}
//---------------------------------------------------------------------------
