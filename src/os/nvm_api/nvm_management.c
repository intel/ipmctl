/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the functionality that is dynamically executed upon library load.
 */

#include "nvm_management.h"
#include "nvm_output_parsing.h"
#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/ShellCommandLib.h>
#include <Library/HiiLib.h>
#include <Protocol/DriverHealth.h>
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <Version.h>
#include <NvmInterface.h>
#include <os_efi_bs_protocol.h>
#include <os_efi_simple_file_protocol.h>
#include <os_efi_shell_parameters_protocol.h>
#include <os_efi_preferences.h>
#include <os_efi_api.h>
#include <Common.h>
#include <NvmDimmConfig.h>
#include <NvmDimmPassThru.h>
#include <os.h>
#include <Dimm.h>
#include <NvmDimmDriver.h>
#include <s_str.h>
#include <wchar.h>
#include <CommandParser.h>
#include "event.h"
#include <Protocol/EfiShellParameters.h>
#include "LoadCommand.h"

#if defined(__LINUX__)
#include <safe_str_lib.h>
#include <safe_mem_lib.h>
#endif

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)
#define VERSION_STR STRINGIZE(__VERSION_NUMBER__)
#define NVM_API_MUTEX "nvm_api"

#define INVALID_DIMM_HANDLE     0
OS_MUTEX *g_api_mutex;
unsigned int g_dimm_cnt;
int g_basic_commands = 0;
DIMM_INFO *g_dimms;
int get_dimm_id(const char *uid, UINT16 *dimm_id, unsigned int *dimm_handle);
void dimm_info_to_device_discovery(DIMM_INFO *p_dimm, struct device_discovery *p_device);
int g_nvm_initialized = 0;
int get_fw_err_log_stats(const unsigned int dimm_id, const unsigned char log_level, const unsigned char log_type, LOG_INFO_DATA_RETURN *log_info);

extern EFI_SHELL_PARAMETERS_PROTOCOL gOsShellParametersProtocol;
extern NVMDIMMDRIVER_DATA *gNvmDimmData;
extern EFI_DRIVER_BINDING_PROTOCOL gNvmDimmDriverDriverBinding;
extern EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *pSystemTable);
extern EFI_STATUS ForceStartTheDriver();
extern EFI_STATUS EFIAPI NvmDimmDriverDriverBindingStart(IN EFI_DRIVER_BINDING_PROTOCOL *pThis, IN EFI_HANDLE ControllerHandle, IN EFI_DEVICE_PATH_PROTOCOL *pRemainingDevicePath OPTIONAL);
extern EFI_DRIVER_BINDING_PROTOCOL gNvmDimmDriverDriverBinding;
extern EFI_STATUS EFIAPI NvmDimmDriverDriverEntryPoint(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *pSystemTable);
extern EFI_DCPMM_CONFIG_PROTOCOL gNvmDimmDriverNvmDimmConfig;
extern EFI_STATUS
EFIAPI
GetCapacities(IN UINT16 DimmPid, OUT UINT64 *pVolatileCapacity, OUT UINT64 *pAppDirectCapacity, OUT UINT64 *pUnconfiguredCapacity, OUT UINT64 *pReservedCapacity, OUT UINT64 *pInaccessibleCapacity);
extern EFI_STATUS
ParseSourceDumpFile(IN CHAR16 *pFilePath, IN EFI_DEVICE_PATH_PROTOCOL *pDevicePath, OUT CHAR8 **pFileString);
extern EFI_STATUS RegisterCommands();
extern int g_fast_path;
extern int acpi_event_create_ctx(unsigned int dimm_handle, void ** ctx);
extern int acpi_event_ctx_get_dimm_handle(void * ctx, unsigned int * dev_handle);
extern int acpi_event_get_event_state(void * ctx, enum acpi_event_type event_type, enum acpi_event_state *event_state);
extern int acpi_event_set_monitor_mask(void * ctx, const unsigned int mask);
extern int acpi_event_get_monitor_mask(void * ctx, unsigned int * mask);
extern int acpi_wait_for_event(void * acpi_event_contexts[], const unsigned int dimm_cnt, const int timeout_sec, enum acpi_get_event_result * event_result);
extern int acpi_event_free_ctx(void * context);

//todo: add error checking
NVM_API int nvm_init()
{
  int rc = NVM_SUCCESS;

  if (g_nvm_initialized) {

    // Clear PCD cache on any API entry point
    ClearPcdCacheOnDimmList();

    return rc;
  }

  // Configure gOsShellParametersProtocol
  if (gOsShellParametersProtocol.StdOut == 0) {
    gOsShellParametersProtocol.StdOut = stdout;
  }
  if (gOsShellParametersProtocol.StdErr == 0) {
    gOsShellParametersProtocol.StdErr = stderr;
  }
  if (gOsShellParametersProtocol.StdIn == 0) {
    gOsShellParametersProtocol.StdIn = stdin;
  }

  NVDIMM_DBG("Nvm Init");

  if (NULL == (g_api_mutex = os_mutex_init(NVM_API_MUTEX)))
  {
    NVDIMM_ERR("Failed to intialize NVM API mutex\n");
    return NVM_ERR_UNKNOWN;
  }

  EFI_HANDLE FakeBindHandle = (EFI_HANDLE)0x1;
  init_protocol_bs();
  init_protocol_simple_file_system_protocol();

  // Initialize Preferences
  if (EFI_SUCCESS != preferences_init(NULL))
  {
    NVDIMM_ERR("Failed to intialize preferences\n");
    rc = NVM_ERR_UNKNOWN;
    goto cleanup_mutex;
  }

  if (EFI_SUCCESS != NvmDimmDriverDriverEntryPoint(0, NULL))
  {
    NVDIMM_ERR("Nvm Dimm driver entry point failed.\n");
    rc = NVM_ERR_UNKNOWN;
    goto cleanup_mutex;
  }

  rc = os_check_admin_permissions();
  if (NVM_SUCCESS != rc) {
#ifndef DEBUG_BUILD
    g_basic_commands = 1;
#else
    // For debug builds, allow non-admin user execution
    NVDIMM_WARN("Running as non-admin user.");
#endif
  }

  if(!g_fast_path && !g_basic_commands)
  {
    NvmDimmDriverDriverBindingStart(&gNvmDimmDriverDriverBinding, FakeBindHandle, NULL);
  }
  g_nvm_initialized = 1;
  return rc;
cleanup_mutex:
  os_mutex_delete(g_api_mutex, NVM_API_MUTEX);
  g_api_mutex = NULL;
  return rc;
}

NVM_API void nvm_uninit()
{
  EFI_HANDLE FakeBindHandle = (EFI_HANDLE)0x1;
  NvmDimmDriverDriverBindingStop(&gNvmDimmDriverDriverBinding, FakeBindHandle, 0, NULL);
  uninit_protocol_shell_parameters_protocol();
  preferences_uninit();

  if (g_api_mutex) {
    os_mutex_delete(g_api_mutex, NVM_API_MUTEX);
    g_api_mutex = NULL;
  }
}

/**
* @brief    Initialize the config file
* Please notice that only the first call to the function changes the conf file
* configuration, the following function calls have no effect and the conf file
* configuration remains unchanged up to next applicaiton execution.
*
* @param    p_ini_file_name Pointer to the name of the ini file to read
* @return   void
*/
NVM_API void nvm_conf_file_init(const char *p_ini_file_name)
{
  preferences_init(p_ini_file_name);
}

/**
* @brief    Flush the config structre to the config file, the previous config
* file content is being overwritten
*
* @param    NA
* @return   void
*/
NVM_API void nvm_conf_file_flush()
{
  preferences_flush_the_file();
}

NVM_API void nvm_sync_lock_api()
{
  if (g_api_mutex)
    os_mutex_lock(g_api_mutex);
}

NVM_API void nvm_sync_unlock_api()
{
  if (g_api_mutex)
    os_mutex_unlock(g_api_mutex);
}

struct Command g_cur_command;
void nvm_current_cmd(struct Command Command)
{
  g_cur_command = Command;
}

//temp, until uefi and os validation agree to
//return code unification.
EFI_STATUS uefi_to_os_ret_val(EFI_STATUS uefi_rc)
{
  EFI_STATUS rc = EFI_SUCCESS;
  switch (uefi_rc)
  {
  case (0):
    break;
  case (2):
    rc = 201;
    break;
  case (EFI_INVALID_PARAMETER):
    rc = 201;
    break;
  default:
    rc = 1;
  }
  return rc;
}

NVM_API int nvm_run_cli(int argc, char *argv[])
{
  EFI_STATUS rc;
  int nvm_status;

  rc = init_protocol_shell_parameters_protocol(argc, argv);
  if (rc == EFI_INVALID_PARAMETER) {
    wprintf(L"Syntax Error: Exceeded input parameters limit.\n");
    return (int)uefi_to_os_ret_val(rc);
  }

  if (gOsShellParametersProtocol.StdOut == stdout)
  {
    //WA to ensure wprintf work throughout invocation of DCPM mgmt stack.
    wprintf(L"\n");
  }

  nvm_status = nvm_init();
  if (NVM_ERR_INVALID_PERMISSIONS != nvm_status && NVM_SUCCESS != nvm_status) {
    CHAR16* ErrStr = GetSingleNvmStatusCodeMessage(NULL, nvm_status);
    wprintf(L"Failed to intialize nvm library (%d): %ls.\n", nvm_status, ErrStr);
    return nvm_status;
  }
  rc = uefi_to_os_ret_val(UefiMain(0, NULL));

  //gOsShellParametersProtocol.StdOut will be overriden when
  //-o xml is used (temp hack)
  if (gOsShellParametersProtocol.StdOut != stdout) {
    enum DisplayType dt;
    UINT8 d;
    wchar_t disp_name[DISP_NAME_LEN];
    wchar_t disp_delims[DISP_DELIMS_LEN];
    GetDisplayInfo(disp_name, DISP_NAME_LEN*sizeof(wchar_t), &d, disp_delims, DISP_DELIMS_LEN * sizeof(wchar_t));
    dt = (enum DisplayType)d;
    process_output(dt, disp_name, disp_delims, (int)rc, gOsShellParametersProtocol.StdOut, argc, argv);
  }
  nvm_uninit();
  return (int)rc;
}



NVM_API int nvm_get_host_name(char *host_name, const NVM_SIZE host_name_len)
{
  int nvm_status;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NULL == host_name) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (0 == os_get_host_name(host_name, (int)host_name_len))
    return NVM_SUCCESS;
  return NVM_ERR_UNKNOWN;
}

NVM_API int nvm_get_host(struct host *p_host)
{
  int nvm_status;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NULL == p_host) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  p_host->mixed_sku = 0;          //not supported
  p_host->sku_violation = 0;      //not supported
  if (0 != os_get_host_name(p_host->name, NVM_COMPUTERNAME_LEN))
    return NVM_ERR_UNKNOWN;
  if (0 != os_get_os_name(p_host->os_name, NVM_OSNAME_LEN))
    return NVM_ERR_UNKNOWN;
  if (0 != os_get_os_version(p_host->os_version, NVM_OSVERSION_LEN))
    return NVM_ERR_UNKNOWN;
  p_host->os_type = os_get_os_type();
  return NVM_SUCCESS;
}

NVM_API int nvm_get_sw_inventory(struct sw_inventory *p_inventory)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 Version[FW_API_VERSION_LEN];
  int nvm_status;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NULL == p_inventory) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  snprintf(p_inventory->mgmt_sw_revision, NVM_VERSION_LEN, "%s", VERSION_STR);
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetDriverApiVersion(&gNvmDimmDriverNvmDimmConfig, Version);
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;
  UnicodeStrToAsciiStr(Version, p_inventory->vendor_driver_revision);
  p_inventory->vendor_driver_compatible = TRUE;
  return NVM_SUCCESS;
}

NVM_API int nvm_get_version(NVM_VERSION version_str, const NVM_SIZE str_len)
{
  if (NULL == version_str) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }
  snprintf(version_str, str_len, "%s", VERSION_STR);
  return NVM_SUCCESS;
}

NVM_API int nvm_get_number_of_sockets(int *count)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int SocketCount = 0;
  SOCKET_INFO *pSockets = NULL;
  int nvm_status;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NULL == count) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSockets(&gNvmDimmDriverNvmDimmConfig, (UINT32 *)&SocketCount, &pSockets);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_UNKNOWN;
  } else if (pSockets == NULL) {
    NVDIMM_ERR("Platform does not support socket SKU limits.\n");
    return NVM_ERR_UNKNOWN;
  }
  *count = SocketCount;
  return NVM_SUCCESS;
}

NVM_API int nvm_get_sockets(struct socket *p_sockets, const NVM_UINT16 count)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int socket_count = 0;
  SOCKET_INFO *p_sockets_info = NULL;
  int nvm_status;
  int index;

  if (NULL == p_sockets) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }
  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSockets(&gNvmDimmDriverNvmDimmConfig, (UINT32 *)&socket_count, &p_sockets_info);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_UNKNOWN;
  } else if (p_sockets_info == NULL) {
    NVDIMM_ERR("Platform does not support socket SKU limits.\n");
    return NVM_ERR_UNKNOWN;
  }

  if (count != socket_count)
  {
    return NVM_ERR_BAD_SIZE;
  }

  for (index = 0; (index < count) && (index < socket_count); index++) {
    p_sockets[index].id = p_sockets_info[index].SocketId;                           // Zero-indexed NUMA node number
    p_sockets[index].mapped_memory_limit = p_sockets_info[index].MappedMemoryLimit; // Maximum allowed memory (via PCAT)
    p_sockets[index].total_mapped_memory = p_sockets_info[index].TotalMappedMemory; // Current occupied memory (via PCAT)
  }
  return NVM_SUCCESS;
}

NVM_API int nvm_get_socket(const NVM_UINT16 socket_id, struct socket *p_socket)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int socket_count = 0;
  SOCKET_INFO *p_sockets_info = NULL;
  int nvm_status;
  int index;

  if (NULL == p_socket) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }
  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSockets(&gNvmDimmDriverNvmDimmConfig, (UINT32 *)&socket_count, &p_sockets_info);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_UNKNOWN;
  } else if (p_sockets_info == NULL) {
    NVDIMM_ERR("Platform does not support socket SKU limits.\n");
    return NVM_ERR_UNKNOWN;
  }

  for (index = 0; (index < socket_count); index++) {
    if (socket_id == p_sockets_info[index].SocketId) {
      p_socket->id = p_sockets_info[index].SocketId;                                  // Zero-indexed NUMA node number
      p_socket->mapped_memory_limit = p_sockets_info[index].MappedMemoryLimit;        // Maximum allowed memory (via PCAT)
      p_socket->total_mapped_memory = p_sockets_info[index].TotalMappedMemory;        // Current occupied memory (via PCAT)
    }
  }
  return NVM_SUCCESS;
}

