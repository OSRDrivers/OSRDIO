//
// DIOTEST.CPP
//
// Test utility for the OSRDIO driver, which was created for our WDF seminar.
//
// This code is purely functional, and is definitely not designed to be any
// sort of example.
//
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <windows.h>

#include <cfgmgr32.h>
#include "..\inc\OsrDio_IOCTL.h"


HANDLE
OpenHandleByGUID()
{
    CONFIGRET configReturn;
    DWORD     lasterror;
    WCHAR     deviceName[MAX_DEVICE_ID_LEN];
    HANDLE    handleToReturn = INVALID_HANDLE_VALUE;

    //
    // Get the device interface -- we only expose one
    //
    deviceName[0] = UNICODE_NULL;

    configReturn = CM_Get_Device_Interface_List((LPGUID)&GUID_DEVINTERFACE_OSRDIO,
                                                nullptr,
                                                deviceName,
                                                _countof(deviceName),
                                                CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (configReturn != CR_SUCCESS) {
        lasterror = GetLastError();

        printf("CM_Get_Device_Interface_List fail: %lx\n",
            lasterror);

        goto Exit;
    }

    //
    // Make sure there's an actual name there
    //
    if (deviceName[0] == UNICODE_NULL) {
        lasterror = ERROR_NOT_FOUND;
        goto Exit;
    }

    //
    // Open the device
    //
    handleToReturn = CreateFile(deviceName,
                                GENERIC_WRITE | GENERIC_READ,
                                0,
                                nullptr,
                                OPEN_EXISTING,
                                0,
                                nullptr);

    if (handleToReturn == INVALID_HANDLE_VALUE) {
        lasterror = GetLastError();
        printf("CreateFile fail: %lx\n",
               lasterror);
    }

Exit:

    //
    // Return a handle to the device
    //
    return handleToReturn;
}

HANDLE
OpenHandle()
{
    HANDLE handle;
    DWORD  lastError;

    //
    // Open the device by name
    //
    handle = CreateFile(LR"(\\.\OSRDIO)",
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        nullptr,
                        OPEN_EXISTING,
                        0,
                        nullptr);
    //
    // If this call fails, check to figure out what the error is and report it.
    //
    if (handle == INVALID_HANDLE_VALUE) {

        lastError = GetLastError();

        printf("CreateFile failed with error 0x%lx\n",
               lastError);

    }

    return handle;
}

void
AwaitCOSFunction()
{
    HANDLE          awaitHandle;
    DWORD           bytesRead;
    DWORD           lastError;
    OSRDIO_CHANGE_DATA newLineState;

    awaitHandle = OpenHandle();

    if (awaitHandle == INVALID_HANDLE_VALUE) {

        printf("\n\t\t\t\t****ERROR: CreateFile for await thread failed!\n");

        return;
    }

    printf("\n\t\t\t\tAwaiting line state change...\n");

    if (!DeviceIoControl(awaitHandle,
                         IOCTL_OSRDIO_WAITFOR_CHANGE,
                         nullptr,
                         0,
                         &newLineState,
                         sizeof(OSRDIO_CHANGE_DATA),
                         &bytesRead,
                         nullptr)) {

        lastError = GetLastError();

        printf("\nDeviceIoControl IOCTL_OSRDIO_READ failed with error 0x%lx\n",
               lastError);

        printf("\nAwait thread EXITING WITH ERROR\n");
        goto done;

    }

    printf("\n\n\t\t\t\tAwait thread: Change Of State Detected!\n");
    printf("\t\t\t\tLatched Line State @ COS = 0x%08lx\n",
           newLineState.LatchedLineState);

done:
    CloseHandle(awaitHandle);
}

int
main(int   argc,
     char* argv[])
{
    HANDLE deviceHandle;
    DWORD  lastErrorStatus;
    DWORD  bytesWritten;
    DWORD  bytesRead;
    DWORD  operation = 0;
    char   inputBuffer[100];

    printf("DIOTEST -- OSRDIO Test Utility V1.2\n");

    if (argc != 1) {

        printf("opening by GUID\n");
        deviceHandle = OpenHandleByGUID();

    } else {

        deviceHandle = OpenHandle();

    }


    if (deviceHandle == INVALID_HANDLE_VALUE) {
        exit(0);
    }


    while (TRUE) {
        char* endPointer = nullptr;

        while (endPointer != inputBuffer + 1) {

            printf("\n\nChoose from the following:\n");
            printf("\t 1. Read current DIO line state\n");
            printf("\t 2. Set output mask\n");
            printf("\t 3. Set lines to assert\n");
            printf("\t 4. Register COS notify\n");
            printf("\t Enter zero to exit\n");

            printf("\nEnter operation to perform: ");

            fgets(inputBuffer,
                  sizeof(inputBuffer),
                  stdin);

            operation = strtoul(inputBuffer,
                                &endPointer,
                                10);
        }

        switch (operation) {

            case 0: {
                exit(EXIT_SUCCESS);
            }

            case 1: {
                OSRDIO_READ_DATA readDataBuffer;

                if (!DeviceIoControl(deviceHandle,
                                     IOCTL_OSRDIO_READ,
                                     nullptr,
                                     0,
                                     &readDataBuffer,
                                     sizeof(OSRDIO_READ_DATA),
                                     &bytesRead,
                                     nullptr)) {

                    lastErrorStatus = GetLastError();

                    printf("DeviceIoControl IOCTL_OSRDIO_READ failed with error 0x%lx\n",
                           lastErrorStatus);

                    exit(lastErrorStatus);
                }

                printf("Bytes read = %lu\n",
                       bytesRead);
                printf("Input Line State = 0x%08lx",
                       readDataBuffer.CurrentLineState);

                break;
            }


            case 2: {
                OSRDIO_SET_OUTPUTS_DATA outputsDataBuffer;
                DWORD               desiredOutputMask;

                printf("Enter desired output mask (hex): ");

                char* result = fgets(inputBuffer,
                                     sizeof(inputBuffer),
                                     stdin);

                if (result != nullptr) {

                    desiredOutputMask = strtoul(inputBuffer,
                                                nullptr,
                                                16);

                    printf("Desired output mask is 0x%08lx\n",
                           desiredOutputMask);

                    outputsDataBuffer.OutputLines = desiredOutputMask;

                    if (!DeviceIoControl(deviceHandle,
                                         IOCTL_OSRDIO_SET_OUTPUTS,
                                         &outputsDataBuffer,
                                         sizeof(OSRDIO_SET_OUTPUTS_DATA),
                                         nullptr,
                                         0,
                                         &bytesWritten,
                                         nullptr)) {

                        lastErrorStatus = GetLastError();

                        printf("DeviceIoControl IOCTL_OSRDIO_SET_OUTPUTS failed with error 0x%lx\n",
                               lastErrorStatus);

                        exit(lastErrorStatus);
                    }

                    printf("Bytes written = %lu\n",
                           bytesWritten);
                }

                break;
            }

            case 3: {
                OSRDIO_WRITE_DATA writeDataBuffer;
                DWORD             linesToAssert;

                printf("ASSERT Lines: Remember output mask will be applied.\n");
                printf("Enter bitmask of lines to assert (hex): ");

                char* result = fgets(inputBuffer,
                                     sizeof(inputBuffer),
                                     stdin);

                if (result != nullptr) {

                    linesToAssert = strtoul(inputBuffer,
                                            nullptr,
                                            16);
                    printf("Mask of lines to assert is 0x%08lx\n",
                           linesToAssert);

                    writeDataBuffer.OutputLineState = linesToAssert;

                    if (!DeviceIoControl(deviceHandle,
                                         IOCTL_OSRDIO_WRITE,
                                         &writeDataBuffer,
                                         sizeof(OSRDIO_WRITE_DATA),
                                         nullptr,
                                         0,
                                         &bytesWritten,
                                         nullptr)) {

                        lastErrorStatus = GetLastError();

                        printf("DeviceIoControl IOCTL_OSRDIO_WRITE failed with error 0x%lx\n",
                               lastErrorStatus);

                        exit(lastErrorStatus);
                    }

                    printf("Bytes written = %lu\n",
                           bytesWritten);
                }
                break;
            }


            case 4: {

                //
                // Fire-up the COS thread
                //
                std::thread ChangeOfStateThread(AwaitCOSFunction);

                ChangeOfStateThread.detach();

                break;
            }
            default: {

                break;
            }
        }

    }
}
