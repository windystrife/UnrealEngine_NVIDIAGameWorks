//---------------------------------------------------------------------------
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

#pragma hdrstop

#include "ConsoleRunner.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)



//Since this is a call back method, I did not write it in the class
BOOL CALLBACK TerminateAppEnum( HWND hwnd, LPARAM lParam ){
    DWORD dwID ;

    GetWindowThreadProcessId(hwnd, &dwID) ;

    if(dwID == (DWORD)lParam)
    {
     PostMessage(hwnd, WM_CLOSE, 0, 0) ;
    }

    return TRUE ;
}

//---------------------------------------------------------------------------
bool TConsoleRunner::CreateChildProcess(AnsiString cmdLine){
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bFuncRetn = FALSE;

    // Set up members of the PROCESS_INFORMATION structure.
    ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

    // Set up members of the STARTUPINFO structure.

    ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hChildStdoutWr;
    siStartInfo.hStdOutput = hChildStdoutWr;
    siStartInfo.hStdInput = hChildStdinRd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;

    siStartInfo.wShowWindow = SW_HIDE;

// Create the child process. 
    
    bFuncRetn = CreateProcess(NULL,
      cmdLine.c_str(),       // command line
      NULL,          // process security attributes 
      NULL,          // primary thread security attributes
      TRUE,          // handles are inherited 
      0,             // creation flags
      NULL,          // use parent's environment 
      NULL,          // use parent's current directory 
      &siStartInfo,  // STARTUPINFO pointer
      &piProcInfo);  // receives PROCESS_INFORMATION 
   
   if (bFuncRetn == 0){
      lastError = "Failed to create process";
      return false;
   }else{
      //CloseHandle(piProcInfo.hProcess);
      //CloseHandle(piProcInfo.hThread);
      childProcessHandle = piProcInfo.hProcess;
      childPid = piProcInfo.dwProcessId;
   }
   return bFuncRetn;
}


//---------------------------------------------------------------------------
bool TConsoleRunner::IsAlive(DWORD pid){
    HANDLE   hProc ;
    int   dwRet ;

    // If we can't open the process with PROCESS_TERMINATE rights,
    // then we give up immediately.
    hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);

    return (hProc != NULL);
}
//---------------------------------------------------------------------------
void TConsoleRunner::ReadFromPipe(vector<string>* answer, HWND parentHandle){
   DWORD dwRead, dwWritten; 
   CHAR chBuf[4096];

	// Close the write end of the pipe before reading from the
    // read end of the pipe.
 
   if (!CloseHandle(hChildStdoutWr)){
      lastError = "Closing handle failed";
   }

// Read output from the child process, and write to parent's STDOUT.

   //if(!WriteFile(hChildStdinWr, "drowssap\r", 9, &dwWritten, NULL)){
   //		OutputDebugString("oops");
   //}



   while(true){
		setmem(chBuf, 4096, 0);
		if( !ReadFile( hChildStdoutRd, chBuf, 4096, &dwRead, NULL) || dwRead == 0){
            break;
        }

        answer->push_back(chBuf);
        if(parentHandle){
            SendMessage(parentHandle, MSG_TO_STDOUT, answer->size() - 1, 0);
        }
        Application->ProcessMessages();
   }

}

/*
* Runs a console based program and puts the results in the "results params"
*
* @param cmdLine Complete path of the executible
* @param results Output parameter that will hold all the results.
* @return
*/
bool TConsoleRunner::Run(AnsiString cmdLine, vector<string>* results, HWND parentHandle){
    SECURITY_ATTRIBUTES saAttr; 
    BOOL fSuccess;
 
    // Set the bInheritHandle flag so pipe handles are inherited.

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Get the handle to the current STDOUT.

    //hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
 
    // Create a pipe for the child process's STDOUT.
 
    if (! CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)){
        lastError = "Stdout pipe creation failed";
        return false;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited.

    SetHandleInformation( hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

    // Create a pipe for the child process's STDIN.
 
    if (! CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)){
        lastError = "Stdin pipe creation failed";
        return false;
    }

    // Ensure the write handle to the pipe for STDIN is not inherited.
 
	SetHandleInformation( hChildStdinWr, HANDLE_FLAG_INHERIT, 0);
 
    // Now create the child process.
   
    fSuccess = CreateChildProcess(cmdLine);
    if (! fSuccess){
        lastError = "Create process failed with";
        return false;
    }

    if(parentHandle != NULL){
        SendMessage(parentHandle, PROCESS_STARTED, childPid, (LPARAM)childProcessHandle);
    }
    // Read from pipe that is the standard output for child process.
    ReadFromPipe(results, parentHandle);


    WaitForSingleObject(childProcessHandle, 4000);
    
    if(GetExitCodeProcess(childProcessHandle, &exitCode) == FALSE){
        exitCode = -1;
    }

    if(exitCode == STILL_ACTIVE){
        exitCode = -1;
    }

    if(parentHandle != NULL){
        SendMessage(parentHandle, PROCESS_TERMINATED, childPid, (LPARAM)childProcessHandle);
    }
    return true;
 

}

//---------------------------------------------------------------------------
int TConsoleRunner::TerminateApp(){
    return TerminateApp(childPid, 5000, true);
}
//---------------------------------------------------------------------------
int TConsoleRunner::TerminateApp(DWORD dwPID, DWORD dwTimeout, bool force ){
    //Read http://support.microsoft.com/default.aspx?scid=KB;en-us;q178893
    //     http://www.codeproject.com/threads/killprocess.asp
    //for more details on this method.


    HANDLE   hProc ;
    int   dwRet ;

    // If we can't open the process with PROCESS_TERMINATE rights,
    // then we give up immediately.
    hProc = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, dwPID);

    if(hProc == NULL){
        lastError = "Unable to get a handle for the running process";
        return 0 ;
    }

    if(force){
        return (TerminateProcess(hProc,0)? 1 : 2);
    }
    // TerminateAppEnum() posts WM_CLOSE to all windows whose PID
    // matches your process's.
    EnumWindows((WNDENUMPROC)TerminateAppEnum, (LPARAM) dwPID) ;

    // Wait on the handle. If it signals, great. If it times out,
    // then you kill it.
    if(WaitForSingleObject(hProc, dwTimeout) != WAIT_OBJECT_0){
        dwRet = (TerminateProcess(hProc,0)? 1 : 2);
    }else{
        dwRet = 1 ;
     }

    CloseHandle(hProc) ;

    return dwRet ;
}



//---------------------------------------------------------------------------
AnsiString TConsoleRunner::Unix2Dos(AnsiString input){
    AnsiString result;

    for(int i = 1; i <= input.Length(); i++){
        if(input[i] == '\n'){

            if((i > 1 && input[i - 1] != '\r') || i == 0){
                result += "\r\n";
            }
        }else{
            result += input[i];
        }
    }
    return result;

}
