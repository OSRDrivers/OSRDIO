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
//    MODULE:
//
//        OsrDio.cpp -- OSR WDF example driver for the
//                      National Instruments PCIe-6509 Digital I/O Board.
//
//    AUTHOR(S):
//
//        OSR Open Systems Resources, Inc.
// 
//
//    Note about the design and implementation of this driver:
//      This driver was written specifically as an instructional sample.
//      The design goals for the driver are correctness, simplicity, and
//      clarity in implementation.  The driver strives to demonstrate "best
//      practices" in WDF driver development.
//
//      For the sake of simplicity of understanding and use in an teaching
//      and learning environment, the features and functions that the driver
//      implements are intentionally only a small subset of the functionality
//      that the NI PCIe-6509 supports. Those features that the driver
//      does support are (intended to be) implemented correctly.  However,
//      it's important to keep in mind that, even while they're correct, the
//      features we implement here are not intended to reflect what we'd do
//      in driver to support the NI PCIe-6509 in a production environment. As
//      just one example, this driver idles its device in low-power mode after
//      10 seconds of inactivity.  That would probably be a VERY bad idea in
//      a production DIO driver (given that any output lines that were aserted
//      would all be de-asserted when the device transitioned to a low power
//      state).
//
//    Notes on supported PCIe-6509 features:
//      Even though the NI PCIe-6509 supports 96 Digital I/O (DIO) lines, this
//      driver only supports the lowest 32 static DIO lines. Despite "best
//      practice" for this hardware being that DIO lines should be written in
//      blocks of 8 to reduce crosstalk, we always read/write all 32 lines
//      simulatneously.  We always monitor any input lines for any state
//      change (signal transitioning from low to high or high to low).  We
//      always set the digital filters to their max values (which will reject
//      transitions less than 2.54ms and accept transitions greater than 5.1ms).
//
//      If you have a PCIe-6509, probably the easiest way to test the driver is
//      to simply connect some of the output lines to the input lines... then
//      change the states of those output lines.  You should get state change
//      notifications of changes on the assocaited input lines.
//
//    About the use of C++ in this driver:
//      The WDF interface, and the basic Windows kernel mode interface, is
//      a C Language interface.  At OSR, we write ALL our drivers using a
//      very limited subset of C++ as a "better C."  Not only does it provide
//      strong type-checking, but it also provides several enhancements to
//      the language (such as constexpr, enum class, RAII patterns, and
//      range-based loops). While many C++ constructs (such as C++ native
//      exception handling) are NOT supported in Windows drivers, Windows
//      definitely (officially) supports the narrow subset of the language
//      that we we use in our code.
//
///////////////////////////////////////////////////////////////////////////////
#include "OsrDio.h"

//
//  DriverEntry
//
//    This routine is called by Windows when the driver is first loaded.  We
//    simply do our initialization, and create our an instance of a
//    WDFDRIVER object to establish communication between this driver and
//    the Framework.
//
//  INPUTS:
//
//      DriverObj       Address of the native WDM DRIVER_OBJECT that Windows
//                      created for this driver.
//
//      RegistryPath    UNICODE_STRING structure that represents this driver's
//                      key in the Registry ("HKLM\System\CCS\Services\OsrDio")
//
NTSTATUS
DriverEntry(PDRIVER_OBJECT  DriverObj,
            PUNICODE_STRING RegistryPath)
{
    NTSTATUS          status;
    WDF_DRIVER_CONFIG config;

    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

#if OSR_FIX_ZERO_BUG_ON_1909

    //
    // Compensate for an error in the 2004 WDK's version of
    // ExInitializeDriverRuntime that mistakenly thinks that
    // Windows 1909 (19H2) is build number 18362 (it is actually
    // 18363).  This error leads to pool allocations NOT being
    // zeroed on Windows 1909.
    //
    RTL_OSVERSIONINFOW versionInfo;

    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);

    RtlZeroMemory(&versionInfo, sizeof(versionInfo));

    (void)RtlGetVersion(&versionInfo);

    if(versionInfo.dwBuildNumber <= 18363) {

        if(ExPoolZeroingNativelySupported == TRUE) {
            ExPoolZeroingNativelySupported = FALSE;
        }

    }

#endif

#if DBG
    DbgPrint("\nOsrDio Driver V1.0 -- Compiled %s %s\n",
             __DATE__,
             __TIME__);
#endif

    //
    // Initialize the Driver Config structure:
    //      Specify our Add Device event callback.
    //
    WDF_DRIVER_CONFIG_INIT(&config,
                           OsrDioEvtDriverDeviceAdd);

    //
    // Create our WDFDRIVER object
    //
    // We specify no object attributes, because we do not need a cleanup or
    // or destroy event callback, or any per-driver context.
    //
    status = WdfDriverCreate(DriverObj,
                             RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &config,
                             WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {

#if DBG
        DbgPrint("WdfDriverCreate failed with status 0x%0x\n",
                 status);
#endif
    }

#if DBG
    DbgPrint("DriverEntry: Leaving\n");
#endif

    return status;
}

