/*++

Module Name:

    webmailwindow.cpp

Abstract:

    Find WebMail Window HID Device and set LED pattern.
    
    usage: webmailwindow.exe -c [r|g|b|rg|rb|gb|rgb|none]

Environment:

    User Mode console application

--*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "hid.h"
#include "list.h"

//****************************************************************************
// Local macro definitions
//****************************************************************************

#define WEBMAILWINDOW_VENDERID  0x1294
#define WEBMAILWINDOW_PRODUCTID 0x1320

//****************************************************************************
// Data types local to the HClient display routines
//****************************************************************************

typedef struct _DEVICE_LIST_NODE
{
    LIST_NODE_HDR   Hdr;
    HID_DEVICE      HidDeviceInfo;
    BOOL            DeviceOpened;

} DEVICE_LIST_NODE, *PDEVICE_LIST_NODE;

//****************************************************************************
// Global module variables
//****************************************************************************

//
// Variables for handling the two different types of devices that can be loaded
//  into the system.  PhysicalDeviceList contains all the actual HID devices
//  attached via the USB bus. 
//

static LIST               PhysicalDeviceList;

//****************************************************************************
// Application Functions
//****************************************************************************

VOID
DestroyDeviceListCallback(
    PLIST_NODE_HDR   ListNode
)
{
    PDEVICE_LIST_NODE   deviceNode;

    deviceNode = (PDEVICE_LIST_NODE) ListNode;
    
    //
    // The callback function needs to do the following steps...
    //   1) Close the HidDevice
    //   2) Unregister device notification (if registered)
    //   3) Free the allocated memory block
    //

    CloseHidDevice(&(deviceNode -> HidDeviceInfo));

    free (deviceNode);

    return;
}

void UpdatePhysicalDeviceList()
{
    PHID_DEVICE                      tempDeviceList;
    ULONG                            numberDevices;
    PHID_DEVICE                      pDevice;
    INT                              iIndex;
    PDEVICE_LIST_NODE                listNode;
    
    //
    // Initialize the device list.
    //  -- PhysicalDeviceList is for devices that are actually attached
    //     to the HID bus
    //
    
    InitializeList(&PhysicalDeviceList);
    
    //
    // Begin by finding all the Physical HID devices currently attached to
    //  the system. If that fails, exit the dialog box.  
    //
    
    if ( ! FindKnownHidDevices(&tempDeviceList, &numberDevices))
    {
        fprintf(stderr, "Faild to find hid devices\n");
        return;
    }
    
    //
    // For each device in the newly acquired list, create a device list
    //  node and add it the the list of physical device on the system  
    //
    
    pDevice = tempDeviceList;
    for (iIndex = 0; (ULONG) iIndex < numberDevices; iIndex++, pDevice++)
    {
        listNode = malloc(sizeof(DEVICE_LIST_NODE));

        if (NULL == listNode)
        {
            //
            // When freeing up the device list, we need to kill those
            //  already in the Physical Device List and close
            //  that have not been added yet in the enumerated list
            //
            
            DestroyListWithCallback(&PhysicalDeviceList, DestroyDeviceListCallback);
            
            CloseHidDevices(pDevice, numberDevices - iIndex);

            free(tempDeviceList);
            
            fprintf(stderr, "Faild to allocate memory for DEVICE_LIST_NODE\n");
            return;
        }

        listNode -> HidDeviceInfo = *pDevice;
        listNode -> DeviceOpened = TRUE;

        InsertTail(&PhysicalDeviceList, listNode);
    }

    //
    // Free the temporary device list...It is no longer needed
    //
    
    free(tempDeviceList);
}

PDEVICE_LIST_NODE FindWebMailWindowNode()
{
    if ( ! IsListEmpty(&PhysicalDeviceList))
    {
        PDEVICE_LIST_NODE currNode, lastNode;
        currNode = (PDEVICE_LIST_NODE) GetListHead(&PhysicalDeviceList);
        lastNode = (PDEVICE_LIST_NODE) GetListTail(&PhysicalDeviceList);
        
        //
        // This loop should always terminate since the device 
        //  handle should be somewhere in the physical device list
        //
        
        for (;;)
        {
            if (currNode->HidDeviceInfo.Attributes.VendorID == WEBMAILWINDOW_VENDERID &&
                currNode->HidDeviceInfo.Attributes.ProductID == WEBMAILWINDOW_PRODUCTID)
            {
                return currNode;
            }
            
            if (currNode == lastNode)
            {
                break;
            }
            
            currNode = (PDEVICE_LIST_NODE) GetNextEntry(currNode);
        }
    }
    
    return NULL;
}

BOOL SetLedPattern(PHID_DEVICE device, ULONG pattern_value)
{
    HID_DEVICE                       writeDevice;
    BOOL                             status;
    
    ZeroMemory(&writeDevice, sizeof(writeDevice));
    
    // Change LED pattern
    
    //
    // In order to write to the device, need to get a
    //  writable handle to the device.  In this case, the
    //  write will be a synchronous write.  Begin by
    //  trying to open a second instance of this device with
    //  write access
    //
    
    status = OpenHidDevice(device->DevicePath, 
                            FALSE,
                            TRUE,
                            FALSE,
                            FALSE,
                            &writeDevice);
    
    if (status)
    {
        // set LED color value
        writeDevice.OutputData->ValueData.Value = pattern_value;
        
        Write(&writeDevice);
        
        CloseHidDevice(&writeDevice);
        return TRUE;
    }
    else
    {
        fprintf(stderr, "Cannot open Webmail Window HID Device\n");
        return FALSE;
    }
}

// parse options for led pattern. if failed, retrun -1.
int ParseOptionsForLedPattern(ULONG argc, PCHAR argv[])
{
    int led_pattern = -1;
    
    UINT i;
    for (i = 0; i < argc; i++)
    {
        if (strncmp("-c", argv[i], 2) == 0 && i + 1 < argc)
        {
            PCHAR pattern = argv[i + 1];
            UINT mask = 0;
            
            if (strstr(pattern, "r")) {
                mask |= 0x2;
            }
            if (strstr(pattern, "g")) {
                mask |= 0x1;
            }
            if (strstr(pattern, "b")) {
                mask |= 0x4;
            }
            if (mask > 0) {
                led_pattern = mask;
            } else if (strncmp("none", pattern, 4) == 0) {
                led_pattern = 0;
            }
        }
    }
    
    return led_pattern;
}


//****************************************************************************
// Application Entry
//****************************************************************************

int __cdecl
main(
    __in ULONG argc,
    __in_ecount(argc) PCHAR argv[]
    )
{
    int retval = 0;
    int led_pattern = -1;   // initialize as invalid value
    
    // Parse Options
    led_pattern = ParseOptionsForLedPattern(argc, argv);
    
    // if LED pattern is not specified, print usage and exit
    if (led_pattern == -1) {
        printf("usage: webmailwindow.exe -c [r|g|b|rg|rb|gb|rgb|none]\n");
        return 0;
    }
    
    // Update HID Device List
    UpdatePhysicalDeviceList();
    
    {
        // Find WebMail Window
        PDEVICE_LIST_NODE node = FindWebMailWindowNode();
        
        if (node)
        {
            // Set LED
            if ( ! SetLedPattern(&node->HidDeviceInfo, led_pattern))
            {
                // failed to set led
                retval = 1;
            }
        }
        else
        {
            fprintf(stderr, "Cannot find Webmail Window HID Device\n");
            retval = 1;
        }
    }
    
    if ( ! IsListEmpty(&PhysicalDeviceList)) {
        DestroyListWithCallback(&PhysicalDeviceList, DestroyDeviceListCallback);
    }

    return retval;
}