NVM_API int nvm_get_number_of_memory_topology_devices(unsigned int *count)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  TOPOLOGY_DIMM_INFO *pDimmTopology = NULL;
  unsigned int DdrDimmCnt = 0;
  unsigned int DpcCnt = 0;
  int nvm_status;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NULL == count) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSystemTopology(&gNvmDimmDriverNvmDimmConfig, &pDimmTopology, (UINT16 *)&DdrDimmCnt);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_UNKNOWN;
  } else if (pDimmTopology == NULL) {
    NVDIMM_ERR("Could not read the system topology.\n");
    return NVM_ERR_UNKNOWN;
  }

  if (NVM_SUCCESS != (nvm_status = nvm_get_number_of_devices(&DpcCnt)))
  {
    NVDIMM_ERR("Failed to obtain the number of devices (%d)\n", nvm_status);
    return NVM_ERR_UNKNOWN;
  }

  *count = DdrDimmCnt + DpcCnt;
  return NVM_SUCCESS;
}

NVM_API int nvm_get_memory_topology(struct memory_topology *  p_devices,
            const NVM_UINT8   count)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  TOPOLOGY_DIMM_INFO *p_dimm_topology = NULL;
  unsigned int topology_dimms_count = 0;
  int nvm_status;
  unsigned int index;
  unsigned int dpc_cnt = 0;

  if (NULL == p_devices)
    return NVM_ERR_INVALID_PARAMETER;
  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSystemTopology(&gNvmDimmDriverNvmDimmConfig, &p_dimm_topology, (UINT16 *)&topology_dimms_count);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_UNKNOWN;
  } else if (p_dimm_topology == NULL) {
    NVDIMM_ERR("Could not read the system topology.\n");
    return NVM_ERR_UNKNOWN;
  }
  for (index = 0; (index < count) && (index < topology_dimms_count); index++) {
    p_devices[index].physical_id = p_dimm_topology[index].DimmID;                                           // Memory device's physical identifier (SMBIOS handle)
    p_devices[index].memory_type = MEMORY_TYPE_DDR4;                                       // Type of memory device
    memcpy_s(p_devices[index].device_locator, NVM_DEVICE_LOCATOR_LEN, p_dimm_topology[index].DeviceLocator, NVM_DEVICE_LOCATOR_LEN);  // Physically-labeled socket of device location
    memcpy_s(p_devices[index].bank_label, NVM_BANK_LABEL_LEN, p_dimm_topology[index].BankLabel, BANKLABEL_LEN);                   // Physically-labeled bank of device location
  }

  DIMM_INFO *pdimms = (DIMM_INFO *)AllocatePool(sizeof(DIMM_INFO) * (count - index));
  if (NULL == pdimms) {
    NVDIMM_ERR("Failed to allocate memory\n");
    return NVM_ERR_UNKNOWN;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetDimms(&gNvmDimmDriverNvmDimmConfig, (UINT32)(count - index), DIMM_INFO_CATEGORY_NONE, pdimms);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    FreePool(pdimms);
    return NVM_ERR_UNKNOWN;
  }

  if (NVM_SUCCESS != nvm_get_number_of_devices(&dpc_cnt)) {
    NVDIMM_ERR("Failed to get number of devices\n");
    FreePool(pdimms);
    return NVM_ERR_UNKNOWN;
  }

  if (dpc_cnt != count)
  {
    FreePool(pdimms);
    return NVM_ERR_BAD_SIZE;
  }

  for (unsigned int i = 0; (index < count) && (i < dpc_cnt); index++,++i) {
    p_devices[index].physical_id = pdimms[i].DimmID; // Memory device's physical identifier (SMBIOS handle)
    p_devices[index].memory_type = MEMORY_TYPE_NVMDIMM; // Type of memory device
    memcpy_s(p_devices[index].device_locator, NVM_DEVICE_LOCATOR_LEN, pdimms[i].DeviceLocator, NVM_DEVICE_LOCATOR_LEN); // Physically-labeled socket of device location
    memcpy_s(p_devices[index].bank_label, NVM_BANK_LABEL_LEN, pdimms[i].BankLabel, BANKLABEL_LEN); // Physically-labeled bank of device location
  }

  return NVM_SUCCESS;
}

NVM_API int nvm_get_number_of_devices(unsigned int *count)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  unsigned int dimm_cnt;
  int nvm_status;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NULL == count) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (0 != g_dimm_cnt)
    goto Finish;

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetDimmCount(&gNvmDimmDriverNvmDimmConfig, (UINT32 *)&dimm_cnt);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_UNKNOWN;
  }
  g_dimm_cnt = dimm_cnt;

Finish:
  *count = g_dimm_cnt;
  return NVM_SUCCESS;
}

NVM_API int nvm_get_devices(struct device_discovery *p_devices, const NVM_UINT8 count)
{
  int nvm_status;
  unsigned int i;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NULL == p_devices) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  unsigned int actual_count = 0;
  if (NVM_SUCCESS != (nvm_status = nvm_get_number_of_devices(&actual_count)))
  {
    NVDIMM_ERR("Failed to obtain the number of devices (%d)\n",nvm_status);
    return NVM_ERR_OPERATION_FAILED;
  }

  EFI_STATUS ReturnCode = EFI_SUCCESS;
  if(count != actual_count)
  {
    return NVM_ERR_BAD_SIZE;
  }

  DIMM_INFO *pdimms = (DIMM_INFO *)AllocatePool(sizeof(DIMM_INFO) * actual_count);
  if (NULL == pdimms) {
    NVDIMM_ERR("Failed to allocate memory\n");
    return NVM_ERR_NOT_ENOUGH_FREE_SPACE;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetDimms(&gNvmDimmDriverNvmDimmConfig, (UINT32)actual_count, DIMM_INFO_CATEGORY_NONE, pdimms);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    FreePool(pdimms);
    return NVM_ERR_OPERATION_FAILED;
  }
  for (i = 0; i < actual_count; ++i)
    dimm_info_to_device_discovery(&pdimms[i], &p_devices[i]);
  FreePool(pdimms);
  return NVM_SUCCESS;
}

NVM_API int nvm_get_devices_nfit(struct device_discovery *p_devices, const NVM_UINT8 count)
{
  int rc = nvm_get_devices(p_devices, count);

  return rc;
}

NVM_API int nvm_get_device_discovery(const NVM_UID    device_uid,
             struct device_discovery *  p_discovery)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM_INFO dimm_info = { 0 };
  UINT16 dimm_id;
  int nvm_status;
  int rc;

  if (NULL == p_discovery) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    return NVM_ERR_DIMM_NOT_FOUND;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetDimm(&gNvmDimmDriverNvmDimmConfig, dimm_id, DIMM_INFO_CATEGORY_NONE, &dimm_info);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_DIMM_NOT_FOUND;
  }
  dimm_info_to_device_discovery(&dimm_info, p_discovery);

  return NVM_SUCCESS;
}

static void dimm_info_to_device_status(DIMM_INFO *p_dimm, struct device_status *p_status)
{
   //DIMM_INFO_CATEGORY_PACKAGE_SPARING
   p_status->package_spares_available = p_dimm->PackageSparesAvailable; // Number of package spares on the DIMM that are available.

   //DIMM_INFO_CATEGORY_ARS_STATUS
   p_status->ars_status = p_dimm->ARSStatus; // Address range scrub operation status for the DIMM

   //DIMM_INFO_CATEGORY_SMART_AND_HEALTH
   p_status->health = p_dimm->HealthState;                         // Overall device health.
   p_status->last_shutdown_status_details = p_dimm->LastShutdownStatusDetails;    // State of last DIMM shutdown.
   p_status->last_shutdown_time = p_dimm->LastShutdownTime;        // Time of the last shutdown - seconds since 1 January 1970
   p_status->ait_dram_enabled = p_dimm->AitDramEnabled;            // Whether or not the AIT DRAM is enabled.

   //DIMM_INFO_CATEGORY_OPTIONAL_CONFIG_DATA_POLICY
   p_status->viral_state = p_dimm->ViralStatus; // Current viral status of DIMM.

   // From global dimm struct
   p_status->is_new = p_dimm->IsNew;                       // Unincorporated with the rest of the devices.
   p_status->is_configured = p_dimm->Configured;           // only the values 1(Success) and 6 (old config used) from CCUR are considered configured
   p_status->overwritedimm_status = p_dimm->OverwriteDimmStatus;     // OverwriteDimm operation status for the DIMM
   p_status->sku_violation = p_dimm->SKUViolation;         // The DIMM configuration is unsupported due to a license issue.
   p_status->config_status = p_dimm->ConfigStatus;         // Status of last configuration request.
   p_status->is_missing = FALSE;                           // If the device is missing.
   //p_status->last_shutdown_status_extended[3];//Extendeded fields as per FIS 1.6
   //p_status->mixed_sku; // One or more DIMMs have different SKUs.
   //p_status->new_error_count; // Count of new fw errors from the DIMM
   //p_status->newest_error_log_timestamp; // Timestamp of the newest log entry in the fw error log
   //p_status->error_log_status;

   //DIMM_INFO_CATEGORY_MEM_INFO_PAGE_3
   p_status->injected_media_errors = p_dimm->MediaTemperatureInjectionsCounter;    // The number of injected media errors on DIMM
   p_status->injected_non_media_errors = p_dimm->PoisonErrorInjectionsCounter;     // The number of injected non-media errors on DIMM
}

NVM_API int nvm_get_device_status(const NVM_UID   device_uid,
          struct device_status *p_status)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM_INFO dimm_info = { 0 };
  UINT16 dimm_id;
  int nvm_status;
  UINT16 BootstatusBitmask;
  if (NULL == p_status) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }
  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }
  if (NVM_SUCCESS != (nvm_status = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", nvm_status);
    p_status->is_missing = TRUE;
    return NVM_ERR_DIMM_NOT_FOUND;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetDimm(&gNvmDimmDriverNvmDimmConfig, dimm_id, DIMM_INFO_CATEGORY_ALL, &dimm_info);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    p_status->is_missing = TRUE;
    return NVM_ERR_DIMM_NOT_FOUND;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetBSRAndBootStatusBitMask(&gNvmDimmDriverNvmDimmConfig, dimm_id, NULL, &BootstatusBitmask);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to get boot status %d\n", ReturnCode);
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    p_status->is_missing = TRUE;
    return NVM_ERR_DIMM_NOT_FOUND;
  }
  p_status->boot_status = BootstatusBitmask;
  dimm_info_to_device_status(&dimm_info, p_status);
  return NVM_SUCCESS;
}

NVM_API int nvm_get_pmon_registers(const NVM_UID   device_uid,
          const NVM_UINT8 SmartDataMask, PMON_REGISTERS *p_output_payload)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 dimm_id;
  int nvm_status;
  int rc;

  if (NULL == p_output_payload) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }
  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }
  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    return NVM_ERR_DIMM_NOT_FOUND;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetPMONRegisters(&gNvmDimmDriverNvmDimmConfig, dimm_id, (UINT8)SmartDataMask, p_output_payload);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_OPERATION_FAILED;
  }
  return NVM_SUCCESS;
}
NVM_API int nvm_set_pmon_registers(const NVM_UID   device_uid,
          NVM_UINT8 PMONGroupEnable)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 dimm_id;
  int nvm_status;
  int rc;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }
  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    return NVM_ERR_DIMM_NOT_FOUND;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.SetPMONRegisters(&gNvmDimmDriverNvmDimmConfig, dimm_id, (UINT8)PMONGroupEnable);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_OPERATION_FAILED;
  }
  return NVM_SUCCESS;
}

NVM_API int nvm_get_device_settings(const NVM_UID   device_uid,
            struct device_settings *  p_settings)
{
  EFI_STATUS ReturnCode;
  DIMM *pDimm = NULL;
  UINT16 dimm_id;
  int rc;
  PT_OPTIONAL_DATA_POLICY_PAYLOAD OptionalDataPolicyPayload;
  PT_VIRAL_POLICY_PAYLOAD ViralPolicyPayload;
  int nvm_status;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NULL == p_settings) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    return rc;
  }

  if (NULL == (pDimm = GetDimmByPid(dimm_id, &gNvmDimmData->PMEMDev.Dimms))) {
    NVDIMM_ERR("Failed to get dimm by Pid (%d)\n", dimm_id);
    return NVM_ERR_UNKNOWN;
  }

  ReturnCode = FwCmdGetOptionalConfigurationDataPolicy(pDimm, &OptionalDataPolicyPayload);
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;

  ReturnCode = FwCmdGetViralPolicy(pDimm, &ViralPolicyPayload);
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;

  p_settings->first_fast_refresh = OptionalDataPolicyPayload.FirstFastRefresh;
  p_settings->viral_policy = ViralPolicyPayload.ViralPolicyEnable;
  p_settings->viral_status = ViralPolicyPayload.ViralStatus;

  return NVM_SUCCESS;
}

NVM_API int nvm_modify_device_settings(const NVM_UID      device_uid,
               const struct device_settings * p_settings)
{
  EFI_STATUS ReturnCode;
  DIMM *pDimm = NULL;
  UINT16 dimm_id;
  int rc;
  PT_OPTIONAL_DATA_POLICY_PAYLOAD OptionalDataPolicyPayload;
  int nvm_status;

  if (NULL == p_settings) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    return rc;
  }

  if (NULL == (pDimm = GetDimmByPid(dimm_id, &gNvmDimmData->PMEMDev.Dimms))) {
    NVDIMM_ERR("Failed to get dimm by Pid (%d)\n", dimm_id);
    return NVM_ERR_UNKNOWN;
  }

  OptionalDataPolicyPayload.FirstFastRefresh = p_settings->first_fast_refresh;

  ReturnCode = FwCmdSetOptionalConfigurationDataPolicy(pDimm, &OptionalDataPolicyPayload);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("FwCmdSetOptionalConfigurationDataPolicy failed (%d)\n", ReturnCode);
    return NVM_ERR_UNKNOWN;
  }

  return NVM_SUCCESS;
}

