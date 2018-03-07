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

#include "TTaskScheduler.h"
#include <stdio.h>

//---------------------------------------------------------------------------

#pragma package(smart_init)


int TTaskScheduler::initialized = 0;
ITaskScheduler* TTaskScheduler::pITS = NULL;


void TTaskParams::getStatusStr(char* buffer, HRESULT phrStatus){
    switch(phrStatus){
        case SCHED_S_TASK_READY:
           strcpy(buffer, "Ready");
           break;
        case SCHED_S_TASK_RUNNING:
           strcpy(buffer, "Running");
           break;
        case SCHED_S_TASK_NOT_SCHEDULED:
           strcpy(buffer, "Not Scheduled");
           break;
        default:
           strcpy(buffer, "Unknown");
    }
}


TTaskScheduler::TTaskScheduler(){

    HRESULT hr;
    if(!initialized){
        hr = CoInitialize(NULL);
        hr = CoCreateInstance(CLSID_CTaskScheduler,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_ITaskScheduler,
                          (void **) &pITS);
        if(FAILED(hr)){
            sprintf(lastError, "Unable to initialize COM library for Task Scheduler, error = 0x%x\n", hr);
        }
        initialized = 1;
    }




}

//---------------------------------------------------------------------------

ITask* TTaskScheduler::Activate(wstring taskName){
    ITask *pITask;

    HRESULT hr = pITS->Activate(taskName.data(),
                      IID_ITask,
                      (IUnknown**) &pITask);



    if (FAILED(hr)){
        sprintf(lastError, "Failed calling ITaskScheduler::Activate, error = 0x%x\n", hr);
        return NULL;
    }

    return pITask;
}
//---------------------------------------------------------------------------
int TTaskScheduler::AddNewScheduledTask(wstring taskName){

    /////////////////////////////////////////////////////////////////
    // Call ITaskScheduler::NewWorkItem to create new task.
    /////////////////////////////////////////////////////////////////
    ITask *pITask;



    HRESULT hr = pITS->NewWorkItem(taskName.data(),           // Name of task
                         CLSID_CTask,            // Class identifier
                         IID_ITask,              // Interface identifier
                         (IUnknown**)&pITask); // Address of task interface



    if (FAILED(hr)){
        sprintf(lastError, "Failed calling NewWorkItem, error = 0x%x\n",hr);
        return 0;
    }


    Save(pITask);

    pITask->Release();
    return 1;

    
}
//---------------------------------------------------------------------------

int TTaskScheduler::DeleteTask(wstring taskName){
    HRESULT hr = pITS->Delete(taskName.data());
    if(hr == S_OK){
        return 1;
    }
    sprintf(lastError, "Unable to delete a task, error = 0x%x\n",hr);
    return 0;
}
//---------------------------------------------------------------------------
void TTaskScheduler::EditExistingTask(wstring taskName){
    
    ITask *pITask = Activate(taskName);

    if(pITask == NULL){
        return;
    }

    ///////////////////////////////////////////////////////////////////
    // Call ITask::EditWorkItem. Note that this method is
    // inherited from IScheduledWorkItem.
    ///////////////////////////////////////////////////////////////////
    HWND hParent = NULL;
    DWORD dwReserved = 0;

    HRESULT hr = pITask->EditWorkItem(hParent, dwReserved);
    // Release ITask interface
    pITask->Release();
    if (FAILED(hr)){
        sprintf(lastError, "Failed calling ITask::EditWorkItem, error = 0x%x\n", hr);
        return;
    }
}
//---------------------------------------------------------------------------
void TTaskScheduler::Execute(wstring taskName){
    ITask *pITask = Activate(taskName);

    if(pITask == NULL){
        return;
    }



    ///////////////////////////////////////////////////////////////////
    // Call ITask::Run to start execution of "Test Task".
    ///////////////////////////////////////////////////////////////////

    HRESULT hr = pITask->Run();
    if (FAILED(hr)){
        sprintf(lastError, "Failed calling ITask::Run, error = 0x%x\n", hr);
        return;
    }


    pITask->Release();

}
//---------------------------------------------------------------------------
/*
* Returns a list of scheduled items
* @param
* @return
*/
void TTaskScheduler::GetScheduledItems(vector<wstring>* answer){

    IEnumWorkItems *pIEnum;
    HRESULT hr = pITS->Enum(&pIEnum);

    if (FAILED(hr)){
        return;
    }


    LPWSTR *lpwszNames;
    DWORD dwFetchedTasks = 0;
    while (SUCCEEDED(pIEnum->Next(5,
                                &lpwszNames,
                                &dwFetchedTasks))
                  && (dwFetchedTasks != 0)){

        while (dwFetchedTasks){
           //wprintf(L"%s\n", lpwszNames[--dwFetchedTasks]);
           wstring aString(lpwszNames[--dwFetchedTasks]);
           answer->push_back(aString);

           char buffer[255];
           sprintf(buffer, "Size is %d", answer->size());
           OutputDebugString(buffer);

           CoTaskMemFree(lpwszNames[dwFetchedTasks]);
        }
        CoTaskMemFree(lpwszNames);
    }

    pIEnum->Release();
}
//---------------------------------------------------------------------------
int TTaskScheduler::GetTaskInfo(wstring taskName, TTaskParams& taskParams){
    ITask *pITask = Activate(taskName);

    if(pITask == NULL){
        return 0;
    }

    LPWSTR ppwszAccountName;
    LPWSTR ppwszComment;
    LPWSTR ppwszCreator;
    LPWSTR lpwszApplicationName;
    LPWSTR lpwszParameters;
    LPWSTR lpwszWorkDir;

    DWORD pdwExitCode;
    DWORD pdwRunTime;
    DWORD pdwPriority;

    WORD pwIdleMinutes;
    WORD pwDeadlineMinutes;
    SYSTEMTIME pstLastRun;
    SYSTEMTIME pstNextRun;
    HRESULT phrStatus;


  
    HRESULT hr = pITask->GetAccountInformation(&ppwszAccountName);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
        taskParams.accountName = L"";
    }else{
        taskParams.accountName = ppwszAccountName;
        CoTaskMemFree(ppwszAccountName);
    }

    hr = pITask->GetApplicationName(&lpwszApplicationName);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.appName = lpwszApplicationName;
        CoTaskMemFree(lpwszApplicationName);
    }

    hr = pITask->GetComment(&ppwszComment);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.comments = ppwszComment;
        CoTaskMemFree(ppwszComment);
    }

    hr = pITask->GetCreator(&ppwszCreator);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.creator = ppwszCreator;
        CoTaskMemFree(ppwszCreator);
    }

    hr = pITask->GetExitCode(&pdwExitCode);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.exitCode = pdwExitCode;
    }

    hr = pITask->GetIdleWait(&pwIdleMinutes, &pwDeadlineMinutes);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.deadlineMinutes = pwDeadlineMinutes;
        taskParams.idleMinutes = pwIdleMinutes;
    }

    hr = pITask->GetMaxRunTime(&pdwRunTime);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.maxRuntime = pdwRunTime;
    }

    hr = pITask->GetMostRecentRunTime(&pstLastRun);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.lastRun = pstLastRun;
    }

    hr = pITask->GetNextRunTime(&pstNextRun);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.nextRun = pstNextRun;
    }

    hr = pITask->GetParameters(&lpwszParameters);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.appParameters = lpwszParameters;
        CoTaskMemFree(lpwszParameters);
    }

    hr = pITask->GetPriority(&pdwPriority);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.priority = pdwPriority;
    }

    hr = pITask->GetStatus(&phrStatus);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.status = phrStatus;
    }

    hr = pITask->GetWorkingDirectory(&lpwszWorkDir);
    if (FAILED(hr)){
        sprintf(lastError, "Failed to retreive task info, error = 0x%x\n",hr);
    }else{
        taskParams.workingDirectory = lpwszWorkDir;
        CoTaskMemFree(lpwszWorkDir);
    }

    pITask->Release();
    return 1;

}

