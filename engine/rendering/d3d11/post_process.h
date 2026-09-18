/**===========================================================================

 File   :  [post_process.h]

 Author : Terry Yeung
 Date   : 2026/3/28
 -----------------------------------------------------------------------------
 Fullscreen post-process (stackable effects) over the offscreen PostRT.
 ===========================================================================**/
#ifndef POST_PROCESS_H
#define POST_PROCESS_H

struct PostFXParams {
	float rgbShift = 0.0f;
	float mosaicBlocks = 0.0f;
	float posterize = 0.0f;
	float noise = 0.0f;
	float grayscale = 0.0f;
	float contrast = 0.0f; // 0 = off; when >0, k for (c-0.5)*k+0.5 (1.0 = neutral)
	float vignette = 0.0f;
	float scanline = 0.0f;
	float flicker = 0.0f;
};

void PostProcess_Initialize();
void PostProcess_Shutdown();
void PostProcess_SetEffects(float rgbShift, float mosaicBlocks);
void PostProcess_SetEffects(const PostFXParams& params);
void PostProcess_GetEffects(float& outRgbShift, float& outMosaicBlocks);
void PostProcess_GetEffects(PostFXParams& out);
void PostProcess_TriggerBurst(float rgbShift, float durationSeconds);
void PostProcess_Update(float dt);
void PostProcess_Draw();

#endif // !POST_PROCESS_H
