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
//        OsrDio_IOCTL.h -- Definitions shared between OsrDio driver and
//                          applications.
//
//    AUTHOR(S):
//
//        OSR Open Systems Resources, Inc.
// 
///////////////////////////////////////////////////////////////////////////////
// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
#pragma once

//
// This header file contains all declarations shared between driver and user
// applications.
//

#include <initguid.h>

//
// Device Interface GUID for OsrDio
//
// {CCF57245-9C4E-4C71-AC65-5217B37847D3}
DEFINE_GUID(GUID_DEVINTERFACE_OSRDIO, 
            0xccf57245, 0x9c4e, 0x4c71, 0xac, 0x65, 0x52, 0x17, 0xb3, 0x78, 0x47, 0xd3);

//
// The following value is arbitrarily chosen from the space defined by
// Microsoft as being "for non-Microsoft use" (0x8000 through 0xFFFF)
//
#define FILE_DEVICE_OSRDIO 0xD056

//
// Device control codes - Values between 2048 and 4095 arbitrarily chosen
//

//
// IOCTL_OSRDIO_READ
//
// Retrieves a bitmap of the current state of both the DIO input and output
// lines.
//
// Input Buffer:
//      (none)
//
// Output Buffer:
//
//      OSRDIO_READ_DATA structure. The CurrentLineState field contains a
//      bitmap indciating the current state of all the DIO lines. A 1 bit
//      indicates the corresponding line is ASSERTED, a 0 bit indicates
//      that the corresponding line is DEASSERTED. The state of both input
//      and output lines is returned by this IOCTL.
//
typedef struct _OSRDIO_READ_DATA {
    ULONG   CurrentLineState;
} OSRDIO_READ_DATA, *POSRDIO_READ_DATA;

#define IOCTL_OSRDIO_READ        CTL_CODE(FILE_DEVICE_OSRDIO, 2049, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// IOCTL_OSRDIO_WRITE
//
// Sets the state of lines that have been previously set to output. Lines that
// have not been previously set to output (using IOCTL_OSRDIO_SET_OUTPUTS) are
// ignored.
//
// Input Buffer:
//
//      OSRDIO_WRITE_DATA structure. The OutputLineState field contains
//      a bitmap indicating the desired state of the output lines.  A 1 bit
//      indicates that the corresponding line should be ASSERTED, a 0 indicates
//      the corresponding line should be DEASSERTED.
//
//      Current line state can be read with IOCTL_OSRDIO_READ.
//
// Output Buffer:
//      (none)
//
//
typedef struct OSRDIO_WRITE_DATA {
    ULONG   OutputLineState;
} OSRDIO_WRITE_DATA, *POSRDIO_WRITE_DATA;

#define IOCTL_OSRDIO_WRITE       CTL_CODE(FILE_DEVICE_OSRDIO, 2050, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// IOCTL_OSRDIO_SET_OUTPUTS
//
// Sets which lines are to be used for OUTPUT (sending) DIO signals.
//
// Input Buffer:
//
//      OSRDIO_SET_OUTPUTS_DATA structure. The OutputLines field contains
//      a bitmap indicating which lines are to be used for output (1 indicates
//      corresponding line is used for output). Lines not set for output are
//      implicitly set for use as input.
//
// Output Buffer:
//      (none)
//
typedef struct _OSRDIO_SET_OUTPUTS_DATA {
    ULONG   OutputLines;
} OSRDIO_SET_OUTPUTS_DATA, *POSRDIO_SET_OUTPUTS_DATA;

#define IOCTL_OSRDIO_SET_OUTPUTS CTL_CODE(FILE_DEVICE_OSRDIO, 2051, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// IOCTL_OSRDIO_WAITFOR_CHANGE
//
// Awaits a state change on one of the input lines. When one or more
// lines changes from DEASSSERTED to ASSERTED, the bitmask of all the line's
// states is returned.
//
// Input Buffer:
//      (none)
//
// Output Buffer:
//
//      OSRDIO_CHANGE_DATA structure. The LatchedInputLineState field contains a
//      bitmap indicating the state of all the DIO lines WHEN THE STATE
//      CHANGE OCCURRED. A 1 bit in the LatchedInputLineState field
//      indicates the corresponding line is ASSERTED, a 0 bit indicates that
//      the corresponding line is DEASSERTED. The state of both input and
//      output lines at the time of the state change is returned in the
//      LatchedInputLineState field of this IOCTL.
//
typedef struct _OSRDIO_CHANGE_DATA {
    ULONG   LatchedLineState;
} OSRDIO_CHANGE_DATA, *POSRDIO_COS_DATA;


#define IOCTL_OSRDIO_WAITFOR_CHANGE   CTL_CODE(FILE_DEVICE_OSRDIO, 2052, METHOD_BUFFERED, FILE_ANY_ACCESS)


