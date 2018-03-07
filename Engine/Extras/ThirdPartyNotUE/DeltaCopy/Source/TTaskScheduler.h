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

#ifndef TTaskSchedulerH
#define TTaskSchedulerH


#include <mstask.h>
#include <initguid.h>
#include <ole2.h>
#include <msterr.h>

#include <vector.h>

#include <string>

//---------------------------------------------------------------------------
struct TTaskParams{
    wstring accountName;
    wstring accountPwd;
    wstring appName;
    wstring comments;
    wstring creator;
    wstring appParameters;
    wstring workingDirectory;
    DWORD exitCode;
    WORD idleMinutes;
    WORD deadlineMinutes;
    DWORD maxRuntime;
    DWORD priority;
    SYSTEMTIME lastRun;
    SYSTEMTIME nextRun;
    HRESULT status;

    TTaskParams(){
        exitCode = -1;
        idleMinutes = -1;
        deadlineMinutes = -1;
        status -1;
    }

    void getStatusStr(char* buffer, HRESULT status);

};

class TTaskScheduler{

private:

    static int initialized;
    static ITaskScheduler *pITS;



    char lastError[512];




    ITask* Activate(wstring);
    int Save(ITask *pITask);

public:
    TTaskScheduler();



    int AddNewScheduledTask(wstring);
    int DeleteTask(wstring);
    void EditExistingTask(wstring);
    void Execute(wstring);
    int IsAvailable(wstring);
    char* GetLastError() {return lastError;}
    void GetScheduledItems(vector<wstring>* answer);
    int GetTaskInfo(wstring taskName, TTaskParams& taskParams);  //Returns 1 for success and 0 for failure
    void SetTaskProperties(wstring taskName, TTaskParams taskParams);

    static void Shutdown();
};
#endif
