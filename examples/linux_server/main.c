/**
 * @file examples/linux_server/main.c
 * @brief Basic Linux UDS server example using socketcan ISO-TP
 */
#include "iso14229.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

static UDSServer_t srv;
static UDSTpIsoTpSock_t tp;
static bool done = false;
static int sleep_ms(uint32_t tms);

void sigint_handler(int signum) {
    printf("SIGINT received\n");
    done = true;
}

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    printf("event: %d\n", ev);
    if(ev >= UDS_EVT_MAX){
	    printf("Unhandled event: %d\n", ev);
        return UDS_NRC_ServiceNotSupported;
    }
    UDSDiagSessCtrlArgs_t *p = (UDSDiagSessCtrlArgs_t *)arg;
    switch (ev)
    {
        case UDS_EVT_DiagSessCtrl:
            printf("DiagSessCtrl: 0x%02X\n", p->type);
            if (p->type >= 0x01 && p->type <= 0x04){
                return UDS_PositiveResponse;
            }
            else{
                return UDS_NRC_SubFunctionNotSupported;
            }
        case UDS_EVT_EcuReset:
            printf("EcuReset\n");
            return UDS_PositiveResponse;

        case UDS_EVT_ClearDiagnosticInfo:
            printf("ClearDiagnosticInfo\n");
            return UDS_PositiveResponse;

        case UDS_EVT_ReadDTCInformation:
            printf("ReadDTCInformation\n");
            return UDS_PositiveResponse;

        case UDS_EVT_ReadDataByIdent:
            printf("ReadDataByIdent\n");
            return UDS_PositiveResponse;

        case UDS_EVT_ReadMemByAddr:
            printf("ReadMemByAddr\n");
            return UDS_PositiveResponse;

        case UDS_EVT_CommCtrl:
            printf("CommCtrl\n");
            return UDS_PositiveResponse;

        case UDS_EVT_SecAccessRequestSeed:
            printf("SecAccessRequestSeed\n");
            return UDS_PositiveResponse;

        case UDS_EVT_SecAccessValidateKey:
            printf("SecAccessValidateKey\n");
            return UDS_PositiveResponse;

        case UDS_EVT_WriteDataByIdent:
            printf("WriteDataByIdent\n");
            return UDS_PositiveResponse;

        case UDS_EVT_WriteMemByAddr:
            printf("WriteMemByAddr\n");
            return UDS_PositiveResponse;

        case UDS_EVT_DynamicDefineDataId:
            printf("DynamicDefineDataId\n");
            return UDS_PositiveResponse;

        case UDS_EVT_IOControl:
            printf("IOControl\n");
            return UDS_PositiveResponse;

        case UDS_EVT_RoutineCtrl:
            printf("RoutineCtrl\n");
            return UDS_PositiveResponse;

        case UDS_EVT_RequestDownload:
            printf("RequestDownload\n");
            return UDS_PositiveResponse;

        case UDS_EVT_RequestUpload:
            printf("RequestUpload\n");
            return UDS_PositiveResponse;

        case UDS_EVT_TransferData:
            printf("TransferData\n");
            return UDS_PositiveResponse;

        case UDS_EVT_RequestTransferExit:
            printf("RequestTransferExit\n");
            return UDS_PositiveResponse;

        case UDS_EVT_SessionTimeout:
            printf("SessionTimeout\n");
            return UDS_PositiveResponse;

        case UDS_EVT_DoScheduledReset:
            printf("DoScheduledReset\n");
            return UDS_PositiveResponse;

        case UDS_EVT_RequestFileTransfer:
            printf("RequestFileTransfer\n");
            return UDS_PositiveResponse;

        case UDS_EVT_ControlDTCSetting:
            printf("ControlDTCSetting\n");
            return UDS_PositiveResponse;

        case UDS_EVT_LinkControl:
            printf("LinkControl\n");
            return UDS_PositiveResponse;

        case UDS_EVT_Custom:
            printf("Custom\n");
            return UDS_PositiveResponse;        
	default:
            printf("Unhandled event: %d\n", ev);
            return UDS_NRC_ServiceNotSupported;
    }
}

int main(int ac, char **av) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    // 1. Initialize a transport
    if (UDSTpIsoTpSockInitServer(&tp, "can0", 0x7E0, 0x7E8, 0x7DF)) {
        fprintf(stderr, "UDSTpIsoTpSockInitServer failed\n");
        exit(-1);
    }

    if (UDSServerInit(&srv)) {
        fprintf(stderr, "UDSServerInit failed\n");
    }

    srv.tp = (UDSTp_t *)&tp;
    srv.fn = fn;

    printf("server up, polling . . .\n");
    while (!done) {
        UDSServerPoll(&srv);
        sleep_ms(1);
    }
    printf("server exiting\n");
    return 0;
}

static int sleep_ms(uint32_t tms) {
    struct timespec ts;
    int ret;
    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;
    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);
    return ret;
}
