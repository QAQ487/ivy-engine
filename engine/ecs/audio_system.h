/**===========================================================================
 File   : audio_system.h
 Brief  : Main-thread flush of deferred AudioComponent commands.
 ===========================================================================**/
#pragma once

void AudioSystem_FlushCommands();
void AudioSystem_Register(class AudioComponent* component);
void AudioSystem_Unregister(class AudioComponent* component);
