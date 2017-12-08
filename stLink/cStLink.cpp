//{{{  includes
#include <stdafx.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cStLink.h"
#include "libusb.h"

//}}}
//{{{  stlink defines
#define USB_ST_VID          0x0483
#define USB_STLINK_32L_PID  0x3748
#define USB_STLINK_V21_PID  0x374B

#define STLINK_V2_RX_EP       (1 | LIBUSB_ENDPOINT_IN)
#define STLINK_V2_0_TX_EP     (2 | LIBUSB_ENDPOINT_OUT)
#define STLINK_V2_0_TRACE_EP  (3 | LIBUSB_ENDPOINT_IN)
#define STLINK_V2_1_TX_EP     (1 | LIBUSB_ENDPOINT_OUT)
#define STLINK_V2_1_TRACE_EP  (2 | LIBUSB_ENDPOINT_IN)

//{{{  STLINK_DEBUG_RESETSYS, etc:
#define STLINK_OK             0x80
#define STLINK_FALSE          0x81

#define STLINK_CORE_RUNNING   0x80
#define STLINK_CORE_HALTED    0x81
#define STLINK_CORE_STAT_UNKNOWN  -1
//}}}

#define STLINK_SWD_ENTER            0x30
#define STLINK_SWD_READCOREID       0x32

#define STLINK_GET_VERSION          0xf1

#define STLINK_DEBUG_COMMAND        0xF2
  #define STLINK_DEBUG_ENTER_JTAG     0x00
  #define STLINK_DEBUG_GETSTATUS      0x01
  #define STLINK_DEBUG_FORCEDEBUG     0x02
  #define STLINK_DEBUG_RESETSYS       0x03
  #define STLINK_DEBUG_READALLREGS    0x04
  #define STLINK_DEBUG_READREG        0x05
  #define STLINK_DEBUG_WRITEREG       0x06
  #define STLINK_DEBUG_READMEM_32BIT  0x07
  #define STLINK_DEBUG_WRITEMEM_32BIT 0x08
  #define STLINK_DEBUG_RUNCORE        0x09
  #define STLINK_DEBUG_STEPCORE       0x0a
  #define STLINK_DEBUG_SETFP          0x0b
  #define STLINK_DEBUG_WRITEMEM_8BIT  0x0d
  #define STLINK_DEBUG_CLEARFP        0x0e
  #define STLINK_DEBUG_WRITEDEBUGREG  0x0f

  #define STLINK_DEBUG_ENTER          0x20
    #define STLINK_DEBUG_ENTER_SWD      0xa3
  #define STLINK_DEBUG_EXIT           0x21
  #define STLINK_DEBUG_READCOREID     0x22

  #define STLINK_DEBUG_APIV2_ENTER           0x30
  #define STLINK_DEBUG_APIV2_READ_IDCODES    0x31
  #define STLINK_DEBUG_APIV2_RESETSYS        0x32
  #define STLINK_DEBUG_APIV2_READREG         0x33
  #define STLINK_DEBUG_APIV2_WRITEREG        0x34
  #define STLINK_DEBUG_APIV2_WRITEDEBUGREG   0x35
  #define STLINK_DEBUG_APIV2_READDEBUGREG    0x36

  #define STLINK_DEBUG_APIV2_READALLREGS     0x3A
  #define STLINK_DEBUG_APIV2_GETLASTRWSTATUS 0x3B
  #define STLINK_DEBUG_APIV2_DRIVE_NRST      0x3C
    #define STLINK_DEBUG_APIV2_DRIVE_NRST_LOW   0x00
    #define STLINK_DEBUG_APIV2_DRIVE_NRST_HIGH  0x01
    #define STLINK_DEBUG_APIV2_DRIVE_NRST_PULSE 0x02

  #define STLINK_DEBUG_APIV2_START_TRACE_RX  0x40
  #define STLINK_DEBUG_APIV2_STOP_TRACE_RX   0x41
  #define STLINK_DEBUG_APIV2_GET_TRACE_NB    0x42
  #define STLINK_DEBUG_APIV2_SWD_SET_FREQ    0x43
    #define STLINK_TRACE_SIZE                  1024
    #define STLINK_TRACE_MAX_HZ                2000000

#define STLINK_DFU_COMMAND           0xF3
  #define STLINK_DFU_EXIT              0x07

#define STLINK_GET_CURRENT_MODE      0xF5
#define STLINK_GET_TARGET_VOLTAGE    0xF7

//{{{  stm32f FPEC flash controller interface, pm0063 manual
// TODO - all of this needs to be abstracted out....
// STM32F05x is identical, based on RM0091 (DM00031936, Doc ID 018940 Rev 2, August 2012)
#define FLASH_REGS_ADDR 0x40022000
#define FLASH_REGS_SIZE 0x28

