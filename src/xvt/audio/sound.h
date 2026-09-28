#ifndef XVT_AUDIO_SOUND_H
#define XVT_AUDIO_SOUND_H

#include "aeron/compat/win_types.h"
#include "xvt/util/win32.h"
#include "xvt/xvt_typedefs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SoundQueueEntry {
	char name[64];
	int volume;
	int pan;
	int loop;
	int param2;
	int priority;
};

struct SoundEffectDef {
	char name[64];
	char fileName[256];
	IDirectSoundBuffer* buffer;
	uint8_t currentPriority;
};

extern IDirectSound* g_directSound;
extern IDirectSoundBuffer* g_soundPrimaryBuffer;

struct ActiveSoundInstance {
	int soundId;
	unsigned int sequence;
	IDirectSoundBuffer* buffer;
};

extern SoundEffectDef g_soundDefs[1000];
extern ActiveSoundInstance g_activeSoundInstances[8];
extern SoundQueueEntry g_soundQueue[5];
extern int g_soundQueueCount;
extern int g_soundCount;

int Sound_Init_Sound_Engine(void* hwnd);
int Sound_Shutdown_Sound_Engine(void);
int Sound_LoadEffect(const char* fileName, const char* name);
int Sound_LoadEffectEx(const char* fileName, const char* name, int createFlags);
void Sound_UnloadAllEffects(void);
int Sound_UnloadEffectByName(const char* name);
void Sound_FlushQueuedEffects(void);
int Sound_QueueEffect(const char* soundName, int param2, int loop, int priority, int volume, int pan);
int Sound_PlayEffectNow(const char* soundName, int param2, int loop, int priority, int volume, int pan);
int Sound_StopOldestInstance(const char* name);
int Sound_StopAllInstances(void);
int Sound_GetPrimaryBufferVolume(void);
int Sound_SetLatestInstanceVolume(const char* name, int volume);
int Sound_GetLatestInstanceVolume(const char* name);
int Sound_SetLatestInstancePan(const char* name, int pan);
int Sound_GetLatestInstancePan(const char* name);
int Sound_SetLatestInstanceFrequency(const char* name, uint32_t frequency);
int Sound_SetEffectCurrentPriority(const char* name, int priority);
int Sound_GetEffectCurrentPriority(const char* name);
int Sound_CountPlayingInstances(const char* name);
void Sound_InsertEffectDefSorted(const SoundEffectDef* effect);
void Sound_RemoveEffectDef(int soundId);
int Sound_FindLoadedEffectByName(const char* name);
int Sound_FindEffectByName(const SoundEffectDef* records, int lastIndex, const char* name);
int Sound_SetParam(int soundId, int param, int value);
int Sound_GetParam(int soundId, int param);
int Sound_UnusedFourArgStub(int arg1, int arg2, int arg3, int arg4);
int Sound_StopOldestInstanceById(int soundId);
void nullsub_10(void);

#ifdef __cplusplus
}
#endif

#endif