//
// OsrDioEvtDriverDeviceAdd
//
//  This is the event processing callback that WDF calls when an instance of
//  of a device is found that our driver supports.
//
//  The main job of this callback is to create a WDFDEVICE object instance that
//  represents the device that has been found and make that device accessible
//  to the user.  We also create any necessary Queues here (to allow us to
//  receive and process Requests) and define attributes of our power management
//  policy.
//
//  INPUTS:
//
//    Driver        Handle to the WDFDRIVER Oject created in DriverEntry
//
//    DeviceInit    Pointer to a framework-allocated WDFDEVICE_INIT structure
//                  the servces as the "initializer" structure for the
//                  WDFDEVICE we'll be creating.
NTSTATUS
OsrDioEvtDriverDeviceAdd(WDFDRIVER       Driver,
                         PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS                              status;
    WDF_PNPPOWER_EVENT_CALLBACKS          pnpPowerCallbacks;
    WDF_OBJECT_ATTRIBUTES                 objAttributes;
    WDFDEVICE                             device;
    POSRDIO_DEVICE_CONTEXT                devContext;
    WDF_IO_QUEUE_CONFIG                   queueConfig;
    WDF_INTERRUPT_CONFIG                  interruptConfig;
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;

#pragma warning(suppress: 26485)   // "No array to pointer decay"
    DECLARE_CONST_UNICODE_STRING(dosDeviceName,
                                 LR"(\DosDevices\OSRDIO)");

    UNREFERENCED_PARAMETER(Driver);

    //
    // Our first task is to instantiate a WDFDEVICE Object
    //

    //
    // Specify the Object Attributes for our WDFDEVICE
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objAttributes);

    //
    // Associate our device context structure type with our WDFDEVICE
    //
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objAttributes,
                                           OSRDIO_DEVICE_CONTEXT);

    //
    // Specify object-specific configuration. We want to specify PnP/Power
    // Callbacks to manage our hardware resources. This is done using the
    // "collector structure" WDF_PNPPOWER_EVENT_CALLBACKS
    //

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

    //
    // Prepare Hardware is called to give us our hardware resources
    // Release Hardware is called at when we need to return hardware resources
    //
    pnpPowerCallbacks.EvtDevicePrepareHardware = OsrDioEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = OsrDioEvtDeviceReleaseHardware;

    //
    // These two callbacks set up and tear down hardware state that must be
    // done every time the device moves in and out of the D0-Working state.
    //
    pnpPowerCallbacks.EvtDeviceD0Entry = OsrDioEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit  = OsrDioEvtDeviceD0Exit;

    //
    // Copy the contents of the PnP/Power Callbacks "collector structure" to
    // our WDFDEVICE_INIT structure (which is the object-specific configurator
    // for WDFDEVICE).
    //
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit,
                                           &pnpPowerCallbacks);

    //
    // And now instantiate the WDFDEVICE Object.
    //
    status = WdfDeviceCreate(&DeviceInit,
                             &objAttributes,
                             &device);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceInitialize failed 0x%0x\n",
                 status);
#endif
        goto done;
    }

    //
    // WDFDEVICE Object creation is complete
    //

    //
    // Create a symbolic link to our WDFDEVICE so users can open
    // the device by NAME.  Note we don't attach any unit number to this
    // device name, so this driver only supports one device.
    //
    status = WdfDeviceCreateSymbolicLink(device,
                                         &dosDeviceName);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreateSymbolicLink failed 0x%0x\n",
                 status);
#endif
        goto done;
    }

    //
    // And make our device accessible via a Device Interface Class GUID.
    // User-mode users would call CM_Get_Device_Interface_List to get a
    // list of OSRDIO devices by specifying GUID_DEVINTERFACE_OSRDIO.
    // Optionally (user mode AND kernel mode) users can also register to be
    // notified of the arrival/departure of this device (for user mode, see
    // CM_Register_Notification).
    //
    status = WdfDeviceCreateDeviceInterface(device,
                                            &GUID_DEVINTERFACE_OSRDIO,
                                            nullptr);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreateDeviceInterface failed 0x%0x\n",
                 status);
#endif
        goto done;
    }

    //
    // Get a pointer to our device context, using the accessor function
    // we have defined (in OsrDio.h)
    //
    devContext = OsrDioGetContextFromDevice(device);

    devContext->WdfDevice = device;

    //
    // Configure a queue to handle incoming requests
    //
    // We use a single, default, queue for receiving Requests, and we only
    // support IRP_MJ_DEVICE_CONTROL.
    //

    //
    // We don't have Object Attributes for our Queue that we need to specify
    //

    //
    // With Sequential Dispatching, we will only get one request at a time
    // from our Queue.
    // 
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
                                           WdfIoQueueDispatchSequential);

    queueConfig.EvtIoDeviceControl = OsrDioEvtIoDeviceControl;

    status = WdfIoQueueCreate(device,
                              &queueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfIoQueueCreate for default queue failed 0x%0x\n",
                 status);
#endif
        goto done;
    }

    //
    // We also create a manual Queue to hold Requests that are waiting for
    // a state change to happen on one of the input lines.
    //
    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig,
                             WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(devContext->WdfDevice,
                              &queueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              &devContext->PendingQueue);

    if (!NT_SUCCESS(status)) {

        DbgPrint("WdfIoQueueCreate for Rx Queue failed 0x%0x\n",
                 status);

        goto done;
    }

    //
    // Create an interrupt object that will later be associated with the
    // device's interrupt resource and connected by the Framework to our ISR.
    //

    //
    // Again, we don't need any Object Attributes for this Object... so we
    // move directly to Object-specific configuration
    //

    //
    // Configure the Interrupt object
    //
    WDF_INTERRUPT_CONFIG_INIT(&interruptConfig,
                              OsrDioEvtInterruptIsr,
                              OsrDioEvtInterruptDpc);

    interruptConfig.EvtInterruptEnable  = OsrDioEvtInterruptEnable;
    interruptConfig.EvtInterruptDisable = OsrDioEvtInterruptDisable;

    status = WdfInterruptCreate(device,
                                &interruptConfig,
                                WDF_NO_OBJECT_ATTRIBUTES,
                                &devContext->WdfInterrupt);
    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfInterruptCreate failed 0x%0x\n",
                 status);