#define FLASH_ACR (FLASH_REGS_ADDR + 0x00)
#define FLASH_KEYR (FLASH_REGS_ADDR + 0x04)
#define FLASH_SR (FLASH_REGS_ADDR + 0x0c)
#define FLASH_CR (FLASH_REGS_ADDR + 0x10)
#define FLASH_AR (FLASH_REGS_ADDR + 0x14)
#define FLASH_OBR (FLASH_REGS_ADDR + 0x1c)
#define FLASH_WRPR (FLASH_REGS_ADDR + 0x20)
//}}}
//{{{  For STM32F05x, the RDPTR_KEY may be wrong, but as it is not used anywhere...
#define FLASH_RDPTR_KEY 0x00a5
#define FLASH_KEY1 0x45670123
#define FLASH_KEY2 0xcdef89ab

#define FLASH_SR_BSY 0
#define FLASH_SR_EOP 5

#define FLASH_CR_PG 0
#define FLASH_CR_PER 1
#define FLASH_CR_MER 2
#define FLASH_CR_STRT 6
#define FLASH_CR_LOCK 7
//}}}
//{{{  32L = 32F1 same CoreID as 32F4!
#define STM32L_FLASH_REGS_ADDR ((uint32_t)0x40023c00)
#define STM32L_FLASH_ACR (STM32L_FLASH_REGS_ADDR + 0x00)
#define STM32L_FLASH_PECR (STM32L_FLASH_REGS_ADDR + 0x04)
#define STM32L_FLASH_PDKEYR (STM32L_FLASH_REGS_ADDR + 0x08)
#define STM32L_FLASH_PEKEYR (STM32L_FLASH_REGS_ADDR + 0x0c)
#define STM32L_FLASH_PRGKEYR (STM32L_FLASH_REGS_ADDR + 0x10)
#define STM32L_FLASH_OPTKEYR (STM32L_FLASH_REGS_ADDR + 0x14)
#define STM32L_FLASH_SR (STM32L_FLASH_REGS_ADDR + 0x18)
#define STM32L_FLASH_OBR (STM32L_FLASH_REGS_ADDR + 0x1c)
#define STM32L_FLASH_WRPR (STM32L_FLASH_REGS_ADDR + 0x20)
#define FLASH_L1_FPRG 10
#define FLASH_L1_PROG 3
//}}}
//{{{  STM32F4
#define FLASH_F4_REGS_ADDR ((uint32_t)0x40023c00)
#define FLASH_F4_KEYR (FLASH_F4_REGS_ADDR + 0x04)
#define FLASH_F4_OPT_KEYR (FLASH_F4_REGS_ADDR + 0x08)
#define FLASH_F4_SR (FLASH_F4_REGS_ADDR + 0x0c)
#define FLASH_F4_CR (FLASH_F4_REGS_ADDR + 0x10)
#define FLASH_F4_OPT_CR (FLASH_F4_REGS_ADDR + 0x14)
#define FLASH_F4_CR_STRT 16
#define FLASH_F4_CR_LOCK 31
#define FLASH_F4_CR_SER 1
#define FLASH_F4_CR_SNB 3
#define FLASH_F4_CR_SNB_MASK 0x38
#define FLASH_F4_SR_BSY 16
//}}}
//{{{  cortex core ids
// TODO clean this up...
#define STM32VL_CORE_ID 0x1ba01477
#define STM32L_CORE_ID 0x2ba01477
#define STM32F3_CORE_ID 0x2ba01477
#define STM32F4_CORE_ID 0x2ba01477
#define STM32F0_CORE_ID 0xbb11477

#define CORE_M3_R1 0x1BA00477
#define CORE_M3_R2 0x4BA00477
#define CORE_M4_R0 0x2BA01477
//}}}
//{{{  Chip IDs are explained in the appropriate programming manual for the * DBGMCU_IDCODE register (0xE0042000)
// stm32 chipids, only lower 12 bits..
#define STM32_CHIPID_F1_MEDIUM 0x410
#define STM32_CHIPID_F2 0x411
#define STM32_CHIPID_F1_LOW 0x412
#define STM32_CHIPID_F3 0x422
#define STM32_CHIPID_F37x 0x432
#define STM32_CHIPID_F4 0x413
#define STM32_CHIPID_F1_HIGH 0x414
#define STM32_CHIPID_L1_MEDIUM 0x416
#define STM32_CHIPID_F1_CONN 0x418
#define STM32_CHIPID_F1_VL_MEDIUM 0x420
#define STM32_CHIPID_F1_VL_HIGH 0x428
#define STM32_CHIPID_F1_XL 0x430
#define STM32_CHIPID_F0 0x440
//}}}
//{{{  Constant STM32 memory map figures
#define STM32_FLASH_BASE 0x08000000
#define STM32_SRAM_BASE  0x20000000
//}}}
//{{{  Cortex™-M3 Technical Reference Manual
/* Debug Halting Control and Status Register */
#define DHCSR 0xe000edf0
#define DCRSR 0xe000edf4
#define DCRDR 0xe000edf8
//#define DBGKEY 0xa05f0000
//}}}
//}}}
//{{{  cortexM defines
#define CORTEX_M_COMMON_MAGIC 0x1A451A45

//{{{  flash page size
#define STM32_FLASH_PGSZ 1024
#define STM32L_FLASH_PGSZ 256
#define STM32F4_FLASH_PGSZ 16384
#define STM32F4_FLASH_SIZE (128 * 1024 * 8)
//}}}
//{{{  sram size
#define STM32_SRAM_SIZE (8 * 1024)
#define STM32L_SRAM_SIZE (16 * 1024)
//}}}

