/**===========================================================================
 File   :  camera_director.h
 Brief  :  Sole writer to global Camera. Blends Follow / SceneNode / Intro.
 ===========================================================================**/
#ifndef CAMERA_DIRECTOR_H
#define CAMERA_DIRECTOR_H

#include "camera_pose.h"
#include <memory>
#include <vector>

class GameObject;

enum class CameraDirectorSource {
	Follow = 0,
	SceneNode = 1,
	Intro = 2,
	FirstPerson = 3,
};

void CameraDirector_Initialize();
void CameraDirector_Update(double elapsed_time, const std::vector<std::unique_ptr<GameObject>>& roots);

void CameraDirector_SetSource(CameraDirectorSource source);
CameraDirectorSource CameraDirector_GetSource();

// Intro producer (written by IntroCameraScript via ScriptApi_CameraSet*).
void CameraDirector_SetIntroPosition(const DirectX::XMFLOAT3& position);
void CameraDirector_SetIntroTarget(const DirectX::XMFLOAT3& target);
void CameraDirector_SetIntroUp(const DirectX::XMFLOAT3& up);
const CameraPose& CameraDirector_GetIntroPose();

void CameraDirector_SetBlendDuration(float seconds);

#endif // !CAMERA_DIRECTOR_H
