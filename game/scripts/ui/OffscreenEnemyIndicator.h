/**===========================================================================
 File   : OffscreenEnemyIndicator.h
 Brief  : Screen-edge arrows for offscreen score opponents.
============================================================================**/
#pragma once

class OffscreenEnemyIndicator {
public:
    OffscreenEnemyIndicator() = delete;
    ~OffscreenEnemyIndicator() = delete;

    static bool Initialize();
    static void Finalize();
    static void Update(double elapsed_time);
    static void Draw();
    static void Reset();

private:
    static int s_arrowTextureId;
};
