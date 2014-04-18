
#include "stdafx.h"

/** Window Service **/
VOID ServiceMainProc();
VOID Install(char* pPath, char* pName);
VOID UnInstall(char* pName);
VOID WriteLog(char* pFile, char* pMsg);
BOOL KillService(char* pName);
BOOL RunService(char* pName);
VOID ExecuteSubProcess();
VOID ProcMonitorThread(VOID *);
BOOL StartProcess(char* strAppPath);
VOID InitRunApps();
VOID StartTimerTask() ;

void GetSectionFromINI(LPCTSTR strSection, LPCTSTR strFilePath, std::vector<std::string> &vecRet) ;
TCHAR *GetCurPath(void) ;
BOOL IsFileExists(std::string strFilePath) ;

VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
VOID WINAPI ServiceHandler(DWORD fdwControl);


/** Window Service **/
const int nBufferSize = 500;
TCHAR pServiceName[nBufferSize+1];  //服务名
TCHAR pServiceDescription[nBufferSize+1];  //服务描述
CHAR pExeFile[nBufferSize+1];
CHAR lpCmdLineData[nBufferSize+1];
CHAR pLogFile[nBufferSize+1];
BOOL ProcessStarted = TRUE;

//Run Apps
std::map<std::string, SYSTEMTIME> g_mapRunAppsTime ;

CRITICAL_SECTION		myCS;
SERVICE_TABLE_ENTRY		lpServiceStartTable[] = 
{
	{pServiceName, ServiceMain},
	{NULL, NULL}
};


SERVICE_STATUS_HANDLE   hServiceStatusHandle; 
SERVICE_STATUS          ServiceStatus; 

int _tmain(int argc, _TCHAR* argv[])
{
	if(argc >= 2)
		strcpy_s(lpCmdLineData, argv[1]);
	ServiceMainProc();
	return 0;
}

VOID ServiceMainProc()
{
	::InitializeCriticalSection(&myCS);
	// initialize variables for .exe and .log file names
	char pModuleFile[nBufferSize+1];
	DWORD dwSize = GetModuleFileName(NULL, pModuleFile, nBufferSize);
	pModuleFile[dwSize] = 0;
	if(dwSize>4 && pModuleFile[dwSize-4] == '.')
	{
		sprintf_s(pExeFile,"%s",pModuleFile);
		pModuleFile[dwSize-4] = 0;
		sprintf_s(pLogFile,"%s.log",pModuleFile);
	}
	strcpy_s(pServiceName,"TimerRunService");
	strcpy_s(pServiceDescription, _T("提供商讯网服务器定时任务功能,一旦停止将影响定时任务执行")) ;

	if(_stricmp("-i",lpCmdLineData) == 0 || _stricmp("-I",lpCmdLineData) == 0)
		Install(pExeFile, pServiceName);
	else if(_stricmp("-k",lpCmdLineData) == 0 || _stricmp("-K",lpCmdLineData) == 0)
		KillService(pServiceName);
	else if(_stricmp("-u",lpCmdLineData) == 0 || _stricmp("-U",lpCmdLineData) == 0)
		UnInstall(pServiceName);
	else if(_stricmp("-r",lpCmdLineData) == 0 || _stricmp("-R",lpCmdLineData) == 0)
		RunService(pServiceName);
	else
		ExecuteSubProcess();
}

VOID Install(char* pPath, char* pName)
{  
	SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE); 
	if (schSCManager==0) 
	{
		long nError = GetLastError();
		char pTemp[121];
		sprintf_s(pTemp, "OpenSCManager failed, error code = %d\n", nError);
		WriteLog(pLogFile, pTemp);
	}
	else
	{
		SC_HANDLE schService = CreateService
		( 
			schSCManager,	/* SCManager database      */ 
			pName,			/* name of service         */ 
			pName,			/* service name to display */ 
			SERVICE_ALL_ACCESS,        /* desired access          */ 
			SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS , /* service type            */ 
			SERVICE_AUTO_START,      /* start type              */ 
			SERVICE_ERROR_NORMAL,      /* error control type      */ 
			pPath,			/* service's binary        */ 
			NULL,                      /* no load ordering group  */ 
			NULL,                      /* no tag identifier       */ 
			NULL,                      /* no dependencies         */ 
			NULL,                      /* LocalSystem account     */ 
			NULL
		);                     /* no password             */ 
		if (schService==0) 
		{
			long nError =  GetLastError();
			char pTemp[121];
			sprintf_s(pTemp, "Failed to create service %s, error code = %d\n", pName, nError);
			WriteLog(pLogFile, pTemp);
		}
		else
		{
			char pTemp[121];
			sprintf_s(pTemp, "Service %s installed\n", pName);
			WriteLog(pLogFile, pTemp);
			CloseServiceHandle(schService); 
		}
		CloseServiceHandle(schSCManager);
	}	
}

