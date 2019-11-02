/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <nvm_management.h>
#include <ctype.h>

#define PRINT_HOST_INFO              (1<<0)
#define PRINT_DIMM_INFO              (1<<1)
#define PRINT_DIMM_HEALTH            (1<<2)
#define PRINT_MEMORY_RESOURCES       (1<<3)
#define PRINT_DIMM_SENSORS           (1<<4)
#define PRINT_ALL                    (PRINT_HOST_INFO|PRINT_DIMM_INFO|PRINT_DIMM_HEALTH|PRINT_MEMORY_RESOURCES|PRINT_DIMM_SENSORS)
#define MAX_CMD_ARGUMENTS            20
#define MAX_CMD_ARG_LENGTH           20
 /**
   Print Host Information
 **/
void print_host_info()
{
  int nvm_return = 0;
  struct host host_info;

  if (NVM_SUCCESS != (nvm_return = nvm_get_host(&host_info)))
  {
    printf("nvm_get_host failed: %d.\n", nvm_return);
    return;
  }
  printf("--Host Information--\n\n");
  printf("name: %s\nos_name: %s\nos_version: %s\nos_type: %d\nmixed_sku: %s\nsku violation: %s\n",
    host_info.name,
    host_info.os_name,
    host_info.os_version,
    host_info.os_type,
    (host_info.mixed_sku ? "true" : "false"),
    (host_info.sku_violation ? "true" : "false"));
}
/**
  Get device discovery structure and dimm count

  @param[out] p_dev_count The dimm count
  @param[out] p_devices device discovery structure for each dimm

  @retval -1 Failure
  @retval 0  Success
**/
int get_devices(unsigned int *p_dev_count, struct device_discovery** p_devices)
{
  int nvm_return = 0;
  if (NVM_SUCCESS != (nvm_return = nvm_get_number_of_devices(p_dev_count)))
  {
    printf("nvm_get_number_of_devices failed: %d.\n", nvm_return);
    return -1;
  }

  if (NULL == (*p_devices = malloc(sizeof(struct device_discovery) * *p_dev_count)))
  {
    printf("failed to allocate device discovery structure.\n");
    return -1;
  }

  if (NVM_SUCCESS != (nvm_return = nvm_get_devices(*p_devices, *p_dev_count)))
  {
    printf("nvm_get_number_of_devices failed: %d.\n", nvm_return);
    free(*p_devices);
    *p_devices = NULL;
    return -1;
  }

  return 0;
}
/**
  Print Dimm information

  Prints Dimm information from device discovery structure
**/
void print_dimm_info(void)
{
  struct device_discovery* p_devices = NULL;
  unsigned int dev_count = 0;

  if (0 != get_devices(&dev_count, &p_devices))
  {
    return;
  }
  printf("--Dimm Information--\n\n");
  printf("Found %d DCPMM devices.\n", dev_count);

  for (unsigned int i = 0; i < dev_count; i++)
  {
    printf("\n---DCPMM #%d---\n", i);

    printf("Device physical_id: %4.4X\n"
      "Vendor ID: %4.4X\n"
      "Device ID: %4.4X\n"
      "Revision ID: %4.4X\n"
      "Channel Postion: %4.4X\n"
      "Channel ID: %4.4X\n"
      "MemoryController ID: %4.4X\n"
      "Socket ID: %4.4X\n"
      "NodeController ID: %4.4X\n"
      "MemoryType: %u\n"
      "Dimm SKU: %u\n"
      "Manufacturer: %4.4X\n"
      "Serial Number: %u\n"
      "Subsystem Vendor ID: %4.4X\n"
      "Subsystem Device ID: %4.4X\n"
      "Subsystem Revision ID: %4.4X\n"
      "Manufacturing Info Valid: %d\n"
      "Manufacturing Location: %4.4X\n"
      "Manufacturing Date: %4.4X\n"
      "Part Number: %s\n"
      "Firmware Revision: %s\n"
      "Firmware Version: %s\n"
      "Device Capacity: %lld\n"
      "Device Handle: %4.4X\n"
      "Lock State: %d\n"
      "Manageablility: %d\n"
      "Master Passphrase Enabled: %d\n"
      ,
      p_devices[i].physical_id,
      p_devices[i].vendor_id,
      p_devices[i].device_id,
      p_devices[i].revision_id,
      p_devices[i].channel_pos,
      p_devices[i].channel_id,
      p_devices[i].memory_controller_id,
      p_devices[i].socket_id,
      p_devices[i].node_controller_id,
      p_devices[i].memory_type,
      p_devices[i].dimm_sku,
      (*(NVM_UINT16 *)&p_devices[i].manufacturer),
      (*(NVM_UINT32 *)&p_devices[i].serial_number),
      p_devices[i].subsystem_vendor_id,
      p_devices[i].subsystem_device_id,
      p_devices[i].subsystem_revision_id,
      p_devices[i].manufacturing_info_valid,
      p_devices[i].manufacturing_location,
      p_devices[i].manufacturing_date,
      p_devices[i].part_number,
      p_devices[i].fw_revision,
      p_devices[i].fw_api_version,
      p_devices[i].capacity,
      p_devices[i].device_handle.handle,
      p_devices[i].lock_state,
      p_devices[i].manageability,
      p_devices[i].master_passphrase_enabled
      );

  }

  free(p_devices);
}
/**
  Get device status structure and dimm count

  @param[out] p_dev_count The dimm count
  @param[out] p_device_status device status structure for each dimm

  @retval -1 Failure
  @retval 0  Success
**/
int get_devices_status(unsigned int *p_dev_count, struct device_status** p_device_status)
{
  int nvm_return = 0;
  struct device_discovery* p_devices = NULL;
  unsigned int Index = 0;
  nvm_return = get_devices(p_dev_count, &p_devices);

  if (nvm_return != 0)
  {
    printf("get_devices failed: %d.\n", nvm_return);
    *p_device_status = NULL;
    return -1;
  }

  if (NULL == (*p_device_status = malloc(sizeof(struct device_status) * *p_dev_count)))
  {
    printf("failed to allocate device status.\n");
    free(p_devices);
    *p_device_status = NULL;
    return -1;
  }

  for (Index = 0; Index < *p_dev_count; Index++)
  {
    if (NVM_SUCCESS != (nvm_return = nvm_get_device_status(p_devices[Index].uid, (*p_device_status)+Index))) {
      printf("failed to get device status.\n");
      free(*p_device_status);
      free(p_devices);
      *p_device_status = NULL;
      return -1;
    }
  }
  free(p_devices);
  return 0;
}
/**
  Print dimm health status

  Prints dimm health status from device_status structure
**/
void print_dimm_health(void)
{
  struct device_status* p_devicestatus = NULL;
  unsigned int dev_count = 0;
  if (0 != get_devices_status(&dev_count, &p_devicestatus))
  {
    return;
  }
  printf("--Dimm Health Information--\n\n");
  printf("Found %d DCPMM devices.\n", dev_count);

  for (unsigned int i = 0; i < dev_count; i++)
  {
    printf("\n---DCPMM #%d---\n", i);
    switch (p_devicestatus[i].health) {
    case HEALTH_STATUS_HEALTHY:
      printf("HealthStatus: Healthy\n"); break;
    case HEALTH_STATUS_NON_CRITICAL_FAILURE:
      printf("HealthStatus: Non Critical Failure\n"); break;
    case HEALTH_STATUS_CRITICAL_FAILURE:
      printf("HealthStatus: Critical Failure\n"); break;
    case HEALTH_STATUS_FATAL_FAILURE:
      printf("HealthStatus: Fatal Failure\n"); break;
    case HEALTH_STATUS_UNMANAGEABLE:
      printf("HealthStatus: Unmanageable\n"); break;
    case HEALTH_STATUS_NON_FUNCTIONAL:
      printf("HealthStatus: Non Functional\n"); break;
    case HEALTH_STATUS_UNKNOWN:
    default:
      printf("HealthStatus: Unknown\n"); break;
    }
  }
  free(p_devicestatus);
}
/**
  Print memory resources

  print memory resources from device capacities structure
**/
void print_memory_resources(void)
{
  struct device_capacities capacities;
  memset(&capacities, 0, sizeof(struct device_capacities));
  int nvm_return = 0;
  if (NVM_SUCCESS != (nvm_return = nvm_get_nvm_capacities(&capacities)))
  {
    printf("failed to get memory resources.\n");
  }
  printf("--Memory Resources Information--\n\n");
  printf("Found DCPMM device capacities.\n");
  printf("Capacity: %llu\n"
    "App Direct Capacity: %llu\n"
    "Unconfigured Capacity: %llu\n"
    "Reserved Capacity: %llu\n"
    "Memory Capacity: %llu\n"
    "Inaccessible Capacity: %llu\n",
    capacities.capacity,
    capacities.app_direct_capacity,
    capacities.unconfigured_capacity,
    capacities.reserved_capacity,
    capacities.memory_capacity,
    capacities.inaccessible_capacity
  );

}
/**
  Free sensor array

  @param[in] dev_count The dimm count
  @param[out] p_sensors address of pointer to array of sensor structures

  **/