#define ITM_TER0   0xE0000E00
#define ITM_TPR    0xE0000E40
#define ITM_TCR    0xE0000E80
#define ITM_LAR    0xE0000FB0
  #define ITM_LAR_KEY 0xC5ACCE55

// SCB - System Control Block - CPUID
#define SCB_CPUID  0xE000ED00

// Debug Control Block
#define DCB_DHCSR  0xE000EDF0
  //{{{  DCB_DHCSR bit and field definitions
  #define DBGKEY      (0xA05F << 16)
  #define C_DEBUGEN   (1 << 0)
  #define C_HALT      (1 << 1)
  #define C_STEP      (1 << 2)
  #define C_MASKINTS  (1 << 3)
  #define S_REGRDY    (1 << 16)
  #define S_HALT      (1 << 17)
  #define S_SLEEP     (1 << 18)
  #define S_LOCKUP    (1 << 19)
  #define S_RETIRE_ST (1 << 24)
  #define S_RESET_ST  (1 << 25)
  //}}}
#define DCB_DCRSR  0xE000EDF4
  #define DCRSR_WnR (1 << 16)
#define DCB_DCRDR  0xE000EDF8
#define DCB_DEMCR  0xE000EDFC
  //{{{  DCB_DEMCR bit and field definitions
  #define TRCENA       (1 << 24)
  #define VC_HARDERR   (1 << 10)
  #define VC_INTERR    (1 << 9)
  #define VC_BUSERR    (1 << 8)
  #define VC_STATERR   (1 << 7)
  #define VC_CHKERR    (1 << 6)
  #define VC_NOCPERR   (1 << 5)
  #define VC_MMERR     (1 << 4)
  #define VC_CORERESET (1 << 0)
  //}}}

#define DWT_CTRL   0xE0001000
#define DWT_CYCCNT 0xE0001004
#define DWT_COMP0  0xE0001020
#define DWT_MASK0  0xE0001024
#define DWT_FUNCTION0 0xE0001028

// FP
#define FP_CTRL    0xE0002000
#define FP_REMAP   0xE0002004
#define FP_COMP0   0xE0002008
#define FP_COMP1   0xE000200C
#define FP_COMP2   0xE0002010
#define FP_COMP3   0xE0002014
#define FP_COMP4   0xE0002018
#define FP_COMP5   0xE000201C
#define FP_COMP6   0xE0002020
#define FP_COMP7   0xE0002024

#define FPU_CPACR  0xE000ED88
#define FPU_FPCCR  0xE000EF34
#define FPU_FPCAR  0xE000EF38
#define FPU_FPDSCR 0xE000EF3C

// TPIU
#define TPIU_SSPSR 0xE0040000
#define TPIU_CSPSR 0xE0040004
#define TPIU_ACPR  0xE0040010
#define TPIU_SPPR  0xE00400F0
#define TPIU_FFSR  0xE0040300
#define TPIU_FFCR  0xE0040304
#define TPIU_FSCR  0xE0040308

// DEBUGMCU
#define DEBUGMCU_IDCODE 0xE0042000
#define DEBUGMCU_CR     0xE0042004
  #define DBGMCU_CR_TRACE_IOEN 0x00000020

//}}}

//{{{
typedef struct tChipParams_ {
  uint32_t chip_id;
  char* description;
  uint32_t flash_size_reg;
  uint32_t flash_pagesize;
  uint32_t sram_size;
  uint32_t bootrom_base, bootrom_size;
  } chip_params_t;
//}}}
//{{{
const tChipParams_ devices[] = {
  { 0x413, "F4 device", 0x1FFF7A10, 0x4000, 0x30000, 0x1fff0000, 0x7800 },
  { 0x449, "F7 device", 0x1ff0f442, 0x4000, 0x50000, 0x00100000, 0xEDC0 }
  };
//}}}

//{{{
//int armv7m_trace_tpiu_config(struct target *target)
  //retval = target_write_u32(target, TPIU_CSPSR, 1 << trace_config->port_size);
  //retval = target_write_u32(target, TPIU_ACPR, prescaler - 1);
  //retval = target_write_u32(target, TPIU_SPPR, trace_config->pin_protocol);

  //uint32_t ffcr;
  //retval = target_read_u32(target, TPIU_FFCR, &ffcr);
  //if (trace_config->formatter)
    //ffcr |= (1 << 1);
  //else
    //ffcr &= ~(1 << 1);
  //retval = target_write_u32(target, TPIU_FFCR, ffcr);

//int armv7m_trace_itm_config(struct target *target)
//  target_write_u32 (target, ITM_LAR, ITM_LAR_KEY);

  /* Enable ITM, TXENA, set TraceBusID and other parameters */
//  target_write_u32(target, ITM_TCR, (1 << 0) | (1 << 3) |
//                                    (trace_config->itm_diff_timestamps << 1) |
//                                    (trace_config->itm_synchro_packets << 2) |
//                                    (trace_config->itm_async_timestamps << 4) |
//                                    (trace_config->itm_ts_prescale << 8) |
//                                    (trace_config->trace_bus_id << 16));