VOID UnInstall(char* pName)
{
	SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
	if (schSCManager==0) 
	{
		long nError = GetLastError();
		char pTemp[121];
		sprintf_s(pTemp, "OpenSCManager failed, error code = %d\n", nError);
		WriteLog(pLogFile, pTemp);
	}
	else
	{
		SC_HANDLE schService = OpenService( schSCManager, pName, SERVICE_ALL_ACCESS);
		if (schService==0) 
		{
			long nError = GetLastError();
			char pTemp[121];
			sprintf_s(pTemp, "OpenService failed, error code = %d\n", nError);
			WriteLog(pLogFile, pTemp);
		}
		else
		{
			if(!DeleteService(schService)) 
			{
				char pTemp[121];
				sprintf_s(pTemp, "Failed to delete service %s\n", pName);
				WriteLog(pLogFile, pTemp);
			}
			else 
			{
				char pTemp[121];
				sprintf_s(pTemp, "Service %s removed\n",pName);
				WriteLog(pLogFile, pTemp);
			}
			CloseServiceHandle(schService); 
		}
		CloseServiceHandle(schSCManager);	
	}
	DeleteFile(pLogFile);
}

VOID WriteLog(char* pFile, char* pMsg)
{
	// write error or other information into log file
	::EnterCriticalSection(&myCS);
	try
	{
		SYSTEMTIME oT;
		::GetLocalTime(&oT);
		FILE* pLog = fopen(pFile,"a");
		fprintf_s(pLog,"%02d/%02d/%04d, %02d:%02d:%02d\n    %s",oT.wMonth,oT.wDay,oT.wYear,oT.wHour,oT.wMinute,oT.wSecond,pMsg); 
		fclose(pLog);
	} catch(...) {}
	::LeaveCriticalSection(&myCS);
}

BOOL KillService(char* pName) 
{ 
	// kill service with given name
	SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
	if (schSCManager==0) 
	{
		long nError = GetLastError();
		char pTemp[121];
		sprintf_s(pTemp, "OpenSCManager failed, error code = %d\n", nError);
		WriteLog(pLogFile, pTemp);
	}
	else
	{
		// open the service
		SC_HANDLE schService = OpenService( schSCManager, pName, SERVICE_ALL_ACCESS);
		if (schService==0) 
		{
			long nError = GetLastError();
			char pTemp[121];
			sprintf_s(pTemp, "OpenService failed, error code = %d\n", nError);
			WriteLog(pLogFile, pTemp);
		}
		else
		{
			// call ControlService to kill the given service
			SERVICE_STATUS status;
			if(ControlService(schService,SERVICE_CONTROL_STOP,&status))
			{
				CloseServiceHandle(schService); 
				CloseServiceHandle(schSCManager); 
				return TRUE;
			}
			else
			{
				long nError = GetLastError();
				char pTemp[121];
				sprintf_s(pTemp, "ControlService failed, error code = %d\n", nError);
				WriteLog(pLogFile, pTemp);
			}
			CloseServiceHandle(schService); 
		}
		CloseServiceHandle(schSCManager); 
	}
	return FALSE;
}

BOOL RunService(char* pName) 
{ 
	// run service with given name
	SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
	if (schSCManager==0) 
	{
		long nError = GetLastError();
		char pTemp[121];
		sprintf_s(pTemp, "OpenSCManager failed, error code = %d\n", nError);
		WriteLog(pLogFile, pTemp);
	}
	else
	{
		// open the service
		SC_HANDLE schService = OpenService( schSCManager, pName, SERVICE_ALL_ACCESS);
		if (schService==0) 
		{
			long nError = GetLastError();
			char pTemp[121];
			sprintf_s(pTemp, "OpenService failed, error code = %d\n", nError);
			WriteLog(pLogFile, pTemp);
		}
		else
		{
			//设置服务的描述
			SERVICE_DESCRIPTION serviceDescrip ;
			serviceDescrip.lpDescription = pServiceDescription ;
			ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &serviceDescrip) ;

			// call StartService to run the service
			if(StartService(schService, 0, (const char**)NULL))
			{
				CloseServiceHandle(schService); 
				CloseServiceHandle(schSCManager); 
				char pTemp[121];
				sprintf_s(pTemp, "Service %s is running\n",pName);
				WriteLog(pLogFile, pTemp);
				return TRUE;
			}
			else
			{
				long nError = GetLastError();
				char pTemp[121];
				sprintf_s(pTemp, "StartService failed, error code = %d\n", nError);
				WriteLog(pLogFile, pTemp);
			}
			CloseServiceHandle(schService); 
		}
		CloseServiceHandle(schSCManager); 
	}
	return FALSE;
}


