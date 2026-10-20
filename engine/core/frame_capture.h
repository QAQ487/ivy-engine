#pragma once

// Optional capture, enabled only by IVY_FRAME_CAPTURE (JSON output path).
void FrameCapture_BeginFrame();
void FrameCapture_EndFrame();
void FrameCapture_BeginDraw();
void FrameCapture_EndDraw();
void FrameCapture_RecordSpriteDraw();
void FrameCapture_RecordTextDraw();
void FrameCapture_Shutdown();