NVM_API int nvm_get_device_details(const NVM_UID    device_uid,
           struct device_details *  p_details)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM_INFO dimm_info = { 0 };
  UINT16 dimm_id;
  int nvm_status;
  int rc;
  SYSTEM_CAPABILITIES_INFO SystemCapabilitiesInfo;

  if (NULL == p_details)
    return NVM_ERR_INVALID_PARAMETER;
  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }
  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    return NVM_ERR_DIMM_NOT_FOUND;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetDimm(&gNvmDimmDriverNvmDimmConfig, dimm_id, DIMM_INFO_CATEGORY_ALL, &dimm_info);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_DIMM_NOT_FOUND;
  }

  // from SMBIOS Type 17 Table
  p_details->form_factor = dimm_info.FormFactor;                                          // The type of DIMM.
  p_details->data_width = dimm_info.DataWidth;                                            // The width in bits used to store user data.
  p_details->total_width = dimm_info.TotalWidth;                                          // The width in bits for data and ECC and/or redundancy.
  p_details->speed = dimm_info.Speed;                                                     // The speed in nanoseconds.
  memcpy_s(p_details->device_locator, NVM_DEVICE_LOCATOR_LEN, dimm_info.DeviceLocator, NVM_DEVICE_LOCATOR_LEN);     // The socket or board position label
  memcpy_s(p_details->bank_label, NVM_BANK_LABEL_LEN, dimm_info.BankLabel, NVM_BANK_LABEL_LEN);                 // The bank label
  p_details->peak_power_budget = dimm_info.PeakPowerBudget;                               // instantaneous power budget in mW (100-20000 mW).
  p_details->avg_power_budget = dimm_info.AvgPowerBudget;                                 // average power budget in mW (100-18000 mW).
        p_details->package_sparing_enabled = dimm_info.PackageSparingEnabled;                   // Enable or disable package sparing.

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig,
    &SystemCapabilitiesInfo);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_UNKNOWN;
  }

  p_details->discovery.security_capabilities.erase_crypto_capable = SystemCapabilitiesInfo.EraseDeviceDataSupported;
  p_details->discovery.security_capabilities.passphrase_capable = SystemCapabilitiesInfo.ChangeDevicePassphraseSupported;
  p_details->discovery.security_capabilities.unlock_device_capable = SystemCapabilitiesInfo.UnlockDeviceSecuritySupported;

  // Basic device identifying information.
  rc = nvm_get_device_discovery(device_uid, &(p_details->discovery));
  if (rc != NVM_SUCCESS)
    return rc;
  // Device health and status.
  rc = nvm_get_device_status(device_uid, &(p_details->status));
  if (rc != NVM_SUCCESS)
    return rc;
  // The firmware image information for the device.
  rc = nvm_get_device_fw_image_info(device_uid, &(p_details->fw_info));
  if (rc != NVM_SUCCESS)
    return rc;
  // A snapshot of the performance metrics.
  rc = nvm_get_device_performance(device_uid, &(p_details->performance));
  if (rc != NVM_SUCCESS)
    return rc;
  // Device sensors.
  rc = nvm_get_sensors(device_uid, p_details->sensors, NVM_MAX_DEVICE_SENSORS);
  if (rc != NVM_SUCCESS)
    return rc;
  // Partition information
  rc = nvm_get_nvm_capacities(&(p_details->capacities));
  if (rc != NVM_SUCCESS)
    return rc;
  // Modifiable features of the device.
  rc = nvm_get_device_settings(device_uid, &(p_details->settings));
  if (rc != NVM_SUCCESS)
    return rc;
  return NVM_SUCCESS;
}

NVM_API int nvm_get_device_performance(const NVM_UID      device_uid,
               struct device_performance *  p_performance)
{
  FW_CMD *cmd = NULL;
  PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE1 *pmem_info_output;
  PT_INPUT_PAYLOAD_MEMORY_INFO mem_info_input;
  int rc = NVM_ERR_UNKNOWN;

  if (NULL == p_performance) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  if (NULL == (cmd = (FW_CMD *)AllocatePool(sizeof(FW_CMD)))) {
    NVDIMM_ERR("Failed to allocate memory\n");
    goto finish;
  }

  ZeroMem(cmd, sizeof(FW_CMD));
  ZeroMem(&mem_info_input, sizeof(mem_info_input));
  mem_info_input.MemoryPage = 1;
  UINT16 dimm_id;
  unsigned int dimm_handle;

  if (NVM_SUCCESS != (rc = get_dimm_id((char *)device_uid, &dimm_id, &dimm_handle))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    goto finish;
  }
  cmd->DimmID = dimm_id; //PassThruCommand needs the dimm_id (not handle)
  cmd->Opcode = PtGetLog;
  cmd->SubOpcode = SubopMemInfo;
  cmd->InputPayloadSize = sizeof(PT_INPUT_PAYLOAD_MEMORY_INFO);
  CopyMem_S(cmd->InputPayload, sizeof(cmd->InputPayload), &mem_info_input, cmd->InputPayloadSize);
  cmd->OutputPayloadSize = sizeof(PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE1);
  if (EFI_SUCCESS == PassThruCommand(cmd, PT_TIMEOUT_INTERVAL)) {
    pmem_info_output = (PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE1 *)cmd->OutPayload;
    p_performance->bytes_read = pmem_info_output->TotalMediaReads.Uint64;
    p_performance->bytes_written = pmem_info_output->TotalMediaWrites.Uint64;
    p_performance->host_reads = pmem_info_output->TotalReadRequests.Uint64;
    p_performance->host_writes = pmem_info_output->TotalWriteRequests.Uint64;
    p_performance->block_reads = 0;
    p_performance->block_writes = 0;
    rc = NVM_SUCCESS;
    goto finish;
  }

finish:
  FREE_POOL_SAFE(cmd);
  return rc;
}


/*!
 * Number of characters allowed for Major revision portion of the revision string
 */
#define COMMON_MAJOR_REVISION_LEN       2

/*!
 * Number of characters allowed for Minor revision portion of the revision string
 */
#define COMMON_MINOR_REVISION_LEN       2

/*!
 * Number of characters allowed for Hotfix revision portion of the revision string
 */
#define COMMON_HOTFIX_REVISION_LEN      2

/*!
 * Number of characters allowed for Build revision portion of the revision string
 */
#define COMMON_BUILD_REVISION_LEN       4

/*
 * Utility method to create version string from parts
 */
void build_revision(char *revision, size_t revision_len,
        unsigned short int major, unsigned short int minor,
        unsigned short int hotfix, unsigned short int build)
{
  if (revision && (revision_len != 0)) {
    // dynamically build revision string format
    char rev_format_str[64];
    rev_format_str[0] = '\0';
    char num_digit_buf[4];


    // Major Revision
    s_strcat(rev_format_str, 64, "%0");
    snprintf(num_digit_buf, 4, "%d", COMMON_MAJOR_REVISION_LEN);
    s_strcat(rev_format_str, 64, num_digit_buf);
    s_strcat(rev_format_str, 64, "hd");

    // Minor Revision
    s_strcat(rev_format_str, 64, ".%0");
    snprintf(num_digit_buf, 4, "%d", COMMON_MINOR_REVISION_LEN);
    s_strcat(rev_format_str, 64, num_digit_buf);
    s_strcat(rev_format_str, 64, "hd");

    // Hotfix Revision
    s_strcat(rev_format_str, 64, ".%0");
    snprintf(num_digit_buf, 4, "%d", COMMON_HOTFIX_REVISION_LEN);
    s_strcat(rev_format_str, 64, num_digit_buf);
    s_strcat(rev_format_str, 64, "hd");

    // Build Revision
    s_strcat(rev_format_str, 64, ".%0");
    snprintf(num_digit_buf, 4, "%d", COMMON_BUILD_REVISION_LEN);
    s_strcat(rev_format_str, 64, num_digit_buf);
    s_strcat(rev_format_str, 64, "hd");
    snprintf(revision, revision_len, rev_format_str, major, minor, hotfix, build);
  }
}

/*
 * Convert a FW version array to a string
 */
#define FW_VER_ARR_TO_STR(arr, str, len) \
  build_revision(str, len, \
           ((((arr[4] >> 4) & 0xF) * 10) + (arr[4] & 0xF)), \
           ((((arr[3] >> 4) & 0xF) * 10) + (arr[3] & 0xF)), \
           ((((arr[2] >> 4) & 0xF) * 10) + (arr[2] & 0xF)), \
           (((arr[1] >> 4) & 0xF) * 1000) + (arr[1] & 0xF) * 100 + \
           (((arr[0] >> 4) & 0xF) * 10) + (arr[0] & 0xF));

#define DEV_FW_COMMIT_ID_LEN    40              /* Length of commit identifier of Firmware including null */
#define DEV_FW_BUILD_CONFIGURATION_LEN 16       /* Size of the build configuration including null */

/*
 * Convert firmware type into firmware type enumeration
 */
enum device_fw_type firmware_type_to_enum(unsigned char fw_type)
{
  enum device_fw_type fw_type_enum;

  if (fw_type == FW_TYPE_PRODUCTION)
    fw_type_enum = DEVICE_FW_TYPE_PRODUCTION;
  else if (fw_type == FW_TYPE_DFX)
    fw_type_enum = DEVICE_FW_TYPE_DFX;
  else if (fw_type == FW_TYPE_DEBUG)
    fw_type_enum = DEVICE_FW_TYPE_DEBUG;
  else
    fw_type_enum = DEVICE_FW_TYPE_UNKNOWN;
  return fw_type_enum;
}

/* Status after the last FW update operation */
enum last_fw_update_status {
  LAST_FW_UPDATE_STAGED_SUCCESS = 1,
  LAST_FW_UPDATE_LOAD_SUCCESS = 2,
  LAST_FW_UPDATE_LOAD_FAILED  = 3
};

enum fw_update_status firmware_update_status_to_enum(unsigned char last_fw_update_status)
{
  enum fw_update_status fw_update_status;

  if (last_fw_update_status == LAST_FW_UPDATE_STAGED_SUCCESS)
    fw_update_status = FW_UPDATE_STAGED;
  else if (last_fw_update_status == LAST_FW_UPDATE_LOAD_SUCCESS)
    fw_update_status = FW_UPDATE_SUCCESS;
  else if (last_fw_update_status == LAST_FW_UPDATE_LOAD_FAILED)
    fw_update_status = FW_UPDATE_FAILED;
  else
    fw_update_status = FW_UPDATE_UNKNOWN;
  return fw_update_status;
}

NVM_API int nvm_get_device_fw_image_info(const NVM_UID    device_uid,
           struct device_fw_info *p_fw_info)
{
  EFI_STATUS ReturnCode;
  DIMM *pDimm = NULL;
  UINT16 dimm_id;
  int rc;
  PT_PAYLOAD_FW_IMAGE_INFO *fw_image_info = NULL;
  int nvm_status;

  if (NULL == p_fw_info) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    return rc;
  }

  if (NULL == (pDimm = GetDimmByPid(dimm_id, &gNvmDimmData->PMEMDev.Dimms))) {
    NVDIMM_ERR("Failed to get dimm by Pid (%d)\n", dimm_id);
    return NVM_ERR_UNKNOWN;
  }

  ReturnCode = FwCmdGetFirmwareImageInfo(pDimm, &fw_image_info);
  if (EFI_ERROR(ReturnCode) || (NULL == fw_image_info)) {
    NVDIMM_ERR("FwCmdGetFirmwareImageInfo failed (%d)\n", ReturnCode);
    return NVM_ERR_UNKNOWN;
  }

  FW_VER_ARR_TO_STR(fw_image_info->FwRevision, p_fw_info->active_fw_revision,
        NVM_VERSION_LEN);

  FW_VER_ARR_TO_STR(fw_image_info->StagedFwRevision, p_fw_info->staged_fw_revision,
        NVM_VERSION_LEN);

  p_fw_info->FWImageMaxSize = fw_image_info->FWImageMaxSize;

  p_fw_info->fw_update_status =
    firmware_update_status_to_enum(fw_image_info->LastFwUpdateStatus);
  free(fw_image_info);
  return NVM_SUCCESS;
}

NVM_API int nvm_update_device_fw(const NVM_UID device_uid,
         const NVM_PATH path, const NVM_SIZE path_len, const NVM_BOOL force)
{
  int rc = NVM_SUCCESS;
  EFI_STATUS ReturnCode;
  COMMAND_STATUS *p_command_status;
  CHAR16 file_name[NVM_PATH_LEN];
  FW_IMAGE_INFO *p_fw_image_info = NULL;
  UINT16 dimm_id;

  if (path_len > NVM_PATH_LEN)
    return NVM_ERR_UNKNOWN;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  ReturnCode = InitializeCommandStatus(&p_command_status);
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;
  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    FreeCommandStatus(&p_command_status);
    NVDIMM_ERR("Failed to get DIMM ID %d\n", rc);
    return rc;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.UpdateFw(&gNvmDimmDriverNvmDimmConfig, &dimm_id, 1, AsciiStrToUnicodeStr(path, file_name),
                NULL, FALSE, force, FALSE, FALSE, p_fw_image_info, p_command_status);
  if (NVM_SUCCESS != ReturnCode) {
    FreeCommandStatus(&p_command_status);
    NVDIMM_ERR("Failed to update the FW, file %s. Return code %d", path, ReturnCode);
    return NVM_ERR_DUMP_FILE_OPERATION_FAILED;
  }
  FreeCommandStatus(&p_command_status);
  return NVM_SUCCESS;
}

NVM_API int nvm_examine_device_fw(const NVM_UID device_uid,
          const NVM_PATH path, const NVM_SIZE path_len,
          NVM_VERSION image_version, const NVM_SIZE image_version_len)
{
  int rc = NVM_SUCCESS;
  EFI_STATUS ReturnCode;
  COMMAND_STATUS *p_command_status;
  CHAR16 file_name[NVM_PATH_LEN];
  FW_IMAGE_INFO *p_fw_image_info = NULL;
  UINT16 dimm_id;

  if ((path_len > NVM_PATH_LEN) || (NULL == image_version))
    return NVM_ERR_UNKNOWN;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  ReturnCode = InitializeCommandStatus(&p_command_status);
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;
  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    FreeCommandStatus(&p_command_status);
    NVDIMM_ERR("Failed to get DIMM ID %d\n", rc);
    return rc;
  }
  p_fw_image_info = AllocateZeroPool(sizeof(*p_fw_image_info));
  if (p_fw_image_info == NULL) {
    NVDIMM_ERR("Failed to allocate memory");
    rc = NVM_ERR_UNKNOWN;
  } else {
    ReturnCode = gNvmDimmDriverNvmDimmConfig.UpdateFw(&gNvmDimmDriverNvmDimmConfig, &dimm_id, 1, AsciiStrToUnicodeStr(path, file_name),
                  NULL, TRUE, FALSE, FALSE, FALSE, p_fw_image_info, p_command_status);
    if (NVM_SUCCESS != ReturnCode) {
      NVDIMM_ERR("Failed to update the FW, file %s. Return code %d", path, ReturnCode);
      rc = NVM_ERR_DUMP_FILE_OPERATION_FAILED;
    } else {
      if (image_version_len > NVM_VERSION_LEN) {
        sprintf_s(image_version, NVM_VERSION_LEN, "%d.%d.%d.%d", p_fw_image_info->ImageVersion.ProductNumber.Version,
          p_fw_image_info->ImageVersion.RevisionNumber.Version,
          p_fw_image_info->ImageVersion.SecurityVersionNumber.Version,
          p_fw_image_info->ImageVersion.BuildNumber.Build);
      }
    }
  }
  FreeCommandStatus(&p_command_status);
  FREE_POOL_SAFE(p_fw_image_info);
  return rc;
}

int driver_features_to_nvm_features(
  const struct driver_feature_flags * p_driver_features,
  struct nvm_features *     p_nvm_features)
{
  int rc = NVM_SUCCESS;

  // get device health - Get Topology, Passthrough
  p_nvm_features->get_device_health = p_driver_features->passthrough;

  // get device settings - Get Topology, Passthrough
  p_nvm_features->get_device_settings = p_driver_features->passthrough;

  // modify device settings - Get Topology, Passthrough
  p_nvm_features->modify_device_settings = p_driver_features->passthrough;

  // get device security - Get Topology, Passthrough
  p_nvm_features->get_device_security = p_driver_features->passthrough;