//  for (unsigned int i = 0; i < 8; i++) {
//    target_write_u32(target, ITM_TER0 + i * 4, trace_config->itm_ter[i]);

//void SWO_Init(uint32_t portBits, uint32_t cpuCoreFreqHz) {
  //uint32_t SWOSpeed = 64000; /* default 64k baud rate */
  /* SWOSpeed in Hz, note that cpuCoreFreqHz is expected to be match the CPU core clock */
  //uint32_t SWOPrescaler = (cpuCoreFreqHz / SWOSpeed) - 1;

  /* enable trace in core debug */
  //CoreDebug->DEMCR = CoreDebug_DEMCR_TRCENA_Msk;

  /* "Selected PIN Protocol Register": Select which protocol to use for trace output (2: SWO NRZ, 1: SWO Manchester encoding) */
  //*((volatile unsigned *)(ITM_BASE + 0x400F0)) = 0x00000002;

  /* "Async Clock Prescaler Register". Scale the baud rate of the asynchronous output */
  /*((volatile unsigned *)(ITM_BASE + 0x40010)) = SWOPrescaler;

  //* ITM Lock Access Register, C5ACCE55 enables more write access to Control Register 0xE00 :: 0xFFC */
  //*((volatile unsigned *)(ITM_BASE + 0x00FB0)) = 0xC5ACCE55;

  /* ITM Trace Control Register */
  //ITM->TCR = ITM_TCR_TraceBusID_Msk | ITM_TCR_SWOENA_Msk | ITM_TCR_SYNCENA_Msk | ITM_TCR_ITMENA_Msk;

  /* ITM Trace Privilege Register */
  //ITM->TPR = ITM_TPR_PRIVMASK_Msk;

  /* ITM Trace Enable Register. Enabled tracing on stimulus ports. One bit per stimulus port. */
  //ITM->TER = portBits;

  //*((volatile unsigned *)(ITM_BASE + 0x01000)) = 0x400003FE; /* DWT_CTRL */
  //}
//}}}
//{{{
//void SWO_Init() {

  //// default 64k baud rate
  //// SWOSpeed in Hz, note that cpuCoreFreqHz is expected to be match the CPU core clock
  //uint32_t SWOSpeed = 64000;
  ////uint32_t SWOSpeed = 200000;
  //uint32_t cpuCoreFreqHz = 168000000;
  //uint32_t SWOPrescaler = (cpuCoreFreqHz / SWOSpeed) - 1;

  //// enable trace in core debug
  //CoreDebug->DEMCR = CoreDebug_DEMCR_TRCENA_Msk;

  //// TPI SPPR - Selected PIN Protocol Register =  protocol for trace output 2: SWO NRZ, 1: SWO Manchester encoding
  //TPI->SPPR = 0x00000002;

  //// TPI Async Clock Prescaler Register =  Scale the baud rate of the asynchronous output
  //TPI->ACPR = SWOPrescaler;

  //// ITM Lock Access Register = ITM_LAR_KEY = C5ACCE55 enables write access to Control Register 0xE00 :: 0xFFC
  //ITM->LAR = ITM_LAR_KEY;

  //// ITM Trace Control Register
  //ITM->TCR = ITM_TCR_TraceBusID_Msk | ITM_TCR_SWOENA_Msk | ITM_TCR_SYNCENA_Msk | ITM_TCR_ITMENA_Msk;

  //// ITM Trace Privilege Register =
  //ITM->TPR = ITM_TPR_PRIVMASK_Msk;

  //// ITM Trace Enable Register =  Enabled tracing on stimulus ports. One bit per stimulus port
  //ITM->TER = 0x1F;

  //// DWT Control Register =
  ////*((volatile unsigned*)(ITM_BASE + 0x01000)) = 0x400003FE;
  //DWT->CTRL = 0x400003FE;

  //// TPI Formatter and Flush Control Register =
  ////*((volatile unsigned*)(ITM_BASE + 0x40304)) = 0x00000100;
  //TPI->FFCR = 0x00000100;
//}
//}}}

cStLink::cStLink() : mCoreStatus (STLINK_CORE_STAT_UNKNOWN) {}
//{{{
cStLink::~cStLink() {

  if (req_trans != NULL)
    libusb_free_transfer (req_trans);
  if (rep_trans != NULL)
    libusb_free_transfer (rep_trans);
  if (mUsbHandle != NULL)
    libusb_close (mUsbHandle);
  libusb_exit (mUsbContext);
  }
//}}}