#endif
        goto done;
    }

    //
    // Initialize our idle policy
    //
    // We accept most of the defaults here. Our device will idle in D3, and
    // WDF will create a property sheet for Device Manager that will allow
    // admin users to specify whether our device should idle in low-power
    // state.
    //
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings,
                                               IdleCannotWakeFromS0);

    //
    // After 10 seconds of no activity, declare our device idle
    // Note that "idle" in this context means that the driver does not have
    // any Requests in progress.  So, while we have a Request on the
    // PendingQueue (waiting to be informed of a line state change), WDF
    // will *not* idle the device. Recall that a device can always be
    // made to enter into, and remain in, D0-Working by calling
    // WdfDeviceStopIdle.
    //
    idleSettings.IdleTimeout = 10 * 1000;

    status = WdfDeviceAssignS0IdleSettings(device,
                                           &idleSettings);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceAssignS0IdleSettings failed 0x%0x\n",
                 status);
#endif
        goto done;
    }

done:

    return status;
}

//
// OsrDioEvtDevicePrepareHardware
//
// This entry point is called when hardware resources are assigned to one of
// our devices.
//
// INPUTS:
//      Device                  Handle to our WDFDEVICE object
//      RawResources            WDFCMRESLIST of hardware resources assigned
//                              to our device.
//      TranslatedResources     A WDFCMRESLIST of hardware resources
//                              assigned to our device, made directly usable
//                              by the driver.  We expect one memory resource
//                              and one interrupt resources.
//
// We almost never use the Raw Resources (as these are of primary interest to
// bus drivers).  Here, we only reference our Translated Resources.
//
NTSTATUS
OsrDioEvtDevicePrepareHardware(WDFDEVICE    Device,
                               WDFCMRESLIST RawResources,
                               WDFCMRESLIST TranslatedResources)
{
    POSRDIO_DEVICE_CONTEXT          devContext;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceTrans;
    NTSTATUS                        status;
    BOOLEAN                         interruptFound = FALSE;
#if DBG
    DbgPrint("EvtPrepareHardware...\n");
#endif

    UNREFERENCED_PARAMETER(RawResources);

    devContext = OsrDioGetContextFromDevice(Device);

#if DBG

    DioUtilDisplayResources(RawResources,
                            TranslatedResources);
#endif

    for (ULONG i = 0; i < WdfCmResourceListGetCount(TranslatedResources); i++) {

        //
        // Get the i'th partial resource descriptor from the list
        //
        resourceTrans = WdfCmResourceListGetDescriptor(TranslatedResources,
                                                       i);

        //
        // Let's examine and store the resources, based on their type
        //
        switch (resourceTrans->Type) {

            case CmResourceTypeMemory: {

                //
                // We identify the correct BAR by its expected size.
                //
                if (resourceTrans->u.Memory.Length == DIO_BAR_SIZE) {

                    ASSERT(devContext->DevBase == nullptr);
#if DBG
                    DbgPrint("Found expected BAR\n");
#endif
                    //
                    // Map the device's registers into kernel mode virtual
                    // address space
                    //
                    devContext->DevBase = static_cast<PDIO_REGISTERS>(
                                    MmMapIoSpaceEx(resourceTrans->u.Memory.Start,
                                                   resourceTrans->u.Memory.Length,
                                                   PAGE_READWRITE));

                    if (devContext->DevBase == nullptr) {

#if DBG
                        DbgPrint("****MapIoSpace for resource %lu FAILED!\n",
                                 i);
#endif
                        devContext->MappedLength = 0;

                    } else {

                        devContext->MappedLength = resourceTrans->u.Memory.Length;

#if DBG
                        DbgPrint("Mapped BAR to KVA 0x%p\n",
                                 devContext->DevBase);

                        DbgPrint("Mapped length = %lu\n",
                                 devContext->MappedLength);
#endif

                    }

                } else {

#if DBG
                    DbgPrint("(not interested in this resource)\n");
#endif
                }

                break;

            }

            case CmResourceTypeInterrupt: {

                ASSERT(interruptFound == FALSE);

                //
                // Because our devices supports only one interrupt, and we
                // create our WDFINTERRUPT Object in our EvtDriverDeviceAdd
                // Event Processing Callback, we don't have to do anything
                // here.  WDF will automatically connect our (one) Interrupt
                // Service Routine to our (one) device interrupt.
                //
                interruptFound = TRUE;
#if DBG
                DbgPrint("Interrupt found\n");
#endif
                break;
            }


            default:

                //
                // This could be any other type of resources, including
                // device-private type added by the PCI bus driver.  We must
                // allow for device-private resources and we must not change
                // them.
                //
#if DBG
                DbgPrint("Resource %lu: Unhandled resource type 0x%0x\n",
                         i,
                         resourceTrans->Type);
#endif
                break;

        }   // end of switch

    }  // end of for

    //
    // If we got both resources that we're expecting
    //
    if (devContext->DevBase == nullptr || !interruptFound) {

#if DBG
        DbgPrint("****** Expected resources NOT FOUND\n");
        DbgPrint("****** Returning error from PrepareHardware\n");
#endif
        status = STATUS_PNP_DRIVER_CONFIGURATION_NOT_FOUND;

        goto done;
    }

    //
    // We initialize all Digital I/O lines as inputs.
    // That means there are NO lines are curerntly set to be used for
    // output.
    //
    devContext->OutputLineMask = 0;

    //
    // And when we power-on, we want the output lines to initially
    // all be DE-ASSERTED
    //
    devContext->SavedOutputLineState = 0;

    //
    // Put the device is a known state, with all interrupts disabled
    //
    DioUtilDeviceReset(devContext);

    status = STATUS_SUCCESS;

done:

    return status;
}

