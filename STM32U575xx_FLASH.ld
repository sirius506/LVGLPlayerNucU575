/*
******************************************************************************
**
**  File        : LinkerScript.ld
**
**  Author		: Auto-generated by STM32CubeIDE
**
**  Abstract    : Linker script for STM32U575xx Device from STM32U5 series
**                      2048Kbytes ROM
**                      784Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
** All rights reserved.</center></h2>
**
** This software component is licensed by ST under BSD 3-Clause license,
** the "License"; You may not use this file except in compliance with the
** License. You may obtain a copy of the License at:
**                        opensource.org/licenses/BSD-3-Clause
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);	/* end of "RAM" Ram type memory */

_Min_Heap_Size = 0x200;      /* required amount of heap  */
_Min_Stack_Size = 0x400; /* required amount of stack */

/* Memories definition */
MEMORY
{
  RAM	(rw)	: ORIGIN = 0x20000000,	LENGTH = 608K  /* 608K */
  RTOSHEAP (rw) : ORIGIN = 0x20098000,  LENGTH = 32K
  LVHEAP   (rw) : ORIGIN = 0x200A0000,  LENGTH = 128K
  ROM	(rx)	: ORIGIN = 0x08000000,	LENGTH = 2048K
  SRAM4	(rw)	: ORIGIN = 0x28000000,	LENGTH = 16K
  BKPSRAM (rw)  : ORIGIN = 0x40036400,  LENGTH = 2K
}

/* Sections */
SECTIONS
{
  /* The startup code into "ROM" Rom type memory */
  .isr_vector :
  {
    . = ALIGN(8);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(8);
  } >ROM

  /* The program code and other data into "ROM" Rom type memory */
  .text :
  {
    . = ALIGN(8);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(8);
    _etext = .;        /* define a global symbols at end of code */
  } >ROM

  /* Constant data into "ROM" Rom type memory */
  .rodata :
  {
    . = ALIGN(8);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(8);
  } >ROM

  .ARM.extab (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(8);
  } >ROM
  
  .ARM (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(8);
  } >ROM

  .preinit_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(8);
  } >ROM
  
  .init_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(8);
  } >ROM
  
  .fini_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(8);
  } >ROM

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections into "RAM" Ram type memory */
  .data : 
  {
    . = ALIGN(8);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(8);
    _edata = .;        /* define a global symbol at data end */
    
  } >RAM AT> ROM

  /* Uninitialized data section into "RAM" Ram type memory */
  . = ALIGN(8);

  OVERLAY : NOCROSSREFS
  {
    .OVERLAY_DOOM
    {
      _sbss = .;         /* define a global symbol at bss start */
      _sdoomvar = .;
      __doomvar_start__ = _sdoomvar;
      *i_video.c.obj (.bss .bss*)
      _edoomvar = .;
      __doomvar_end = _edoomvar;
    }
    .OVERLAY_AVRCP
    {
      _savrcpvar = .;
      __avrcpvar_start__ = _savrcpvar;
      *a2dp_sink.c.obj (.bss .bss*)
      *a2dp.c.obj (.bss .bss*)
      *hal_audio.c.obj (.bss .bss*)
      *avdtp.c.obj (.bss .bss*)
      *avdtp_sink.c.obj (.bss .bss*)
      *avdtp_util.c.obj (.bss .bss*)
      *avrcp.c.obj (.bss .bss*)
      *avdtp_sink.c.obj (.bss .bss*)
      *btstack_sbc_decoder_bluedroid.c.obj (.bss .bss*)
      *btstack_sbc_plc.c.obj (.bss .bss*)
      *btstack_audio_embedded.c.obj (.bss .bss*)
      *avrcp.c.obj (.bss .bss*)
      *avrcp_controller.c.obj (.bss .bss*)
      *avrcp_target.c.obj (.bss .bss*)
      *decoder-sbc.c.obj (.bss .bss*)
      _eavrcpvar = .;
      __avrcpvar_end = _eavrcpvar;
    }
    .OVERLAY_OSCM
    {
      _soscmvar = .;
      __oscmvar_start__ = _soscmvar;
      *app_osc_music.c.obj (.bss .bss*)
      _eoscmvar = .;
      __oscmvar_end = _eoscmvar;
    }
  } > RAM

  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    /* _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(8);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* User_heap_stack section, used to check that there is enough "RAM" Ram  type memory left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
  .SRDSRAMSection (NOLOAD) : { *(.SRDSRAMSection) } > SRAM4
  .BKPSRAMSection (NOLOAD) : { *(.BKPSRAMSection) } > BKPSRAM
}
