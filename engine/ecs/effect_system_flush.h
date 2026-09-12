/**===========================================================================
 File   : effect_system_flush.h
 Brief  : Main-thread flush of deferred EffectComponent commands.
 ===========================================================================**/
#pragma once

// Named to avoid clashing with engine/rendering/effect/effect_system.h (Effekseer).
void EffectSystem_FlushCommands();
void EffectSystem_RegisterComponent(class EffectComponent* component);
void EffectSystem_UnregisterComponent(class EffectComponent* component);