//
// OsrDioEvtDeviceReleaseHardware
//
// This function is called any time Windows wants us to release our
// hardware resources.  Examples include "bus rebalancing" and when
// the "Disable Device" function is selected in Device Manager.
// This callback IS NOT CALLED during system shutdown.
//
// INPUTS:
//  Device              Handle to our WDFDEVICE
//  ResourcesTranslated The resources we're returning
//
NTSTATUS
OsrDioEvtDeviceReleaseHardware(WDFDEVICE    Device,
                               WDFCMRESLIST ResourcesTranslated)
{
    POSRDIO_DEVICE_CONTEXT devContext;

#if DBG
    DbgPrint("EvtReleaseHardware...\n");
#endif

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    devContext = OsrDioGetContextFromDevice(Device);

    if (devContext->DevBase != nullptr) {

        MmUnmapIoSpace(devContext->DevBase,
                       devContext->MappedLength);

        devContext->DevBase      = nullptr;
        devContext->MappedLength = 0;

    }

    //
    // Note that we don't have to do anything in this function to disconnect
    // or "return" our interrupt resource. WDF will automatically disconect
    // our ISR from any interrupts.  Also, interrupts from the device have
    // already been disabled at this point, because EvtDeviceInterruptDisable
    // was called before this callback. 
    //

    return STATUS_SUCCESS;
}


//
// OsrDioEvtDeviceD0Entry
//
// This function is called each time our device has been transitioned into
// the D0-Working (fully powered on) state. This includes during the
// "implicit power on" that occurs after the device is first discovered.
// Our job here is to initialize or restore the state of our device.
//
// The device is already in D0 when this function is called.
//
// INPUTS:
//  Device          Handle to our WDFDEVICE
//  PreviousState   The state from which we transitioned to D0
//
NTSTATUS
#pragma warning(suppress: 26812)  // "Prefer 'enum class' over 'enum'"
OsrDioEvtDeviceD0Entry(WDFDEVICE              Device,
                       WDF_POWER_DEVICE_STATE PreviousState)
{
    POSRDIO_DEVICE_CONTEXT devContext;

#if DBG
    DbgPrint("D0Entry...\n");
#endif

    UNREFERENCED_PARAMETER(PreviousState);

    devContext = OsrDioGetContextFromDevice(Device);

#if DBG
    DbgPrint("Restoring Output Line state = 0x%08x\n",
             devContext->SavedOutputLineState);
#endif
    WRITE_REGISTER_ULONG(&devContext->DevBase->Static_Digital_Output_Register,
                         devContext->SavedOutputLineState);

    return STATUS_SUCCESS;
}

//
// OsrDioEvtDeviceD0Exit
//
// This function is called when our device is about to transition OUT of D0.
// The target state is passed as an argument.  Our job here is to save any
// state associated with the device, so it can be restored when power is
// returned to the device.
//
// The device is still in D0 when this function is called.
//
// INPUTS:
//  Device          Handle to our WDFDEVICE
//  TagetState  The state to which we're transitioning, from D0
//
NTSTATUS
OsrDioEvtDeviceD0Exit(WDFDEVICE              Device,
                      WDF_POWER_DEVICE_STATE TargetState)
{
    POSRDIO_DEVICE_CONTEXT devContext;
    ULONG                  outputLineState;

#if DBG
    DbgPrint("D0Exit...\n");
#endif

    UNREFERENCED_PARAMETER(TargetState);

    devContext = OsrDioGetContextFromDevice(Device);

    outputLineState =
        READ_REGISTER_ULONG(&devContext->DevBase->Static_Digital_Input_Register);

    outputLineState &= devContext->OutputLineMask;

    devContext->SavedOutputLineState = outputLineState;

#if DBG
    DbgPrint("Saved Output Line state = 0x%08x\n",
             devContext->SavedOutputLineState);
#endif

    return STATUS_SUCCESS;
}

//
// OsrDioEvtInterruptEnable
//
// Called by WDF to ask us to enable hardware interrupts on our device.
//
// INPUTS:
//  Interrupt           Handle to our WDFINTERRUPT object
//  Device              the usual
//
NTSTATUS
OsrDioEvtInterruptEnable(WDFINTERRUPT Interrupt,
                         WDFDEVICE    Device)
{
    POSRDIO_DEVICE_CONTEXT devContext;

#if DBG
    DbgPrint("EvtInterruptEnable\n");
#endif

    UNREFERENCED_PARAMETER(Interrupt);

    devContext = OsrDioGetContextFromDevice(Device);

    //
    // Set the device's interrupt logic to a known state, ACK'ing any
    // outstanding interrupts and ensuring no interrupts are enabled.
    //
    DioUtilResetDeviceInterrupts(devContext);

    //
    // And enable interrupts from the Digital Inputs, from State Changes,
    // and from the card to the host.
    //
    DioUtilEnableDeviceInterrupts(devContext);

    //
    // Tell the device that we're interested in getting an interrupt when
    // the state of any of the input lines changes.
    //
    DioUtilProgramLineDirectionAndChangeMasks(devContext);

    return STATUS_SUCCESS;
}

//
// OsrDioEvtInterruptDisable
//
// Called by WDF to ask us to DISABLE interrupt on our device.
//
// INPUTS:
//  Interrupt       Handle to our WDFINTERRUPT object
//  Device          the usual
//
NTSTATUS
OsrDioEvtInterruptDisable(WDFINTERRUPT Interrupt,
                          WDFDEVICE    Device)
{
    POSRDIO_DEVICE_CONTEXT devContext;

#if DBG
    DbgPrint("EvtInterruptDisable \n");
#endif

    UNREFERENCED_PARAMETER(Interrupt);

    devContext = OsrDioGetContextFromDevice(Device);

    //
    // ACK and disable any pending interrupts
    //
    DioUtilResetDeviceInterrupts(devContext);

    return STATUS_SUCCESS;
}

