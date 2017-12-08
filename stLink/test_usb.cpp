#include "stdafx.h"
#include "cStLink.h"

int main (int ac, char** av) {

  cStLink stlink;
  if (!stlink.openUsb()) {
    printf ("no stLink found\n");
    Sleep (2000);
    return 0;
    }

  cortex_m3_cpuid_t cpuid;
  stlink.getCpuId (&cpuid);
  printf ("Cpuid implId:%0#x, variant:%#x part:%#x, rev:%#x\n", cpuid.implementer_id, cpuid.variant, cpuid.part, cpuid.revision);

  static const uint32_t mem = 0x20000000;
  printf ("Read %x\n", mem);
  uint32_t off = 0;
  for (int j = 0; j < 16; j++) {
    printf ("%x - ", mem+off);
    for (int i= 0; i < 8; i++, off += 4)
      printf ("%08x ", stlink.readMem32 (mem + off, 4));
    printf ("\n");
    }


  reg regs;
  stlink.readAllRegs (&regs);
  stlink.getStatus ();

  stlink.run ();
  stlink.getStatus ();
  stlink.readAllRegs (&regs);

  stlink.traceEnable();
  uint8_t traceBuf[1024];
  for (int i = 0; i < 100; i++) {
    auto size = stlink.readTrace (traceBuf, 1024);
    if (size > 0)
      printf ("trace %d %s\n", size, traceBuf);
    }

  Sleep (10000);
  stlink.exitDebugMode ();
  return 0;
  }
