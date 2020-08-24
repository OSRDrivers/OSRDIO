///////////////////////////////////////////////////////////////////////////////
//
//    (C) Copyright 2020 OSR Open Systems Resources, Inc.
//    All Rights Reserved
//
//    This software is supplied for instructional purposes only.
//
//    OSR Open Systems Resources, Inc. (OSR) expressly disclaims any warranty
//    for this software.  THIS SOFTWARE IS PROVIDED  "AS IS" WITHOUT WARRANTY
//    OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION,
//    THE IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
//    PURPOSE.  THE ENTIRE RISK ARISING FROM THE USE OF THIS SOFTWARE REMAINS
//    WITH YOU.  OSR's entire liability and your exclusive remedy shall not
//    exceed the price paid for this material.  In no event shall OSR or its
//    suppliers be liable for any damages whatsoever (including, without
//    limitation, damages for loss of business profit, business interruption,
//    loss of business information, or any other pecuniary loss) arising out
//    of the use or inability to use this software, even if OSR has been
//    advised of the possibility of such damages.  Because some states/
//    jurisdictions do not allow the exclusion or limitation of liability for
//    consequential or incidental damages, the above limitation may not apply
//    to you.
//
//    OSR Open Systems Resources, Inc.
//    889 Elm St, Sixth Floor
//    Manchester, NH 03101
//    email bugs to: bugs@osr.com
//
//
//    MODULE:
//
//        OsrDio.h -- WDF Example Driver, master header file
//
//    AUTHOR(S):
//
//        OSR Open Systems Resources, Inc.
// 
///////////////////////////////////////////////////////////////////////////////
#pragma once

//
// Standard Windows headers
#include <wdm.h> 
#include <wdf.h>

#define OSR_FIX_ZERO_BUG_ON_1909    1

// ReSharper disable once CppUnusedIncludeDirective
#include "OsrDio_IOCTL.h"

#pragma warning(disable: 4201)          // "non-standard extension used"

//
// WDF drivers should typically be built with Code Analysis enabled for all
// builds, and with the setting set to "Microsoft Driver Minimum Rules"
// (which is actually the MSFT recommended rule set).
//
//
// We generally do not recommend routinely building with Code Analysis (CA)
// checking for the CPP Core Guidelines enabled.  But... occasionally
// running CA with these warnings enabled can be useful.  To facilitate that
// we disable the following warnings that we really don't care about:
//
//  26493   no C-style casts
//  26461   variable can be made const
//  26464   initialize everything
//  26438   avoid goto
//  26489   don't deref a point that could be null
//
#pragma warning(disable: 26493 26461 26494 26464 26438 26489)

//
// The size of the device memory area on the NI PCIe-6509
//
constexpr ULONG DIO_BAR_SIZE = 512 * 1024;


// ReSharper disable CppInconsistentNaming

//
// Macros for building a structure for the register map, even though
// the map is large and non-contiguous.
//
// We thank @shuffle2 via ghettoha.xxx for inspiring this technique
//
// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
// ReSharper disable IdentifierTypo

#define CAT_(x, y) x ## y
#define CAT(x, y) CAT_(x, y)

#define REGPAD(_size_) UCHAR CAT(_pad_, __COUNTER__)[_size_]
#define REGSTRUCT_START(_name_, _size_) typedef struct _##_name_ { union { REGPAD(_size_);
#define REGSTRUCT_END(_name_) };} _name_, *P##_name_  // NOLINT(bugprone-macro-parentheses)

#define REGDEF_ULONG(_field_, _off_) struct { REGPAD(_off_); ULONG _field_; }
#define REGDEF(_off_, _field_) struct { REGPAD(_off_); _field_; }

//
// DIO_REGISTERS Structure Definition
//
// The NI PCIe-6509 has a register map that is spread-out through its
// 512K of Memory Mapper I/O space. We define a typedef'ed structure
// named "DIO_REGISTERS" that describes the register map using the
// macros defined above.  The registers are all 32-bits wide.
//
// All register names are as specified in the NI documentation.
//
REGSTRUCT_START(DIO_REGISTERS, DIO_BAR_SIZE);
         ULONG CHInCh_Identification_Register;    // This register is at offset 0