VOID ExecuteSubProcess()
{
	/*if(_beginthread(ProcMonitorThread, 0, NULL) == -1)
	{
		long nError = GetLastError();
		char pTemp[121];
		sprintf(pTemp, "StartService failed, error code = %d\n", nError);
		WriteLog(pLogFile, pTemp);
	}*/
	if(!StartServiceCtrlDispatcher(lpServiceStartTable))
	{
		long nError = GetLastError();
		char pTemp[121];
		sprintf_s(pTemp, "StartServiceCtrlDispatcher failed, error code = %d\n", nError);
		WriteLog(pLogFile, pTemp);
	}
	::DeleteCriticalSection(&myCS);
}

VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	DWORD   status = 0; 
    DWORD   specificError = 0xfffffff; 
 
    ServiceStatus.dwServiceType        = SERVICE_WIN32; 
    ServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
    ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE; 
    ServiceStatus.dwWin32ExitCode      = 0; 
    ServiceStatus.dwServiceSpecificExitCode = 0; 
    ServiceStatus.dwCheckPoint         = 0; 
    ServiceStatus.dwWaitHint           = 0; 
 
    hServiceStatusHandle = RegisterServiceCtrlHandler(pServiceName, ServiceHandler); 
    if (hServiceStatusHandle==0) 
    {
		long nError = GetLastError();
		char pTemp[121];
		sprintf_s(pTemp, "RegisterServiceCtrlHandler failed, error code = %d\n", nError);
		WriteLog(pLogFile, pTemp);
        return; 
    } 
 
    // Initialization complete - report running status 
    ServiceStatus.dwCurrentState       = SERVICE_RUNNING; 
    ServiceStatus.dwCheckPoint         = 0; 
    ServiceStatus.dwWaitHint           = 0;  
    if(!SetServiceStatus(hServiceStatusHandle, &ServiceStatus)) 
    { 
		long nError = GetLastError();
		char pTemp[121];
		sprintf_s(pTemp, "SetServiceStatus failed, error code = %d\n", nError);
		WriteLog(pLogFile, pTemp);
    } 

	InitRunApps();
	StartTimerTask() ;  
}

VOID InitRunApps()
{
	//加载配置文件
	TCHAR szIniFilePath[200] ;
	_tcscpy(szIniFilePath, GetCurPath() ) ;
	_tcscat(szIniFilePath,  _T("\\RunAppConfig.ini")) ;

	std::vector<std::string> vecRunApps ;
	GetSectionFromINI(_T("RunApp"), szIniFilePath, vecRunApps) ;

	TCHAR szAppPath[100] ;
	SYSTEMTIME timeRun ;
	timeRun.wYear = timeRun.wMonth = timeRun.wDay = timeRun.wDayOfWeek = timeRun.wMilliseconds = 0 ;
	std::string strAppPath, strRunTime ;

	int nSize = vecRunApps.size() ;
	for (int i=0 ; i<nSize; ++i )
	{
		int nPos = vecRunApps[i].find(_T("&")) ;
		if ( -1 == nPos )
		{
			continue; 
		}
		//解析文件路径
		strAppPath = vecRunApps[i] ;
		strAppPath.erase(nPos, strAppPath.length()-nPos) ;
		//解析运行时间
		strRunTime = vecRunApps[i] ;
		strRunTime.erase(0, nPos+1) ;
		sscanf(strRunTime.c_str(), _T("%d:%d:%d"), &timeRun.wHour, &timeRun.wMinute, &timeRun.wSecond) ;
		
		g_mapRunAppsTime[strAppPath] = timeRun ;	

		char pTemp[121];
		sprintf_s(pTemp, "定时任务%d : %s\n", i , vecRunApps[i].c_str());
		WriteLog(pLogFile, pTemp);
	}
}

TCHAR *GetCurPath(void)
{
	static TCHAR s_szCurPath[MAX_PATH] = {0};

	GetModuleFileName(NULL, s_szCurPath, MAX_PATH);

	TCHAR *p = _tcsrchr(s_szCurPath, '\\');
	*p = 0;

	return s_szCurPath;
}