//
// OsrDioEvtIoDeviceControl
//
// Process an device control (IRP_MJ_DEVICE_CONTROL).
//
// INPUTS:
//  Queue        The queue from which the request is being dispatched
//  Request      The WDFREQUEST that describes this I/O request
//  OutputBufferLength, InputBufferLength, and IoControlCode
//
// NOTES -- Queuing Model
//
// WDF calls us at this entry point when we have a device control to process.
// Note that back in OsrDioEvtDevceAdd, when we created and initialized our
// default Queue, we set the queue dispatch type to be SEQUENTIAL.  This means
// that WDF will send our driver ONE REQUEST AT A TIME from this Queue, and
// will not call us with another request until we're "done" processing the
// current Request.
//
// What's interesting is that this does NOT imply that we must complete
// every Request synchronously (that is, in its EvtIoxxx callback).  Look at
// the code for supporting IOCTL_OSRDIO_WAITFOR_CHANGE and you'll see that
// instead of completing this Request we forward it to a manual Queue and then
// return with that Request in progress.  This serial model makes things very
// easy for us and there's very little synchronization required.
//
VOID
OsrDioEvtIoDeviceControl(WDFQUEUE   Queue,
                         WDFREQUEST Request,
                         size_t     OutputBufferLength,
                         size_t     InputBufferLength,
                         ULONG      IoControlCode)
{
    POSRDIO_DEVICE_CONTEXT devContext;
    NTSTATUS               status;
    ULONG                  bytesReadorWritten;

#if DBG
    DbgPrint("OsrDioEvtIoDeviceControl\n");
#endif

    UNREFERENCED_PARAMETER(InputBufferLength);

    //
    // Get a pointer to our WDFDEVICE Context
    //
    devContext = OsrDioGetContextFromDevice(WdfIoQueueGetDevice(Queue));

    //
    // Switch based on the control code specified by the user when they
    // issued the DeviceIoControl function call:
    //
    switch (IoControlCode) {


        case IOCTL_OSRDIO_READ: {
            POSRDIO_READ_DATA readBuffer;
#if DBG
            DbgPrint("Ioctl: IOCTL_OSRDIO_READ\n");
#endif
            status = WdfRequestRetrieveOutputBuffer(Request,
                                                    sizeof(OSRDIO_READ_DATA),
                                                    (PVOID*)&readBuffer,
                                                    nullptr);

            if (!NT_SUCCESS(status)) {

#if DBG
                DbgPrint("Error retrieving inBuffer 0x%0x\n",
                         status);
#endif
                bytesReadorWritten = 0;

                goto done;
            }

            //
            // Get the current line state from the device and return it in the
            // user's output buffer.
            //
            readBuffer->CurrentLineState =
                READ_REGISTER_ULONG(&devContext->DevBase->Static_Digital_Input_Register);

            status             = STATUS_SUCCESS;
            bytesReadorWritten = sizeof(OSRDIO_READ_DATA);

            break;
        }

        case IOCTL_OSRDIO_WRITE: {

            POSRDIO_WRITE_DATA writeBuffer;
            ULONG              linesToAssert;
#if DBG
            DbgPrint("Ioctl: IOCTL_OSRDIO_WRITE\n");
#endif
            //
            // We can't write anything if there are no lines set to output
            //
            if (devContext->OutputLineMask == 0) {

#if DBG
                DbgPrint("ERROR! Write with output line mask set to zero\n");
#endif
                //
                // STATUS_INVALID_DEVICE_STATE becomes ERROR_BAD_COMMAND in
                // Win32.
                //
                status = STATUS_INVALID_DEVICE_STATE;
                bytesReadorWritten = 0;

                goto done;
            }

            status = WdfRequestRetrieveInputBuffer(Request,
                                                   sizeof(OSRDIO_WRITE_DATA),
                                                   (PVOID*)&writeBuffer,
                                                   nullptr);

            if (!NT_SUCCESS(status)) {

#if DBG
                DbgPrint("Error retrieving inBuffer 0x%08lx\n",
                         status);
#endif

                bytesReadorWritten = 0;

                goto done;
            }

            //
            // Get the bitmask of lines the user wants to assert
            //
            linesToAssert = writeBuffer->OutputLineState;

            //
            // Only allow the user to set to "1" those lines that have
            // previously been set as outline lines
            //
            linesToAssert &= devContext->OutputLineMask;

            WRITE_REGISTER_ULONG(&devContext->DevBase->Static_Digital_Output_Register,
                                 linesToAssert);

            status             = STATUS_SUCCESS;
            bytesReadorWritten = sizeof(OSRDIO_WRITE_DATA);

            break;
        }

        case IOCTL_OSRDIO_SET_OUTPUTS: {
            POSRDIO_SET_OUTPUTS_DATA outputsBuffer;
#if DBG
            DbgPrint("Ioctl: IOCTL_OSRDIO_SET_OUTPUTS\n");
#endif
            status = WdfRequestRetrieveInputBuffer(Request,
                                                   sizeof(OSRDIO_SET_OUTPUTS_DATA),
                                                   (PVOID*)&outputsBuffer,
                                                   nullptr);

            if (!NT_SUCCESS(status)) {

#if DBG
                DbgPrint("Error retrieving inBuffer 0x%08lx\n",
                         status);
#endif

                bytesReadorWritten = 0;

                goto done;
            }

            //
            // Get the mask of lines that the user wants to set to Output
            //
            devContext->OutputLineMask = outputsBuffer->OutputLines;

            //
            // Program the output mask on the device (and enable related
            // state-change interrupts)
            //
            DioUtilProgramLineDirectionAndChangeMasks(devContext);

            status             = STATUS_SUCCESS;
            bytesReadorWritten = sizeof(OSRDIO_SET_OUTPUTS_DATA);

            break;
        }


        case IOCTL_OSRDIO_WAITFOR_CHANGE: {
#if DBG
            DbgPrint("Ioctl: IOCTL_OSRDIO_WAITFOR_CHANGE\n");
#endif
            //
            // Before doing anything... Be sure some lines are
            // set for input that we could wait to see a change on.
            //
            if ((~devContext->OutputLineMask) == 0) {

#if DBG
                DbgPrint("ERROR!  No lines set to inputs. Can't wait for change\n");
#endif
                //
                // ALL the lines are set to OUTPUT... So, there are no lines
                // set to input that we can wait to change.
                //
                // STATUS_NONE_MAPPED becomes ERROR_NONE_MAPPED in Win32
                //
                status             = STATUS_NONE_MAPPED;
                bytesReadorWritten = 0;

                goto done;
            }

            //
            // Check to see if the buffer passed in is what we expect
            //
            if (OutputBufferLength < sizeof(OSRDIO_CHANGE_DATA)) {

#if DBG
                DbgPrint("ERROR! Invalid output buffer size on WAITFOR\n");
#endif
                //
                // STATUS_INVALID_BUFFER_SIZE becomes ERROR_INVALID_USER_BUFFER
                // in Win32.
                //
                status             = STATUS_INVALID_BUFFER_SIZE;
                bytesReadorWritten = 0;

                goto done;
            }

#if DBG
            DbgPrint("Queueing Request %p, waiting for state change\n",
                     Request);
#endif

            //
            // Forward the Request to the PendingQueue, where it'll wait for
            // the Change Of State interrupt.
            //
            status = WdfRequestForwardToIoQueue(Request,
                                                devContext->PendingQueue);

            if (!NT_SUCCESS(status)) {

                //
                // Odd... forwarding the Request to the PendingQueue failed.
                // Return that error to our caller.
                //
                bytesReadorWritten = 0;

                goto done;
            }

            //
            // Request has been successfully forwarded to the PendingQueue.
            // We now return WITH THAT REQUEST IN PROGRESS.  We'll complete
            // it later, in our DpcForIsr, after a state change triggers an
            // interrupt and our ISR queues a callback to our DpcForIsr.
            //
            goto doneDoNotComplete;
        }

        default: {
#if DBG
            DbgPrint("Received IOCTL 0x%x\n",
                     IoControlCode);
#endif
            //
            // STATUS_INVALID_PARAMETER becomes ERROR_INVALID_PARAMETER
            // in Win32
            //
            status             = STATUS_INVALID_PARAMETER;
            bytesReadorWritten = 0;

            break;
        }
    }

done:

    WdfRequestCompleteWithInformation(Request,
                                      status,
                                      bytesReadorWritten);
doneDoNotComplete:

    return;
}

