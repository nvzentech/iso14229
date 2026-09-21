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
#include <cjson/cJSON.h>


static UDSServer_t srv;
static UDSTpIsoTpSock_t tp;
static bool done = false;
static int sleep_ms(uint32_t tms);


#define JSON_FILE_PATH "/home/rpi/can_uds_cloud_project/iso14229/data.json"

typedef struct
{
    char boot_software_id[11];                  // F180: "BOOT_1.0.0"
    char application_software_id[10];           // F181: "APP_1.2.3"
    char application_data_id[11];               // F182: "DATA_1.0.0"

    char boot_software_fingerprint[14];         // F183: "BLFP_20260905"
    char application_software_fingerprint[14];  // F184: "SWFP_20260905"
    char application_data_fingerprint[14];      // F185: "DAFP_20260905"

    uint8_t active_diagnostic_session;          // F186: 0x03

    char vehicle_manufacturer_spare_part[14];   // F187: "SPARE-ECU-001"
    char vehicle_manufacturer_ecu_sw_number[11]; // F188: "ECU-SW-001"
    char vehicle_manufacturer_ecu_sw_version[7]; // F189: "V1.2.3"

    char system_supplier_id[13];                // F18A: "SUPPLIER-001"
    char ecu_manufacturing_date[9];             // F18B: "20260905"
    char serial[8];                             // F18C: "SN12345"
    char supported_functional_units[11];        // F18D: "UDS-OBD-CAN"
    char kit_assembly_part_number[12];          // F18E: "KIT-ECU-001"
    char regulation_sw_id[11];                  // F18F: "REGSW-001"

    char vvin[18];                              // F190: "MYVIN1234567890AB"

    char vehicle_manufacturer_ecu_hw_number[12]; // F191: "HW-ECU-001"
    char system_supplier_ecu_hw_number[12];      // F192: "SUP-HW-001"
    char system_supplier_ecu_hw_version[8];      // F193: "HW-V1.0"
    char system_supplier_ecu_sw_number[12];      // F194: "SUP-SW-001"
    char system_supplier_ecu_sw_version[11];     // F195: "SUP-V1.2.3"

    char exhaust_type_approval_number[13];       // F196: "TYPE-APP-001"
    char system_name_engine_type[12];            // F197: "TEST-ENGINE"
    char repair_shop_tester_id[9];               // F198: "SHOP-001"
    char programming_date[9];                    // F199: "20260901"

    char calibration_repair_shop_id[13];         // F19A: "CAL-TOOL-001"
    char calibration_date[9];                    // F19B: "20260825"
    char calibration_software_number[11];        // F19C: "CAL-SW-1.0"
    char ecu_installation_date[9];               // F19D: "20260820"
    char odx_file_reference[13];                 // F19E: "ODX-TEST-001"
    char entity_data[11];                        // F19F: "ENTITY-001"

} json_data_t;

json_data_t json_data;

void sigint_handler(int signum) {
    printf("SIGINT received\n");
    done = true;
}

