/*
 *  ITE.cpp
 *  FakeSMCLPCMonitor
 *
 *  Created by Mozodojo on 29/05/10.
 *  Copyright 2010 mozodojo. All rights reserved.
 *
 *  This code contains parts of original code from Open Hardware Monitor
 *  Copyright 2010 Michael Möller. All rights reserved.
 *
 */

#include <IOKit/IOService.h>

#include "ITE.h"
#include "ITESensors.h"
#include "ITEFanController.h"

void ITE::WriteByte(UInt8 reg, UInt8 value)
{
	outb(m_Address + ITE_ADDRESS_REGISTER_OFFSET, reg);
	outb(m_Address + ITE_DATA_REGISTER_OFFSET, value);
}

UInt8 ITE::ReadByte(UInt8 reg, bool* valid)
{
	outb(m_Address + ITE_ADDRESS_REGISTER_OFFSET, reg);
	
	UInt8 value = inb(m_Address + ITE_DATA_REGISTER_OFFSET);
	
	valid = (bool*)(reg == inb(m_Address + ITE_DATA_REGISTER_OFFSET));
	
	return value;
}

UInt16 ITE::ReadWord(UInt8 reg1, UInt8 reg2, bool* valid)
{	
	return ReadByte(reg1, valid) << 8 | ReadByte(reg2, valid);
}

SInt16 ITE::ReadTemperature(UInt8 index)
{
	bool* valid;
	return ReadByte(ITE_TEMPERATURE_BASE_REG + index, valid);
}

SInt16 ITE::ReadVoltage(UInt8 index)
{
	bool* valid;
	m_RawVCore = ReadByte(ITE_VOLTAGE_BASE_REG + index, valid);
	return m_RawVCore << 4;
}

SInt16 ITE::ReadTachometer(UInt8 index)
{
	bool* valid;
	int value = ReadByte(ITE_FAN_TACHOMETER_REG[index], valid);
	
	value |= ReadByte(ITE_FAN_TACHOMETER_EXT_REG[index], valid) << 8;
	
	return value > 0x3f && value < 0xffff ? (float)(1350000 + value) / (float)(value * 2) : 0;
}

void ITE::Enter()
{
	outb(m_RegisterPort, 0x87);
	outb(m_RegisterPort, 0x01);
	outb(m_RegisterPort, 0x55);
	
	if (m_RegisterPort == 0x4e) 
	{
		outb(m_RegisterPort, 0xaa);
	}
	else
	{
		outb(m_RegisterPort, 0x55);
	}
}

void ITE::Exit()
{
	outb(m_RegisterPort, SUPERIO_CONFIGURATION_CONTROL_REGISTER);
	outb(m_ValuePort, 0x02);
}

bool ITE::ProbePort()
{
	UInt16 id = ListenPortWord(SUPERIO_CHIP_ID_REGISTER);
	
	if (id == 0 || id == 0xffff)
		return false;
	
	switch (id)
	{
		case IT8512F:
		case IT8712F:
		case IT8716F:
		case IT8718F:
		case IT8720F: 
		case IT8726F:
		case IT8752F:
			m_Model = id; 
			break; 
		default:
			InfoLog("ITE: Found unsupported chip ID=0x%x", id);
			return false;
	}
	
	Select(IT87_ENVIRONMENT_CONTROLLER_LDN);
	
	m_Address = ListenPortWord(SUPERIO_BASE_ADDRESS_REGISTER);
	
	IOSleep(1000);
	
	UInt16 verify = ListenPortWord(SUPERIO_BASE_ADDRESS_REGISTER);
	
	
	if (m_Address != verify || m_Address < 0x100 || (m_Address & 0xF007) != 0)
		return false;
	
	bool* valid;
	
	UInt8 vendor = ReadByte(ITE_VENDOR_ID_REGISTER, valid);
	
	if (!valid || vendor != ITE_VENDOR_ID)
		return false;
	
	if ((ReadByte(ITE_CONFIGURATION_REGISTER, valid) & 0x10) == 0)
		return false;
	
	if (!valid)
		return false;
		
	return true;
}

void ITE::Start()
{
	// Temperature Sensors
	if (OSDictionary* config = OSDynamicCast(OSDictionary, m_Service->getProperty("Sensor Configuration")))
	{
		if (OSDictionary* container = OSDynamicCast(OSDictionary, config->getObject("ITE")))
		{			
			for (int i = 0; i < 4; i++) 
			{	
				char key[8];
				
				snprintf(key, 8, "TEMPIN%X", i);
				
				if (OSString* name = OSDynamicCast(OSString, container->getObject(key)))
				{					
					if (name->isEqualTo("Processor"))
					{
						// Heatsink
						AddSensor(new ITETemperatureSensor(this, i, KEY_CPU_HEATSINK_TEMPERATURE, TYPE_SP78, 2));
					}
					else if (name->isEqualTo("System"))
					{
						// Northbridge
						AddSensor(new ITETemperatureSensor(this, i, KEY_NORTHBRIDGE_TEMPERATURE, TYPE_SP78, 2));
					}
				}
			}
		}
	}

	// CPU Vcore
	AddSensor(new ITEVoltageSensor(this, 0, KEY_CPU_VOLTAGE, TYPE_FP2E, 2));
		
	// FANs
	
	// Fan Control Setup
	if (m_FanControlEnabled && m_FanVoltageControlled)
	{
		bool* valid;
		UInt8 control = ReadByte(ITE_SMARTGUARDIAN_MAIN_CONTROL, valid);
		WriteByte(ITE_SMARTGUARDIAN_MAIN_CONTROL, control | 0x7);
	}
	
	// Sensors
	for (int i = 0; i < 5; i++) 
	{
		char* key = (char*)IOMalloc(5);
		
		if (m_FanForced[i] || ReadTachometer(i) > 0)
		{
			int offset = GetNextFanIndex();
			
			if (offset != -1) 
			{
				if (m_FanName[i] && strlen(m_FanName[i]) > 0)
				{
					snprintf(key, 5, KEY_FORMAT_FAN_ID, offset); 
					FakeSMCAddKey(key, TYPE_CH8, strlen(m_FanName[i]), (char*)m_FanName[i]);
					
					InfoLog("%s name associated with hardware Fan%d", m_FanName[i], i);
				}
				else 
				{
					InfoLog("Fan %d name associated with hardware Fan%d", offset, i);
				}

				snprintf(key, 5, KEY_FORMAT_FAN_SPEED, offset); 
				AddSensor(new ITETachometerSensor(this, i, key, TYPE_FPE2, 2));
				
				m_FanIndex[m_FanCount++] = i;
			}
			
			// Fan Control Support
			if (m_FanControlEnabled && m_FanControl[i])
				AddController(new ITEFanController(this, i));
		}
		
		IOFree(key, 5);
	}
	
	UpdateFNum();
}