//{{{
int cStLink::openUsb() {

  if (libusb_init (&mUsbContext)) {
    printf ("failed to init libusb context\n");
    return 0;
    }

  libusb_device** devs;
  auto numDevices = libusb_get_device_list (mUsbContext, &devs);
  for (int i = 0; i < numDevices; i++) {
    struct libusb_device_descriptor dev_desc;
    if (libusb_get_device_descriptor (devs[i], &dev_desc) != 0) {
      printf ("%d Usb device - libusb_get_device_descriptor failed\n", i);
      continue;
      }

    printf ("%d Usb device %4x:%4x\n", i, dev_desc.idVendor, dev_desc.idProduct);
    mV21 = false;
    if (dev_desc.idVendor != USB_ST_VID)
      continue;
    if (dev_desc.idProduct == USB_STLINK_32L_PID)
      mV21 = false;
    else if (dev_desc.idProduct == USB_STLINK_V21_PID)
      mV21 = true;
    else
      continue;
    if (libusb_open (devs[i], &mUsbHandle) != 0) {
      printf ("- libusb_open failed\n");
      continue;
      }
    break;
    }

  if (!mUsbHandle)
    return 0;

  int current_config = -1;
  libusb_get_configuration (mUsbHandle, &current_config);

  struct libusb_device* udev = libusb_get_device (mUsbHandle);
  struct libusb_config_descriptor* config = NULL;
  libusb_get_config_descriptor (udev, 0, &config);
  if (current_config != config->bConfigurationValue)
    libusb_set_configuration (mUsbHandle, config->bConfigurationValue);
  libusb_free_config_descriptor(config);

  if (libusb_claim_interface (mUsbHandle, 0) != 0) {
    printf ("- libusb_claim_interface interface failed");
    return 0;
    }

  ep_rep = STLINK_V2_RX_EP;
  ep_req = mV21 ? STLINK_V2_1_TX_EP : STLINK_V2_0_TX_EP;
  ep_trace = mV21 ? STLINK_V2_1_TRACE_EP : STLINK_V2_0_TRACE_EP;

  if (getCurrentMode(0) == STLINK_DEV_DFU_MODE)
    exitDfuMode();
  if (getCurrentMode(0) != STLINK_DEV_DEBUG_MODE)
    enterSwdMode();

  //reset();
  loadDeviceParams();
  return 1;
  }
//}}}

//{{{
void cStLink::getVersion() {

  mCmdBuf[0] = STLINK_GET_VERSION;
  auto size = sendRecv (1, 6);
  if (size == -1) {
    printf("[!] send_recv\n");
    }

  uint32_t b0 = mDataBuf[0]; //lsb
  uint32_t b1 = mDataBuf[1];
  uint32_t b2 = mDataBuf[2];
  uint32_t b3 = mDataBuf[3];
  uint32_t b4 = mDataBuf[4];
  uint32_t b5 = mDataBuf[5]; //msb

  // b0 b1                       || b2 b3  | b4 b5
  // 4b        | 6b     | 6b     || 2B     | 2B
  // stlink_v  | jtag_v | swim_v || st_vid | stlink_pid
  version.stlink_v =   (b0 & 0xf0) >> 4;
  version.jtag_v =    ((b0 & 0x0f) << 2) | ((b1 & 0xc0) >> 6);
  version.swim_v =      b1 & 0x3f;
  version.st_vid =     (b3 << 8) | b2;
  version.stlink_pid = (b5 << 8) | b4;

  printf ("StLink vid:pid 0x%04x:0x%04x version:%d jtag version:%d\n",
          version.st_vid, version.stlink_pid, version.stlink_v, version.jtag_v);
  }
//}}}
//{{{
uint32_t cStLink::getCoreId() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_READCOREID;
  auto size = sendRecv (2, 4);
  if (size == -1) {
    printf("[!] send_recv\n");
    return 0;
    }

  memcpy (&mCoreId, mDataBuf, 4);
  return mCoreId;
  }
//}}}
//{{{
uint32_t cStLink::getChipId() {

  mIdCode = readDebug32 (DEBUGMCU_IDCODE);
  mChipId = mIdCode & 0xFFF;
  mRevId = mIdCode >> 16;

  // Fix mChipId for F4 rev A errata , Read cpuId, as CoreId is the same for F2/F4
  if (mChipId == 0x411) {
    // SCB - System Control Block - CPUID
    uint32_t cpuid = readDebug32 (SCB_CPUID);
    if ((cpuid  & 0xfff0) == 0xC240) {
      printf ("fixing cpuId for f4 rev A\n");
      mChipId = 0x413;
      }
    }

  return mChipId;
  }
//}}}
//{{{
void cStLink::getCpuId (cortex_m3_cpuid_t *cpuid) {

  uint32_t cpuIdReg = readDebug32 (SCB_CPUID);

  cpuid->implementer_id = (cpuIdReg >> 24) &  0x7f;
  cpuid->variant =        (cpuIdReg >> 20) &   0xf;
  cpuid->part =           (cpuIdReg >> 4)  & 0xfff;
  cpuid->revision =        cpuIdReg & 0xf;
  return;
  }
//}}}
//{{{
void cStLink::getStatus() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_GETSTATUS;
  auto size = sendRecv (2, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  if (mDataLen <= 0)
     return;

  switch (mDataBuf[0]) {
    case STLINK_CORE_RUNNING:
      mCoreStatus = STLINK_CORE_RUNNING;
      printf ("getStatus: running\n");
      return;
    case STLINK_CORE_HALTED:
      mCoreStatus = STLINK_CORE_HALTED;
      printf ("getStatus: halted\n");
      return;
    default:
      mCoreStatus = STLINK_CORE_STAT_UNKNOWN;
      printf ("getStatus: unknown\n");
    }
  }