static int load_json_data(const char *filename)
{
    FILE *fp;
    long file_size;
    char *buffer;
    cJSON *root;
    cJSON *dids;
    cJSON *item;

    fp = fopen(filename, "rb");

    if (fp == NULL)
    {
        perror("Failed to open JSON file");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    if (file_size <= 0)
    {
        fclose(fp);
        printf("JSON file is empty\n");
        return -1;
    }

    buffer = malloc(file_size + 1);

    if (buffer == NULL)
    {
        fclose(fp);
        printf("Memory allocation failed\n");
        return -1;
    }

    size_t read_size = fread(buffer, 1, file_size, fp);

    fclose(fp);

    buffer[read_size] = '\0';

    root = cJSON_Parse(buffer);

    free(buffer);

    if (root == NULL)
    {
        printf("Failed to parse JSON\n");
        return -1;
    }

    dids = cJSON_GetObjectItem(root, "DIDs");

    if (!cJSON_IsArray(dids))
    {
        printf("DIDs array not found\n");

        cJSON_Delete(root);

        return -1;
    }

    cJSON_ArrayForEach(item, dids)
    {
        cJSON *did_json =
            cJSON_GetObjectItem(item, "did");

        cJSON *value_json =
            cJSON_GetObjectItem(item, "value");

        if (!cJSON_IsString(did_json))
        {
            continue;
        }

        unsigned long did =
            strtoul(did_json->valuestring, NULL, 0);

        if (did == 0xF186)
        {
            if (cJSON_IsString(value_json))
            {
                json_data.active_diagnostic_session =
                    (uint8_t)strtoul(
                        value_json->valuestring,
                        NULL,
                        0
                    );
            }

            printf("DID F186 loaded: 0x%02X\n",
                json_data.active_diagnostic_session);

            continue;
        }

        /*
        * All other DIDs are strings.
        */
        if (!cJSON_IsString(value_json))
        {
            continue;
        }

        switch (did)
        {
            case 0xF180:
                strncpy(json_data.boot_software_id,value_json->valuestring,sizeof(json_data.boot_software_id) - 1);
                json_data.boot_software_id[sizeof(json_data.boot_software_id) - 1] = '\0';
                printf("DID F180 loaded: %s\n", json_data.boot_software_id);
                break;

            case 0xF181:
                strncpy(json_data.application_software_id,value_json->valuestring,sizeof(json_data.application_software_id) - 1);
                json_data.application_software_id[sizeof(json_data.application_software_id) - 1] = '\0';
                printf("DID F181 loaded: %s\n", json_data.application_software_id);
                break;

            case 0xF182:
                strncpy(json_data.application_data_id,value_json->valuestring, sizeof(json_data.application_data_id) - 1);
                json_data.application_data_id[sizeof(json_data.application_data_id) - 1] = '\0';
                printf("DID F182 loaded: %s\n", json_data.application_data_id);
                break;

            case 0xF183:
                strncpy(json_data.boot_software_fingerprint,value_json->valuestring,sizeof(json_data.boot_software_fingerprint) - 1);
                json_data.boot_software_fingerprint[sizeof(json_data.boot_software_fingerprint) - 1] = '\0';
                printf("DID F183 loaded: %s\n", json_data.boot_software_fingerprint);
                break;

            case 0xF184:
                strncpy(json_data.application_software_fingerprint,value_json->valuestring,sizeof(json_data.application_software_fingerprint) - 1);
                json_data.application_software_fingerprint[sizeof(json_data.application_software_fingerprint) - 1] = '\0';
                printf("DID F184 loaded: %s\n", json_data.application_software_fingerprint);
                break;

            case 0xF185:
                strncpy(json_data.application_data_fingerprint,value_json->valuestring,sizeof(json_data.application_data_fingerprint) - 1);
                json_data.application_data_fingerprint[sizeof(json_data.application_data_fingerprint) - 1] = '\0';
                printf("DID F185 loaded: %s\n", json_data.application_data_fingerprint);
                break;

            case 0xF187:    
                strncpy(json_data.vehicle_manufacturer_spare_part,value_json->valuestring,sizeof(json_data.vehicle_manufacturer_spare_part) - 1);
                json_data.vehicle_manufacturer_spare_part[sizeof(json_data.vehicle_manufacturer_spare_part) - 1] = '\0';
                printf("DID F187 loaded: %s\n", json_data.vehicle_manufacturer_spare_part);
                break;

            case 0xF188:
                strncpy(json_data.vehicle_manufacturer_ecu_sw_number,value_json->valuestring,sizeof(json_data.vehicle_manufacturer_ecu_sw_number) - 1);
                json_data.vehicle_manufacturer_ecu_sw_number[sizeof(json_data.vehicle_manufacturer_ecu_sw_number) - 1] = '\0';
                printf("DID F188 loaded: %s\n", json_data.vehicle_manufacturer_ecu_sw_number);
                break;

            case 0xF189:
                strncpy(json_data.vehicle_manufacturer_ecu_sw_version,value_json->valuestring,sizeof(json_data.vehicle_manufacturer_ecu_sw_version) - 1);
                json_data.vehicle_manufacturer_ecu_sw_version[sizeof(json_data.vehicle_manufacturer_ecu_sw_version) - 1] = '\0';
                printf("DID F189 loaded: %s\n", json_data.vehicle_manufacturer_ecu_sw_version);
                break;

            case 0xF18A:
                strncpy(json_data.system_supplier_id,value_json->valuestring,sizeof(json_data.system_supplier_id) - 1);
                json_data.system_supplier_id[sizeof(json_data.system_supplier_id) - 1] = '\0';
                printf("DID F18A loaded: %s\n", json_data.system_supplier_id);
                break;

            case 0xF18B:
                strncpy(json_data.ecu_manufacturing_date,value_json->valuestring,sizeof(json_data.ecu_manufacturing_date) - 1);
                json_data.ecu_manufacturing_date[sizeof(json_data.ecu_manufacturing_date) - 1] = '\0';
                printf("DID F18B loaded: %s\n", json_data.ecu_manufacturing_date);
                break;

            case 0xF18C:
                strncpy(json_data.serial,value_json->valuestring,sizeof(json_data.serial) - 1);
                json_data.serial[sizeof(json_data.serial) - 1] = '\0';
                printf("DID F18C loaded: %s\n", json_data.serial);
                break;

            case 0xF18D:
                strncpy(json_data.supported_functional_units,value_json->valuestring,sizeof(json_data.supported_functional_units) - 1);
                json_data.supported_functional_units[sizeof(json_data.supported_functional_units) - 1] = '\0';
                printf("DID F18D loaded: %s\n", json_data.supported_functional_units);
                break;

            case 0xF18E:
                strncpy(json_data.kit_assembly_part_number,value_json->valuestring,sizeof(json_data.kit_assembly_part_number) - 1);
                json_data.kit_assembly_part_number[sizeof(json_data.kit_assembly_part_number) - 1] = '\0';
                printf("DID F18E loaded: %s\n", json_data.kit_assembly_part_number);
                break;

            case 0xF18F:
                strncpy(json_data.regulation_sw_id,value_json->valuestring,sizeof(json_data.regulation_sw_id) - 1);
                json_data.regulation_sw_id[sizeof(json_data.regulation_sw_id) - 1] = '\0';
                printf("DID F18F loaded: %s\n", json_data.regulation_sw_id);
                break;

            case 0xF190:
                strncpy(json_data.vvin,value_json->valuestring,sizeof(json_data.vvin) - 1);
                json_data.vvin[sizeof(json_data.vvin) - 1] = '\0';
                printf("DID F190 loaded: %s\n", json_data.vvin);
                break;

            case 0xF191:
                strncpy(json_data.vehicle_manufacturer_ecu_hw_number,value_json->valuestring,sizeof(json_data.vehicle_manufacturer_ecu_hw_number) - 1);
                json_data.vehicle_manufacturer_ecu_hw_number[sizeof(json_data.vehicle_manufacturer_ecu_hw_number) - 1] = '\0';
                printf("DID F191 loaded: %s\n", json_data.vehicle_manufacturer_ecu_hw_number);
                break;

            case 0xF192:
                strncpy(json_data.system_supplier_ecu_hw_number,value_json->valuestring,sizeof(json_data.system_supplier_ecu_hw_number) - 1);
                json_data.system_supplier_ecu_hw_number[sizeof(json_data.system_supplier_ecu_hw_number) - 1] = '\0';
                printf("DID F192 loaded: %s\n", json_data.system_supplier_ecu_hw_number);
                break;

            case 0xF193:
                strncpy(json_data.system_supplier_ecu_hw_version,value_json->valuestring,sizeof(json_data.system_supplier_ecu_hw_version) - 1);
                json_data.system_supplier_ecu_hw_version[sizeof(json_data.system_supplier_ecu_hw_version) - 1] = '\0';
                printf("DID F193 loaded: %s\n", json_data.system_supplier_ecu_hw_version);
                break;

            case 0xF194:
                strncpy(json_data.system_supplier_ecu_sw_number,value_json->valuestring,sizeof(json_data.system_supplier_ecu_sw_number) - 1);
                json_data.system_supplier_ecu_sw_number[sizeof(json_data.system_supplier_ecu_sw_number) - 1] = '\0';
                printf("DID F194 loaded: %s\n", json_data.system_supplier_ecu_sw_number);
                break;

            case 0xF195:
                strncpy(json_data.system_supplier_ecu_sw_version,value_json->valuestring,sizeof(json_data.system_supplier_ecu_sw_version) - 1);
                json_data.system_supplier_ecu_sw_version[sizeof(json_data.system_supplier_ecu_sw_version) - 1] = '\0';
                printf("DID F195 loaded: %s\n", json_data.system_supplier_ecu_sw_version);
                break;

            case 0xF196:
                strncpy(json_data.exhaust_type_approval_number,value_json->valuestring,sizeof(json_data.exhaust_type_approval_number) - 1);
                json_data.exhaust_type_approval_number[sizeof(json_data.exhaust_type_approval_number) - 1] = '\0';
                printf("DID F196 loaded: %s\n", json_data.exhaust_type_approval_number);
                break;

            case 0xF197:
                strncpy(json_data.system_name_engine_type,value_json->valuestring,sizeof(json_data.system_name_engine_type) - 1);
                json_data.system_name_engine_type[sizeof(json_data.system_name_engine_type) - 1] = '\0';
                printf("DID F197 loaded: %s\n", json_data.system_name_engine_type);
                break;

            case 0xF198:
                strncpy(json_data.repair_shop_tester_id,value_json->valuestring,sizeof(json_data.repair_shop_tester_id) - 1);
                json_data.repair_shop_tester_id[sizeof(json_data.repair_shop_tester_id) - 1] = '\0';
                printf("DID F198 loaded: %s\n", json_data.repair_shop_tester_id);
                break;

            case 0xF199:
                strncpy(json_data.programming_date,value_json->valuestring,sizeof(json_data.programming_date) - 1);
                json_data.programming_date[sizeof(json_data.programming_date) - 1] = '\0';
                printf("DID F199 loaded: %s\n", json_data.programming_date);
                break;

            case 0xF19A:
                strncpy(json_data.calibration_repair_shop_id,value_json->valuestring,sizeof(json_data.calibration_repair_shop_id) - 1);
                json_data.calibration_repair_shop_id[sizeof(json_data.calibration_repair_shop_id) - 1] = '\0';
                printf("DID F19A loaded: %s\n", json_data.calibration_repair_shop_id);
                break;

            case 0xF19B:
                strncpy(json_data.calibration_date,value_json->valuestring,sizeof(json_data.calibration_date) - 1);
                json_data.calibration_date[sizeof(json_data.calibration_date) - 1] = '\0';
                printf("DID F19B loaded: %s\n", json_data.calibration_date);
                break;

            case 0xF19C:
                strncpy(json_data.calibration_software_number,value_json->valuestring,sizeof(json_data.calibration_software_number) - 1);
                json_data.calibration_software_number[sizeof(json_data.calibration_software_number) - 1] = '\0';
                printf("DID F19C loaded: %s\n", json_data.calibration_software_number);
                break;

            case 0xF19D:
                strncpy(json_data.ecu_installation_date,value_json->valuestring,sizeof(json_data.ecu_installation_date) - 1);
                json_data.ecu_installation_date[sizeof(json_data.ecu_installation_date) - 1] = '\0';
                printf("DID F19D loaded: %s\n", json_data.ecu_installation_date);
                break;

            case 0xF19E:
                strncpy(json_data.odx_file_reference,value_json->valuestring,sizeof(json_data.odx_file_reference) - 1);
                json_data.odx_file_reference[sizeof(json_data.odx_file_reference) - 1] = '\0';
                printf("DID F19E loaded: %s\n", json_data.odx_file_reference);
                break;

            case 0xF19F:
                strncpy(json_data.entity_data,value_json->valuestring,sizeof(json_data.entity_data) - 1);
                json_data.entity_data[sizeof(json_data.entity_data) - 1] = '\0';
                printf("DID F19F loaded: %s\n", json_data.entity_data);
                break;

            default:
                break;
        }
    }

    cJSON_Delete(root);

    return 0;
}

uint16_t data_bal_len = 0;
uint8_t data_bal[1024] = {0};

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    
    switch (ev) {
        case UDS_EVT_DiagSessCtrl:
            printf("DiagSessCtrl\n");
            return UDS_PositiveResponse;

        case UDS_EVT_EcuReset:
            printf("EcuReset\n");
            return UDS_PositiveResponse;

        case UDS_EVT_ClearDiagnosticInfo:
            printf("ClearDiagnosticInfo\n");
            return UDS_PositiveResponse;

        case UDS_EVT_ReadDTCInformation:
            printf("ReadDTCInformation\n");
            return UDS_PositiveResponse;

        case UDS_EVT_ReadDataByIdent:{
                UDSRDBIArgs_t *args = (UDSRDBIArgs_t *)arg;
 		   
                printf("ReadDataByIdent: DID 0x%04X\n", args->dataId);
                
                switch (args->dataId)
                {
                    case 0xF180:
                        args->copy(srv,json_data.boot_software_id,strlen(json_data.boot_software_id));
                        printf("DID 0xF180: %s\n", json_data.boot_software_id);
                        return UDS_PositiveResponse;

                    case 0xF181:
                        args->copy(srv,json_data.application_software_id,strlen(json_data.application_software_id));
                        printf("DID 0xF181: %s\n", json_data.application_software_id);
                        return UDS_PositiveResponse;

                    case 0xF182:
                        args->copy(srv,json_data.application_data_id,strlen(json_data.application_data_id));
                        printf("DID 0xF182: %s\n", json_data.application_data_id);
                        return UDS_PositiveResponse;

                    case 0xF183:
                        args->copy(srv,json_data.boot_software_fingerprint,strlen(json_data.boot_software_fingerprint));
                        return UDS_PositiveResponse;

                    case 0xF184:
                        args->copy(srv,json_data.application_software_fingerprint,strlen(json_data.application_software_fingerprint));
                        return UDS_PositiveResponse;

                    case 0xF185:
                        args->copy(srv,json_data.application_data_fingerprint,strlen(json_data.application_data_fingerprint));
                        return UDS_PositiveResponse;

                    case 0xF186:
                        args->copy(srv,&json_data.active_diagnostic_session,1);
                        return UDS_PositiveResponse;

                    case 0xF187:
                        args->copy(srv,json_data.vehicle_manufacturer_spare_part,strlen(json_data.vehicle_manufacturer_spare_part));
                        return UDS_PositiveResponse;

                    case 0xF188:
                        args->copy(srv,json_data.vehicle_manufacturer_ecu_sw_number,strlen(json_data.vehicle_manufacturer_ecu_sw_number));
                        return UDS_PositiveResponse;

                    case 0xF189:
                        args->copy(srv,json_data.vehicle_manufacturer_ecu_sw_version,strlen(json_data.vehicle_manufacturer_ecu_sw_version));
                        return UDS_PositiveResponse;

                    case 0xF18A:
                        args->copy(srv,json_data.system_supplier_id,strlen(json_data.system_supplier_id));
                        return UDS_PositiveResponse;

                    case 0xF18B:
                        args->copy(srv,json_data.ecu_manufacturing_date,strlen(json_data.ecu_manufacturing_date));
                        return UDS_PositiveResponse;

                    case 0xF18C:
                        args->copy(srv,json_data.serial,strlen(json_data.serial));
                        return UDS_PositiveResponse;

                    case 0xF18D:
                        args->copy(srv,json_data.supported_functional_units,strlen(json_data.supported_functional_units));
                        return UDS_PositiveResponse;

                    case 0xF18E:
                        args->copy(srv,json_data.kit_assembly_part_number,strlen(json_data.kit_assembly_part_number));
                        return UDS_PositiveResponse;

                    case 0xF18F:
                        args->copy(srv,json_data.regulation_sw_id,strlen(json_data.regulation_sw_id));
                        return UDS_PositiveResponse;

                    case 0xF190:
                        args->copy(srv,json_data.vvin,strlen(json_data.vvin));
                        return UDS_PositiveResponse;

                    case 0xF191:
                        args->copy(srv,json_data.vehicle_manufacturer_ecu_hw_number,strlen(json_data.vehicle_manufacturer_ecu_hw_number));
                        return UDS_PositiveResponse;

                    case 0xF192:
                        args->copy(srv,json_data.system_supplier_ecu_hw_number,strlen(json_data.system_supplier_ecu_hw_number));
                        return UDS_PositiveResponse;

                    case 0xF193:
                        args->copy(srv,json_data.system_supplier_ecu_hw_version,strlen(json_data.system_supplier_ecu_hw_version));
                        return UDS_PositiveResponse;

                    case 0xF194:
                        args->copy(srv,json_data.system_supplier_ecu_sw_number,strlen(json_data.system_supplier_ecu_sw_number));
                        return UDS_PositiveResponse;

                    case 0xF195:
                        args->copy(srv,json_data.system_supplier_ecu_sw_version,strlen(json_data.system_supplier_ecu_sw_version));
                        return UDS_PositiveResponse;

                    case 0xF196:
                        args->copy(srv,json_data.exhaust_type_approval_number,strlen(json_data.exhaust_type_approval_number));
                        return UDS_PositiveResponse;

                    case 0xF197:
                        args->copy(srv,json_data.system_name_engine_type,strlen(json_data.system_name_engine_type));
                        return UDS_PositiveResponse;

                    case 0xF198:
                        args->copy(srv,json_data.repair_shop_tester_id,strlen(json_data.repair_shop_tester_id));
                        return UDS_PositiveResponse;

                    case 0xF199:
                        args->copy(srv,json_data.programming_date,strlen(json_data.programming_date));
                        return UDS_PositiveResponse;

                    case 0xF19A:
                        args->copy(srv,json_data.calibration_repair_shop_id,strlen(json_data.calibration_repair_shop_id));
                        return UDS_PositiveResponse;

                    case 0xF19B:
                        args->copy(srv,json_data.calibration_date,strlen(json_data.calibration_date));
                        return UDS_PositiveResponse;

                    case 0xF19C:
                        args->copy(srv,json_data.calibration_software_number,strlen(json_data.calibration_software_number));
                        return UDS_PositiveResponse;

                    case 0xF19D:
                        args->copy(srv,json_data.ecu_installation_date,strlen(json_data.ecu_installation_date));
                        return UDS_PositiveResponse;

                    case 0xF19E:
                        args->copy(srv,json_data.odx_file_reference,strlen(json_data.odx_file_reference));
                        return UDS_PositiveResponse;

                    case 0xF19F:
                        args->copy(srv,json_data.entity_data,strlen(json_data.entity_data));
                        return UDS_PositiveResponse;

                    default:
                        printf("DID 0x%04X not supported\n",args->dataId);

                        return UDS_NRC_RequestOutOfRange;
                }
        }

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

    if (load_json_data(JSON_FILE_PATH) != 0){
        fprintf(stderr,"Failed to load JSON data\n");

        return -1;
    }

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
        //sleep_ms(1);
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