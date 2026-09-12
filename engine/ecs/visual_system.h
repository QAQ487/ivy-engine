/**===========================================================================
 File   : visual_system.h
 Brief  : Main-thread skinning upload / GPU palette publish after parallel Update.
 ===========================================================================**/
#pragma once

// Maps dirty CPU-skinned VBs and publishes GPU bone palettes (front buffer).
// Call once per frame after JobSystem_WaitAll (also when UseParallel is false).
void VisualSystem_UploadSkinning();