//}}}
//{{{
int cStLink::getCurrentMode (int report) {

  mCmdBuf[0] = STLINK_GET_CURRENT_MODE;
  auto size = sendRecv (1, 2);
  if (size == -1) {
      printf("[!] send_recv\n");
      return -1;
    }
  int mode = mDataBuf[0];

  switch (mode) {
    case STLINK_DEV_DFU_MODE:
      if (report)
        printf ("stlink current mode: dfu\n");
      break;
    case STLINK_DEV_DEBUG_MODE:
      if (report)
        printf ("stlink current mode: debug (jtag or swd)\n");
      break;
    case STLINK_DEV_MASS_MODE:
      if (report)
        printf ("stlink current mode: dfu\n");
      break;
    default:
      printf ("stlink mode: unknown!\n");
      mode = STLINK_DEV_UNKNOWN_MODE;
      break;
      }

  return mode;
  }
//}}}
//{{{
float cStLink::getTargetVoltage() {

  mCmdBuf[0] = STLINK_GET_TARGET_VOLTAGE;
  auto size = sendRecv (1, 8);
  if (size == -1) {
    printf("[!] send_recv\n");
    return 0;
    }

  uint32_t adc_results[2];
  memcpy (&adc_results[0], mDataBuf, 4);
  memcpy(&adc_results[1], mDataBuf + 4, 4);

  float targetVoltage = 0;

  if (adc_results[0])
    targetVoltage = 2 * ((float)adc_results[1]) * (float)(1.2 / adc_results[0]);

  return targetVoltage;
  }
//}}}

//{{{
void cStLink::enterSwdMode() {

  printf ("enterSwdMode\n");
  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_ENTER;
  mCmdBuf[2] = STLINK_DEBUG_ENTER_SWD;
  send (mCmdBuf, 3);
  }
//}}}
//{{{
void cStLink::exitDebugMode() {

  printf ("exitDebugMode\n");
  writeDebug32 (DHCSR, DBGKEY);
  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_EXIT;
  send (mCmdBuf, 2);
  }
//}}}
//{{{
void cStLink::exitDfuMode() {

  printf ("exitDfuMode\n");
  mCmdBuf[0] = STLINK_DFU_COMMAND;
  mCmdBuf[1] = STLINK_DFU_EXIT;
  send (mCmdBuf, 2);
  }
