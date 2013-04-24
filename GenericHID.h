/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2008. All Rights Reserved.							  */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in $(WIND_BASE)/WPILib.  */
/*----------------------------------------------------------------------------*/

#ifndef GENERIC_HID_H
#define GENERIC_HID_H

#include <vxWorks.h>

/** GenericHID Interface
 */
class GenericHID
{
public:
	typedef enum {
		kLeftHand = 0,
		kRightHand = 1
	} JoystickHand;

	virtual ~GenericHID() {}

	virtual float GetX() = 0;
	virtual float GetY() = 0;
	virtual float GetZ() = 0;
	virtual float GetTwist() = 0;
	virtual float GetThrottle() = 0;
	virtual float GetRawAxis(UINT32 axis) = 0;

	virtual bool GetTrigger() = 0;
	virtual bool GetTop() = 0;
	virtual bool GetBumper() = 0;
	virtual bool GetRawButton(UINT32 button) = 0;
};

#endif