//
// OsrDioEvtInterruptIsr
//
// This is our driver's interrupt service routine.
//
// INPUTS:
//  Interrupt       Handle to our WDFINTERRUPT object describing this Interrupt
//
//  MessageId       The zero-based message number of the MSI/MSI-x message
//                  we're processing.  This device only has one MSI, so there's
//                  no need to check the message ID that's passed to us.
//
BOOLEAN
OsrDioEvtInterruptIsr(WDFINTERRUPT Interrupt,
                      ULONG        MessageID)
{
    POSRDIO_DEVICE_CONTEXT devContext;
    ULONG                  interruptStatus;
    ULONG                  lineState;
    BOOLEAN                returnValue;
    ULONG                  changeDetectReg;

#if DBG
    DbgPrint("ISR...\n");
#endif

    UNREFERENCED_PARAMETER(MessageID);

    devContext = OsrDioGetContextFromDevice(WdfInterruptGetDevice(Interrupt));

    //
    // Get the pending interrupt status
    //
    // If an interrupt is being request from the device to the host, this
    // will also acknowledge (and clear) that interrupt
    //
    interruptStatus =
        READ_REGISTER_ULONG(&devContext->DevBase->Volatile_Interrupt_Status_Register);

#if DBG
    DbgPrint("IntStatus = 0x%08x\n",
             interruptStatus);
#endif

    //
    // Is there an interrupt pending from this device?
    //
    if ((interruptStatus & Vol_Int) == 0) {

        //
        // Our device DID NOT cause this interrupt.  We therefore will
        // return FALSE to the Windows Interrupt Dispatcher
        //
        returnValue = FALSE;

        DbgPrint("Not our interrupt\n");

        //
        // The interrupt was not caused by our device
        //
        goto done;
    }

    //
    // Our device DID cause this interrupt, so we will return TRUE to the
    // Windows Interrupt Dispatcher
    //
    returnValue = TRUE;

    //
    // So... Our device interrupted.  Find out why.
    //
    // Is the interrupt because a Digital Input line state change was detected?
    //
    changeDetectReg =
        READ_REGISTER_ULONG(&devContext->DevBase->ChangeDetectStatusRegister);

    if (((changeDetectReg & ChangeDetectStatus) == 1) &&
        ((changeDetectReg & ChangeDetectError) == 0)) {

        //
        // Yes... the state of one of the Digital Input lines has changed,
        // AND the ERROR bit is not set.  So, we will notify the user, if
        // they have asked to be notified.
        //
#if DBG
        DbgPrint("ChangeDetectReg: Line state change SET and NO ERROR\n");
#endif

        //
        // Read the latched state of the DIO lines at the change
        //
        lineState =
            READ_REGISTER_ULONG(&devContext->DevBase->DI_ChangeDetectLatched_Register);

#if DBG
        DbgPrint("Line state latched on change = 0x%08x\n",
                 lineState);
#endif

        //
        // Save the state of the lines at change, for returning to the
        // user
        //
        devContext->LatchedInputLineState = lineState;

        //
        // Queue a DpcForIsr to return the data to the user and notify
        // them of this state change
        //
        WdfInterruptQueueDpcForIsr(Interrupt);
    }

    //
    // Acknowledge (and clear) the condition that caused the interrupt.
    // Doing this "resets" the Digital Input state change logic, and will
    // cause it to recognize new state changes.
    //

    if (changeDetectReg & ChangeDetectStatus) {
#if DBG
        DbgPrint("ACK'ing change detect\n");
#endif
        //
        // Acknowledge the state change on the Digital Input lines
        //
        WRITE_REGISTER_ULONG(&devContext->DevBase->ChangeDetectIRQ_Register,
                             ChangeDetectIRQ_Acknowledge);
    }

    //
    // If there was an error on the Digital Input lines, this would also
    // cause an interrupt. If there IS an error, ACK and clear that error
    // (so we will get notification of subsequent line state changes)
    //
    if (changeDetectReg & ChangeDetectError) {

#if DBG
        DbgPrint("ACK'ing change detect ERROR\n");
#endif
        WRITE_REGISTER_ULONG(&devContext->DevBase->ChangeDetectIRQ_Register,
                             ChangeDetectErrorIRQ_Acknowledge);
    }

done:
    return returnValue;
}