//}}}
//{{{
// Force the core into the debug mode -> halted state.
void cStLink::forceDebug() {

  printf ("forceDebug\n");
  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_FORCEDEBUG;
  auto size = sendRecv (2, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}

//{{{
void cStLink::traceEnable() {

  printf ("traceEnable\n");
  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_APIV2_START_TRACE_RX;

  trace.enabled = true;
  trace.source_hz = STLINK_TRACE_MAX_HZ;
  uint16_t traceSize = STLINK_TRACE_SIZE;
  memcpy (&mCmdBuf[2], &traceSize, 2);
  memcpy (&mCmdBuf[4], &trace.source_hz, 4);
  send (mCmdBuf, 8);
  }
//}}}
//{{{
int cStLink::readTrace (uint8_t* buf, int size) {

  int transferred = 0;
  libusb_bulk_transfer (mUsbHandle, ep_trace, buf, size, &transferred, 1000);
  return transferred;
  }
//}}}

//{{{
uint32_t cStLink::readDebug32 (uint32_t addr) {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_APIV2_READDEBUGREG;
  memcpy (&mCmdBuf[2], &addr, 4);
  auto size = sendRecv (6, 8);
  if (size == -1) {
    printf("[!] send_recv\n");
    return 0;
    }

  uint32_t data;
  memcpy (&data, mDataBuf+4, 4);
  return data;
  }
//}}}
//{{{
uint32_t cStLink::readMem32 (uint32_t addr, uint16_t len) {

  if (len % 4 != 0) {
    // !!! never ever: fw gives just wrong values
    printf ("Error: Data length doesn't have a 32 bit alignment: +%d byte.\n", len % 4);
    abort();
    }

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_READMEM_32BIT;
  memcpy (&mCmdBuf[2], &addr, 4);
  memcpy (&mCmdBuf[6], &len, 2);
  mDataLen = sendRecv (8, len);
  if (mDataLen == -1) {
    printf("[!] send_recv\n");
    return 0;
    }
  //printf ("readMem32 %d\n", mDataLen);

  uint32_t data;
  memcpy (&data, mDataBuf, 4);
  return data;
  }
//}}}
//{{{
uint32_t cStLink::readReg (int r_idx, reg* regp) {

  if (r_idx > 20 || r_idx < 0) {
    printf ("Error: register index must be in [0..20]\n");
    return 0;
    }

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_APIV2_READREG;
  mCmdBuf[2] = (uint8_t) r_idx;
  mDataLen = sendRecv (3, 8);
  if (mDataLen != 8) {
    printf("[!] send_recv\n");
    return 0;
    }

  uint32_t r;
  memcpy (&r, mDataBuf+4, 4);
  switch (r_idx) {
    case 16:
      regp->xpsr = r;
      break;
    case 17:
      regp->main_sp = r;
      break;
    case 18:
      regp->process_sp = r;
      break;
    case 19:
      regp->rw = r; /* XXX ?(primask, basemask etc.) */
      break;
    case 20:
      regp->rw2 = r; /* XXX ?(primask, basemask etc.) */
      break;
    default:
      regp->r[r_idx] = r;
    }
  return r;
  }
//}}}
//{{{
void cStLink::readAllRegs (reg* regp) {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_READALLREGS;
  auto size = sendRecv (2, 84);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  mDataLen = (size_t) size;

  for (int i = 0; i < 16; i++)
    memcpy (&regp->r[i], mDataBuf + i*4, 4);
  memcpy (&regp->xpsr, mDataBuf + 64, 4);
  memcpy (&regp->main_sp, mDataBuf + 68, 4);
  memcpy (&regp->process_sp, mDataBuf + 72, 4);
  memcpy (&regp->rw, mDataBuf + 76, 4);
  memcpy (&regp->rw2, mDataBuf + 80, 4);

  printf ("xpsr       = 0x%08x\n", regp->xpsr);
  printf ("main_sp    = 0x%08x\n", regp->main_sp);
  printf ("process_sp = 0x%08x\n", regp->process_sp);
  printf ("rw         = 0x%08x\n", regp->rw);
  printf ("rw2        = 0x%08x\n", regp->rw2);
  }
//}}}
//{{{
void cStLink::readUnsupportedReg (int r_idx, reg* regp) {

  int r_convert;

  /* Convert to values used by DCRSR */
  if (r_idx >= 0x1C && r_idx <= 0x1F) { /* primask, basepri, faultmask, or control */
    r_convert = 0x14;
    }
  else if (r_idx == 0x40) {     /* FPSCR */
    r_convert = 0x21;
    }
  else if (r_idx >= 0x20 && r_idx < 0x40) {
    r_convert = 0x40 + (r_idx - 0x20);
    }
  else {
    printf ("Error: register address must be in [0x1C..0x40]\n");
    return;
    }

  uint32_t r;

  mDataBuf[0] = (unsigned char) r_idx;
  for (int i = 1; i < 4; i++) {
    mDataBuf[i] = 0;
    }

  writeMem32 (DCRSR, 4);
  readMem32 (DCRDR, 4);
  memcpy (&r, mDataBuf, 4);

  switch (r_idx) {
    case 0x14:
      regp->primask = (uint8_t) (r & 0xFF);
      regp->basepri = (uint8_t) ((r>>8) & 0xFF);
      regp->faultmask = (uint8_t) ((r>>16) & 0xFF);
      regp->control = (uint8_t) ((r>>24) & 0xFF);
      break;
    case 0x21:
      regp->fpscr = r;
      break;
    default:
      regp->s[r_idx - 0x40] = r;
      break;
    }
  }
//}}}
//{{{
void cStLink::readAllUnsupportedRegs (reg* regp) {

  readUnsupportedReg (0x14, regp);
  readUnsupportedReg (0x21, regp);
  for (int i = 0; i < 32; i++)
    readUnsupportedReg (0x40+i, regp);
  }
//}}}

//{{{
void cStLink::writeDebug32 (uint32_t addr, uint32_t data) {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_APIV2_WRITEDEBUGREG;
  memcpy (&mCmdBuf[2], &addr, 4);
  memcpy (&mCmdBuf[6], &data, 4);
  auto size = sendRecv (10, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::writeMem32 (uint32_t addr, uint16_t len) {
  if (len % 4 != 0) {
    fprintf(stderr, "Error: Data length doesn't have a 32 bit alignment: +%d byte.\n", len % 4);
    abort();
    }

  int i = 0;
  mCmdBuf[i++] = STLINK_DEBUG_COMMAND;
  mCmdBuf[i++] = STLINK_DEBUG_WRITEMEM_32BIT;
  memcpy (&mCmdBuf[i], &addr, 4);
  memcpy (&mCmdBuf[i+4], &len, 2);
  send (mCmdBuf, 10);
  send (mDataBuf, len);
  }
//}}}
//{{{
void cStLink::writeMem8 (uint32_t addr, uint16_t len) {

  if (len > 0x40 ) { // !!! never ever: Writing more then 0x40 bytes gives unexpected behaviour
    printf ("Error: Data length > 64: +%d byte.\n", len);
    abort();
    }

  int i = 0;
  mCmdBuf[i++] = STLINK_DEBUG_COMMAND;
  mCmdBuf[i++] = STLINK_DEBUG_WRITEMEM_8BIT;
  memcpy (&mCmdBuf[i], &addr, 4);
  memcpy (&mCmdBuf[i+4], &len, 2);
  send (mCmdBuf, 8);
  send (mDataBuf, len);
  }
//}}}
//{{{
void cStLink::writeReg (uint32_t reg, int idx) {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_WRITEREG;
  mCmdBuf[2] = idx;
  memcpy (&mCmdBuf[3], &reg, 4);
  auto size = sendRecv (7, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  mDataLen = (size_t) size;
  }
//}}}
//{{{
void cStLink::writeUnsupportedReg (uint32_t val, int r_idx, reg* regp) {
  int r_convert;

  /* Convert to values used by DCRSR */
  if (r_idx >= 0x1C && r_idx <= 0x1F) { /* primask, basepri, faultmask, or control */
    r_convert = r_idx;  /* The backend function handles this */
  } else if (r_idx == 0x40) {     /* FPSCR */
    r_convert = 0x21;
  } else if (r_idx >= 0x20 && r_idx < 0x40) {
    r_convert = 0x40 + (r_idx - 0x20);
  } else {
    printf ("Error: register address must be in [0x1C..0x40]\n");
    return;
    }

  if (r_idx >= 0x1C && r_idx <= 0x1F) {
    /* primask, basepri, faultmask, or control These are held in the same register */
    readUnsupportedReg (0x14, regp);
    val = (uint8_t) (val>>24);
    switch (r_idx) {
      case 0x1C:  /* control */
        val = (((uint32_t) val) << 24) | (((uint32_t) regp->faultmask) << 16) |
              (((uint32_t) regp->basepri) << 8) | ((uint32_t) regp->primask);
        break;
      case 0x1D:  /* faultmask */
        val = (((uint32_t) regp->control) << 24) | (((uint32_t) val) << 16) |
              (((uint32_t) regp->basepri) << 8) | ((uint32_t) regp->primask);
        break;
      case 0x1E:  /* basepri */
        val = (((uint32_t) regp->control) << 24) | (((uint32_t) regp->faultmask) << 16) |
              (((uint32_t) val) << 8) | ((uint32_t) regp->primask);
        break;
      case 0x1F:  /* primask */
        val = (((uint32_t) regp->control) << 24) | (((uint32_t) regp->faultmask) << 16) |
              (((uint32_t) regp->basepri) << 8) | ((uint32_t) val);
        break;
      }
    r_idx = 0x14;
    }

  memcpy (mDataBuf, &val, 4);
  writeMem32 (DCRDR, 4);

  mDataBuf[0] = (unsigned char)r_idx;
  mDataBuf[1] = 0;
  mDataBuf[2] = 0x01;
  mDataBuf[3] = 0;
  writeMem32 (DCRSR, 4);
  }
//}}}

//{{{
void cStLink::reset() {

  printf ("reset\n");
  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_APIV2_RESETSYS;
  auto size = sendRecv (2, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::jtagReset (int value) {

  printf ("jtagReset\n");
  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_APIV2_DRIVE_NRST;
  mCmdBuf[2] = (value) ? 0 : 1;
  auto size = sendRecv (3, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::step() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_STEPCORE;
  auto size = sendRecv (2, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::run() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_RUNCORE;
  auto size = sendRecv (2, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::runAt (stm32_addr_t addr) {

  writeReg (addr, 15); /* pc register */
  run();
  while (is_core_halted() == 0)
    Sleep(3000000);
  }
//}}}

// private
//{{{
unsigned int cStLink::is_core_halted() {

  /* return non zero if core is halted */
  getStatus();
  return mDataBuf[0] == STLINK_CORE_HALTED;
  }
//}}}
//{{{
int cStLink::loadDeviceParams() {

  printf ("CoreId:%x ChipId:%x IdCode:%x RevId:%x\n",  getCoreId(), getChipId(), mIdCode, mRevId);

  const chip_params_t* params = NULL;
  for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
    if (devices[i].chip_id == mChipId) {
      params = &devices[i];
      break;
      }
    }
  if (params == NULL) {
    printf ("Unknown chip id %#x\n", mChipId);
    return -1;
    }

  flash_base = STM32_FLASH_BASE;
  if (mChipId == STM32_CHIPID_F2)
    flash_size = 0x100000;
  else if (mChipId == STM32_CHIPID_F4)
    flash_size = 0x100000;
  else {
    uint32_t flash_size = readDebug32(params->flash_size_reg) & 0xffff;
    flash_size = flash_size * 1024;
    }
  flash_pgsz = params->flash_pagesize;

  sram_base = STM32_SRAM_BASE;
  sram_size = params->sram_size;

  sys_base = params->bootrom_base;
  sys_size = params->bootrom_size;

  printf ("%s %zdK flash %zd pages %zdk sram\n",
          params->description, flash_size / 1024, flash_pgsz,sram_size / 1024);

  getVersion();

  printf ("Target voltage %4.3f\n", getTargetVoltage());
  return 0;
  }
//}}}

//{{{
int cStLink::sendRecv (int txSize, int rxSize) {

  int transferred = 0;
  libusb_bulk_transfer (mUsbHandle, ep_req, mCmdBuf, txSize, &transferred, 1000);
  libusb_bulk_transfer (mUsbHandle, ep_rep, mDataBuf, rxSize, &transferred, 1000);
  return transferred;
  }
//}}}
//{{{
int cStLink::send (unsigned char* txBuf, int txSize) {
  int transferred = 0;
  libusb_bulk_transfer (mUsbHandle, ep_req, txBuf, txSize, &transferred, 1000);
  return transferred;
  }
//}}}