//---------------------------------------------------------------------------
/*
*
* @return Return a positive value if the task is found. Otherwise a 0 is returned.
*/
int TTaskScheduler::IsAvailable(wstring taskName){
    vector<wstring> answer;

    GetScheduledItems(&answer);

    for(unsigned int i = 0; i < answer.size(); i++){
        if(taskName == answer.at(i).data()){
            return 1;
        }
    }
    return 0;
}

//---------------------------------------------------------------------------
int TTaskScheduler::Save(ITask *pITask){
    /////////////////////////////////////////////////////////////////
    // Call IUnknown::QueryInterface to get a pointer to
    // IPersistFile and IPersistFile::Save to save
    // the new task to disk.
    /////////////////////////////////////////////////////////////////
    IPersistFile *pIPersistFile;
    HRESULT hr = pITask->QueryInterface(IID_IPersistFile,
                              (void **)&pIPersistFile);


    if (FAILED(hr)){
        sprintf(lastError, "Failed calling NewWorkItem, error = 0x%x\n",hr);
        return 0;
    }


    hr = pIPersistFile->Save(NULL, TRUE);
    pIPersistFile->Release();
    if (FAILED(hr)){
        sprintf(lastError, "Failed calling NewWorkItem, error = 0x%x\n", hr);
        return 0;
    }

    return 1;
}
//---------------------------------------------------------------------------
void TTaskScheduler::SetTaskProperties(wstring taskName, TTaskParams taskParams){
    ITask *pITask = Activate(taskName);

    if(pITask == NULL){
        return;
    }


    if(taskParams.accountName.length() > 0 && taskParams.accountPwd.length() > 0){
        pITask->SetAccountInformation(taskParams.accountName.data(),
                                      taskParams.accountPwd.data());
    }

    if(taskParams.appName.length() > 0){
        pITask->SetApplicationName(taskParams.appName.data());
    }

    if(taskParams.appParameters.length() > 0){
        pITask->SetParameters(taskParams.accountName.data());
    }

    if(taskParams.comments.length() > 0){
        pITask->SetComment(taskParams.comments.data());
    }

    if(taskParams.workingDirectory.length() > 0){
        pITask->SetWorkingDirectory(taskParams.workingDirectory.data());
    }

    Save(pITask);
    pITask->Release();

}


//---------------------------------------------------------------------------
void TTaskScheduler::Shutdown(){
    if(pITS != NULL){
        pITS->Release();
    }

    CoUninitialize();
}
