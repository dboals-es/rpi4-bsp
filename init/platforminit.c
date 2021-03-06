//==============================================================================
// Copyright 2020 Daniel Boals & Michael Boals
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
// THE SOFTWARE.
//==============================================================================

#include "types.h"
#include "../../rtos/include/process.h"
#include "memory.h"
#include "devices.h"
#include "platform.h"

#include "mailbox.h"
#include "framebuffer.h"
#include "gic400.h"


static status_t platformBuildTable(void);


memoryDescriptor_t  platformMemory[3];
device_t            platformDeviceList[] = 
{
    // GIC400 Distributor
    {
        .driverInit         = Gic400DistributorInit,
        .mmio.memoryBase    = 0x4C0041000ULL,
        .mmio.memorySize    = 4*1024,
        .mmio.memoryType    = MEM_TYPE_DEVICE_MEMORY,
        .mem.memoryBase     = 0,
        .mem.memorySize     = 0,
        .mem.memoryType     = MEM_TYPE_NONE,
    },
   
    // GIC400 CPU
   {
        .driverInit         = Gic400CpuInit,
        .mmio.memoryBase    = 0x4C0042000ULL,
        .mmio.memorySize    = 4*1024,
        .mmio.memoryType    = MEM_TYPE_DEVICE_MEMORY,
        .mem.memoryBase     = 0,
        .mem.memorySize     = 0,
        .mem.memoryType     = MEM_TYPE_NONE,        
    },
/*
    // rpi Video
    {
        .driverInit         = frameBufferInit,
        .mmio.memoryBase    = 0,
        .mmio.memorySize    = 0,
        .mmio.memoryType    = MEM_TYPE_NONE,
        .mem.memoryBase     = 0,
        .mem.memorySize     = 0,
        .mem.memoryType     = MEM_TYPE_VIDEO_MEMORY,
    },
*/
    // EndOfListTag
    {
        .driverInit         = NULL,
        .mmio.memoryBase    = -1,
        .mmio.memorySize    = -1,
        .mmio.memoryType    = -1,
        .mem.memoryBase     = -1,
        .mem.memorySize     = -1,
        .mem.memoryType     = -1,
    },
};