  // modify device security - Get Topology, Passthrough
  p_nvm_features->modify_device_security = p_driver_features->passthrough;

  // get device performance - Get Topology, Passthrough
  p_nvm_features->get_device_performance = p_driver_features->passthrough;

  // get device firmware - Get Topology, Passthrough
  p_nvm_features->get_device_firmware = p_driver_features->passthrough;

  // update device firmware - Get Topology, Passthrough
  p_nvm_features->update_device_firmware = p_driver_features->passthrough;

  // get sensors - Get Topology, Passthrough
  p_nvm_features->get_sensors = p_driver_features->passthrough;

  // modify sensors - Get Topology, Set Features
  p_nvm_features->modify_sensors = p_driver_features->passthrough;

  // get device capacity - Get Topology, Passthrough
  p_nvm_features->get_device_capacity = p_driver_features->passthrough;

  // modify device capacity - Get Topology, Get Platform Capabilities, Passthrough
  p_nvm_features->modify_device_capacity = p_driver_features->passthrough;

  // get pools
  p_nvm_features->get_regions = (p_driver_features->passthrough &
             p_driver_features->get_interleave);

  // get address scrub data - Get Topology, Get Address Scrub
  p_nvm_features->get_address_scrub_data = p_driver_features->get_address_scrub_data;

  // start address scrub - Get Topology, Passthrough
  p_nvm_features->start_address_scrub = p_driver_features->passthrough;

  // quick diagnostic - Get Topology, Passthrough
  p_nvm_features->quick_diagnostic = p_driver_features->passthrough;

  // security diagnostic - Get Topology, Passthrough
  p_nvm_features->security_diagnostic = p_driver_features->passthrough;

  // platform config diagnostic - Get Topology, Get Platform Capabilities
  p_nvm_features->platform_config_diagnostic =
    p_driver_features->get_platform_capabilities && p_driver_features->passthrough;

  // fw consistency diagnostic - Get Topology, Passthrough
  p_nvm_features->fw_consistency_diagnostic = p_driver_features->passthrough;

  p_nvm_features->error_injection = p_driver_features->passthrough;

  // Namespace features correlate directly to driver features
  p_nvm_features->get_namespaces = 0;
  p_nvm_features->get_namespace_details = 0;
  p_nvm_features->create_namespace = 0;
  p_nvm_features->rename_namespace = 0;
  p_nvm_features->grow_namespace = 0;
  p_nvm_features->shrink_namespace = 0;
  p_nvm_features->enable_namespace = 0;
  p_nvm_features->disable_namespace = 0;
  p_nvm_features->delete_namespace = 0;

  // PM metadata diagnostic - Run Diagnostic
  p_nvm_features->pm_metadata_diagnostic = p_driver_features->run_diagnostic;

  // Driver memory mode capabilities
  p_nvm_features->app_direct_mode = p_driver_features->app_direct_mode;
  p_nvm_features->storage_mode = p_driver_features->storage_mode;

  return rc;
}

NVM_API int nvm_get_nvm_capabilities(struct nvm_capabilities *p_capabilties)
{
  int nvm_status;

  if (NULL == p_capabilties) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }
  // all capabilities are disabled by default
  memset(p_capabilties, 0, sizeof(struct nvm_capabilities));
  p_capabilties->nvm_features.get_devices = 1;
  p_capabilties->nvm_features.get_platform_capabilities = 1;
  p_capabilties->nvm_features.get_device_smbios = 1;

  // Start by retrieving and set the capabilities based
  // on what the driver supports.
  struct nvm_driver_capabilities driver_caps;
  memset(&driver_caps, 0, sizeof(driver_caps));
  os_get_driver_capabilities(&driver_caps);
  p_capabilties->sw_capabilities.min_namespace_size =
    (driver_caps.min_namespace_size < BYTES_PER_GIB) ? BYTES_PER_GIB :
    driver_caps.min_namespace_size;
  p_capabilties->sw_capabilities.namespace_memory_page_allocation_capable =
    driver_caps.namespace_memory_page_allocation_capable;
  driver_features_to_nvm_features(&driver_caps.features, &p_capabilties->nvm_features);
  //todo, impl bios capabilities

    return NVM_SUCCESS;
}

NVM_API int nvm_get_nvm_capacities(struct device_capacities *p_capacities)
{
  UINT64 VolatileCapacity;
  UINT64 AppDirectCapacity;
  UINT64 UnconfiguredCapacity;
  UINT64 ReservedCapacity;
  UINT64 InaccessibleCapacity;
  unsigned int i;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int rc = NVM_SUCCESS;
  UINT32 dimm_cnt;

  if (NULL == p_capacities) {
    NVDIMM_ERR("NULL input parameter\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  if (NVM_SUCCESS != nvm_get_number_of_devices(&dimm_cnt)) {
    NVDIMM_ERR("Failed to get number of devices\n");
    return NVM_ERR_UNKNOWN;
  }

  DIMM_INFO *pdimms = (DIMM_INFO *)AllocatePool(sizeof(DIMM_INFO) * dimm_cnt);
  if (NULL == pdimms) {
    NVDIMM_ERR("Failed to allocate memory\n");
    return NVM_ERR_UNKNOWN;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetDimms(&gNvmDimmDriverNvmDimmConfig, (UINT32)dimm_cnt, DIMM_INFO_CATEGORY_NONE, pdimms);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }
  for (i = 0; i < dimm_cnt; ++i) {
    ReturnCode = GetCapacities(pdimms[i].DimmID, &VolatileCapacity, &AppDirectCapacity, &UnconfiguredCapacity, &ReservedCapacity, &InaccessibleCapacity);
    if (EFI_ERROR(ReturnCode)) {
      rc = NVM_ERR_UNKNOWN;
      goto Finish;
    }
    p_capacities->capacity += VolatileCapacity + AppDirectCapacity + UnconfiguredCapacity + ReservedCapacity + InaccessibleCapacity;
    p_capacities->app_direct_capacity += AppDirectCapacity;
    p_capacities->unconfigured_capacity += UnconfiguredCapacity;
    p_capacities->reserved_capacity += ReservedCapacity;
    p_capacities->memory_capacity += VolatileCapacity;
    p_capacities->inaccessible_capacity += InaccessibleCapacity;
  }
Finish:
  FreePool(pdimms);
  return rc;
}

NVM_API int nvm_set_passphrase(const NVM_UID device_uid,
             const NVM_PASSPHRASE old_passphrase, const NVM_SIZE old_passphrase_len,
             const NVM_PASSPHRASE new_passphrase, const NVM_SIZE new_passphrase_len)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int rc = NVM_ERR_API_NOT_SUPPORTED;
  SYSTEM_CAPABILITIES_INFO SystemCapabilitiesInfo;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig,
      &SystemCapabilitiesInfo);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  if (!SystemCapabilitiesInfo.EnableDeviceSecuritySupported) {
    rc = NVM_ERR_OPERATION_NOT_SUPPORTED;
  }

Finish:
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveFormatsSupported);
  return rc;
}

NVM_API int nvm_remove_passphrase(const NVM_UID device_uid,
          const NVM_PASSPHRASE passphrase, const NVM_SIZE passphrase_len)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int rc = NVM_ERR_API_NOT_SUPPORTED;
  int nvm_status = NVM_SUCCESS;
  SYSTEM_CAPABILITIES_INFO SystemCapabilitiesInfo;
  UINT16 dimm_id;
  unsigned int dimm_handle, dimm_count;
  COMMAND_STATUS *p_command_status = NULL;
  CHAR16 UnicodePassphrase[PASSPHRASE_BUFFER_SIZE];

  SetMem(UnicodePassphrase, sizeof(UnicodePassphrase), 0x0);
  SystemCapabilitiesInfo.PtrInterleaveFormatsSupported = 0;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    rc = nvm_status;
    goto Finish;
  }

  ReturnCode = InitializeCommandStatus(&p_command_status);
  if (EFI_ERROR(ReturnCode)) {
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig,
      &SystemCapabilitiesInfo);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  if (!SystemCapabilitiesInfo.DisableDeviceSecuritySupported) {
    rc = NVM_ERR_OPERATION_NOT_SUPPORTED;
    goto Finish;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, &dimm_handle))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    goto Finish;
  }

  dimm_count = 1;
  ReturnCode = gNvmDimmDriverNvmDimmConfig.SetSecurityState(&gNvmDimmDriverNvmDimmConfig, &dimm_id,
    dimm_count, SECURITY_OPERATION_DISABLE_PASSPHRASE, AsciiStrToUnicodeStr(passphrase, UnicodePassphrase), NULL,
    p_command_status);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

Finish:
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveFormatsSupported);
  FreeCommandStatus(&p_command_status);
  return rc;
}

NVM_API int nvm_unlock_device(const NVM_UID device_uid,
            const NVM_PASSPHRASE passphrase, const NVM_SIZE passphrase_len)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int rc = NVM_ERR_API_NOT_SUPPORTED;
  SYSTEM_CAPABILITIES_INFO SystemCapabilitiesInfo;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig,
      &SystemCapabilitiesInfo);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  if (!SystemCapabilitiesInfo.UnlockDeviceSecuritySupported) {
    rc = NVM_ERR_OPERATION_NOT_SUPPORTED;
  }

Finish:
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveFormatsSupported);
  return rc;
}

NVM_API int nvm_freezelock_device(const NVM_UID device_uid)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int rc = NVM_ERR_API_NOT_SUPPORTED;
  SYSTEM_CAPABILITIES_INFO SystemCapabilitiesInfo;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig,
      &SystemCapabilitiesInfo);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  if (!SystemCapabilitiesInfo.FreezeDeviceSecuritySupported) {
    rc = NVM_ERR_OPERATION_NOT_SUPPORTED;
  }

Finish:
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveFormatsSupported);
  return rc;
}

NVM_API int nvm_erase_device(const NVM_UID device_uid,
           const NVM_PASSPHRASE passphrase, const NVM_SIZE passphrase_len)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int rc = NVM_ERR_API_NOT_SUPPORTED;
  SYSTEM_CAPABILITIES_INFO SystemCapabilitiesInfo;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig,
      &SystemCapabilitiesInfo);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  if (!SystemCapabilitiesInfo.EraseDeviceDataSupported) {
    rc = NVM_ERR_OPERATION_NOT_SUPPORTED;
  }

Finish:
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveFormatsSupported);
  return rc;
}

static void get_sensor_units(const enum sensor_type type, struct sensor *psensor)
{
  switch (type) {
  case SENSOR_MEDIA_TEMPERATURE:
  case SENSOR_CONTROLLER_TEMPERATURE:
    psensor->units = UNIT_CELSIUS;
    break;
  case SENSOR_PERCENTAGE_REMAINING:
    psensor->units = UNIT_PERCENT;
    break;
  case SENSOR_POWERCYCLES:
    psensor->units = UNIT_CYCLES;
    break;
  case SENSOR_UPTIME:
  case SENSOR_POWERONTIME:
    psensor->units = UNIT_SECONDS;
    break;
  case SENSOR_DIRTYSHUTDOWNS:
  case SENSOR_FWERRORLOGCOUNT:
  case SENSOR_HEALTH:
    psensor->units = UNIT_COUNT;
    break;
  }
}

int fill_sensor_info(DIMM_SENSOR    DimmSensorsSet[SENSOR_TYPE_COUNT],
         struct sensor *    p_sensor,
         const enum sensor_type type)
{
  if ((int)type > (int)SENSOR_TYPE_COUNT) {
    NVDIMM_ERR_W(L"Sensor type (%d) not supported\n", (int)type);
    return NVM_ERR_INVALIDPARAMETER;
  } else {
    p_sensor->type = (enum sensor_type)DimmSensorsSet[type].Type;
    p_sensor->current_state = (enum sensor_status)DimmSensorsSet[type].State;
    p_sensor->settings.lower_critical_threshold = DimmSensorsSet[type].CriticalLowerThreshold;
    p_sensor->settings.upper_critical_threshold = DimmSensorsSet[type].CriticalUpperThreshold;
    p_sensor->settings.upper_fatal_threshold = DimmSensorsSet[type].FatalThreshold;
    p_sensor->settings.upper_noncritical_threshold = DimmSensorsSet[type].NonCriticalThreshold;
    p_sensor->settings.enabled = DimmSensorsSet[type].Enabled;
    p_sensor->reading = DimmSensorsSet[type].Value;
    p_sensor->lower_critical_settable = (DimmSensorsSet[type].SettableThresholds & ThresholdLowerCritical) ? TRUE : FALSE;
    p_sensor->upper_critical_settable = (DimmSensorsSet[type].SettableThresholds & ThresholdUpperCritical) ? TRUE : FALSE;
    p_sensor->lower_fatal_settable = FALSE;
    p_sensor->upper_fatal_settable = (DimmSensorsSet[type].SettableThresholds & ThresholdUpperFatal) ? TRUE : FALSE;
    p_sensor->lower_noncritical_settable = (DimmSensorsSet[type].SettableThresholds & ThresholdLowerNonCritical) ? TRUE : FALSE;
    p_sensor->upper_noncritical_settable = (DimmSensorsSet[type].SettableThresholds & ThresholdUpperNonCritical) ? TRUE : FALSE;
    p_sensor->lower_critical_support = (DimmSensorsSet[type].SupportedThresholds & ThresholdLowerCritical) ? TRUE : FALSE;
    p_sensor->lower_fatal_support = FALSE;
    p_sensor->lower_noncritical_support = (DimmSensorsSet[type].SupportedThresholds & ThresholdLowerNonCritical) ? TRUE : FALSE;
    p_sensor->upper_noncritical_support = (DimmSensorsSet[type].SupportedThresholds & ThresholdUpperNonCritical) ? TRUE : FALSE;
    p_sensor->upper_fatal_support = (DimmSensorsSet[type].SupportedThresholds & ThresholdUpperFatal) ? TRUE : FALSE;
    p_sensor->upper_critical_support = (DimmSensorsSet[type].SupportedThresholds & ThresholdUpperCritical) ? TRUE : FALSE;
    get_sensor_units((enum sensor_type) DimmSensorsSet[type].Type, p_sensor);
    return NVM_SUCCESS;
  }
}

NVM_API int nvm_get_sensors(const NVM_UID device_uid, struct sensor *p_sensors,
          const NVM_UINT16 count)
{
  EFI_STATUS ReturnCode;
  UINT16 dimm_id;
  DIMM_SENSOR DimmSensorsSet[SENSOR_TYPE_COUNT];
  int rc = NVM_SUCCESS;
  int i;

  if (NULL == p_sensors) {
    NVDIMM_ERR("NULL input parameter\n");
    rc = NVM_ERR_INVALID_PARAMETER;
    goto Finish;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    goto Finish;
  }

  ReturnCode = GetSensorsInfo(&gNvmDimmDriverNvmDimmConfig, dimm_id, DimmSensorsSet);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(L"Failed to GetSensorsInfo\n");
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  for (i = 0; i < SENSOR_TYPE_COUNT; ++i) {
    rc = fill_sensor_info(DimmSensorsSet, &p_sensors[i], (enum sensor_type)i);
    if (EFI_ERROR(ReturnCode)) {
      rc = NVM_ERR_OPERATION_FAILED;
    }
  }

Finish:
  return rc;
}