//
//
//                                                     OFFSET
//              REGISTER NAME                          from BAR 0
//              ==============================         ==========
REGDEF_ULONG(   Static_Digital_Input_Register,         0x20530   );

REGDEF_ULONG(   Static_Digital_Output_Register,        0x204B0   );
REGDEF_ULONG(   DIO_Direction_Register,                0x204B4   );
REGDEF_ULONG(   DI_FilterRegister_Port0and1,           0x2054C   );
REGDEF_ULONG(   DI_FilterRegister_Port2and3,           0x20550   );

//
// DIO Change of State (RE = "Rising Edge", FE "Falling Edge")
// and DIO Interrupt Registers
//
REGDEF_ULONG(   ChangeDetectStatusRegister,            0x20540   );  // READ
REGDEF_ULONG(   DI_ChangeIrqRE_Register,               0x20540   );  // WRITE
REGDEF_ULONG(   DI_ChangeIrqFE_Register,               0x20544   );  // WRITE
REGDEF_ULONG(   DI_ChangeDetectLatched_Register,       0x20544   );

REGDEF_ULONG(   GlobalInterruptStatus_Register,        0x20070   );
REGDEF_ULONG(   GlobalInterruptEnable_Register,        0x20078   );
REGDEF_ULONG(   DI_Interrupt_Status_Register,          0x2007E   );
REGDEF_ULONG(   ChangeDetectIRQ_Register,              0x20554   );

//
// Board-Wide Interrupt Controller Registers
//
REGDEF_ULONG(   Interrupt_Mask_Register,               0x0005C   );
REGDEF_ULONG(   Interrupt_Status_Register,             0x00060   );
REGDEF_ULONG(   Volatile_Interrupt_Status_Register,    0x00068   );
REGDEF_ULONG(   IntForwarding_ControlStatus,           0x22204   );
REGDEF_ULONG(   IntForwarding_DestinationReg,          0x22208   );

//
// Miscellaneous Board-Level Registers
//
REGDEF_ULONG(   Scrap_Register,                        0X00200   );
REGDEF_ULONG(   PCI_Subsystem_ID_Access_Register,      0x010AC   );
REGDEF_ULONG(   ScratchpadRegister,                    0x20004   );
REGDEF_ULONG(   Signature_Register,                    0x20060   );
REGDEF_ULONG(   Joint_Reset_Register,                  0x20064   );  // WRITE
REGDEF_ULONG(   TimeSincePowerUpRegister,              0x20064   );  // READ

REGSTRUCT_END(DIO_REGISTERS);

// ReSharper disable IdentifierTypo
// ReSharper restore CppClangTidyCppcoreguidelinesMacroUsage

constexpr ULONG BIT_NUMBER(int x)
{
    return (ULONG)1<<x;
}

//
// Bit definitions for above registers
//
// (all names as specified in the NI documentation)
//

// Bit Definitions: Interrupt_Mask_Register
constexpr ULONG Set_CPU_Int     = BIT_NUMBER(31);
constexpr ULONG Clear_CPU_Int   = BIT_NUMBER(30);
constexpr ULONG Set_STC3_Int    = BIT_NUMBER(11);
constexpr ULONG Clear_STC3_Int  = BIT_NUMBER(10);

// Bit Definitions: GlobalInterruptEnable_Register
constexpr ULONG WatchdogTimer_Interrupt_Disable = BIT_NUMBER(26);
constexpr ULONG DI_Interrupt_Disable            = BIT_NUMBER(22);
constexpr ULONG WatchdogTimer_Interrupt_Enable  = BIT_NUMBER(10);
constexpr ULONG DI_Interrupt_Enable             = BIT_NUMBER(6);


