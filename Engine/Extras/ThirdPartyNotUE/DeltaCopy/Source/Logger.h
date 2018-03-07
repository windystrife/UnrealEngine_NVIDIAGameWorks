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

#ifndef LoggerH
#define LoggerH

#include <Classes.hpp>
#include <Forms.hpp>

//---------------------------------------------------------------------------

class TLogger{
    AnsiString debugFileName;
    int fHandle;

    void Initialize();
public:
    TLogger();
    TLogger(AnsiString);
    ~TLogger();
    AnsiString GetDebugFileName(){return debugFileName;}
    void Log(AnsiString s);
};

extern TLogger *theLogger;
#endif
