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

#ifndef RSyncH
#define RSyncH

#include <Classes.hpp>
#include <vector.h>
#include <string>
//---------------------------------------------------------------------------

class TRsync{
    AnsiString lastError;

    AnsiString GetErrorReason(DWORD code);
    bool IsError(AnsiString firstLine);
public:
    int FetchModules(AnsiString server, TStrings* results);
    AnsiString FixPath(AnsiString original, bool source);
    AnsiString GetLastError(){ return lastError;}
	int Run(AnsiString server, AnsiString parameters,
            	bool isSSH,
                AnsiString sourceFile,
                AnsiString moduleName,
				AnsiString targetDir,
				AnsiString userID,
				AnsiString password,
                vector<string>* list,
                HWND parentWindow,
                bool, bool);
};
#endif
