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

#ifndef ConsoleRunnerH
#define ConsoleRunnerH
//---------------------------------------------------------------------------

#include <vector.h>
#include <string>
#include <Classes.hpp>
#include <Forms.hpp>


#define MSG_TO_STDOUT WM_USER + 1000
#define PROCESS_STARTED WM_USER + 1001
#define PROCESS_TERMINATED WM_USER + 1002


class TConsoleRunner{

    HANDLE hChildStdinRd, hChildStdinWr,
    hChildStdoutRd, hChildStdoutWr;

    HANDLE childProcessHandle;
    DWORD  childPid;
    AnsiString lastError;
    DWORD exitCode;


    bool CreateChildProcess(AnsiString);
    void ReadFromPipe(vector<string>* answer, HWND parentHandle);
public:
    TConsoleRunner(){

    }

    HANDLE GetChildProcessHandle() {return childProcessHandle;}
    DWORD GetChildPid() {return childPid;}
    DWORD GetExitCode() {return exitCode;}
    AnsiString GetLastError(){ return lastError;}
    bool IsAlive(DWORD pid);
    bool Run(AnsiString cmdLine, vector<string>* answer, HWND parentHandle);
    int TerminateApp();
    int TerminateApp(DWORD dwPID, DWORD dwTimeout, bool);
    static AnsiString Unix2Dos(AnsiString);

};
#endif
