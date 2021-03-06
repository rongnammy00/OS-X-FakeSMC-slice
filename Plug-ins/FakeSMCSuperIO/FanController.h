/*
 *  FanController.h
 *  FakeSMCSuperIO
 *
 *  Created by mozo on 27/06/10.
 *  Copyright 2010 mozodojo. All rights reserved.
 *
 */

#ifndef _FANCONTROLLER_H 
#define _FANCONTROLLER_H

#include "FakeSMC.h"
#include "Sensor.h"
#include "Controller.h"

class FanController : public Controller 
{
private:
	bool	m_Active;
	Sensor*	m_Input;
	UInt8	m_Index;
	float	m_Multiplier;
	SInt32	m_LastValue;
	UInt8	m_StartTemperature;
	UInt8	m_StartThrottle;
	UInt8	m_HighThrottle;
	UInt8	m_HighTemperature;
	
protected:
	SuperIO*	m_Provider;
	
	virtual bool UpdateConfiguration(OSDictionary* configuration);
	virtual void CalculateMultiplier() { m_Multiplier = float(m_HighThrottle - m_StartThrottle) / float(m_HighTemperature - m_StartTemperature); };
	
public:
	FanController(SuperIO* provider, UInt8 index)
	{
		m_Provider = provider;
		m_Index = index;
		
		if (OSDictionary* configuration = OSDynamicCast(OSDictionary, m_Provider->GetService()->getProperty("Fan Configuration")))
		{
			char key[5];
		
			snprintf(key, 5, "Fan%d", m_Index);
			
			if (OSDictionary* fan = OSDynamicCast(OSDictionary, configuration->getObject(key)))
				if (OSDictionary* control = OSDynamicCast(OSDictionary, fan->getObject("Software Fan Control")))
					UpdateConfiguration(control);
		}
	};
	
	void Activate() 
	{ 
		InfoLog("Activating software control on Fan%d", m_Index);
		m_Active = true; 
	};
	
	void Deactivate() 
	{
		InfoLog("Software control on Fan%d deactivated", m_Index);
		m_Active = false; 
	};
	
	void TimerEvent();
	
	// Events
		
	virtual void ForceFan(__unused UInt8 index, __unused UInt8 value) 
	{ 
		// 
	};
		
	virtual IOReturn OnKeyRead(__unused const char* key, __unused char* data)
	{
		return kIOReturnInvalid;
	};
	
	virtual IOReturn OnKeyWrite(__unused const char* key, __unused char* data)
	{
		return kIOReturnInvalid;
	};
};

#endif