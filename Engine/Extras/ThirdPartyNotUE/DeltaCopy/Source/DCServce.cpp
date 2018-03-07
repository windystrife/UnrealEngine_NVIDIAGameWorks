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

#include <SysUtils.hpp>
#include <SvcMgr.hpp>
#pragma hdrstop
#define Application Svcmgr::Application
USEFORM("DCServiceMain.cpp", DeltaCopyService); /* TService: File Type */
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try
	{
		Application->Initialize();
		Application->Title = "DeltaCopy Service";
         Application->CreateForm(__classid(TDeltaCopyService), &DeltaCopyService);
         Application->Run();
	}
	catch (Exception &exception)
	{
		Sysutils::ShowException(&exception, System::ExceptAddr());
	}
        catch(...)
        {
		try
		{
	        	throw Exception("");
		}
		catch(Exception &exception)
		{
			Sysutils::ShowException(&exception, System::ExceptAddr());
		}
        }
	return 0;
}
