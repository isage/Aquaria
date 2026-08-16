/*
Copyright (C) 2007, 2010 - Bit-Blot

This file is part of Aquaria.

Aquaria is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "Core.h"


#ifdef __LINUX__
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include <iostream>

#define BITS_TO_LONGS(x) \
	(((x) + 8 * sizeof (unsigned long) - 1) / (8 * sizeof (unsigned long)))
#define AQUARIA_BITS_PER_LONG (sizeof(long) * 8)
#define AQUARIA_OFF(x)  ((x)%AQUARIA_BITS_PER_LONG)
#define AQUARIA_BIT(x)  (1UL<<AQUARIA_OFF(x))
#define AQUARIA_LONG(x) ((x)/AQUARIA_BITS_PER_LONG)
#define test_bit(bit, array) ((array[AQUARIA_LONG(bit)] >> AQUARIA_OFF(bit)) & 1)
#endif

Joystick::Joystick()
{
	xinited = false;
	stickIndex = -1;
	sdl_controller = NULL;
	sdl_haptic = NULL;
	sdl_joy = NULL;
	inited = false;
	for (int i = 0; i < maxJoyBtns; i++)
	{
		buttons[i] = UP;
	}
	deadZone1 = 0.3;
	deadZone2 = 0.3;

	clearRumbleTime= 0;
	leftThumb = rightThumb = false;
	leftTrigger = rightTrigger = 0;
	rightShoulder = leftShoulder = false;
	dpadRight = dpadLeft = dpadUp = dpadDown = false;
	btnStart = false;
	btnSelect = false;

	s1ax = 0;
	s1ay = 1;
	s2ax = 4;
	s2ay = 3;
}

void Joystick::init(int stick)
{
	std::ostringstream os;

	stickIndex = stick;

    int numJoy = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoy);

	os << "Found [" << numJoy << "] joysticks";
	debugLog(os.str());

    if (!joysticks || stick < 0 || stick >= numJoy)
    {
        SDL_free(joysticks);
		debugLog("Not enough Joystick(s) found");
        return;
    }

	{
        SDL_JoystickID id = joysticks[stick];
        SDL_free(joysticks);

		if (SDL_IsGamepad(id))
		{
			sdl_controller = SDL_OpenGamepad(stick);
			if (sdl_controller)
				sdl_joy = SDL_GetGamepadJoystick(sdl_controller);
		}
		if (!sdl_joy)
			sdl_joy = SDL_OpenJoystick(stick);

		if (sdl_joy && SDL_IsJoystickHaptic(sdl_joy))
		{
			sdl_haptic = SDL_OpenHapticFromJoystick(sdl_joy);
			bool rumbleok = false;
			if (sdl_haptic && SDL_HapticRumbleSupported(sdl_haptic))
				rumbleok = (SDL_InitHapticRumble(sdl_haptic) == 0);
			if (!rumbleok)
			{
				SDL_CloseHaptic(sdl_haptic);
				sdl_haptic = NULL;
			}
		}

		if (!sdl_joy)
			sdl_joy = SDL_OpenJoystick(stick);

		if (sdl_joy)
		{
			inited = true;
			debugLog(std::string("Initialized Joystick [") + std::string(SDL_GetJoystickName(sdl_joy)) + std::string("]"));
			if (sdl_controller) debugLog(std::string("Joystick is a Game Controller"));
			if (sdl_haptic) debugLog(std::string("Joystick has force feedback support"));
		}
		else
		{
			std::ostringstream os;
			os << "Failed to init Joystick [" << stick << "]";
			debugLog(os.str());
		}
	}
}

void Joystick::shutdown()
{
	if (sdl_haptic)
	{
		SDL_CloseHaptic(sdl_haptic);
		sdl_haptic = 0;
	}
	if (sdl_controller)
	{
		SDL_CloseGamepad(sdl_controller);
		sdl_controller = 0;
		sdl_joy = 0; // SDL_GameControllerClose() frees this
	}
	if (sdl_joy)
	{
		SDL_CloseJoystick(sdl_joy);
		sdl_joy = 0;
	}
}

void Joystick::rumble(float leftMotor, float rightMotor, float time)
{
	if (core->joystickEnabled && inited)
	{
		if (sdl_haptic)
		{
			const float power = (leftMotor + rightMotor) / 2.0f;
			if ((power > 0.0f) && (time > 0.0f))
			{
				clearRumbleTime = time;
				SDL_PlayHapticRumble(sdl_haptic, power, (Uint32) (time * 1000.0f));
			}
			else
			{
				clearRumbleTime = -1;
				SDL_StopHapticRumble(sdl_haptic);
			}
		}

	}
}

void Joystick::callibrate(Vector &calvec, float deadZone)
{
	//float len = position.getLength2D();
	if (calvec.isLength2DIn(deadZone))
	{
		calvec = Vector(0,0,0);
	}
	else
	{
		if (!calvec.isZero())
		{				
			Vector pos2 = calvec;
			pos2.setLength2D(deadZone);
			calvec -= pos2;
			float mult = 1.0f/float(1.0f-deadZone);
			calvec.x *= mult;
			calvec.y *= mult;
			if (calvec.x > 1)
				calvec.x = 1;
			else if (calvec.x < -1)
				calvec.x = -1;

			if (calvec.y > 1)
				calvec.y = 1;
			else if (calvec.y < -1)
				calvec.y = -1;
		}
	}
}

void Joystick::update(float dt)
{
	if (core->joystickEnabled && inited && sdl_joy && stickIndex != -1)
	{
		if (!SDL_JoystickConnected(sdl_joy))
		{
			debugLog("Lost Joystick");
			if (sdl_haptic) { SDL_CloseHaptic(sdl_haptic); sdl_haptic = NULL; }
			if (!sdl_controller)
				SDL_CloseJoystick(sdl_joy);
			else
			{
				SDL_CloseGamepad(sdl_controller);
				sdl_controller = NULL;
			}
			sdl_joy = NULL;
			return;
		}

		if (sdl_controller)
		{
			Sint16 xaxis = SDL_GetGamepadAxis(sdl_controller, SDL_GAMEPAD_AXIS_LEFTX);
			Sint16 yaxis = SDL_GetGamepadAxis(sdl_controller, SDL_GAMEPAD_AXIS_LEFTY);
			position.x = float(xaxis)/32768.0f;
			position.y = float(yaxis)/32768.0f;

			Sint16 xaxis2 = SDL_GetGamepadAxis(sdl_controller, SDL_GAMEPAD_AXIS_RIGHTX);
			Sint16 yaxis2 = SDL_GetGamepadAxis(sdl_controller, SDL_GAMEPAD_AXIS_RIGHTY);
			rightStick.x = float(xaxis2)/32768.0f;
			rightStick.y = float(yaxis2)/32768.0f;
		}
		else
		{
			Sint16 xaxis = SDL_GetJoystickAxis(sdl_joy, s1ax);
			Sint16 yaxis = SDL_GetJoystickAxis(sdl_joy, s1ay);
			position.x = float(xaxis)/32768.0f;
			position.y = float(yaxis)/32768.0f;

			Sint16 xaxis2 = SDL_GetJoystickAxis(sdl_joy, s2ax);
			Sint16 yaxis2 = SDL_GetJoystickAxis(sdl_joy, s2ay);
			rightStick.x = float(xaxis2)/32768.0f;
			rightStick.y = float(yaxis2)/32768.0f;
		}
		/*
		std::ostringstream os;
		os << "joy(" << position.x << ", " << position.y << ")";
		debugLog(os.str());
		*/


		callibrate(position, deadZone1);

		callibrate(rightStick, deadZone2);
		

		/*
		std::ostringstream os2;
		os2 << "joy2(" << position.x << ", " << position.y << ")";
		debugLog(os2.str());
		*/
		if (sdl_controller)
		{
			for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
				buttons[i] = SDL_GetGamepadButton(sdl_controller, (SDL_GamepadButton)i)?DOWN:UP;
			for (int i = SDL_GAMEPAD_BUTTON_COUNT; i < maxJoyBtns; i++)
				buttons[i] = UP;
		}
		else
		{
			for (int i = 0; i < maxJoyBtns; i++)
				buttons[i] = SDL_GetJoystickButton(sdl_joy, i)?DOWN:UP;
		}
	}

	if (clearRumbleTime >= 0)
	{
		clearRumbleTime -= dt;
		if (clearRumbleTime <= 0)
		{
			rumble(0,0,0);
		}
	}
		
		
		/*
		std::ostringstream os;
		os << "j-pos(" << position.x << ", " << position.y << " - b0[" << buttons[0] << "]) - len[" << len << "]";
		debugLog(os.str());
		*/
}

bool Joystick::anyButton()
{
	for (int i = 0; i < maxJoyBtns; i++)
	{
		if (buttons[i]) return true;
	}
	return false;
}