NVM_API int nvm_get_sensor(const NVM_UID device_uid, const enum sensor_type type,
         struct sensor *p_sensor)
{
  EFI_STATUS EFIReturnCode = EFI_INVALID_PARAMETER;
  UINT16 dimm_id;
  DIMM_SENSOR DimmSensorsSet[SENSOR_TYPE_COUNT];
  int rc = NVM_SUCCESS;

  if (NULL == p_sensor) {
    NVDIMM_ERR("NULL input parameter\n");
    rc = NVM_ERR_INVALID_PARAMETER;
    goto Finish;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    goto Finish;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    goto Finish;
  }

  EFIReturnCode = GetSensorsInfo(&gNvmDimmDriverNvmDimmConfig, dimm_id, DimmSensorsSet);
  if (EFI_ERROR(EFIReturnCode)) {
    NVDIMM_ERR_W(L"Failed to GetSensorsInfo\n");
    rc = NVM_ERR_OPERATION_FAILED;
    goto Finish;
  }
  EFIReturnCode = fill_sensor_info(DimmSensorsSet, p_sensor, type);
  if (EFI_ERROR(EFIReturnCode)) {
    NVDIMM_ERR_W(L"Failed to FillSensorsInfo\n");
    rc = NVM_ERR_OPERATION_FAILED;
  }

Finish:
  return rc;
}

NVM_API int nvm_set_sensor_settings(const NVM_UID device_uid,
            const enum sensor_type type, const struct sensor_settings *p_settings)
{
  EFI_STATUS ReturnCode;
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT16 dimm_id;
  int rc = NVM_SUCCESS;

  if (NULL == p_settings) {
    NVDIMM_ERR("NULL input parameter\n");
    rc = NVM_ERR_INVALID_PARAMETER;
    goto Finish;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    goto Finish;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    goto Finish;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    rc = NVM_ERR_OPERATION_FAILED;
    goto Finish;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.SetAlarmThresholds(
    &gNvmDimmDriverNvmDimmConfig,
    (UINT16 *)&dimm_id,
    1,
    (UINT8)type,
    (INT16)p_settings->upper_noncritical_threshold,
    (UINT8)p_settings->enabled,
    pCommandStatus);

  if (EFI_ERROR(ReturnCode))
    rc = NVM_ERR_OPERATION_FAILED;

  FreeCommandStatus(&pCommandStatus);
Finish:
  return rc;
}

static unsigned int convert_event_filter_data_and_return_event_type(const struct event_filter *p_filter, NVM_UID dimm_uid, unsigned int *p_event_id)
{
  unsigned int event_type_mask = 0;

  if (NULL == p_filter) {
    event_type_mask = SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_ALL_MASK) |
      SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_ONLY_MASK);

    return event_type_mask;
  }

  if (NVM_FILTER_ON_TYPE & p_filter->filter_mask) {
    switch (p_filter->type) {
    case EVENT_TYPE_ALL:
      event_type_mask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_ALL_MASK);
      break;
    case EVENT_TYPE_CONFIG:
      event_type_mask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_CONFIG_MASK);
      break;
    case EVENT_TYPE_HEALTH:
      event_type_mask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_HEALTH_MASK);
      break;
    case EVENT_TYPE_MGMT:
      event_type_mask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_MGMT_MASK);
      break;
    case EVENT_TYPE_DIAG:
      event_type_mask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_DIAG_MASK);
      break;
    case EVENT_TYPE_DIAG_QUICK:
      event_type_mask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_QUICK_MASK);
      break;
    case EVENT_TYPE_DIAG_PLATFORM_CONFIG:
      event_type_mask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_PM_MASK);
      break;
    case EVENT_TYPE_DIAG_SECURITY:
      event_type_mask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_SECURITY_MASK);
      break;
    case EVENT_TYPE_DIAG_FW_CONSISTENCY:
      event_type_mask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_CAT_FW_MASK);
      break;
    }
  }
  if (NVM_FILTER_ON_SEVERITY & p_filter->filter_mask) {
    switch (p_filter->severity) {
    case EVENT_SEVERITY_INFO:
      event_type_mask |= SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_INFO_MASK);
      break;
    case EVENT_SEVERITY_WARN:
      event_type_mask |= SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_WARNING_MASK);
      break;
    case EVENT_SEVERITY_CRITICAL:
    case EVENT_SEVERITY_FATAL:
      event_type_mask |= SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_ERROR_MASK);
      break;
    }
  }
  if (NVM_FILTER_ON_CODE & p_filter->filter_mask) {
    // Not implemented yet
  }
  if (NVM_FILTER_ON_UID & p_filter->filter_mask)
    AsciiStrnCpy(dimm_uid, p_filter->uid, sizeof(p_filter->uid));
  if (NVM_FILTER_ON_AFTER & p_filter->filter_mask) {
    // Not implemented yet
  }
  if (NVM_FILTER_ON_BEFORE & p_filter->filter_mask) {
    // Not implemented yet
  }
  if (NVM_FILTER_ON_EVENT & p_filter->filter_mask)
    *p_event_id = (unsigned int)p_filter->event_id;
  if (NVM_FILTER_ON_AR & p_filter->filter_mask)
    event_type_mask |= (SYSTEM_EVENT_TYPE_AR_STATUS_SET(TRUE) | SYSTEM_EVENT_TYPE_AR_EVENT_SET(p_filter->action_required));

  return event_type_mask;
}

static void convert_log_entry_to_event(log_entry *p_log_entry, char *event_message, struct event *p_event)
{
  char *p_src_msg = event_message;
  char *p_dst_msg = p_event->message;

  if (SYSTEM_EVENT_TYPE_CATEGORY_MASK & p_log_entry->event_type) {
    switch (SYSTEM_EVENT_TYPE_CATEGORY_GET(p_log_entry->event_type)) {
    case SYSTEM_EVENT_CAT_CONFIG:
      p_event->type = EVENT_TYPE_CONFIG;
      break;
    case SYSTEM_EVENT_CAT_HEALTH:
      p_event->type = EVENT_TYPE_HEALTH;
      break;
    case SYSTEM_EVENT_CAT_MGMT:
      p_event->type = EVENT_TYPE_MGMT;
      break;
    case SYSTEM_EVENT_CAT_DIAG:
      p_event->type = EVENT_TYPE_DIAG;
      break;
    case SYSTEM_EVENT_CAT_QUICK:
      p_event->type = EVENT_TYPE_DIAG_QUICK;
      break;
    case SYSTEM_EVENT_CAT_PM:
      p_event->type = EVENT_TYPE_DIAG_PLATFORM_CONFIG;
      break;
    case SYSTEM_EVENT_CAT_SECURITY:
      p_event->type = EVENT_TYPE_DIAG_SECURITY;
      break;
    case SYSTEM_EVENT_CAT_FW:
      p_event->type = EVENT_TYPE_DIAG_FW_CONSISTENCY;
      break;
    }
  }
  if (SYSTEM_EVENT_TYPE_SEVERITY_MASK & p_log_entry->event_type) {
    switch (SYSTEM_EVENT_TYPE_SEVERITY_GET(p_log_entry->event_type)) {
    case SYSTEM_EVENT_TYPE_INFO:
      p_event->severity = EVENT_SEVERITY_INFO;
      break;
    case SYSTEM_EVENT_TYPE_WARNING:
      p_event->severity = EVENT_SEVERITY_WARN;
      break;
    case SYSTEM_EVENT_TYPE_ERROR:
      p_event->severity = EVENT_SEVERITY_CRITICAL;
      break;
    }
  }
  if (SYSTEM_EVENT_TYPE_NUMBER_MASK & p_log_entry->event_type)
    p_event->code = SYSTEM_EVENT_TYPE_NUMBER_GET(p_log_entry->event_type);

  for (; (*p_src_msg != '\n') && (*p_src_msg != 0); p_src_msg++, p_dst_msg++)
    *p_dst_msg = *p_src_msg;
  *p_dst_msg = 0; // add the null terminator
}

NVM_API int nvm_get_number_of_events(const struct event_filter *p_filter, int *count)
{
  unsigned int event_type_mask = 0;
  NVM_UID dimm_uid = { 0 };
  unsigned int event_id = 0;
  log_entry *p_log_entry = NULL;
  log_entry *p_prev_log_entry = NULL;
  int rc = NVM_SUCCESS;

  if (NULL == count) {
    return NVM_ERR_INVALIDPARAMETER;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  event_type_mask = convert_event_filter_data_and_return_event_type(p_filter, dimm_uid, &event_id);
  // Get events form system log
  nvm_get_log_entries_from_file(event_type_mask, dimm_uid, event_id, SYSTEM_EVENT_NOT_APPLICABLE, &p_log_entry);
  *count = 0;
  while (p_log_entry) {
    *count += 1;
    p_prev_log_entry = p_log_entry;
    p_log_entry = p_log_entry->p_next;
    FreePool(p_prev_log_entry);
  }
  return NVM_SUCCESS;
}

NVM_API int nvm_get_events(const struct event_filter *p_filter,
         struct event *p_events, const NVM_UINT16 count)
{
  char *event_buffer = NULL;
  unsigned int events_number = count;
  log_entry *p_log_entry = NULL;
  log_entry *p_previous_entry = NULL;
  char *p_event_message = NULL;
  struct event *p_current_event = p_events;
  int bytes_in_event_buffer = 0;
  unsigned int event_type_mask = 0;
  NVM_UID dimm_uid = { 0 };
  unsigned int event_id = 0;
  int rc = NVM_SUCCESS;
  int nvm_status = 0;

  if (NULL == p_events) {
    return NVM_ERR_INVALIDPARAMETER;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  int actual_count = 0;
  if (NVM_SUCCESS != (nvm_status = nvm_get_number_of_events(p_filter, &actual_count)))
  {
    NVDIMM_ERR("Failed to obtain the number of devices (%d)\n",nvm_status);
    return NVM_ERR_OPERATION_FAILED;
  }

  if (count != actual_count)
  {
    return NVM_ERR_BAD_SIZE;
  }

  event_type_mask = convert_event_filter_data_and_return_event_type(p_filter, dimm_uid, &event_id);
  // Get events form system log
  bytes_in_event_buffer = nvm_get_events_from_file(event_type_mask, dimm_uid, event_id, events_number, &p_log_entry, &event_buffer);
  while ((bytes_in_event_buffer > 0) && (events_number > 0)) {
    p_event_message = event_buffer + p_log_entry->message_offset;
    convert_log_entry_to_event(p_log_entry, p_event_message, p_current_event);
    nvm_get_event_id_form_entry(p_current_event->message, &(p_current_event->event_id));
    nvm_get_uid_form_entry(p_current_event->message, sizeof(p_current_event->uid), p_current_event->uid);

    p_previous_entry = p_log_entry;
    p_log_entry = p_log_entry->p_next;
    if (p_log_entry && (p_log_entry->message_offset == p_previous_entry->message_offset)) {
      // It looks the event log service could not allocate enough memory
      // to get all entries and the next messages are pointing out to the same location
      rc = NVM_ERR_NOT_ENOUGH_FREE_SPACE;
    }
    FreePool(p_previous_entry);

    p_current_event++;
    events_number--;
  }
  free(event_buffer);
  return rc;
}

NVM_API int nvm_purge_events(const struct event_filter *p_filter)
{
  unsigned int event_type_mask = 0;
  NVM_UID dimm_uid = { 0 };
  unsigned int event_id = 0;
  int rc = NVM_SUCCESS;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  event_type_mask = convert_event_filter_data_and_return_event_type(p_filter, dimm_uid, &event_id);
  return nvm_remove_events_from_file(event_type_mask, dimm_uid, event_id);
}

NVM_API int nvm_acknowledge_event(NVM_UINT32 event_id)
{
  int rc = NVM_SUCCESS;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  return nvm_clear_action_required(event_id);
}

NVM_API int nvm_get_number_of_regions(NVM_UINT8 *count)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  COMMAND_STATUS *pCommandStatus = NULL;
  unsigned int region_count = 0;
  int rc = NVM_SUCCESS;

  if (NULL == count)
    return NVM_ERR_INVALIDPARAMETER;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetRegionCount(
    &gNvmDimmDriverNvmDimmConfig,
    &region_count);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    rc = NVM_ERR_UNKNOWN;
  }
  FreeCommandStatus(&pCommandStatus);
Finish:
  *count = (NVM_UINT8)region_count;
  return rc;
}

NVM_API int nvm_get_regions(struct region *p_regions, NVM_UINT8 *count)
{
  COMMAND_STATUS *pCommandStatus = NULL;
  NVM_UINT8 RegionCount, Index, DimmIndex;
  REGION_INFO *pRegions = NULL;
  EFI_STATUS erc;
  int rc = NVM_SUCCESS;

  if ((NULL == p_regions) || (NULL == count)) {
    return NVM_ERR_INVALIDPARAMETER;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  erc = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(erc))
    return NVM_ERR_UNKNOWN;

  if (NVM_SUCCESS != (rc = nvm_get_number_of_regions(&RegionCount))) {
    FreeCommandStatus(&pCommandStatus);
    return rc;
  }

  pRegions = AllocateZeroPool(sizeof(REGION_INFO) * RegionCount);
  if (pRegions == NULL) {
    FreeCommandStatus(&pCommandStatus);
    return NVM_ERR_NO_MEM;
  }

  erc = gNvmDimmDriverNvmDimmConfig.GetRegions(&gNvmDimmDriverNvmDimmConfig, RegionCount, pRegions, pCommandStatus);
  if (EFI_ERROR(erc)) {
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  if (RegionCount > *count)
    RegionCount = *count;

  for (Index = 0; Index < RegionCount; Index++) {
    memset(&p_regions[Index], 0, sizeof(struct region));
    p_regions[Index].socket_id = pRegions[Index].SocketId;
    p_regions[Index].isetId = pRegions[Index].CookieId;
    p_regions[Index].capacity = pRegions[Index].Capacity;
    p_regions[Index].free_capacity = pRegions[Index].FreeCapacity;
    p_regions[Index].health = pRegions[Index].Health;
    p_regions[Index].type = pRegions[Index].RegionType;
    p_regions[Index].dimm_count = pRegions[Index].DimmIdCount;

    for (DimmIndex = 0; DimmIndex < pRegions[Index].DimmIdCount; DimmIndex++)
      p_regions[Index].dimms[DimmIndex] = pRegions[Index].DimmId[DimmIndex];
  }

  *count = RegionCount;

Finish:
  FreeCommandStatus(&pCommandStatus);
  FreePool(pRegions);
  return rc;
}

NVM_API int nvm_create_config_goal(NVM_UID *p_device_uids, NVM_UINT32 device_uids_count,
           struct config_goal_input *p_goal_input)
{
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT16 *p_dimm_ids = NULL;
  int rc = NVM_SUCCESS;
  EFI_STATUS efi_rc = EFI_INVALID_PARAMETER;
  unsigned int Index = 0;

  if (NULL == p_goal_input) {
    return NVM_ERR_INVALIDPARAMETER;
  }

  // if no device UIDs force count to 0
  if (NULL == p_device_uids) {
    device_uids_count = 0;
  }

  efi_rc = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(efi_rc)) {
    return NVM_ERR_UNKNOWN;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    goto Finish;
  }

  // If user passed DIMM uids, convert to id
  if (p_device_uids != NULL && device_uids_count > 0) {
    p_dimm_ids = (UINT16 *) AllocateZeroPool(sizeof(UINT16) * device_uids_count);
        if (NULL == p_dimm_ids) {
            NVDIMM_ERR("Failed to allocate zero region");
            rc = NVM_ERR_NOT_ENOUGH_FREE_SPACE;
            goto Finish;
        }
    for (Index = 0; Index < device_uids_count; Index++) {
      if (NVM_SUCCESS != (rc = get_dimm_id(p_device_uids[Index], &p_dimm_ids[Index], NULL))) {
        NVDIMM_ERR("Failed to get DIMM ID %d\n", rc);
        goto Finish;
      }
    }
  }

  efi_rc = gNvmDimmDriverNvmDimmConfig.CreateGoalConfig(&gNvmDimmDriverNvmDimmConfig,
                    FALSE, p_dimm_ids, device_uids_count, NULL, 0,
                    p_goal_input->persistent_mem_type, p_goal_input->volatile_percent,
                    p_goal_input->reserved_percent, p_goal_input->reserve_dimm,
                    p_goal_input->namespace_label_major, p_goal_input->namespace_label_minor,
                    pCommandStatus);

  if (EFI_ERROR(efi_rc))
    rc = NVM_ERR_UNKNOWN;
Finish:
    FreeCommandStatus(&pCommandStatus);
    FREE_POOL_SAFE(p_dimm_ids);
  return rc;
}