void GetSectionFromINI(LPCTSTR strSection, LPCTSTR strFilePath, std::vector<std::string> &vecRet)
{
	TCHAR tcOperTypes[4048] ;
	memset(tcOperTypes , L'/0' , sizeof(tcOperTypes)) ;

	int nRetCount = ::GetPrivateProfileSection(strSection , tcOperTypes ,sizeof(tcOperTypes), strFilePath) ;

	std::string strKeyValue = _T("") ;
	for (int i=0 ; i<nRetCount ; ++i)
	{
		if (tcOperTypes[i] != L'\0')
		{
			strKeyValue += tcOperTypes[i] ;
		}
		else
		{
			int nPos = strKeyValue.find(_T("=")) ;
			if (nPos != -1)
			{
				strKeyValue.erase(nPos, strKeyValue.length()-nPos) ;
				vecRet.push_back( strKeyValue) ;
			}
			else
			{
				vecRet.push_back(strKeyValue) ;
			}

			strKeyValue.clear() ;
		}
	}
}

VOID StartTimerTask()
{
	SYSTEMTIME currentTime ;
	int nDifferMinites = 0 ;
	int nDifferHours = 0 ;
	std::map<std::string, SYSTEMTIME>::iterator mapIter ;
	while ( true && g_mapRunAppsTime.size() != 0)
	{
		Sleep(60000) ;
		GetLocalTime(&currentTime) ;
		mapIter = g_mapRunAppsTime.begin() ;
		for ( ; mapIter!=g_mapRunAppsTime.end(); ++mapIter)
		{
			nDifferHours = mapIter->second.wHour - currentTime.wHour ;
			nDifferMinites = mapIter->second.wMinute - currentTime.wMinute ;
			if ( nDifferHours == 0 && nDifferMinites > 0 && nDifferMinites <= 1 )
			{
				if ( !IsFileExists(mapIter->first) )
				{
					char pTemp[121];
					sprintf(pTemp, _T("程序文件不存在:%s"), mapIter->first.c_str());
					WriteLog(pLogFile, pTemp);

					g_mapRunAppsTime.erase(mapIter) ;
					continue;
				}
				ShellExecute(NULL, _T("open"), mapIter->first.c_str(), NULL, NULL, SW_HIDE) ;
			}
		}
	}

}

BOOL IsFileExists(std::string strFilePath)
{
	fstream file;
	file.open(strFilePath, ios::in);
	if(!file)
	{
		return FALSE ;
	}
	else
	{
		return TRUE ;
	}
}

VOID WINAPI ServiceHandler(DWORD fdwControl)
{
	switch(fdwControl) 
	{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			ProcessStarted = FALSE;
			ServiceStatus.dwWin32ExitCode = 0; 
			ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
			ServiceStatus.dwCheckPoint    = 0; 
			ServiceStatus.dwWaitHint      = 0;
			// terminate all processes started by this service before shutdown
			{
				g_mapRunAppsTime.clear() ;
			}
			break; 
		case SERVICE_CONTROL_PAUSE:
			ServiceStatus.dwCurrentState = SERVICE_PAUSED; 
			break;
		case SERVICE_CONTROL_CONTINUE:
			ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
			break;
		case SERVICE_CONTROL_INTERROGATE:
			break;
		default:
			break;
	};
    if (!SetServiceStatus(hServiceStatusHandle,  &ServiceStatus)) 
	{ 
		long nError = GetLastError();
		char pTemp[121];
		sprintf_s(pTemp, "SetServiceStatus failed, error code = %d\n", nError);
		WriteLog(pLogFile, pTemp);
    } 
}

//VOID EndProcess(int ProcessIndex)
//{
//	if(ProcessIndex >=0 && ProcessIndex <= MAX_NUM_OF_PROCESS)
//	{
//		if(pProcInfo[ProcessIndex].hProcess)
//		{
//			// post a WM_QUIT message first
//			PostThreadMessage(pProcInfo[ProcessIndex].dwThreadId, WM_QUIT, 0, 0);
//			Sleep(1000);
//			// terminate the process by force
//			TerminateProcess(pProcInfo[ProcessIndex].hProcess, 0);
//		}
//	}
//}
//VOID ProcMonitorThread(VOID *)
//{
//	while(ProcessStarted == TRUE)
//	{
//		DWORD dwCode;
//		for(int iLoop = 0; iLoop < MAX_NUM_OF_PROCESS; iLoop++)
//		{
//			if(::GetExitCodeProcess(pProcInfo[iLoop].hProcess, &dwCode) && pProcInfo[iLoop].hProcess != NULL)
//			{
//				if(dwCode != STILL_ACTIVE)
//				{
//					if(StartProcess(iLoop))
//					{
//						char pTemp[121];
//						sprintf(pTemp, "Restarted process %d\n", iLoop);
//						WriteLog(pLogFile, pTemp);
//					}
//				}
//			}
//		}
//	}
//}