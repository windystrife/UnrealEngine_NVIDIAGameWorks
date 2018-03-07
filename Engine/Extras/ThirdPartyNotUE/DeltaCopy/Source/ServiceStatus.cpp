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

#include "ServiceStatus.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)


//---------------------------------------------------------------------------

TServiceInfo::TServiceInfo(){
    hSC = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (hSC == NULL){
        lastError = GetLastError();
    }

}

//---------------------------------------------------------------------------

TServiceInfo::TServiceInfo(bool forCreateService){
    hSC = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE | SERVICE_QUERY_STATUS | DELETE);
    if (hSC == NULL){
        lastError = GetLastError();
    }

}

//---------------------------------------------------------------------------

int TServiceInfo::CheckStatus(AnsiString serviceName){

    SC_HANDLE   hSchSvc = NULL;
    hSchSvc = OpenService(hSC,
                          serviceName.c_str(),
                          SERVICE_QUERY_STATUS);

    SERVICE_STATUS SvcStatus;
    
    if (QueryServiceStatus(hSchSvc, &SvcStatus) == FALSE){
        CloseServiceHandle(hSchSvc);
        lastError = GetLastError();

        switch(lastError){
            case ERROR_ACCESS_DENIED:
                lastErrorStr = "The specified handle was not opened with SERVICE_QUERY_STATUS access.";
                break;
            case ERROR_INVALID_HANDLE:
                lastErrorStr = "The specified handle is invalid.";
                break;
            default:
                lastErrorStr = "Unknown error occurred";
        }

        return STATUS_ERROR;
    }

    CloseServiceHandle(hSchSvc);

    return SvcStatus.dwCurrentState;


}
//---------------------------------------------------------------------------
int TServiceInfo::CreateNewService(AnsiString serviceName,
            AnsiString displayName, AnsiString path,
            AnsiString userId, AnsiString pwd){

    SC_HANDLE retVal = CreateService(hSC, serviceName.c_str(), //Service Name
                displayName.c_str(), //Display Name
                SC_MANAGER_ALL_ACCESS,     //Desired Access
                SERVICE_WIN32_OWN_PROCESS, //Service Type
                SERVICE_AUTO_START, //Start Type
                SERVICE_ERROR_NORMAL, //Error Control
                path.c_str(), NULL, //Load Order
                NULL, //Tag ID
                NULL, //NO dependencies
                userId != NULL ? userId.c_str() : NULL, //User ID
                pwd.c_str());

    if(retVal > 0){
        return 1;
    }

    lastError = GetLastError();

    switch(lastError){
        case ERROR_ACCESS_DENIED:
            lastErrorStr = "The handle to the SCM database does not have the SC_MANAGER_CREATE_SERVICE access right.";
            break;
        case ERROR_CIRCULAR_DEPENDENCY:
            lastErrorStr = "A circular service dependency was specified.";
            break;
        case ERROR_DUPLICATE_SERVICE_NAME:
            lastErrorStr = "The display name already exists in the service control manager database either as a service name or as another display name.";
            break;
        case ERROR_INVALID_HANDLE:
            lastErrorStr = "The handle to the specified service control manager database is invalid.";
            break;
        case ERROR_INVALID_NAME:
            lastErrorStr = "The specified service name is invalid.";
            break;
        case ERROR_INVALID_PARAMETER:
            lastErrorStr = "A parameter that was specified is invalid.";
            break;
        case ERROR_INVALID_SERVICE_ACCOUNT:
            lastErrorStr = "The user account name specified in the lpServiceStartName parameter does not exist.";
            break;
        case ERROR_SERVICE_EXISTS:
            lastErrorStr = "The specified service already exists in this database.";
            break;

        default:
            lastErrorStr = "Unknown error occurred";

    }
    return 0;

}
//---------------------------------------------------------------------------
int TServiceInfo::DeleteExistingService(AnsiString serviceName){
    SC_HANDLE   hSchSvc = NULL;
    hSchSvc = OpenService(hSC,
                          serviceName.c_str(),
                          SERVICE_START | SERVICE_QUERY_STATUS | DELETE);

    SERVICE_STATUS SvcStatus;

    if (DeleteService(hSchSvc) == FALSE){
        CloseServiceHandle(hSchSvc);
        lastError = GetLastError();

        switch(lastError){
        case ERROR_ACCESS_DENIED:
            lastErrorStr = "The handle does not have the DELETE access right.";
            break;
        case ERROR_INVALID_HANDLE:
            lastErrorStr = "The handle to the specified service control manager database is invalid.";
            break;
        case ERROR_SERVICE_MARKED_FOR_DELETE:
            lastErrorStr = "The specified service has already been marked for deletion.";
            break;
        default:
            lastErrorStr = "Unknown error occurred";

        }
        return STATUS_ERROR;
    }

    return STATUS_SUCCESS;
}
//---------------------------------------------------------------------------
int TServiceInfo::RunService(AnsiString serviceName){
    SC_HANDLE   hSchSvc = NULL;
    hSchSvc = OpenService(hSC,
                          serviceName.c_str(),
                          SERVICE_START | SERVICE_QUERY_STATUS);

    SERVICE_STATUS SvcStatus;

    if (QueryServiceStatus(hSchSvc, &SvcStatus) == FALSE){
        CloseServiceHandle(hSchSvc);
        lastError = GetLastError();
        return STATUS_ERROR;
    }

    if(SvcStatus.dwCurrentState == SERVICE_RUNNING){
        //Service already running.
        CloseServiceHandle(hSchSvc);
        lastErrorStr = "Service is already running";
        return STATUS_ERROR;
    }

    if (::StartService(hSchSvc, 0, NULL) == FALSE){
        CloseServiceHandle(hSchSvc);
        lastError = GetLastError();

        switch(lastError){
            case ERROR_ACCESS_DENIED:
                lastErrorStr = "The specified handle was not opened with SERVICE_QUERY_STATUS access.";
                break;
            case ERROR_INVALID_HANDLE:
                lastErrorStr = "The specified handle is invalid.";
                break;
            case ERROR_PATH_NOT_FOUND:
                lastErrorStr = "The service binary file could not be found.";
                break;
            case ERROR_SERVICE_ALREADY_RUNNING:
                lastErrorStr = "An instance of the service is already running.";
                break;
            case ERROR_SERVICE_DATABASE_LOCKED:
                lastErrorStr = "The database is locked.";
                break;
            case ERROR_SERVICE_DEPENDENCY_DELETED:
                lastErrorStr = "The service depends on a service that does not exist or has been marked for deletion.";
                break;
            case ERROR_SERVICE_DEPENDENCY_FAIL:
                lastErrorStr = "The service depends on another service that has failed to start.";
                break;
            case ERROR_SERVICE_DISABLED:
                lastErrorStr = "The service has been disabled.";
                break;
            case ERROR_SERVICE_LOGON_FAILED:
                lastErrorStr = "The service could not be logged on. Check user id and password specified for login";
                break;
            case ERROR_SERVICE_MARKED_FOR_DELETE:
                lastErrorStr = "The service has been marked for deletion.";
                break;
            case ERROR_SERVICE_NO_THREAD:
                lastErrorStr = "A thread could not be created for the Win32 service.";
                break;
            case ERROR_SERVICE_REQUEST_TIMEOUT	:
                lastErrorStr = "The service did not respond to the start request in a timely fashion.";
                break;

            default:
                lastErrorStr = "Unknown error occurred";
        }
        return STATUS_ERROR;
    }

    CloseServiceHandle(hSchSvc);

    return STATUS_SUCCESS;
}
//---------------------------------------------------------------------------
int TServiceInfo::StopService(AnsiString serviceName){
    SC_HANDLE   hSchSvc = NULL;
    hSchSvc = OpenService(hSC,
                          serviceName.c_str(),
                          SERVICE_STOP | SERVICE_QUERY_STATUS);

    SERVICE_STATUS SvcStatus;

    if (QueryServiceStatus(hSchSvc, &SvcStatus) == FALSE){
        CloseServiceHandle(hSchSvc);
        lastError = GetLastError();
        return STATUS_ERROR;
    }

    if(SvcStatus.dwCurrentState != SERVICE_RUNNING ){
        //Service is not running.
        CloseServiceHandle(hSchSvc);
        return STATUS_ERROR;
    }


    if (ControlService(hSchSvc, SERVICE_CONTROL_STOP, &SvcStatus) == 0){
        lastError = GetLastError();
        CloseServiceHandle(hSchSvc);

        switch(lastError){
            case ERROR_ACCESS_DENIED:
                lastErrorStr = "The specified handle was not opened with SERVICE_QUERY_STATUS access.";
                break;
            case ERROR_DEPENDENT_SERVICES_RUNNING:
                lastErrorStr = "The service cannot be stopped because other running services are dependent on it.";
                break;
            case ERROR_INVALID_SERVICE_CONTROL:
                lastErrorStr = "The requested control code is not valid, or it is unacceptable to the service.";
                break;
            case ERROR_SERVICE_CANNOT_ACCEPT_CTRL:
                lastErrorStr = "The requested control code cannot be sent to the service because the state of the service is SERVICE_STOPPED, SERVICE_START_PENDING, or SERVICE_STOP_PENDING.";
                break;
            case ERROR_SERVICE_NOT_ACTIVE:
                lastErrorStr = "The service has not been started.";
                break;
            case ERROR_SERVICE_REQUEST_TIMEOUT:
                lastErrorStr = "The service did not respond to the start request in a timely fashion.";
                break;

            default:
                lastErrorStr = "Unknown error occurred";
        }
   
        return STATUS_ERROR;
    }

    CloseServiceHandle(hSchSvc);

    return STATUS_SUCCESS;
}