NVM_API int nvm_get_config_goal(NVM_UID *p_device_uids, NVM_UINT32 device_uids_count,
        struct config_goal *p_goal)
{
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT16 *p_dimm_ids = NULL;
  unsigned int region_configs_count;
  int rc = NVM_SUCCESS;
  EFI_STATUS efi_rc = EFI_INVALID_PARAMETER;
  REGION_GOAL_PER_DIMM_INFO *pRegionConfigsInfo = NULL;
  unsigned int Index = 0;
  unsigned int Index2 = 0;

  if (NULL == p_goal) {
    return NVM_ERR_INVALIDPARAMETER;
  }

  // if no device UIDs force count to 0
  if (NULL == p_device_uids) {
    device_uids_count = 0;
  }

  efi_rc = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(efi_rc)) {
    return NVM_ERR_UNKNOWN;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    goto Finish;
  }

  // If user passed DIMM uids, convert to id
  if (p_device_uids != NULL && device_uids_count > 0) {
    p_dimm_ids = (UINT16 *) AllocateZeroPool(sizeof(UINT16) * device_uids_count);
        if (NULL == p_dimm_ids) {
            NVDIMM_ERR("Failed to allocate zero region");
            rc = NVM_ERR_NOT_ENOUGH_FREE_SPACE;
            goto Finish;
        }
        for (Index = 0; Index < device_uids_count; Index++) {
      if (NVM_SUCCESS != (rc = get_dimm_id(p_device_uids[Index], &p_dimm_ids[Index], NULL))) {
        NVDIMM_ERR("Failed to get DIMM ID %d\n", rc);
        goto Finish;
      }
    }
  }

  pRegionConfigsInfo = AllocateZeroPool(sizeof(*pRegionConfigsInfo) * MAX_DIMMS);
    if (NULL == pRegionConfigsInfo) {
        NVDIMM_ERR("Failed to allocate zero region: pRegionConfigsInfo");
        rc = NVM_ERR_NOT_ENOUGH_FREE_SPACE;
        goto Finish;
    }
  efi_rc = gNvmDimmDriverNvmDimmConfig.GetGoalConfigs(&gNvmDimmDriverNvmDimmConfig,
                  p_dimm_ids, device_uids_count, NULL, 0, MAX_DIMMS, pRegionConfigsInfo,
                  &region_configs_count, pCommandStatus);

    if (EFI_ERROR(efi_rc)) {
        rc = NVM_ERR_UNKNOWN;
        goto Finish;
    }

  for (Index = 0; Index < region_configs_count; Index++) {
    UnicodeStrToAsciiStr(pRegionConfigsInfo[Index].DimmUid, p_goal[Index].dimm_uid);
    p_goal[Index].socket_id = pRegionConfigsInfo[Index].SocketId;
    p_goal[Index].persistent_regions = pRegionConfigsInfo[Index].PersistentRegions;
    p_goal[Index].volatile_size = pRegionConfigsInfo[Index].VolatileSize;
    p_goal[Index].storage_capacity = pRegionConfigsInfo[Index].StorageCapacity;
    for (Index2 = 0; Index2 < MAX_IS_PER_DIMM; Index2++) {
      p_goal[Index].appdirect_size[Index2] =
        pRegionConfigsInfo[Index].AppDirectSize[Index2];
      p_goal[Index].interleave_set_type[Index2] =
        pRegionConfigsInfo[Index].InterleaveSetType[Index2];
      p_goal[Index].imc_interleaving[Index2] =
        pRegionConfigsInfo[Index].ImcInterleaving[Index2];
      p_goal[Index].channel_interleaving[Index2] =
        pRegionConfigsInfo[Index].ChannelInterleaving[Index2];
      p_goal[Index].appdirect_index[Index2] =
        pRegionConfigsInfo[Index].AppDirectIndex[Index2];
    }
    p_goal[Index].status = pRegionConfigsInfo[Index].Status;
  }

Finish:
    FreeCommandStatus(&pCommandStatus);
    FREE_POOL_SAFE(p_dimm_ids);
    FREE_POOL_SAFE(pRegionConfigsInfo);
  return rc;
}

NVM_API int nvm_delete_config_goal(NVM_UID *p_device_uids, NVM_UINT32 device_uids_count)
{
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT16 *p_dimm_ids = NULL;
  EFI_STATUS efi_rc = EFI_INVALID_PARAMETER;
  int rc = NVM_SUCCESS;
  unsigned int i;

  // if no device UIDs force count to 0
  if (NULL == p_device_uids) {
    device_uids_count = 0;
  }

  efi_rc = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(efi_rc)) {
    return NVM_ERR_UNKNOWN;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    goto Finish;
  }

  // If user passed DIMM uids, convert to id
  if (p_device_uids != NULL && device_uids_count > 0) {
    p_dimm_ids = (UINT16 *) AllocateZeroPool(sizeof(UINT16) * device_uids_count);
        if (NULL == p_dimm_ids) {
            NVDIMM_ERR("Failed to allocate memory: p_dimm_ids");
            rc = NVM_ERR_NOT_ENOUGH_FREE_SPACE;
            goto Finish;
        }
    for (i = 0; i < device_uids_count; i++) {
      if (NVM_SUCCESS != (rc = get_dimm_id(p_device_uids[i], &p_dimm_ids[i], NULL))) {
        NVDIMM_ERR("Failed to get DIMM ID %d\n", rc);
        goto Finish;
      }
    }
  }

  efi_rc = gNvmDimmDriverNvmDimmConfig.DeleteGoalConfig(&gNvmDimmDriverNvmDimmConfig,
                    p_dimm_ids, device_uids_count, NULL, 0, pCommandStatus);

  if (EFI_ERROR(efi_rc))
    rc = NVM_ERR_UNKNOWN;
Finish:
    FreeCommandStatus(&pCommandStatus);
    FREE_POOL_SAFE(p_dimm_ids);
  return rc;
}



NVM_API int nvm_dump_goal_config(const NVM_PATH file,
         const NVM_SIZE file_len)
{
  int rc = NVM_SUCCESS;
  EFI_STATUS ReturnCode;
  COMMAND_STATUS *p_command_status;
  CHAR16 file_name[NVM_PATH_LEN];

  if (file_len > NVM_PATH_LEN)
    return NVM_ERR_UNKNOWN;
  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  ReturnCode = InitializeCommandStatus(&p_command_status);
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;
  ReturnCode = gNvmDimmDriverNvmDimmConfig.DumpGoalConfig(&gNvmDimmDriverNvmDimmConfig, AsciiStrToUnicodeStr(file, file_name), NULL, p_command_status);
  if (NVM_SUCCESS != ReturnCode) {
    NVDIMM_ERR("Failed to get the DIMMs goal configuration. Return code %d", ReturnCode);
    return NVM_ERR_DUMP_FILE_OPERATION_FAILED;
  }
  FreeCommandStatus(&p_command_status);
  return NVM_SUCCESS;
}


NVM_API int nvm_load_goal_config(const NVM_PATH file,
         const NVM_SIZE file_len)
{
  int rc = NVM_SUCCESS;
  EFI_STATUS ReturnCode;
  COMMAND_STATUS *p_command_status;
  unsigned int dimm_count = 0;
  DIMM_INFO *pdimms = NULL;
  UINT16 *p_dimm_ids = NULL;
  unsigned int index;
  unsigned int socket_count = 0;
  SOCKET_INFO *p_sockets = NULL;
  UINT16 *p_socket_ids = NULL;
  char *p_file_string = NULL;
  CHAR16 file_name[NVM_PATH_LEN];

  if (file_len > NVM_PATH_LEN)
    return NVM_ERR_UNKNOWN;
  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  ReturnCode = InitializeCommandStatus(&p_command_status);
  if (EFI_ERROR(ReturnCode)) {
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }
  if (NVM_SUCCESS != nvm_get_number_of_devices(&dimm_count)) {
    NVDIMM_ERR("Failed to get number of devices\n");
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }
  pdimms = (DIMM_INFO *)AllocatePool(sizeof(DIMM_INFO) * dimm_count);
  if (NULL == pdimms) {
    NVDIMM_ERR("Failed to allocate memory\n");
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetDimms(&gNvmDimmDriverNvmDimmConfig, dimm_count, DIMM_INFO_CATEGORY_NONE, pdimms);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to get DIMMs information. Return code %d", ReturnCode);
    rc = NVM_ERR_DIMM_NOT_FOUND;
    goto Finish;
  }
  p_dimm_ids = (UINT16 *)AllocatePool(sizeof(UINT16) * dimm_count);
  if (NULL == p_dimm_ids) {
    NVDIMM_ERR("Failed to allocate memory\n");
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }
  for (index = 0; index < dimm_count; index++)
    p_dimm_ids[index] = pdimms[index].DimmID;
  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetSockets(&gNvmDimmDriverNvmDimmConfig, &socket_count, &p_sockets);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to get DIMMs information. Return code %d", ReturnCode);
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }
  p_socket_ids = (UINT16 *)AllocatePool(sizeof(UINT16) * socket_count);
  if (NULL == p_socket_ids) {
    NVDIMM_ERR("Failed to allocate memory\n");
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }
  for (index = 0; index < socket_count; index++)
    p_socket_ids[index] = p_sockets[index].SocketId;
  ReturnCode = ParseSourceDumpFile(AsciiStrToUnicodeStr(file, file_name), NULL, &p_file_string);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to dump a file %s. Return code &d\n", file, ReturnCode);
    rc = NVM_ERR_UNKNOWN;
    goto Finish;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.LoadGoalConfig(&gNvmDimmDriverNvmDimmConfig, p_dimm_ids, dimm_count, p_socket_ids, socket_count, p_file_string, p_command_status);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to load the goal configuration. Return code &d\n", ReturnCode);
    rc = NVM_ERR_CREATE_GOAL_NOT_ALLOWED;
    goto Finish;
  }
Finish:
  FreeCommandStatus(&p_command_status);
  FREE_POOL_SAFE(pdimms);
  FREE_POOL_SAFE(p_dimm_ids);
  FREE_POOL_SAFE(p_sockets);
  FREE_POOL_SAFE(p_socket_ids);
  FREE_POOL_SAFE(p_file_string);
  return rc;
}

void get_version_numbers(int *major, int *minor, int *hotfix, int *build)
{
  int first;
  int second;
  int third;
  int fourth;

  sscanf_s(VERSION_STR, "%d.%d.%d.%d", &first, &second, &third, &fourth);

  if(major)
    *major = first;

  if(minor)
    *minor = second;

  if(hotfix)
    *hotfix = third;

  if(build)
    *build = fourth;
}

NVM_API int nvm_get_major_version()
{
  int major = 0;
  get_version_numbers(&major, NULL, NULL, NULL);
  return major;
}

NVM_API int nvm_get_minor_version()
{
  int minor = 0;
  get_version_numbers(NULL, &minor, NULL, NULL);
  return minor;
}

NVM_API int nvm_get_hotfix_number()
{
  int hotfix = 0;
  get_version_numbers(NULL, NULL, &hotfix, NULL);
  return hotfix;
}

NVM_API int nvm_get_build_number()
{
  int build = 0;
  get_version_numbers(NULL, NULL, NULL, &build);
  return build;
}


EFI_STATUS execute_cli_cmd(wchar_t * cmdline)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  struct CommandInput Input;
  struct Command Command;

  ReturnCode = RegisterCommands();
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;

  FillCommandInput(cmdline, &Input);
  ReturnCode = Parse(&Input, &Command);
  if (!EFI_ERROR(ReturnCode))
  {
    /* parse success, now run the command */
    ReturnCode = ExecuteCmd(&Command);
  }
  FreeCommandInput(&Input);
  return ReturnCode;
}

NVM_API int nvm_gather_support(const NVM_PATH support_file, const NVM_SIZE support_file_len)
{
  int rc = NVM_SUCCESS;
  unsigned int Index;

#define MAX_EXEC_CMDS 10
  CHAR16 exec_commands[MAX_EXEC_CMDS][100] = {
    { L"version"         },
    { L"show -memoryresources"     },
    { L"show -a -dimm"       },
    { L"show -a -system -capabilities" },
    { L"show -a -topology"       },
    { L"show -a -sensor"       },
    { L"show -dimm -performance"     },
    { L"show -system -host"      },
    { L"start -diagnostic"       },
    { L"show -event"       }
  };

  if (support_file_len > NVM_PATH_LEN)
    return NVM_ERR_UNKNOWN;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  if (NULL == (gOsShellParametersProtocol.StdOut = fopen(support_file, "w+")))
    return NVM_ERR_UNKNOWN;

  for (Index = 0; Index < MAX_EXEC_CMDS; ++Index) 
  {
    execute_cli_cmd(exec_commands[Index]);
  }

  fclose(gOsShellParametersProtocol.StdOut);
  gOsShellParametersProtocol.StdOut = stdout;

  return NVM_SUCCESS;
}