// Bit Definitions: ChangeDetectIRQ_Register
constexpr ULONG ChangeDetectErrorIRQ_Enable         = BIT_NUMBER(7);
constexpr ULONG ChangeDetectErrorIRQ_Disable        = BIT_NUMBER(6);
constexpr ULONG ChangeDetectIRQ_Enable              = BIT_NUMBER(5);
constexpr ULONG ChangeDetectIRQ_Disable             = BIT_NUMBER(4);
constexpr ULONG ChangeDetectErrorIRQ_Acknowledge    = BIT_NUMBER(1);
constexpr ULONG ChangeDetectIRQ_Acknowledge         = BIT_NUMBER(0);

// Bit Definitions: Joint_Reset_Register
constexpr ULONG Software_Reset  = BIT_NUMBER(0);


// Bit Defintions: ChangeDetectStatusRegister
constexpr ULONG ChangeDetectError   = BIT_NUMBER(1);
constexpr ULONG ChangeDetectStatus  = BIT_NUMBER(0);

//
// Bit Definitions: Interrupt_Status_Register
//
constexpr ULONG Int             = BIT_NUMBER(31);
constexpr ULONG Additional_Int  = BIT_NUMBER(30);
constexpr ULONG External        = BIT_NUMBER(29);
constexpr ULONG DAQ_STC3_Int    = BIT_NUMBER(11);

//
// Bit Definitions: Volatile_Interrupt_Status_Register
//
constexpr ULONG Vol_Int             = BIT_NUMBER(31);
constexpr ULONG Vol_Additional_Int  = BIT_NUMBER(30);
constexpr ULONG Vol_External        = BIT_NUMBER(29);
constexpr ULONG Vol_STC3_Int        = BIT_NUMBER(11);

//
// Bit Definitions: DI_FilterRegister_Port0and1, DI_FilterRegister_Port2and3
//
constexpr ULONG Filter_Large_All_Lines = 0xFFFFFFFF;

// ReSharper restore CppInconsistentNaming

//
// Device Context
//
typedef struct _OSRDIO_DEVICE_CONTEXT
{
    WDFDEVICE           WdfDevice;
    WDFINTERRUPT        WdfInterrupt;

    PDIO_REGISTERS      DevBase;
    ULONG               MappedLength;

    WDFQUEUE            PendingQueue;

    ULONG               OutputLineMask;

    ULONG               SavedOutputLineState;

    ULONG               LatchedInputLineState;

}   OSRDIO_DEVICE_CONTEXT, *POSRDIO_DEVICE_CONTEXT;

//
// Define data type of our device context structure, and also
// create the helper function OsrDioGetContextFromDevice that will
// retrieve the context from the WDFDevice.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(OSRDIO_DEVICE_CONTEXT, OsrDioGetContextFromDevice)

//
// Forward Declarations
//
// We use "Function Roll Type" declarations for our driver's callbacks.
// This both provides the SAL annotations for the functions and also informs
// Code Analysis and Static Driver Verifier the "roll" or purpose of the
// particular callback.
//
extern "C"  {
DRIVER_INITIALIZE DriverEntry;
}
EVT_WDF_DRIVER_DEVICE_ADD OsrDioEvtDriverDeviceAdd;

EVT_WDF_DEVICE_PREPARE_HARDWARE OsrDioEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE OsrDioEvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY OsrDioEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT OsrDioEvtDeviceD0Exit;

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL OsrDioEvtIoDeviceControl;

EVT_WDF_INTERRUPT_ENABLE OsrDioEvtInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE OsrDioEvtInterruptDisable;
EVT_WDF_INTERRUPT_ISR OsrDioEvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC OsrDioEvtInterruptDpc;

//
// Utility functions
//

VOID DioUtilProgramLineDirectionAndChangeMasks(_In_ POSRDIO_DEVICE_CONTEXT DevContext);

VOID DioUtilResetDeviceInterrupts(_In_ POSRDIO_DEVICE_CONTEXT DevContext);

VOID DioUtilEnableDeviceInterrupts(_In_ POSRDIO_DEVICE_CONTEXT DevContext);

VOID DioUtilDeviceReset(_In_ POSRDIO_DEVICE_CONTEXT DevContext);

#if DBG
VOID DioUtilDisplayResources(_In_ WDFCMRESLIST Resources, _In_ WDFCMRESLIST ResourcesTranslated);
#endif
