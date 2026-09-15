/**===========================================================================

 File   :  [debug_camera.h]

 Author : Terry Yeung
 Date   : 2025/11/4
 -----------------------------------------------------------------------------
 
 Maya-style camera controls:
 - Alt + Left Mouse: Orbit/Tumble around focal point
 - Alt + Middle Mouse: Pan/Track camera
 - Alt + Right Mouse: Dolly/Zoom in and out
 - Mouse Wheel: Dolly/Zoom in and out
 - RMB + WASD + Shift: Fly camera (Unity-style)
 
 F1 to toggle debug camera on/off

 ===========================================================================**/
#ifndef DEBUG_CAMERA_H
#define DEBUG_CAMERA_H
#include <DirectXMath.h>

enum class DebugCameraControlMode {
    Orbit = 0, // Alt + mouse orbit (Maya-style)
    Fly   = 1, // RMB + WASD move + RMB mouse look
};

void DebugCamera_Update(double dt);
bool DebugCamera_IsEnabled();
void DebugCamera_SetEnabled(bool enabled);

void DebugCamera_SetControlMode(DebugCameraControlMode mode);
DebugCameraControlMode DebugCamera_GetControlMode();

// Optional parameters
void DebugCamera_SetMoveSpeed(float speed);        // Base speed (units/sec)
void DebugCamera_SetBoostMultiplier(float mult);   // Shift boost multiplier
void DebugCamera_SetMouseSensitivity(float degPerPixel); // Mouse sensitivity (deg/pixel)
void DebugCamera_SetOrbitSensitivity(float sensitivity); // Orbit rotation sensitivity
void DebugCamera_SetPanSensitivity(float sensitivity);   // Pan movement sensitivity
void DebugCamera_SetDollySensitivity(float sensitivity); // Dolly/zoom sensitivity
void DebugCamera_SetFocalPoint(DirectX::XMFLOAT3 point);

// Sync internal orbit state from the current Camera position/target.
void DebugCamera_SyncOrbitFromCurrentView();

// Fly mode: set world position and view angles (degrees).
void DebugCamera_SetFlyView(DirectX::XMFLOAT3 position, float yawDeg, float pitchDeg);

#endif // !DEBUG_CAMERA_H