//
// OsrDioEvtInterruptDpc
//
// This is our DpcForIsr function, where we complete any processing that was
// started in our ISR.
//
// INPUTS:
//  Interrupt       WDFINTERRUPT related to this call
//  Device          WDFDEVICE handle
//
VOID
OsrDioEvtInterruptDpc(WDFINTERRUPT Interrupt,
                      WDFOBJECT    Device)
{
    POSRDIO_DEVICE_CONTEXT devContext;
    WDFREQUEST             waitingRequest;
    POSRDIO_COS_DATA       changeDataToReturn;
    NTSTATUS               status;
    ULONG_PTR              bytesReturned;

    UNREFERENCED_PARAMETER(Interrupt);

#if DBG
    DbgPrint("DPC for ISR...\n");
#endif

    devContext = OsrDioGetContextFromDevice(Device);

    //
    // IF there's a IOCTL_OSRDIO_WAITFOR_CHANGE Request that's pending,
    // get a handle to it from the Queue where we stored it earlier.
    //
    status = WdfIoQueueRetrieveNextRequest(devContext->PendingQueue,
                                           &waitingRequest);
    //
    // If there's no Requests waiting to be notified of the state change
    // (or if there was some other odd error) just leave the DpcForIsr.
    //
    if (!NT_SUCCESS(status)) {

#if DBG
        DbgPrint("RetrieveNextRequest failed.  Status = 0x%0x\n",
                 status);

        DbgPrint("Leaving DPC\n");
#endif
        goto done;
    }

    ASSERT(waitingRequest != nullptr);

    //
    // Get the requestor's output buffer, so we can return the state of the
    // Digital Input lines.
    //
    status = WdfRequestRetrieveOutputBuffer(waitingRequest,
                                            sizeof(OSRDIO_CHANGE_DATA),
                                            (PVOID*)&changeDataToReturn,
                                            nullptr);
    if (NT_SUCCESS(status)) {

        //
        // Return the data to the user
        //
        changeDataToReturn->LatchedLineState = devContext->LatchedInputLineState;

#if DBG
        DbgPrint("Completing Request %p: Returning latched line state = 0x%08lx\n",
                 waitingRequest,
                 changeDataToReturn->LatchedLineState);
#endif

        status        = STATUS_SUCCESS;
        bytesReturned = sizeof(OSRDIO_CHANGE_DATA);

    } else {

        //
        // We'll return whatever status WdfRequestRetrieveOutputBuffer returned
        // and zero bytes of data.
        //
        bytesReturned = 0;
    }


    WdfRequestCompleteWithInformation(waitingRequest,
                                      status,
                                      bytesReturned);

done:

    return;
}

//
// Utility Routines
//
// We would ordinarily locate these in a separate module from the mainline
// driver code. But given that this example driver is so short, it seems
// more convenient to just put there here.
//


//
// DioUtilProgramLineDirectionAndChangeMasks
//
// Set the line directions (indicating which lines are used for input and
// which for output) as well as the digtal filters for the input lines.  Also
// program the device to interrupt whenever the state of one of the
// input lines changes (in either direction).
//
_Use_decl_annotations_
VOID
DioUtilProgramLineDirectionAndChangeMasks(POSRDIO_DEVICE_CONTEXT DevContext)
{
#if DBG
    DbgPrint("DioUtilProgramLineDirectionAndChangeMasks...\n");
#endif

    //
    // Set digital filters on the input lines to maximum filtering, to
    // eliminate noise-related artifacts from showing-up on input lines
    // during state changes.
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->DI_FilterRegister_Port0and1,
                         Filter_Large_All_Lines);

    WRITE_REGISTER_ULONG(&DevContext->DevBase->DI_FilterRegister_Port2and3,
                         Filter_Large_All_Lines);

    //
    // Tell the device which lines are Digital Inputs and which are Digital
    // Outputs
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->DIO_Direction_Register,
                         DevContext->OutputLineMask);

    //
    // Having set the OUTPUT lines, set the remaining lines (which are
    // INPUT lines) to detect state changes
    //

    //
    // Enable "rising edge" state change interrupts
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->DI_ChangeIrqRE_Register,
                         (~DevContext->OutputLineMask));

    //
    // Enable "falling edge" state change interrupts
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->DI_ChangeIrqFE_Register,
                         (~DevContext->OutputLineMask));
}

//
// DioUtilResetDeviceInterrupts
//
// ACKs, clears, and leave DISabled all device interrupts
//
_Use_decl_annotations_
VOID
DioUtilResetDeviceInterrupts(POSRDIO_DEVICE_CONTEXT DevContext)
{
#if DBG
    DbgPrint("DioUtilResetDeviceInterrupts...\n");
#endif

    //
    // Software reset the device
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->Joint_Reset_Register,
                         Software_Reset);

    //
    // Disable and acknowledge all interrupts (per NI Spec, section 2)
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->Interrupt_Mask_Register,
                         (Clear_CPU_Int | Clear_STC3_Int));

    WRITE_REGISTER_ULONG(&DevContext->DevBase->GlobalInterruptEnable_Register,
                         (DI_Interrupt_Disable |
                          WatchdogTimer_Interrupt_Disable));

    WRITE_REGISTER_ULONG(&DevContext->DevBase->ChangeDetectIRQ_Register,
                         (ChangeDetectIRQ_Acknowledge |
                          ChangeDetectIRQ_Disable |
                          ChangeDetectErrorIRQ_Acknowledge |
                          ChangeDetectErrorIRQ_Disable));
}

