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
#include "DCServiceMain.h"
#include "DCConfig.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"

TDeltaCopyService *DeltaCopyService;
//---------------------------------------------------------------------------
__fastcall TDeltaCopyService::TDeltaCopyService(TComponent* Owner)
	: TService(Owner)
{
}

TServiceController __fastcall TDeltaCopyService::GetServiceController(void)
{
	return (TServiceController) ServiceController;
}

void __stdcall ServiceController(unsigned CtrlCode)
{
	DeltaCopyService->Controller(CtrlCode);
}
//---------------------------------------------------------------------------
void __fastcall TDeltaCopyService::ServiceExecute(TService *Sender)
{
    TDCConfig config(false);
    AnsiString path = config.GetAppPath();

    if(path == ""){
        path = "c:\\DeltaService.log";
    }else{
        path = path + "DeltaService.log";
    }
    theLogger = new TLogger(path);
    theLogger->Log("DeltaCopy Service Starting up...");

    
	
    try {

        new TChildProcessThread(this);
        while (!Terminated){
           ServiceThread->ProcessRequests(true);
        }

        for(unsigned int i = 0; i < results.size(); i++){
            theLogger->Log(results.at(i).c_str());
        }

        if(results.size() == 0){
            theLogger->Log("No results were generated");
        }
        childInfo.childHandle = cRunner.GetChildProcessHandle();
        childInfo.childPid = cRunner.GetChildPid();

        if(!TerminateProcess(childInfo.childHandle, 0)){
            theLogger->Log("Unable to terminate rsync process.");
        }

    }
    __finally
    {

    }
    theLogger->Log("DeltaCopy Service Terminating");

}

//---------------------------------------------------------------------------
void TDeltaCopyService::SetCurrentDir(){
    TDCConfig config(false);
    AnsiString path = config.GetAppPath();

    if(path == ""){
        theLogger->Log("Unable to locate application path in the registry. Service won't run correctly. "
        "Profile application path in \\HKEY_LOCAL_MACHINE\\SOFTWARE\\Synametrics\\DeltaCopy\\AppPath");
        return;
    }

    SetCurrentDirectory(path.c_str());

}
//---------------------------------------------------------------------------
void TDeltaCopyService::SpawnChildProcess(){

    SetCurrentDir();
    AnsiString cmdLine = "rsync.exe -v --daemon --config=deltacd.conf --no-detach";  //--config=deltacd.conf

    //cmdLine = "SlowChildApp.exe";
    if(cRunner.Run(cmdLine, &results, NULL)){
        theLogger->Log("rsync thread successfully terminated...");
    }else{

        theLogger->Log("Unable to start rsync daemon. " + cRunner.GetLastError());
    }
}
//---------------------------------------------------------------------------


