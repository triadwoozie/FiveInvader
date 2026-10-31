// GTA V Input Remapping and Patches
// Handles controller input translation for GTA V controls

#include "gta5_input_patches.h"
#include <stdafx.h>
#include <hid/hid.h>
#include <user/config.h>
#include <os/logger.h>
#include <kernel/xdm.h>

static GTA5InputState g_gta5InputState;

// Dead zone handling - uses GTA V standard deadzones
static int16_t ApplyDeadzone(int16_t value, int16_t deadzone = GTA5Input::LEFT_STICK_DEADZONE)
{
    if (abs(value) < deadzone)
        return 0;
    
    // Scale remaining range to full range
    int16_t sign = value > 0 ? 1 : -1;
    int32_t magnitude = abs(value) - deadzone;
    int32_t scaledMax = 32767 - deadzone;
    
    return static_cast<int16_t>(sign * (magnitude * 32767 / scaledMax));
}

// Update GTA V input state from HID
void GTA5_UpdateInputState()
{
    XAMINPUT_STATE state;
    if (hid::GetState(0, &state) != ERROR_SUCCESS)
    {
        memset(&g_gta5InputState, 0, sizeof(g_gta5InputState));
        return;
    }
    
    auto& pad = state.Gamepad;
    auto& gta = g_gta5InputState;
    
    // Buttons
    gta.buttons = pad.wButtons;
    
    // Analog sticks with deadzone
    gta.leftStickX = ApplyDeadzone(pad.sThumbLX);
    gta.leftStickY = ApplyDeadzone(pad.sThumbLY);
    gta.rightStickX = ApplyDeadzone(pad.sThumbRX, 8689); // Right stick has larger deadzone
    gta.rightStickY = ApplyDeadzone(pad.sThumbRY, 8689);
    
    // Triggers
    gta.leftTrigger = pad.bLeftTrigger;
    gta.rightTrigger = pad.bRightTrigger;
    
    // Derived states
    gta.isAiming = pad.bLeftTrigger > GTA5Input::LT_THRESHOLD;
    gta.isShooting = pad.bRightTrigger > GTA5Input::RT_THRESHOLD;
    gta.isSprinting = (pad.wButtons & GTA5Input::BUTTON_A) != 0;
}

// Get current GTA V input state
const GTA5InputState& GTA5_GetInputState()
{
    return g_gta5InputState;
}

// Check if a button is pressed
bool GTA5_IsButtonDown(uint16_t button)
{
    return (g_gta5InputState.buttons & button) != 0;
}

// Check if a button was just pressed (requires previous state tracking)
static uint16_t g_prevButtons = 0;

bool GTA5_IsButtonPressed(uint16_t button)
{
    bool wasDown = (g_prevButtons & button) != 0;
    bool isDown = (g_gta5InputState.buttons & button) != 0;
    return isDown && !wasDown;
}

bool GTA5_IsButtonReleased(uint16_t button)
{
    bool wasDown = (g_prevButtons & button) != 0;
    bool isDown = (g_gta5InputState.buttons & button) != 0;
    return !isDown && wasDown;
}

void GTA5_UpdatePreviousState()
{
    g_prevButtons = g_gta5InputState.buttons;
}

// Vibration/Rumble support
void GTA5_SetVibration(uint16_t leftMotor, uint16_t rightMotor)
{
    XAMINPUT_VIBRATION vibration;
    vibration.wLeftMotorSpeed = leftMotor;
    vibration.wRightMotorSpeed = rightMotor;
    hid::SetState(0, &vibration);
}

void GTA5_StopVibration()
{
    GTA5_SetVibration(0, 0);
}

// Input remapping hooks (to be connected to game functions)
// These will intercept GTA V's input reading and provide our translated input

// Hook for XInputGetState - intercepts game's input reading
extern "C" uint32_t GTA5_XInputGetState(uint32_t dwUserIndex, void* pState)
{
    if (dwUserIndex != 0 || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;
    
    GTA5_UpdateInputState();
    
    // Copy our processed state to the game's expected format
    auto* gameState = reinterpret_cast<XAMINPUT_STATE*>(pState);
    
    XAMINPUT_STATE hostState;
    if (hid::GetState(0, &hostState) == ERROR_SUCCESS)
    {
        *gameState = hostState;
        return ERROR_SUCCESS;
    }
    
    return ERROR_DEVICE_NOT_CONNECTED;
}

// Initialize GTA V input system
void GTA5_InitInput()
{
    LOGN("GTA V Input system initialized");
    memset(&g_gta5InputState, 0, sizeof(g_gta5InputState));
    g_prevButtons = 0;
}
