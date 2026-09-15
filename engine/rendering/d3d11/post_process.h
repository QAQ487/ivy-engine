/**===========================================================================

 File   :  [post_process.h]

 Author : Terry Yeung
 Date   : 2026/3/28
 -----------------------------------------------------------------------------
 Fullscreen post-process (RGB shift / mosaic) over the offscreen PostRT.
 ===========================================================================**/
#ifndef POST_PROCESS_H
#define POST_PROCESS_H

void PostProcess_Initialize();
void PostProcess_Shutdown();
void PostProcess_SetEffects(float rgbShift, float mosaicBlocks);
void PostProcess_GetEffects(float& outRgbShift, float& outMosaicBlocks);
void PostProcess_TriggerBurst(float rgbShift, float durationSeconds);
void PostProcess_Update(float dt);
void PostProcess_Draw();

#endif // !POST_PROCESS_H