void free_sensor_array(struct sensor*** p_sensors, unsigned int dev_count)
{
  unsigned int dev_index = 0;
  if (*p_sensors != NULL)
  {
    for (dev_index = 0; dev_index < dev_count; dev_index++)
    {
      if ((*p_sensors)[dev_index] != NULL)
      {
        free((*p_sensors)[dev_index]);
        (*p_sensors)[dev_index] = NULL;
      }
    }
    free(*p_sensors);
  }
}
/**
  Get device sensors and dimm count

  @param[out] p_dev_count The dimm count
  @param[out] p_sensors address of pointer to array of sensor structures

  @retval -1 Failure
  @retval 0  Success
**/
int get_devices_sensors(unsigned int *p_dev_count, struct sensor*** p_sensors)
{
  int nvm_return = 0;
  struct device_discovery* p_devices = NULL;
  unsigned int Index = 0;
  nvm_return = get_devices(p_dev_count, &p_devices);

  if (nvm_return != 0)
  {
    printf("get_devices failed: %d.\n", nvm_return);
    *p_sensors = NULL;
    return -1;
  }

  if (NULL == ((*p_sensors) = malloc(sizeof(struct sensor *)* *p_dev_count)))
  {
    printf("failed to allocate sensor array\n");
    *p_sensors = NULL;
    free(p_devices);
    return -1;
  }
  for (Index = 0; Index < *p_dev_count; Index++)
  {
    if (NULL == ((*p_sensors)[Index] = malloc(sizeof(struct sensor) * SENSOR_COUNT)))
    {
      printf("failed to allocate sensor structure.\n");
      free_sensor_array(p_sensors, *p_dev_count);
      free(p_devices);
      *p_sensors = NULL;
      return -1;
    }
  }
  for (Index = 0; Index < *p_dev_count; Index++)
  {
    if (NVM_SUCCESS != (nvm_return = nvm_get_sensors(p_devices[Index].uid, ((*p_sensors)[Index]), SENSOR_COUNT))) {
      printf("failed to get sensor information.\n");
      free_sensor_array(p_sensors, *p_dev_count);
      free(p_devices);
      *p_sensors = NULL;
      return -1;
    }
  }
  free(p_devices);
  return 0;
}
/**
  Print dimm sensors information

  Print dimm sensors information from sensors structure
**/
void print_dimm_sensors(void)
{
  struct sensor ** p_sensors  = NULL;
  unsigned int dev_count = 0;
  unsigned int dev_index = 0;
  unsigned int sensor_index = 0;
  if (0 != get_devices_sensors(&dev_count, &p_sensors))
  {
    return;
  }
  printf("--Dimm Sensors Information--\n\n");
  printf("Found %d DCPMM devices.\n", dev_count);

  for (dev_index = 0; dev_index < dev_count; dev_index++)
  {
    printf("\n---DCPMM #%d---\n", dev_index);
    for (sensor_index = 0; sensor_index < SENSOR_COUNT; sensor_index++)
    {
      printf("\nSensor Index #%d\n", sensor_index);
      switch (p_sensors[dev_index][sensor_index].type)
      {
      case SENSOR_HEALTH:
        printf("Sensor Type : Health\n"); break;
      case SENSOR_MEDIA_TEMPERATURE:
        printf("Sensor Type : Media Temperature\n"); break;
      case SENSOR_CONTROLLER_TEMPERATURE:
        printf("Sensor Type : Controller Temperature\n"); break;
      case SENSOR_PERCENTAGE_REMAINING:
        printf("Sensor Type : Percentage Remaining\n"); break;
      case SENSOR_LATCHED_DIRTY_SHUTDOWN_COUNT:
        printf("Sensor Type : Latched Dirty Shutdown Count\n"); break;
      case SENSOR_POWERONTIME:
        printf("Sensor Type : Power On Time\n"); break;
      case SENSOR_UPTIME:
        printf("Sensor Type : Uptime\n"); break;
      case SENSOR_POWERCYCLES:
        printf("Sensor Type : Power Cycles\n"); break;
      case SENSOR_FWERRORLOGCOUNT:
        printf("Sensor Type : Fw Error Log Count\n"); break;
      case SENSOR_UNLATCHED_DIRTY_SHUTDOWN_COUNT:
        printf("Sensor Type : Unlatched Dirty Shutdown Count\n"); break;
      default:
        printf("Sensor Type : Unknown\n"); break;
      }
      switch (p_sensors[dev_index][sensor_index].units)
      {
      case UNIT_COUNT:
        printf("Sensor Units : Count\n"); break;
      case UNIT_CELSIUS:
        printf("Sensor Units : Celsius\n"); break;
      case UNIT_SECONDS:
        printf("Sensor Units : Seconds\n"); break;
      case UNIT_MINUTES:
        printf("Sensor Units : Minutes\n"); break;
      case UNIT_HOURS:
        printf("Sensor Units : Hours\n"); break;
      case UNIT_CYCLES:
        printf("Sensor Units : Cycles\n"); break;
      case UNIT_PERCENT:
        printf("Sensor Units : Percent\n"); break;
      default:
        printf("Sensor Units : Unknown\n"); break;
      }
      switch (p_sensors[dev_index][sensor_index].current_state)
      {
      case SENSOR_NOT_INITIALIZED:
        printf("Sensor State : Not Initialized\n"); break;
      case SENSOR_NORMAL:
        printf("Sensor State : Normal\n"); break;
      case SENSOR_NONCRITICAL:
        printf("Sensor State : Non Critical\n"); break;
      case SENSOR_CRITICAL:
        printf("Sensor State : Critical\n"); break;
      case SENSOR_FATAL:
        printf("Sensor State : Fatal\n"); break;
      case SENSOR_UNKNOWN:
        printf("Sensor State : Unknown\n"); break;
      default:
        printf("Sensor State : Unknown\n"); break;
      }
      printf("Sensor Reading : %llu\n", p_sensors[dev_index][sensor_index].reading);
    }
  }
  free_sensor_array(&p_sensors, dev_count);
}
/**
  Parse command line arguments

  @param[in] p_dev_count The dimm count
  @param[in] p_sensors address of pointer to array of sensor structures
  @param[out] p_help_flag help flag
  @param[out] p_print_flag print flag
**/
void parse_arguments(int argc, char* argv[], int * p_help_flag, unsigned int * p_print_flag)
{
  int Index = 0;
  int printflagset = 0;
  int stringIndex = 0;
  char * token = NULL;
  char * input = NULL;
#ifdef _MSC_VER
  char *next = NULL;
#endif
  *p_help_flag = 0;
  *p_print_flag = 0;
  if (argc < 2)
  {
    *p_help_flag = 1;
    return;
  }
  for (Index = 1; Index < argc; Index++)
  {
    input = malloc(sizeof(char)* MAX_CMD_ARG_LENGTH);
    if (input == NULL)
    {
      *p_help_flag = 1;
      return;
    }

    //convert argv[Index] to lowercase
    for (stringIndex = 0; argv[Index][stringIndex]!='\0' && stringIndex<MAX_CMD_ARG_LENGTH-1; stringIndex++) {
      input[stringIndex] = tolower(argv[Index][stringIndex]);
    }
    input[stringIndex] = '\0';

    //check for help flag
    if (strncmp(input, "-h", 2) == 0)
    {
      *p_help_flag = 1;
      free(input);
      continue;
    }
    if (strncmp(input, "--h", 3) == 0)
    {
      *p_help_flag = 1;
      free(input);
      continue;
    }
    if (strncmp(input, "-help", 5) == 0)
    {
      *p_help_flag = 1;
      free(input);
      continue;
    }
    if (strncmp(input, "--help", 6) == 0)
    {
      *p_help_flag = 1;
      free(input);
      continue;
    }
    if (strncmp(input, "help", 4) == 0)
    {
      *p_help_flag = 1;
      free(input);
      continue;
    }
    //check for print flag
    if (strncmp(input, "-p", 2) == 0)
    {
      printflagset = 1;
      free(input);
      continue;
    }
    if (strncmp(input, "--p", 3) == 0)
    {
      printflagset = 1;
      free(input);
      continue;
    }
    if (strncmp(input, "-print", 6) == 0)
    {
      printflagset = 1;
      free(input);
      continue;
    }
    if (strncmp(input, "--print", 7) == 0)
    {
      printflagset = 1;
      free(input);
      continue;
    }
    if (strncmp(input, "print", 5) == 0)
    {
      printflagset = 1;
      free(input);
      continue;
    }
    //check for print options
#ifdef _MSC_VER
    token = strtok_s(input, ",",&next);
#else
    token = strtok(input, ",");
#endif
    while (token) {
      while (token && *token == ' ') {
        token++;
      }
      if (token) {
        if (printflagset == 1 && strncmp(token, "host", 4) == 0)
        {
          *p_print_flag |= PRINT_HOST_INFO;
        }
        if (printflagset == 1 && strncmp(token, "dimm", 4) == 0)
        {
          *p_print_flag |= PRINT_DIMM_INFO;
        }
        if (printflagset == 1 && strncmp(token, "health", 6) == 0)
        {
          *p_print_flag |= PRINT_DIMM_HEALTH;
        }
        if (printflagset == 1 && strncmp(token, "memory", 6) == 0)
        {
          *p_print_flag |= PRINT_MEMORY_RESOURCES;
        }
        if (printflagset == 1 && strncmp(token, "sensors", 7) == 0)
        {
          *p_print_flag |= PRINT_DIMM_SENSORS;
        }
        if (printflagset == 1 && strncmp(token, "all", 3) == 0)
        {
          *p_print_flag |= PRINT_ALL;
        }
      }
#ifdef _MSC_VER
      token = strtok_s(NULL, ",", &next);
#else
      token = strtok(NULL, ",");
#endif
    }
    free(input);
  }
  if (*p_help_flag == 0 && *p_print_flag == 0)
  {
    *p_help_flag = 1;
  }
  if (printflagset == 1 && *p_print_flag == 0)
  {
    *p_help_flag = 1;
  }
}
/**
  Print help text

  Prints relevant help text
**/
void print_help(void)
{
  printf("nvm_api_sample: Sample application to illustrate usage of nvm_api\n");
  printf("\t Usage: nvm_api_sample <verb> <options>\n");
  printf("Verbs:\n");
  printf("\t Display help text.\n");
  printf("\t help\n");
  printf("\t Print options information.\n");
  printf("\t print\n");
  printf("Options:\n");
  printf("\t Display Host information.\n");
  printf("\t host\n");
  printf("\t Display Dimm information.\n");
  printf("\t dimm\n");
  printf("\t Display Dimm Health information.\n");
  printf("\t health\n");
  printf("\t Display Memory Resources information.\n");
  printf("\t memory\n");
  printf("\t Display Dimm Sensors information.\n");
  printf("\t sensors\n");
  printf("\t Display All information.\n");
  printf("\t all\n");
}
/**
  Main

  Main function
**/
int main(int argc, char* argv[])
{
  int helpFlag = 0;
  unsigned int printFlag = 0;
  int Index = 0;

  //Sanitize argc and argv
  if (argc > MAX_CMD_ARGUMENTS) {
    print_help();
    return 0;
  }
  for (Index = 0; Index < argc; Index++) {
    if (strlen(argv[Index]) > MAX_CMD_ARG_LENGTH) {
      print_help();
      return 0;
    }
  }
    
  parse_arguments(argc, argv, &helpFlag, &printFlag);

  if (helpFlag == 1) {
    print_help();
  }
  else {
    nvm_init();
    if (printFlag & PRINT_HOST_INFO) {
      print_host_info();
    }
    if (printFlag & PRINT_DIMM_INFO) {
      if (printFlag > PRINT_DIMM_INFO) {
        printf("\n\n");
      }
      print_dimm_info();
    }
    if (printFlag & PRINT_DIMM_HEALTH) {
      if (printFlag > PRINT_DIMM_HEALTH) {
        printf("\n\n");
      }
      print_dimm_health();
    }
    if (printFlag & PRINT_MEMORY_RESOURCES) {
      if (printFlag > PRINT_MEMORY_RESOURCES) {
        printf("\n\n");
      }
      print_memory_resources();
    }
    if (printFlag & PRINT_DIMM_SENSORS) {
      if (printFlag > PRINT_DIMM_SENSORS) {
        printf("\n\n");
      }
      print_dimm_sensors();
    }
    nvm_uninit();
  }
  return 0;
}