NVM_API int nvm_inject_device_error(const NVM_UID		device_uid,
            const struct device_error * p_error)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 DimmId;
  UINT32 DimmCount;
  UINT32 rc = NVM_SUCCESS;
  UINT8 ClearStatus = 0;
  COMMAND_STATUS *pCommandStatus = NULL;

  if (NULL == p_error)
    return NVM_ERR_INVALIDPARAMETER;

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    goto Finish;
  }
  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &DimmId, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    goto Finish;
  }

  DimmCount = 1;
  ReturnCode = gNvmDimmDriverNvmDimmConfig.InjectError(&gNvmDimmDriverNvmDimmConfig, &DimmId, DimmCount,
                   (UINT8)p_error->type, ClearStatus, (UINT64 *)&p_error->temperature, (UINT64 *)&p_error->dpa,
                   (UINT8 *)&p_error->memory_type, (UINT8 *)&p_error->percentageRemaining, pCommandStatus);

  if (EFI_ERROR(ReturnCode))
    rc = NVM_ERR_UNKNOWN;

Finish:
  FreeCommandStatus(&pCommandStatus);
  return rc;
}

NVM_API int nvm_clear_injected_device_error(const NVM_UID device_uid,
              const struct device_error *p_error)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 DimmId;
  UINT32 DimmCount;
  UINT32 rc = NVM_SUCCESS;
  UINT8 ClearStatus = 1;
  COMMAND_STATUS *pCommandStatus = NULL;

  if (NULL == p_error)
    return NVM_ERR_INVALIDPARAMETER;

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    goto Finish;
  }
  if (NVM_SUCCESS != (rc = get_dimm_id(device_uid, &DimmId, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    goto Finish;
  }

  DimmCount = 1;
  ReturnCode = gNvmDimmDriverNvmDimmConfig.InjectError(&gNvmDimmDriverNvmDimmConfig, &DimmId, DimmCount,
                   (UINT8)p_error->type, ClearStatus, (UINT64 *)&p_error->temperature, (UINT64 *)&p_error->dpa,
                   (UINT8 *)&p_error->memory_type, (UINT8 *)&p_error->percentageRemaining, pCommandStatus);

  if (EFI_ERROR(ReturnCode))
    rc = NVM_ERR_UNKNOWN;

Finish:
  FreeCommandStatus(&pCommandStatus);
  return rc;
}

NVM_API int nvm_run_diagnostic(const NVM_UID device_uid,
             const struct diagnostic *p_diagnostic, NVM_UINT32 *p_results)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT8 diag_tests = 0;
  UINT16 dimm_id;
  UINT16 *p_dimm_id;
  CHAR16 *pFinalDiagnosticsResult = NULL;
  UINT32 dimm_count;
  int rc = NVM_SUCCESS;

  if (NULL == p_diagnostic)
    return NVM_ERR_INVALIDPARAMETER;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  if (NULL == device_uid) {
    dimm_count = 0;
    p_dimm_id = NULL;
  } else {
    if (NVM_SUCCESS != (rc = get_dimm_id((char *)device_uid, &dimm_id, NULL))) {
      NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
      goto Finish;
    } else {
      dimm_count = 1;
      p_dimm_id = (UINT16 *)&dimm_id;
    }
  }

  if (DIAG_TYPE_QUICK == p_diagnostic->test) {
    diag_tests = DIAGNOSTIC_TEST_QUICK;
  } else if (DIAG_TYPE_PLATFORM_CONFIG == p_diagnostic->test) {
    diag_tests = DIAGNOSTIC_TEST_CONFIG;
  } else if (DIAG_TYPE_SECURITY == p_diagnostic->test) {
    diag_tests = DIAGNOSTIC_TEST_SECURITY;
  } else if (DIAG_TYPE_FW_CONSISTENCY == p_diagnostic->test) {
    diag_tests = DIAGNOSTIC_TEST_FW;
  } else {
    rc = NVM_ERR_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = gNvmDimmDriverNvmDimmConfig.StartDiagnostic(
    &gNvmDimmDriverNvmDimmConfig,
    p_dimm_id,
    dimm_count,
    diag_tests,
    DISPLAY_DIMM_ID_UID,
    &pFinalDiagnosticsResult);

  Print(pFinalDiagnosticsResult);
  FreePool(pFinalDiagnosticsResult);
  if (EFI_ERROR(ReturnCode))
    rc = NVM_ERR_UNKNOWN;

Finish:
  return rc;
}

NVM_API int nvm_set_user_preference(const NVM_PREFERENCE_KEY  key,
            const NVM_PREFERENCE_VALUE  value)
{
  int nvm_status;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 KeyWide[NVM_THRESHOLD_STR_LEN];
  CHAR16 ValueWide[NVM_THRESHOLD_STR_LEN];
  CHAR16 CmdLineWide[4096];

  if (NULL == key || NULL == value)
  {
    return NVM_ERR_INVALIDPARAMETER;
  }

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }

  AsciiStrToUnicodeStr(key, KeyWide);
  AsciiStrToUnicodeStr(value, ValueWide);
  UnicodeSPrint(CmdLineWide, sizeof(CmdLineWide), L"set -preferences " FORMAT_STR L"=" FORMAT_STR, KeyWide, ValueWide);
  ReturnCode = execute_cli_cmd(CmdLineWide);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Set Preference CMD failed (%d)\n", ReturnCode);
    return NVM_ERR_UNKNOWN;
  }
  //flush dictionary to file
  preferences_flush_the_file();
  return NVM_SUCCESS;
}

NVM_API int nvm_clear_dimm_lsa(const NVM_UID device_uid)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 dimm_id;
  int nvm_status;
  COMMAND_STATUS *p_command_status;

  if (NVM_SUCCESS != (nvm_status = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", nvm_status);
    return nvm_status;
  }
  ReturnCode = InitializeCommandStatus(&p_command_status);
  if (EFI_ERROR(ReturnCode))
    return NVM_ERR_UNKNOWN;
  if (NVM_SUCCESS != (nvm_status = get_dimm_id((char *)device_uid, &dimm_id, NULL))) {
    FreeCommandStatus(&p_command_status);
    NVDIMM_ERR("Failed to get dimm ID %d\n", nvm_status);
    return NVM_ERR_DIMM_NOT_FOUND;
  }
  ReturnCode = gNvmDimmDriverNvmDimmConfig.DeletePcd(&gNvmDimmDriverNvmDimmConfig, &dimm_id, 1, p_command_status);
  if (EFI_ERROR(ReturnCode)) {
    FreeCommandStatus(&p_command_status);
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    return NVM_ERR_UNKNOWN;
  }
  FreeCommandStatus(&p_command_status);
  return NVM_SUCCESS;
}

/*
 * Function enables disables the debug logger
 */
extern int EFIAPI DebugLoggerEnable(IN BOOLEAN EnableDbgLogger);
/*
* Function returns the current state of the debug logger
*/
extern BOOLEAN EFIAPI IsDebugLoggerEnabled();

NVM_API int nvm_debug_logging_enabled()
{
  int rc = NVM_SUCCESS;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  return IsDebugLoggerEnabled();
}

NVM_API int nvm_toggle_debug_logging(const NVM_BOOL enabled)
{
  int rc = NVM_SUCCESS;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  return DebugLoggerEnable(enabled);
}

NVM_API int nvm_purge_debug_log()
{
  unsigned int event_type_mask = 0;
  int rc = NVM_SUCCESS;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  event_type_mask = SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_DEBUG_MASK);
  return nvm_remove_events_from_file(event_type_mask, NULL, SYSTEM_EVENT_NOT_APPLICABLE);
}

NVM_API int nvm_get_number_of_debug_logs(int *count)
{
  unsigned int event_type_mask = 0;
  log_entry *p_log_entry = NULL;
  log_entry *p_prev_log_entry = NULL;
  int rc = NVM_SUCCESS;

  if (NULL == count)
    return NVM_ERR_UNKNOWN;
  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  event_type_mask = SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_DEBUG_MASK);
  // Get events form system log
  nvm_get_log_entries_from_file(event_type_mask, NULL, SYSTEM_EVENT_NOT_APPLICABLE, SYSTEM_EVENT_NOT_APPLICABLE, &p_log_entry);
  *count = 0;
  while (p_log_entry) {
    *count += 1;
    p_prev_log_entry = p_log_entry;
    p_log_entry = p_log_entry->p_next;
    FreePool(p_prev_log_entry);
  }
  return NVM_SUCCESS;
}

static void convert_debug_log_entry_to_event(log_entry *p_log_entry, char *event_message, struct nvm_log *p_event)
{
  char *p_src_msg = event_message;
  char *p_dst_msg = p_event->message;

  for (; (*p_src_msg != '\n') && (*p_src_msg != 0); p_src_msg++, p_dst_msg++)
    *p_dst_msg = *p_src_msg;
  *p_dst_msg = 0; // add the null terminator
}

NVM_API int nvm_get_debug_logs(struct nvm_log *p_logs, const NVM_UINT32 count)
{
  char *event_buffer = NULL;
  unsigned int events_number = count;
  log_entry *p_log_entry = NULL;
  log_entry *p_previous_entry = NULL;
  char *p_event_message = NULL;
  struct nvm_log *p_current_event = p_logs;
  int bytes_in_event_buffer = 0;
  unsigned int event_type_mask = 0;
  int rc = NVM_SUCCESS;
  int nvm_status = 0;

  if (NULL == p_logs)
    return NVM_ERR_INVALIDPARAMETER;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  int actual_count = 0;
  if (NVM_SUCCESS != (nvm_status = nvm_get_number_of_debug_logs(&actual_count)))
  {
    NVDIMM_ERR("Failed to obtain the number of devices (%d)\n",nvm_status);
    return NVM_ERR_OPERATION_FAILED;
  }

  if (count != actual_count)
  {
    return NVM_ERR_BAD_SIZE;
  }

  event_type_mask = SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_DEBUG_MASK);
  // Get events form system log
  bytes_in_event_buffer = nvm_get_events_from_file(event_type_mask, NULL, SYSTEM_EVENT_NOT_APPLICABLE, events_number, &p_log_entry, &event_buffer);
  while ((bytes_in_event_buffer > 0) && (events_number > 0)) {
    p_event_message = event_buffer + p_log_entry->message_offset;
    convert_debug_log_entry_to_event(p_log_entry, p_event_message, p_current_event);

    p_previous_entry = p_log_entry;
    p_log_entry = p_log_entry->p_next;
    if (p_log_entry && (p_log_entry->message_offset == p_previous_entry->message_offset)) {
      // It looks the event log service could not allocate enough memory
      // to get all entries and the next messages are pointing out to the same location
      rc = NVM_ERR_NOT_ENOUGH_FREE_SPACE;
    }
    FreePool(p_previous_entry);

    p_current_event++;
    events_number--;
  }
  free(event_buffer);
  return rc;
}

#pragma pack(push)
#pragma pack(1)
struct pt_payload_sanitize_dimm_status {
  unsigned char state;
  unsigned char progress;
  unsigned char reserved[126];
};

/*
 * Passthrough Payload:
 *    Opcode:     0x02h (Get Security Info)
 *    Sub-Opcode: 0x01h (Get Sanitize State)
 * Small Output Payload
 */

struct pt_payload_get_sanitize_state {
   /*
    * 0x00 = idle
    * 0x01 = in progress
    * 0x02 = completed
    * 0x03-0xff - Reserved
    */
   unsigned char  sanitize_status;
   /*
    * Percent complete the DIMM has been sanitized so far, 0-100
    */
   unsigned char  sanitize_progress;
};

#pragma pack(pop)

/* Sanitize Status */
enum sanitize_status {
  SAN_IDLE  = 0,
  SAN_INPROGRESS  = 1,
  SAN_COMPLETED = 2
};

NVM_API int nvm_get_jobs(struct job *p_jobs, const NVM_UINT32 count)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int rc = NVM_SUCCESS;
  FW_CMD *cmd;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  struct pt_payload_sanitize_dimm_status *p_sanitize_status;
  int job_index = 0;
  unsigned int i;
  int nvm_status = 0;

  if (NULL == p_jobs)
    return NVM_ERR_INVALID_PARAMETER;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  unsigned int actual_count = 0;
  if (NVM_SUCCESS != (nvm_status = nvm_get_number_of_devices(&actual_count)))
  {
    NVDIMM_ERR("Failed to obtain the number of devices (%d)\n",nvm_status);
    return NVM_ERR_OPERATION_FAILED;
  }

  if(count != actual_count)
  {
    return NVM_ERR_BAD_SIZE;
  }

  if (NULL == (cmd = (FW_CMD *)AllocatePool(sizeof(FW_CMD)))) {
    NVDIMM_ERR("Failed to allocate memory\n");
    return NVM_ERR_NOT_ENOUGH_FREE_SPACE;
  }

  ZeroMem(cmd, sizeof(FW_CMD));
  p_sanitize_status = (struct pt_payload_sanitize_dimm_status *)cmd->OutPayload;
  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(&gNvmDimmDriverNvmDimmConfig, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to get dimm list %d\n", (int)ReturnCode);
    FreePool(cmd);
    return NVM_ERR_OPERATION_FAILED;
  }

  for (i = 0; i < DimmCount; ++i) {
    if (i >= count)
      break;

    cmd->DimmID = pDimms[i].DimmID; //PassThruCommand needs the dimm_id (not handle)
    cmd->Opcode = PtGetSecInfo;
    cmd->SubOpcode = 0x1;
    cmd->OutputPayloadSize = sizeof(struct pt_payload_sanitize_dimm_status);

    if (EFI_SUCCESS == PassThruCommand(cmd, PT_TIMEOUT_INTERVAL)) {
      if (p_sanitize_status->state != SAN_IDLE) {
        if (p_sanitize_status->state == SAN_INPROGRESS)
          p_jobs[i].status = NVM_JOB_STATUS_RUNNING;
        else if (p_sanitize_status->state == SAN_COMPLETED)
          p_jobs[i].status = NVM_JOB_STATUS_COMPLETE;
        else
          p_jobs[i].status = NVM_JOB_STATUS_UNKNOWN;

        p_jobs[i].type = NVM_JOB_TYPE_SANITIZE;
        p_jobs[i].percent_complete = p_sanitize_status->progress;
        memmove(p_jobs[i].uid, pDimms[i].DimmUid, MAX_DIMM_UID_LENGTH);
        memmove(p_jobs[i].affected_element, pDimms[i].DimmUid, MAX_DIMM_UID_LENGTH);
        p_jobs[i].result = NULL;
        job_index++;
      }
    }
  }
  FreePool(cmd);
  return NVM_SUCCESS;
}

NVM_API int nvm_create_context()
{
  return NVM_SUCCESS;
}

NVM_API int nvm_free_context(const NVM_BOOL force)
{
  return NVM_SUCCESS;
}

