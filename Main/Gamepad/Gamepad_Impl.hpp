#pragma once
#include "Gamepad.hpp"

#ifdef _WIN32
#include "SDL_joystick.h"
#else
#include "SDL2/SDL_joystick.h"
#endif


class Gamepad_Impl : public Gamepad
{
public:
	~Gamepad_Impl();
	bool Init(Graphics::Window* window, uint32 deviceIndex);

	// Handles input events straight from the event loop
	void HandleInputEvent(uint32 buttonIndex, uint8 newState);
	void HandleAxisEvent(uint32 axisIndex, int16 newValue);
	void HandleHatEvent(uint32 hadIndex, uint8 newValue);

	class Graphics::Window* m_window;
	uint32 m_deviceIndex;
	SDL_Joystick* m_joystick;

	Vector<float> m_axisState;
	Vector<uint8> m_buttonStates;

	bool GetButton(uint8 button) const override;
	float GetAxis(uint8 idx) const override;
	uint32 NumButtons() const override;
	uint32 NumAxes() const override;
};