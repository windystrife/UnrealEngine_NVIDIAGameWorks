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


#pragma hdrstop

#include "Logger.h"


//---------------------------------------------------------------------------

#pragma package(smart_init)

TLogger *theLogger;

//---------------------------------------------------------------------------
TLogger::TLogger(){
    debugFileName = ExtractFilePath(Application->ExeName) + "dcLog.log";
    Initialize();
}

TLogger::TLogger(AnsiString fileName){
    debugFileName = fileName;
    Initialize();
}

//---------------------------------------------------------------------------
TLogger::~TLogger(){
    FileClose(fHandle);
}
//---------------------------------------------------------------------------
void TLogger::Initialize(){
    if(!FileExists(debugFileName)){
        fHandle = FileCreate(debugFileName);
        FileClose(fHandle);
    }

    fHandle = FileOpen(debugFileName, fmOpenWrite | fmShareDenyNone );

    FileSeek(fHandle, 0, 2);

    Log("--------------------------------------------------------");
}
//---------------------------------------------------------------------------
void TLogger::Log(AnsiString msg){
    AnsiString time = TDateTime::CurrentDateTime().DateTimeString();

    msg = time + " - " + msg + "\r\n";
    FileWrite(fHandle, msg.c_str(), msg.Length());
}