//-----------------------------------------------------------------------------
// platform Init.   Platform Specific Initialization
//-----------------------------------------------------------------------------
void platformInit(void)
{
    mbxMessage_t    mbxMsg;
    uint32_t        resp;

    MbxInitMsgBuffer(&mbxMsg);
    MbxAddMsgTag(&mbxMsg, MBX_VC_GET_FW_VER, NULL);
    MbxAddMsgTag(&mbxMsg, MBX_VC_GET_HW_MDL, NULL);
    MbxAddMsgTag(&mbxMsg, MBX_VC_GET_BRD_VER, NULL);
    MbxAddMsgTag(&mbxMsg, MBX_VC_GET_BRD_MAC, NULL);
    MbxAddMsgTag(&mbxMsg, MBX_VC_GET_BRD_SN, NULL);
    MbxAddMsgTag(&mbxMsg, MBX_VC_GET_ARM_MEM, NULL);
    MbxAddMsgTag(&mbxMsg, MBX_VC_GET_VC_MEM, NULL);
    MbxAddMsgTag(&mbxMsg, MBX_TAG_LAST, NULL);    
    if (MbxSendMsg(&mbxMsg) == STATUS_SUCCESS)
    {        
        MbxUnpackNextTagResp(&mbxMsg, &resp, (uint32_t*)&gFwVersion);
        MbxUnpackNextTagResp(&mbxMsg, &resp, (uint32_t*)&gHwModel);
        MbxUnpackNextTagResp(&mbxMsg, &resp, (uint32_t*)&gBoardVersion);
        MbxUnpackNextTagResp(&mbxMsg, &resp, (uint32_t*)&gMacAddress);
        MbxUnpackNextTagResp(&mbxMsg, &resp, (uint32_t*)&gBoardSerialNumber);
        MbxUnpackNextTagResp(&mbxMsg, &resp, (uint32_t*)gLowMemory);
        MbxUnpackNextTagResp(&mbxMsg, &resp, gVideoMemory);

        switch (gBoardVersion)
        {
            case 0xa03111:
            {
                gTopOfMemory    = (uint64_t)(1LL*1024LL*1024LL*1024LL) - 1;
                break;
            }

            case 0xb03111:
            case 0xb03112:
            {
                gTopOfMemory    = (uint64_t)(2LL*1024LL*1024LL*1024LL) - 1;
                break;
            }

            case 0xc03111:
            case 0xc03112:
            {
                gTopOfMemory    = (uint64_t)(4LL*1024LL*1024LL*1024LL) - 1;
                break;
            }            
        }
    }

    platformBuildTable();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static status_t platformBuildTable(void)
{
    // fill in the platform memory table

    platformMemory[0].memoryBase    = gLowMemory[0];
    platformMemory[0].memorySize    = gLowMemory[1];
    platformMemory[0].memoryType    = MEM_TYPE_NORMAL_MEMORY;

    platformMemory[1].memoryBase    = ((1024) * (1024) * (1024));  // TODO clean up these magic numbers
    platformMemory[1].memorySize    = (gTopOfMemory + 1) - ((1024) * (1024) * (1024));
    platformMemory[1].memoryType    = MEM_TYPE_NORMAL_MEMORY;

    platformMemory[2].memoryBase    = -1;
    platformMemory[2].memorySize    = -1;
    platformMemory[2].memoryType    = -1;

    // fill in the video memory information
    platformDeviceList[1].mem.memoryBase   = gVideoMemory[0];
    platformDeviceList[1].mem.memorySize   = gVideoMemory[1];

#if 0

    // map the Kernel Memory 
    uint64_t memoryRegionStart  = 0;
    uint64_t memoryRegionSize   = 0x100000; // 1st 1 Meg of RAM is for the kernel (for now)
    MmuMapRange(memoryRegionStart, memoryRegionStart, memoryRegionSize, MEM_REGION_NORMAL_MEM_CACHEABLE); // Normal Memory Cache enabled privileged 

    // map the memory range from 0 to the start of Video RAM
    memoryRegionStart   = 0x100000;
    memoryRegionSize    = gLowMemory[1] - 0x100000;
    MmuMapRange(memoryRegionStart, memoryRegionStart, memoryRegionSize, MEM_REGION_NORMAL_MEM_CACHEABLE); // Normal Memory Cache enabled not privileged

    // map the memory range for Video RAM
    memoryRegionStart   = gVideoMemory[0];
    memoryRegionSize    = gVideoMemory[1];
    MmuMapRange(memoryRegionStart, memoryRegionStart, memoryRegionSize, MEM_REGION_NORMAL_MEM_NONCACHEABLE);  // Cache disabled privileged 

    // Map the reset of Memory below the TTs
    memoryRegionStart   = 
    memoryRegionSize    = (gTopOfMemory - memoryRegionStart - (100 * 1024 * 1024) + 1);
    MmuMapRange(memoryRegionStart, memoryRegionStart, memoryRegionSize, MEM_REGION_NORMAL_MEM_CACHEABLE);  // Normal Memory Cache enabled not Privileged 

    // Map TTs
    memoryRegionStart = gTopOfMemory - ((1024) * (1024) * (1024)) + 1; // TODO clean up these magic numbers.
    memoryRegionSize = (100 * 1024 * 1024);
    MmuMapRange(memoryRegionStart, memoryRegionStart, memoryRegionSize, MEM_REGION_NORMAL_MEM_CACHEABLE);  // Normal Memory Cache enabled Privileged  

    // Map VC Mailbox interface
    memoryRegionStart = 0x47E00B000; // TODO clean up these magic numbers.
    memoryRegionSize =  4 * 1024;
    MmuMapRange(memoryRegionStart, memoryRegionStart, memoryRegionSize, MEM_REGION_DEVICE_IO);  // Normal Memory Cache enabled Privileged  


    // Map The Gic Distributor MMIO
    memoryRegionStart = 0x4C0040000ULL; //GIC_DISTRIBUTOR_START_ADDRESS;
    memoryRegionSize = 32*1024;
    MmuMapRange(memoryRegionStart, memoryRegionStart, memoryRegionSize, MEM_REGION_DEVICE_IO);  // Device Memory Disabled Cache privileged 

#if 0
    // Map The Gic CPU MMIO
    memoryRegionStart = GIC_CPU_START_ADDRESS;
    memoryRegionSize = ;
    MmuMapRange(memoryRegionStart, memoryRegionStart, memoryRegionSize, MMU_BLOCK_DESC_DEVICE_MEM_SECURE_NONCACHEABLE);  // Device Memory Disabled Cache privileged 


    // Map EMMC MMIO
    memoryRegionStart = EMMC_START_ADDRESS;
    memoryRegionSize = ;
    MmuMapRange(memoryRegionStart, memoryRegionStart, memoryRegionSize, MMU_BLOCK_DESC_DEVICE_MEM_SECURE_NONCACHEABLE);  // Device Memory Disabled Cache privileged 
#endif 
    MmuEnable();
#endif

    return STATUS_SUCCESS;
}