//
// DioUtilEnableDeviceInterrupts
//
// Enables the Digital Inputs to interrupt the device's interrupt controller,
// and the device's interrupt controller to interrupt the host.
//
_Use_decl_annotations_
VOID
DioUtilEnableDeviceInterrupts(POSRDIO_DEVICE_CONTEXT DevContext)
{
#if DBG
    DbgPrint("DioUtilEnableDeviceInterrupts...\n");
#endif

    //
    // Enable interrupts from the Digital Inputs 
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->GlobalInterruptEnable_Register,
                         DI_Interrupt_Enable);

    //
    // And enable interrupts as a result of state changes on the Digital Input
    // lines
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->ChangeDetectIRQ_Register,
                         (ChangeDetectErrorIRQ_Enable |
                          ChangeDetectIRQ_Enable));

    //
    // Enable interrupts from the device to the host
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->Interrupt_Mask_Register,
                         (Set_CPU_Int | Set_STC3_Int));
}

//
// DioUtilDeviceReset
//
// Puts the device in a known, pristine, condition... ready to accept user
// commands.  All previous settings on the device are lost/reset.
//
_Use_decl_annotations_
VOID
DioUtilDeviceReset(POSRDIO_DEVICE_CONTEXT DevContext)
{
#if DBG
    DbgPrint("DioUtilDeviceReset...\n");
#endif

    //
    // Reset/Clear/ACK any interrupts on the device
    //
    DioUtilResetDeviceInterrupts(DevContext);

    //
    // Set all lines for INPUT, and ensure the output line state is
    // set to "all lines DEASSERTED"
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->DIO_Direction_Register,
                         0x00000000);

    //
    // Reset the device's idea of the output line state, just in case it
    // "remembers" a previous state from when the output lines were enabled.
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->Static_Digital_Output_Register,
                         0x00000000);

    //
    // Set the change detect registers to zeros.  We set these to functional
    // values when we set the OUTPUT mask.
    //
    WRITE_REGISTER_ULONG(&DevContext->DevBase->DI_ChangeIrqRE_Register,
                         0x00000000);

    WRITE_REGISTER_ULONG(&DevContext->DevBase->DI_ChangeIrqFE_Register,
                         0x00000000);
}

#if DBG
///////////////////////////////////////////////////////////////////////////////
//
//  DioUtilDisplayResources
//
//    Debugging function to display the resources assigned to the device
//
// INPUTS:
//
//      Resources               WDFCMRESLIST of hardware resources assigned
//                              to our device.
//
//      ResourcesTranslated     A WDFCMRESLIST of hardware resources
//                              assigned to our device, made usable.
//
// RETURNS:
// 
//      None.
//      
//  IRQL:
//  
//      Always PASSIVE_LEVEL
//
///////////////////////////////////////////////////////////////////////////////
_Use_decl_annotations_
VOID
DioUtilDisplayResources(
    WDFCMRESLIST Resources,
    WDFCMRESLIST ResourcesTranslated)
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceTrans;

    UNREFERENCED_PARAMETER(Resources);

    DbgPrint("Dumping device resources:\n");

    for (ULONG i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {

        //
        // Get the i'th partial resource descriptor from the list
        //
        resourceTrans = WdfCmResourceListGetDescriptor(ResourcesTranslated,
                                                       i);

        if (!resourceTrans) {

            DbgPrint("NULL resource returned??\n");

            goto done;
        }


        //
        // Examine and print the resources, based on their type
        //
        switch (resourceTrans->Type) {

            case CmResourceTypeMemory: {
                DbgPrint("\tResource %lu: Register\n",
                         i);

                DbgPrint("\t\tBase: 0x%I64x\n",
                         resourceTrans->u.Memory.Start.QuadPart);

                DbgPrint("\t\tLength: %lu\n",
                         resourceTrans->u.Memory.Length);


                break;
            }
            case CmResourceTypeInterrupt: {

                DbgPrint("\tResource %lu: Interrupt\n",
                         i);

                DbgPrint("\t\tInt type: %s\n",
                         (resourceTrans->Flags & CM_RESOURCE_INTERRUPT_MESSAGE ?
                              "MSI" :
                              "LBI"));

                if (resourceTrans->Flags & CM_RESOURCE_INTERRUPT_MESSAGE) {
                    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceRaw;

                    resourceRaw = WdfCmResourceListGetDescriptor(Resources,
                                                                 i);

                    DbgPrint("\t\tMSI Messages Allocated: %u\n",
                             resourceRaw->u.MessageInterrupt.Raw.MessageCount);
                }

                break;
            }

            case CmResourceTypePort: {
                DbgPrint("\tResource %lu: Port\n",
                         i);
                break;
            }
            case CmResourceTypeDma: {
                DbgPrint("\tResource %lu: DMA\n",
                         i);
                break;
            }
            case CmResourceTypeBusNumber: {
                DbgPrint("\tResource %lu: BusNumber\n",
                         i);
                break;
            }
            case CmResourceTypeMemoryLarge: {
                DbgPrint("\tResource %lu: MemLarge\n",
                         i);
                break;
            }
            case CmResourceTypeNonArbitrated: {
                DbgPrint("\tResource %lu: NonArbitrated\n",
                         i);
                break;
            }
            case CmResourceTypeDevicePrivate: {
                DbgPrint("\tResource %lu: DevicePrivate\n",
                         i);
                break;
            }
            case CmResourceTypePcCardConfig: {
                DbgPrint("\tResource %lu: PcCardConfig\n",
                         i);
                break;
            }
            default: {

                DbgPrint("\tResource %lu: Unhandled resource type 0x%0x\n",
                         i,
                         resourceTrans->Type);
                break;
            }
        }

    }

done:

    return;
}
#endif
