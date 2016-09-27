#include "../../webit.h"
#include "../../event/wbt_event.h"
#include "wbt_service.h"

extern wbt_atomic_t wbt_wating_to_exit;

SERVICE_STATUS_HANDLE service_handle = 0;
static SERVICE_STATUS service_status;

/* Service control callback */
void __stdcall service_handler(DWORD fdwControl)
{
	switch(fdwControl){
		case SERVICE_CONTROL_CONTINUE:
			/* Continue from Paused state. */
			break;
		case SERVICE_CONTROL_PAUSE:
			/* Pause service. */
			break;
		case SERVICE_CONTROL_SHUTDOWN:
			/* System is shutting down. */
		case SERVICE_CONTROL_STOP:
			/* Service should stop. */
			service_status.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus(service_handle, &service_status);

            wbt_wating_to_exit = 1;
			break;
	}
}

/* Function called when started as a service. */
void __stdcall service_main(DWORD dwArgc, LPTSTR *lpszArgv) {
    char *argv[1];
    char conf_path[MAX_PATH + 20];
    int rc;

    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = SERVICE_START_PENDING;
    service_status.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;
    service_status.dwWin32ExitCode = NO_ERROR;
    service_status.dwCheckPoint = 0;

    service_handle = RegisterServiceCtrlHandler("webit", service_handler);
	if(!service_handle){
        service_status.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus( service_handle, &service_status );
        return;
    }

    rc = GetEnvironmentVariable( "WEBIT_DIR", conf_path, MAX_PATH );
    if( !rc || rc == MAX_PATH || !SetCurrentDirectory( conf_path ) ) {
        service_status.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus( service_handle, &service_status );
        return;
    }

    service_status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus( service_handle, &service_status );

    argv[0] = "webit";
    wbt_main( 1, argv );

	service_status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus( service_handle, &service_status );
}

void service_install() {
	SC_HANDLE sc_manager, svc_handle;
	char exe_path[MAX_PATH + 20];
	SERVICE_DESCRIPTION svc_desc;

	GetModuleFileName(NULL, exe_path, MAX_PATH);
	//strcat(exe_path, " run");

	sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if(sc_manager){
		svc_handle = CreateService(sc_manager, "webit", "Webit", 
				SERVICE_START | SERVICE_STOP | SERVICE_CHANGE_CONFIG,
				SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
				exe_path, NULL, NULL, NULL, NULL, NULL);

		if(svc_handle){
			svc_desc.lpDescription = "Message Queue and Broker";
			ChangeServiceConfig2(svc_handle, SERVICE_CONFIG_DESCRIPTION, &svc_desc);
			CloseServiceHandle(svc_handle);
		}
		CloseServiceHandle(sc_manager);
	}
}

void service_uninstall() {
	SC_HANDLE sc_manager, svc_handle;
	SERVICE_STATUS status;

	sc_manager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT);
	if(sc_manager){
		svc_handle = OpenService(sc_manager, "webit", SERVICE_QUERY_STATUS | DELETE);
		if(svc_handle){
			if(QueryServiceStatus(svc_handle, &status)){
				if(status.dwCurrentState == SERVICE_STOPPED){
					DeleteService(svc_handle);
				}
			}
			CloseServiceHandle(svc_handle);
		}
		CloseServiceHandle(sc_manager);
	}
}

void main() {
#ifdef WBT_DEBUG
	char *argv[1];
	argv[0] = "webit";
	wbt_main(1, argv);
#else
	SERVICE_TABLE_ENTRY ste[] = {
		{ "webit", service_main },
		{ NULL, NULL }
	};

	StartServiceCtrlDispatcher(ste);
#endif
}
