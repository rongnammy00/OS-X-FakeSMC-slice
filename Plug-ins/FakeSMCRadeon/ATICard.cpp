/*
 *  ATICard.cpp
 *  FakeSMCRadeon
 *
 *  Created by Slice on 24.07.10.
 *  Copyright 2010 Applelife.ru. All rights reserved.
 *
 */

#include "ATICard.h"
#include "radeon_chipinfo_gen.h"
#include "Sensor.h"


//OSDefineMetaClassAndStructors(ATICard, OSObject)

bool ATICard::initialize()
{
	rinfo = (RADEONCardInfo*)IOMalloc(sizeof(RADEONCardInfo));
	VCard->setMemoryEnable(true);
	/*	
	 // PCI dump
	 for (int i=0; i<0xff; i +=16) {
		IOLog("%02lx: ", (long unsigned int)i);
		for (int j=0; j<16; j += 4) {
			IOLog("%08lx ", (long unsigned int)VCard->configRead32(i+j));
		}
		IOLog("\n");
	 }
	 */
	for (UInt32 i = 0; (mmio = VCard->mapDeviceMemoryWithIndex(i)); i++)
	{
		long unsigned int mmio_base_phys = mmio->getPhysicalAddress();
		// Make sure we  select MMIO registers
		if (((mmio->getLength()) <= 0x00020000) && (mmio_base_phys != 0))
			break;
	}
	if (mmio)
	{
		mmio_base = (volatile UInt8 *)mmio->getVirtualAddress();
	} 
	else
	{
		InfoLog(" have no mmio\n ");
		return false;
	}
	
	//getRadeonBIOS(); -  anahuya?
	if(!getRadeonInfo())
		return false;
/*	if (!rinfo) {
		return false;
	}*/
	switch (rinfo->ChipFamily) {
		case CHIP_FAMILY_R600:
		case CHIP_FAMILY_RV610:
		case CHIP_FAMILY_RV630:
		case CHIP_FAMILY_RV670:
			setup_R6xx();
			break;
		case CHIP_FAMILY_R700:
		case CHIP_FAMILY_R710:
		case CHIP_FAMILY_R730:
		case CHIP_FAMILY_RV740:
		case CHIP_FAMILY_RV770:
		case CHIP_FAMILY_RV790:
			setup_R7xx();
			break;
		case CHIP_FAMILY_Evergreen:
			setup_Evergreen();
			break;
			
		default:
			InfoLog("sorry, but your card %04lx is not supported!\n", (long unsigned int)(rinfo->device_id));
			return false;
	}
	return true;
}

bool ATICard::getRadeonInfo()
{
	UInt16 devID = chipID & 0xffff;
	RADEONCardInfo *devices = radeon_device_list;
	rinfo = new RADEONCardInfo;
	while (devices->device_id != NULL) {
		//IOLog("check %d/n", devices->device_id ); //Debug
		if ((devices->device_id & 0xffff) == devID ) {
			//			rinfo->device_id = devID;
			rinfo->device_id = devices->device_id;
			rinfo->ChipFamily = devices->ChipFamily;
			family = devices->ChipFamily;
			rinfo->igp = devices->igp;
			rinfo->is_mobility = devices->is_mobility;
			IOLog(" Found ATI Radeon %04lx\n", (long unsigned int)devID);
			return true;
		}
		devices++;
	}
	
/*	
	for (int i=0; radeon_device_list[i].device_id; i++) {
		if (devID == radeon_device_list[i].device_id) {
			rinfo = &radeon_device_list[i];
			family = radeon_device_list[i].ChipFamily;
			return;
		}
	}
	if (!rinfo) {
		InfoLog("your DeviceID is unknown!\n");
	}
 */
	InfoLog("Unknown DeviceID!\n");
	return false;
}

void ATICard::setup_R6xx()
{
	char key[5];
	int id = GetNextUnusedKey(KEY_FORMAT_GPU_DIODE_TEMPERATURE, key);
	if (id == -1) {
		InfoLog("No new GPU SMC key!\n");
		return;
	}
	card_number = id;
	tempSensor = new R6xxTemperatureSensor(this, id, key, TYPE_SP78, 2);
	Caps = GPU_TEMP_MONITORING;	
}

void ATICard::setup_R7xx()
{
	char key[5];
	int id = GetNextUnusedKey(KEY_FORMAT_GPU_DIODE_TEMPERATURE, key);
	if (id == -1) {
		InfoLog("No new GPU SMC key!\n");
		return;
	}
	card_number = id;
	tempSensor = new R7xxTemperatureSensor(this, id, key, TYPE_SP78, 2);
	Caps = GPU_TEMP_MONITORING;
}

void ATICard::setup_Evergreen()
{
	char key[5];
	int id = GetNextUnusedKey(KEY_FORMAT_GPU_DIODE_TEMPERATURE, key);
	if (id == -1) {
		InfoLog("No new GPU SMC key!\n");
		return;
	}
	card_number = id;
	tempSensor = new EverTemperatureSensor(this, id, key, TYPE_SP78, 2);
	Caps = GPU_TEMP_MONITORING;
}

UInt32 ATICard::read32(UInt32 reg)
{
	return INVID(reg);
}