NVM_API int nvm_get_fw_error_log_entry_cmd(
  const NVM_UID   device_uid,
  const unsigned short  seq_num,
  const unsigned char log_level,
  const unsigned char log_type,
  ERROR_LOG * error_entry)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT16 dimm_id;
  unsigned int max_errors;
  int rc = NVM_SUCCESS;

  if (log_level > 1)
  {
    NVDIMM_ERR("Invalid log level.  Only valid values are 0: Low, and 1: High\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (log_type > 1)
  {
    NVDIMM_ERR("Invalid log type.  Only valid values are 0: Media, and 1: Thermal\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  if (!error_entry)
  {
    NVDIMM_ERR("Invalid error_entry paramter (NULL).\n");
    return NVM_ERR_INVALID_PARAMETER;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    return NVM_ERR_UNKNOWN;
  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    goto Finish;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id((char *)device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    goto Finish;
  }

  max_errors = 1;

  ReturnCode = gNvmDimmDriverNvmDimmConfig.GetErrorLog(
    &gNvmDimmDriverNvmDimmConfig,
    (UINT16 *)&dimm_id,
    1,
    log_type,
    seq_num,
    log_level,
    &max_errors,
    (ERROR_LOG_INFO *)error_entry,
    pCommandStatus);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR_W(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    rc = NVM_ERR_UNKNOWN;
  }

  if (0 == max_errors) {
  
    rc = NVM_SUCCESS_NO_ERROR_LOG_ENTRY;
  }

Finish:
  FreeCommandStatus(&pCommandStatus);
  return rc;
}

NVM_API int nvm_get_config_int(const char *param_name, int default_val)
{
  int val = default_val;
  int size = sizeof(val);
  EFI_GUID g = { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
  int rc = NVM_SUCCESS;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  preferences_get_var_ascii(param_name, g, (void *)&val, (UINTN *)&size);
  return val;
}

NVM_API int nvm_get_fw_err_log_stats(const NVM_UID      device_uid,
             struct device_error_log_status * error_log_stats)
{
  UINT16 dimm_id;
  LOG_INFO_DATA_RETURN get_error_log_output;
  int rc = NVM_SUCCESS;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  if (NVM_SUCCESS != (rc = get_dimm_id((char *)device_uid, &dimm_id, NULL))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
  } else {
    get_fw_err_log_stats(
      dimm_id,
      ErrorLogLowPriority,
      ErrorLogTypeMedia,
      &get_error_log_output);
    error_log_stats->media_low.oldest = get_error_log_output.OldestSequenceNum;
    error_log_stats->media_low.current = get_error_log_output.CurrentSequenceNum;

    get_fw_err_log_stats(
      dimm_id,
      ErrorLogHighPriority,
      ErrorLogTypeMedia,
      &get_error_log_output);
    error_log_stats->media_high.oldest = get_error_log_output.OldestSequenceNum;
    error_log_stats->media_high.current = get_error_log_output.CurrentSequenceNum;

    get_fw_err_log_stats(
      dimm_id,
      ErrorLogLowPriority,
      ErrorLogTypeThermal,
      &get_error_log_output);
    error_log_stats->therm_low.oldest = get_error_log_output.OldestSequenceNum;
    error_log_stats->therm_low.current = get_error_log_output.CurrentSequenceNum;

    get_fw_err_log_stats(
      dimm_id,
      ErrorLogHighPriority,
      ErrorLogTypeThermal,
      &get_error_log_output);
    error_log_stats->therm_high.oldest = get_error_log_output.OldestSequenceNum;
    error_log_stats->therm_high.current = get_error_log_output.CurrentSequenceNum;
  }
  return rc;
}

NVM_API int nvm_get_dimm_id(const NVM_UID device_uid,
          unsigned int *  dimm_id,
          unsigned int *  dimm_handle)
{
  int rc = NVM_SUCCESS;

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }
  rc = get_dimm_id((char *)device_uid, (UINT16 *)dimm_id, dimm_handle);
  return rc;
}

int get_dimm_id(const char *uid, UINT16 *dimm_id, unsigned int *dimm_handle)
{
  EFI_STATUS rc;
  CHAR16 uid_wide[MAX_DIMM_UID_LENGTH];
  int i;

  if (NULL == g_dimms) {
    if (NVM_SUCCESS != nvm_get_number_of_devices(&g_dimm_cnt)) {
      NVDIMM_ERR("Failed to get number of devices\n");
      return NVM_ERR_UNKNOWN;
    }

    g_dimms = (DIMM_INFO *)AllocatePool(sizeof(DIMM_INFO) * g_dimm_cnt);
    if (NULL == g_dimms) {
      NVDIMM_ERR("Failed to allocate memory\n");
      return NVM_ERR_UNKNOWN;
    }

    rc = gNvmDimmDriverNvmDimmConfig.GetDimms(&gNvmDimmDriverNvmDimmConfig, (UINT32)g_dimm_cnt, DIMM_INFO_CATEGORY_NONE, g_dimms);
    if (EFI_ERROR(rc)) {
      FreePool(g_dimms);
      g_dimms = NULL;
      NVDIMM_ERR("GetDimms failed (%d)\n", rc);
      return NVM_ERR_UNKNOWN;
    }
  }

  AsciiStrToUnicodeStr(uid, uid_wide);
  for (i = 0; i < g_dimm_cnt; ++i) {
    if (0 == StrCmp(uid_wide, g_dimms[i].DimmUid)) {
      if (dimm_id)
        *dimm_id = g_dimms[i].DimmID;
      if (dimm_handle)
        *dimm_handle = g_dimms[i].DimmHandle;
      return NVM_SUCCESS;
    }
  }
  return NVM_ERR_UNKNOWN;
}

void dimm_info_to_device_discovery(DIMM_INFO *p_dimm, struct device_discovery *p_device)
{
  p_device->all_properties_populated = FALSE;
  p_device->device_handle.handle = p_dimm->DimmHandle;
  p_device->physical_id = p_dimm->DimmID;
  p_device->vendor_id = p_dimm->VendorId;
  p_device->device_id = p_dimm->DeviceId;
  p_device->revision_id = p_dimm->Rid;
  p_device->channel_pos = p_dimm->ChannelPos;
  p_device->channel_id = p_dimm->ChannelId;
  p_device->memory_controller_id = p_dimm->ImcId;
  p_device->socket_id = p_dimm->SocketId;
  p_device->node_controller_id = p_dimm->NodeControllerID;
  p_device->memory_type = p_dimm->MemoryType;
  p_device->dimm_sku = p_dimm->SkuInformation;
  CopyMem_S(p_device->manufacturer, sizeof(p_device->manufacturer), &(p_dimm->ManufacturerId), sizeof(UINT16));
  CopyMem_S(p_device->serial_number, sizeof(p_device->serial_number), &(p_dimm->SerialNumber), sizeof(UINT32));
  p_device->subsystem_vendor_id = p_dimm->SubsystemVendorId;
  p_device->subsystem_device_id = p_dimm->SubsystemDeviceId;
  p_device->subsystem_revision_id = p_dimm->SubsystemRid;
  p_device->manufacturing_info_valid = p_dimm->ManufacturingInfoValid;
  p_device->manufacturing_location = p_dimm->ManufacturingLocation;
  p_device->manufacturing_date = p_dimm->ManufacturingDate;
  CopyMem_S(p_device->part_number, sizeof(p_device->part_number), p_dimm->PartNumber, PART_NUMBER_LEN);
  build_revision(p_device->fw_revision, NVM_VERSION_LEN, p_dimm->FwVer.FwProduct, p_dimm->FwVer.FwRevision,
		 p_dimm->FwVer.FwSecurityVersion, p_dimm->FwVer.FwBuild);
  snprintf(p_device->fw_api_version, NVM_VERSION_LEN, "%02d.%02d", p_dimm->FwVer.FwApiMajor, p_dimm->FwVer.FwApiMinor);
  p_device->capacity = p_dimm->Capacity;
  CopyMem_S(p_device->interface_format_codes, sizeof(p_device->interface_format_codes), p_dimm->InterfaceFormatCode, sizeof(UINT16) * 2);
  UnicodeStrToAsciiStr(p_dimm->DimmUid, p_device->uid);
  p_device->lock_state = p_dimm->SecurityState;
  p_device->manageability = p_dimm->ManageabilityState;
}

int get_fw_err_log_stats(
  const unsigned int	dimm_id,
  const unsigned char	log_level,
  const unsigned char	log_type,
  LOG_INFO_DATA_RETURN *	log_info)
{
  int rc = NVM_ERR_UNKNOWN;
  FW_CMD *cmd;
  PT_INPUT_PAYLOAD_GET_ERROR_LOG get_error_log_input;

  if (NULL == (cmd = (FW_CMD *)AllocatePool(sizeof(FW_CMD)))) {
    NVDIMM_ERR("Failed to allocate memory\n");
    goto finish;
  }
  ZeroMem(cmd, sizeof(FW_CMD));
  ZeroMem(&get_error_log_input, sizeof(get_error_log_input));
  get_error_log_input.SequenceNumber = 0;
  get_error_log_input.LogParameters.Separated.LogInfo = 1;
  get_error_log_input.LogParameters.Separated.LogLevel = log_level;
  get_error_log_input.LogParameters.Separated.LogType = log_type;
  get_error_log_input.LogParameters.Separated.LogEntriesPayloadReturn = 0;

  cmd->DimmID = dimm_id;
  cmd->Opcode = PtGetLog;
  cmd->SubOpcode = SubopErrorLog;
  cmd->InputPayloadSize = sizeof(PT_INPUT_PAYLOAD_GET_ERROR_LOG);
  CopyMem_S(cmd->InputPayload, sizeof(cmd->InputPayload), &get_error_log_input, cmd->InputPayloadSize);
  cmd->OutputPayloadSize = sizeof(LOG_INFO_DATA_RETURN);
  if (EFI_SUCCESS == PassThruCommand(cmd, PT_TIMEOUT_INTERVAL)) {
    memcpy_s(log_info, sizeof(LOG_INFO_DATA_RETURN), cmd->OutPayload, cmd->OutputPayloadSize);
    rc = NVM_SUCCESS;
  }
finish:
  if (cmd)
    FreePool(cmd);
  return rc;
}

NVM_API int nvm_send_device_passthrough_cmd(const NVM_UID   device_uid,
              struct device_pt_cmd *  p_cmd)
{
  FW_CMD *cmd = NULL;
  UINT16 dimm_id;
  unsigned int dimm_handle;
  int rc = NVM_ERR_UNKNOWN;

  if (p_cmd->input_payload_size > MAX_IN_PAYLOAD_SIZE ||
    p_cmd->large_input_payload_size > MAX_IN_MB_SIZE)
  {
    NVDIMM_ERR("Invalid input payload size(s)\n");
    rc = NVM_ERR_INVALID_PARAMETER;
    goto finish;

  }

  if (p_cmd->output_payload_size > MAX_OUT_PAYLOAD_SIZE ||
    p_cmd->large_output_payload_size > MAX_OUT_MB_SIZE)
  {
    NVDIMM_ERR("Invalid output payload size(s)\n");
    rc = NVM_ERR_INVALID_PARAMETER;
    goto finish;

  }

  if (NVM_SUCCESS != (rc = nvm_init())) {
    NVDIMM_ERR("Failed to intialize nvm library %d\n", rc);
    return rc;
  }

  if (NULL == (cmd = (FW_CMD *)AllocatePool(sizeof(FW_CMD)))) {
    NVDIMM_ERR("Failed to allocate memory\n");
    goto finish;
  }

  ZeroMem(cmd, sizeof(FW_CMD));

  if (NVM_SUCCESS != (rc = get_dimm_id((char *)device_uid, &dimm_id, &dimm_handle))) {
    NVDIMM_ERR("Failed to get dimm ID %d\n", rc);
    goto finish;
  }

  cmd->DimmID = dimm_id; //PassThruCommand needs the dimm_id (not handle)
  cmd->Opcode = p_cmd->opcode;
  cmd->SubOpcode = p_cmd->sub_opcode;
  cmd->InputPayloadSize = p_cmd->input_payload_size;
  CopyMem_S(cmd->InputPayload, sizeof(cmd->InputPayload), p_cmd->input_payload, cmd->InputPayloadSize);
  cmd->OutputPayloadSize = p_cmd->output_payload_size;
  cmd->LargeInputPayloadSize = p_cmd->large_input_payload_size;
  cmd->LargeOutputPayloadSize = p_cmd->large_output_payload_size;
  CopyMem_S(cmd->LargeInputPayload, sizeof(cmd->LargeInputPayload), p_cmd->large_input_payload, cmd->LargeInputPayloadSize);

  if (EFI_SUCCESS != PassThruCommand(cmd, PT_TIMEOUT_INTERVAL))
  {
    NVDIMM_ERR("Passthru command failed\n");
    goto finish;
  }
  else
  {
    NVDIMM_DBG("Passthru command executed successfully\n");
    rc = NVM_SUCCESS;
  }

  if (cmd->LargeOutputPayloadSize)
  {
    if(p_cmd->large_output_payload_size < cmd->LargeOutputPayloadSize)
    {
      p_cmd->large_output_payload_size = 0; //indicate to caller that nothing was copied into their large output payload buffer
      NVDIMM_ERR("Not enough memory to copy the large output payload\n", rc);
      rc = NVM_ERR_INVALID_PARAMETER;
      goto finish;
    }
    CopyMem_S(p_cmd->large_output_payload, p_cmd->large_output_payload_size, cmd->LargeOutputPayload, cmd->LargeOutputPayloadSize);
    p_cmd->large_output_payload_size = cmd->LargeOutputPayloadSize;
  }
  else if (cmd->OutputPayloadSize)
  {
    if(p_cmd->output_payload_size < cmd->OutputPayloadSize)
    {
      p_cmd->output_payload_size = 0; //indicate to caller that nothing was copied into their output payload buffer
      NVDIMM_ERR("Not enough memory to copy the output payload\n", rc);
      rc = NVM_ERR_INVALID_PARAMETER;
      goto finish;
    }
    CopyMem_S(p_cmd->output_payload, p_cmd->output_payload_size, cmd->OutPayload, cmd->OutputPayloadSize);
    p_cmd->output_payload_size = cmd->OutputPayloadSize;
  }
finish:
  FREE_POOL_SAFE(cmd);
  return rc;
}

NVM_API int nvm_acpi_event_create_ctx(unsigned int dimm_handle, void ** ctx)
{
  return acpi_event_create_ctx(dimm_handle, ctx);
}

NVM_API int nvm_acpi_event_free_ctx(void * ctx)
{
  return acpi_event_free_ctx(ctx);
}

NVM_API int nvm_acpi_event_ctx_get_dimm_handle(void * ctx, unsigned int  * dev_handle)
{
  return acpi_event_ctx_get_dimm_handle(ctx, dev_handle);
}

NVM_API int nvm_acpi_event_get_event_state(void * ctx, enum acpi_event_type event_type, enum acpi_event_state *event_state)
{
  return acpi_event_get_event_state(ctx, event_type, event_state);
}

NVM_API int nvm_acpi_event_set_monitor_mask(void * ctx, const unsigned int acpi_monitored_event_mask)
{
  return acpi_event_set_monitor_mask(ctx, acpi_monitored_event_mask);
}

NVM_API int nvm_acpi_event_get_monitor_mask(void * ctx, unsigned int * mask)
{
  return acpi_event_get_monitor_mask(ctx, mask);
}

NVM_API int nvm_acpi_wait_for_event(void * acpi_event_contexts[], const NVM_UINT32 dimm_cnt, const int timeout_sec, enum acpi_get_event_result * event_result)
{
  return acpi_wait_for_event(acpi_event_contexts, dimm_cnt, timeout_sec, event_result);
}
