#include "xvt/audio/sound.h"

#include "aeron/compat/dsound.h"
#include "xvt/audio/direct_sound.h"
#include "xvt/audio/fsfx.h"

#include <string.h>

// GLOBAL: XVT 0xA10514
IDirectSound* g_directSound = 0;
// GLOBAL: XVT 0xA10518
IDirectSoundBuffer* g_soundPrimaryBuffer = 0;
// GLOBAL: XVT 0xA1051C
SoundEffectDef g_soundDefs[1000] = { { 0 } };
// GLOBAL: XVT 0xA604CC
ActiveSoundInstance g_activeSoundInstances[8] = { { 0 } };
// GLOBAL: XVT 0xA6052C
SoundQueueEntry g_soundQueue[5] = { { 0 } };
// GLOBAL: XVT 0xA606D0
int g_soundQueueCount = 0;
// GLOBAL: XVT 0xA606D4
int g_soundCount = 0;
// GLOBAL: XVT 0xA606D8
int g_activeSoundCount = 0;
// GLOBAL: XVT 0xA606DC
unsigned int g_nextSoundInstanceSeq = 0;

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x42CD50
int Sound_Init_Sound_Engine(void* hwnd) {
	int instanceIndex;
	int soundId;
	DSBUFFERDESC primaryBufferDesc;
	WAVEFORMATEX primaryFormat;
	HRESULT result;

	if (g_directSound != NULL) {
		return 1;
	}
	for (instanceIndex = 0; instanceIndex < 8; ++instanceIndex) {
		g_activeSoundInstances[instanceIndex].soundId = -1;
		g_activeSoundInstances[instanceIndex].sequence = 0;
		g_activeSoundInstances[instanceIndex].buffer = NULL;
	}
	g_activeSoundCount = 0;
	g_soundCount = 0;
	g_nextSoundInstanceSeq = 0;
	for (soundId = 0; soundId < 1000; ++soundId) {
		g_soundDefs[soundId].buffer = NULL;
		g_soundDefs[soundId].name[0] = '\0';
	}

	if (DirectSoundCreate(NULL, (void**)&g_directSound, NULL) != 0) {
		return 0;
	}
	if (g_directSound->lpVtbl->SetCooperativeLevel(g_directSound, hwnd, 2) != 0) {
		Sound_Shutdown_Sound_Engine();
		return 0;
	}

	memset(&primaryBufferDesc, 0, sizeof(primaryBufferDesc));
	primaryBufferDesc.dwSize = 20;
	primaryBufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
	result = g_directSound->lpVtbl->CreateSoundBuffer(g_directSound, &primaryBufferDesc,
													  &g_soundPrimaryBuffer, NULL);
	if (result != 0) {
		Sound_Shutdown_Sound_Engine();
		return 0;
	}
	memset(&primaryFormat, 0, sizeof(primaryFormat));
	return 1;
}

// FUNCTION: XVT 0x42CE60
int Sound_Shutdown_Sound_Engine(void) {
	int soundId;
	int instanceIndex;

	if (g_directSound == NULL)
		return 1;
	Sound_UnloadAllEffects();
	g_directSound->lpVtbl->Release(g_directSound);
	g_directSound = NULL;
	for (soundId = 0; soundId < 1000; ++soundId) {
		g_soundDefs[soundId].buffer = NULL;
		g_soundDefs[soundId].name[0] = '\0';
	}
	for (instanceIndex = 0; instanceIndex < 8; ++instanceIndex) {
		g_activeSoundInstances[instanceIndex].soundId = -1;
		g_activeSoundInstances[instanceIndex].sequence = 0;
		g_activeSoundInstances[instanceIndex].buffer = NULL;
	}
	g_soundPrimaryBuffer = NULL;
	g_nextSoundInstanceSeq = 0;
	return 1;
}

// FUNCTION: XVT 0x42CEE0
int Sound_LoadEffect(const char* fileName, const char* name) { return Sound_LoadEffectEx(fileName, name, 0); }

// FUNCTION: XVT 0x42CF00
int Sound_LoadEffectEx(const char* fileName, const char* name, int createFlags) {
	SoundEffectDef effect;

	if (*fileName == '\0') {
		return 0;
	}
	if (*name == '\0') {
		return 0;
	}
	if (g_soundCount >= 1000) {
		return 0;
	}
	if (g_directSound == NULL) {
		return 0;
	}
	if (Sound_FindLoadedEffectByName(name) != -1) {
		return 0;
	}
	effect.buffer = DirectSound_LoadWaveBuffer(g_directSound, fileName, createFlags);
	if (effect.buffer != NULL) {
		effect.buffer->lpVtbl->SetCurrentPosition(effect.buffer, 0);
		strncpy(effect.name, name, sizeof(effect.name));
		effect.name[63] = '\0';
		strncpy(effect.fileName, fileName, sizeof(effect.fileName));
		effect.fileName[191] = '\0';
		effect.currentPriority = 0;
		Sound_InsertEffectDefSorted(&effect);
		return effect.buffer != NULL;
	}
	return 0;
}

// FUNCTION: XVT 0x42D020
void Sound_UnloadAllEffects(void) {
	int soundId;

	soundId = 0;
	do {
		Sound_UnloadEffectByName(g_soundDefs[soundId].name);
		++soundId;
	} while (soundId < 1000);
}

// FUNCTION: XVT 0x42D040
int Sound_UnloadEffectByName(const char* name) {
	int soundId;

	if (*name == '\0')
		return 0;
	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1)
		return 0;
	while (Sound_StopOldestInstance(g_soundDefs[soundId].name) == 1) {
	}
	g_soundDefs[soundId].buffer->lpVtbl->Release(g_soundDefs[soundId].buffer);
	Sound_RemoveEffectDef(soundId);
	return 1;
}

// FUNCTION: XVT 0x42D0D0
void Sound_FlushQueuedEffects(void) {
	int queueIndex;

	queueIndex = 0;
	if (g_soundQueueCount > 0) {
		do {
			Sound_PlayEffectNow(g_soundQueue[queueIndex].name, g_soundQueue[queueIndex].param2,
								g_soundQueue[queueIndex].loop, g_soundQueue[queueIndex].priority,
								g_soundQueue[queueIndex].volume, g_soundQueue[queueIndex].pan);
			++queueIndex;
		} while (queueIndex < g_soundQueueCount);
	}
	g_soundQueueCount = 0;
}

#ifndef XVT_MODERN
#pragma function(memcpy)
#endif
// FUNCTION: XVT 0x42D120
int Sound_QueueEffect(const char* soundName, int param2, int loop, int priority, int volume, int pan) {
	int queueIndex;
	int queuedPriority;
	int queueCount;
	const char* name;
	IDirectSound* directSound;
	SoundQueueEntry* queueEntry;

	name = soundName;
	directSound = g_directSound;
	if (directSound == NULL)
		return 0;
	if (*name == '\0')
		return 0;
	if (Sound_FindLoadedEffectByName(name) == -1)
		return 0;
#ifdef XVT_MODERN
	/* Loop callers test only playing instances; several simulation steps before
	   a flush would otherwise start identical, phase-aligned loops. */
	if (loop == 1) {
		for (queueIndex = 0; queueIndex < g_soundQueueCount; ++queueIndex) {
			if (g_soundQueue[queueIndex].loop == 1 &&
				strncmp(g_soundQueue[queueIndex].name, name, sizeof(g_soundQueue[queueIndex].name)) == 0)
				return 1;
		}
	}
#endif

	queueIndex = 0;
	if (g_soundQueueCount > queueIndex) {
		queuedPriority = priority;
		do {
			if (g_soundQueue[queueIndex].priority < priority)
				break;
			++queueIndex;
		} while (g_soundQueueCount > queueIndex);
	} else {
		queuedPriority = priority;
	}
	if (g_soundQueueCount == queueIndex && queueIndex == 4)
		return 0;

	queueEntry = &g_soundQueue[queueIndex];
#ifdef XVT_MODERN
	memmove(&g_soundQueue[queueIndex + 1], queueEntry,
			sizeof(SoundQueueEntry) * (g_soundQueueCount - queueIndex));
#else
	memcpy(&g_soundQueue[queueIndex + 1], queueEntry,
		   sizeof(SoundQueueEntry) * (g_soundQueueCount - queueIndex));
#endif
	strncpy(queueEntry->name, name, sizeof(queueEntry->name));
	g_soundQueue[queueIndex].loop = loop;
	g_soundQueue[queueIndex].param2 = param2;
	g_soundQueue[queueIndex].priority = queuedPriority;
	g_soundQueue[queueIndex].volume = volume;
	g_soundQueue[queueIndex].pan = pan;
	queueCount = g_soundQueueCount + 1;
	g_soundQueueCount = queueCount;
	if (queueCount > 4)
		g_soundQueueCount = 4;
	return 1;
}

// FUNCTION: XVT 0x42D230
int Sound_PlayEffectNow(const char* soundName, int param2, int loop, int priority, int volume, int pan) {
	int soundId;
	int instanceIndex;
	int scanIndex;
	int activeSoundId;
	int currentPriority;
	int clampedVolume;
	int clampedPan;
	uint32_t bufferStatus;
	uint32_t playFlags;
	HRESULT result;
	ActiveSoundInstance* instance;
	IDirectSoundBuffer* buffer;
	IDirectSoundBuffer* duplicate;

	if (g_directSound == NULL) {
		return 0;
	}
	if (*soundName == '\0') {
		return 0;
	}
	soundId = Sound_FindLoadedEffectByName(soundName);
	if (soundId == -1) {
		return 0;
	}

	if (g_activeSoundCount == 8) {
		instanceIndex = 0;
		do {
			activeSoundId = g_activeSoundInstances[instanceIndex].soundId;
			if (activeSoundId != -1) {
				buffer = g_activeSoundInstances[instanceIndex].buffer;
				if (buffer->lpVtbl->GetStatus(buffer, &bufferStatus) != 0) {
					break;
				}
				if ((bufferStatus & (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING)) == 0) {
					break;
				}
			}
			++instanceIndex;
		} while (instanceIndex < 8);
		if (instanceIndex < 8) {
			g_activeSoundInstances[instanceIndex].buffer->lpVtbl->Release(
				g_activeSoundInstances[instanceIndex].buffer);
			g_activeSoundInstances[instanceIndex].soundId = -1;
			g_activeSoundInstances[instanceIndex].buffer = NULL;
			g_activeSoundCount = g_activeSoundCount - 1;
		}

		if (instanceIndex == 8) {
			currentPriority = priority;
			instanceIndex = 8;
			scanIndex = 0;
			do {
				activeSoundId = g_activeSoundInstances[scanIndex].soundId;
				if (currentPriority > g_soundDefs[activeSoundId].currentPriority) {
					instanceIndex = scanIndex;
					currentPriority = g_soundDefs[activeSoundId].currentPriority;
				}
				++scanIndex;
			} while (scanIndex < 8);

			if (instanceIndex != 8) {
				instance = &g_activeSoundInstances[instanceIndex];
				if (instance->soundId == soundId) {
					instance->buffer->lpVtbl->SetCurrentPosition(instance->buffer, 0);
					instance->sequence = g_nextSoundInstanceSeq++;
					return 1;
				}
				buffer = instance->buffer;
				buffer->lpVtbl->Stop(buffer);
				buffer->lpVtbl->Release(buffer);
				instance->soundId = -1;
				instance->buffer = NULL;
				--g_activeSoundCount;
			} else {
				if (param2 != 0) {
					instanceIndex = 0;
					do {
						activeSoundId = g_activeSoundInstances[instanceIndex].soundId;
						if (activeSoundId == soundId) {
							instance = &g_activeSoundInstances[instanceIndex];
							instance->buffer->lpVtbl->SetCurrentPosition(instance->buffer, 0);
							instance->sequence = g_nextSoundInstanceSeq++;
							return 1;
						}
						++instanceIndex;
					} while (instanceIndex < 8);
				}
				return 0;
			}
		}
	} else {
		instanceIndex = 0;
		do {
			if (g_activeSoundInstances[instanceIndex].soundId == -1) {
				break;
			}
			++instanceIndex;
		} while (instanceIndex < 8);
	}

	g_directSound->lpVtbl->DuplicateSoundBuffer(g_directSound, g_soundDefs[soundId].buffer, &duplicate);
	if (duplicate == NULL) {
		return 0;
	}
	duplicate->lpVtbl->SetCurrentPosition(duplicate, 0);
	clampedVolume = volume;
	if (clampedVolume > 127) {
		clampedVolume = 127;
	} else if (clampedVolume < 0) {
		clampedVolume = 0;
	}
	duplicate->lpVtbl->SetVolume(duplicate, 400 * (5 * clampedVolume - 635) / 127);
	clampedPan = pan;
	if (clampedPan > 127) {
		clampedPan = 127;
	}
	if (clampedPan < 0) {
		clampedPan = 0;
	}
	duplicate->lpVtbl->SetPan(duplicate, 400 * (5 * clampedPan - 315) / 63);
	playFlags = loop == 1;
	result = duplicate->lpVtbl->Play(duplicate, 0, 0, playFlags);
	if (result == (HRESULT)0x88780096) {
		result = DirectSound_ReloadWaveBuffer(g_soundDefs[soundId].buffer, g_soundDefs[soundId].fileName);
		if (result == 1) {
			duplicate->lpVtbl->SetCurrentPosition(duplicate, 0);
			result = duplicate->lpVtbl->Play(duplicate, 0, 0, playFlags);
			if (result == 0) {
				buffer = duplicate;
				instance = &g_activeSoundInstances[instanceIndex];
				instance->soundId = soundId;
				instance->buffer = buffer;
				instance->sequence = g_nextSoundInstanceSeq++;
				++g_activeSoundCount;
			} else {
				return 0;
			}
		}
	} else if (result == 0) {
		buffer = duplicate;
		instance = &g_activeSoundInstances[instanceIndex];
		instance->soundId = soundId;
		instance->buffer = buffer;
		instance->sequence = g_nextSoundInstanceSeq++;
		++g_activeSoundCount;
		return 1;
	}
	return result;
}

// FUNCTION: XVT 0x42D600
int Sound_StopOldestInstance(const char* name) {
	int soundId;
	int instanceIndex;
	int oldestSequence;
	int oldestIndex;
	HRESULT stopResult;
	IDirectSoundBuffer* buffer;

	if (g_directSound == NULL)
		return 0;
	if (*name == '\0')
		return 0;
	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1)
		return 0;
	oldestSequence = (int)g_nextSoundInstanceSeq + 1;
	oldestIndex = -1;
	for (instanceIndex = 0; instanceIndex < 8; ++instanceIndex) {
		if (g_activeSoundInstances[instanceIndex].soundId != soundId)
			continue;
		if (oldestSequence <= (int)g_activeSoundInstances[instanceIndex].sequence)
			continue;
		oldestSequence = (int)g_activeSoundInstances[instanceIndex].sequence;
		oldestIndex = instanceIndex;
	}
	if (oldestIndex == -1)
		return 0;
	buffer = g_activeSoundInstances[oldestIndex].buffer;
	if (buffer == NULL)
		return 0;
	stopResult = buffer->lpVtbl->Stop(buffer);
	buffer->lpVtbl->Release(buffer);
	g_activeSoundInstances[oldestIndex].buffer = NULL;
	g_activeSoundInstances[oldestIndex].soundId = -1;
	--g_activeSoundCount;
	return stopResult >= 0;
}

// FUNCTION: XVT 0x42D6D0
int Sound_StopAllInstances(void) {
	int result;
	int instanceIndex;

	result = 1;
	for (instanceIndex = 0; instanceIndex < 8; ++instanceIndex) {
		if (g_activeSoundInstances[instanceIndex].soundId != -1)
			result &=
				Sound_StopOldestInstance(g_soundDefs[g_activeSoundInstances[instanceIndex].soundId].name);
	}
	return result;
}

// FUNCTION: XVT 0x42D770
int Sound_GetPrimaryBufferVolume(void) {
	int32_t volumeMillibels;

	if (g_directSound == 0)
		return 0;
	if (g_soundPrimaryBuffer->lpVtbl->GetVolume(g_soundPrimaryBuffer, &volumeMillibels) != 0)
		return 0;
	volumeMillibels = 127 * volumeMillibels / 2000 + 127;
	return volumeMillibels;
}

// FUNCTION: XVT 0x42D7C0
int Sound_SetLatestInstanceVolume(const char* name, int volume) {
	int soundId;
	int newestIndex;
	int instanceIndex;
	int newestSequence;
	int clampedVolume;

	if (g_directSound == 0) {
		return 0;
	}
	if (name[0] == '\0') {
		return 0;
	}

	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1) {
		return 0;
	}

	newestSequence = -1;
	instanceIndex = 0;
	newestIndex = -1;
	do {
		if (g_activeSoundInstances[instanceIndex].soundId == soundId &&
			(int)g_activeSoundInstances[instanceIndex].sequence > newestSequence) {
			newestSequence = g_activeSoundInstances[instanceIndex].sequence;
			newestIndex = instanceIndex;
		}
		++instanceIndex;
	} while (instanceIndex < 8);

	if (newestIndex == -1) {
		return 0;
	}

	clampedVolume = volume;
	if (clampedVolume > 127) {
		clampedVolume = 127;
	} else if (clampedVolume < 0) {
		clampedVolume = 0;
	}

	clampedVolume = 400 * (5 * clampedVolume - 635) / 127;
	return g_activeSoundInstances[newestIndex].buffer->lpVtbl->SetVolume(
			   g_activeSoundInstances[newestIndex].buffer, clampedVolume) == 0;
}

// FUNCTION: XVT 0x42D880
int Sound_GetLatestInstanceVolume(const char* name) {
	int soundId;
	int newestIndex;
	int instanceIndex;
	int newestSequence;
	int32_t volumeMillibels;

	if (g_directSound == 0)
		return 0;
	if (name[0] == '\0')
		return 0;
	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1)
		return 0;
	newestSequence = -1;
	instanceIndex = 0;
	newestIndex = -1;
	do {
		if (g_activeSoundInstances[instanceIndex].soundId == soundId &&
			(int)g_activeSoundInstances[instanceIndex].sequence > newestSequence) {
			newestSequence = g_activeSoundInstances[instanceIndex].sequence;
			newestIndex = instanceIndex;
		}
		++instanceIndex;
	} while (instanceIndex < 8);
	if (newestIndex == -1)
		return 0;
	if (g_activeSoundInstances[newestIndex].buffer->lpVtbl->GetVolume(
			g_activeSoundInstances[newestIndex].buffer, &volumeMillibels) != 0)
		return 0;
	volumeMillibels = 127 * volumeMillibels / 2000 + 127;
	return volumeMillibels;
}

// FUNCTION: XVT 0x42D940
int Sound_SetLatestInstancePan(const char* name, int pan) {
	int soundId;
	int newestIndex;
	int instanceIndex;
	int newestSequence;
	int clampedPan;

	if (g_directSound == 0) {
		return 0;
	}
	if (name[0] == '\0') {
		return 0;
	}

	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1) {
		return 0;
	}

	newestSequence = -1;
	instanceIndex = 0;
	newestIndex = -1;
	do {
		if (g_activeSoundInstances[instanceIndex].soundId == soundId &&
			(int)g_activeSoundInstances[instanceIndex].sequence > newestSequence) {
			newestSequence = g_activeSoundInstances[instanceIndex].sequence;
			newestIndex = instanceIndex;
		}
		++instanceIndex;
	} while (instanceIndex < 8);

	if (newestIndex == -1) {
		return 0;
	}

	clampedPan = pan;
	if (clampedPan > 127) {
		clampedPan = 127;
	}
	if (clampedPan < 0) {
		clampedPan = 0;
	}

	clampedPan = 400 * (5 * clampedPan - 315) / 63;
	return g_activeSoundInstances[newestIndex].buffer->lpVtbl->SetPan(
			   g_activeSoundInstances[newestIndex].buffer, clampedPan) == 0;
}

// FUNCTION: XVT 0x42DA00
int Sound_GetLatestInstancePan(const char* name) {
	int soundId;
	int newestSequence;
	int instanceIndex;
	int newestIndex;
	int32_t panMillibels;

	if (g_directSound == 0)
		return 0;
	if (name[0] == '\0')
		return 0;
	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1)
		return 0;
	newestSequence = -1;
	instanceIndex = 0;
	newestIndex = -1;
	do {
		if (g_activeSoundInstances[instanceIndex].soundId == soundId &&
			(int)g_activeSoundInstances[instanceIndex].sequence > newestSequence) {
			newestSequence = g_activeSoundInstances[instanceIndex].sequence;
			newestIndex = instanceIndex;
		}
		++instanceIndex;
	} while (instanceIndex < 8);
	if (newestIndex == -1)
		return 0;
	if (g_activeSoundInstances[newestIndex].buffer->lpVtbl->GetPan(g_activeSoundInstances[newestIndex].buffer,
																   &panMillibels) != 0)
		return 0;
	panMillibels = 63 * panMillibels / 10000 + 63;
	return panMillibels;
}

// FUNCTION: XVT 0x42DAC0
int Sound_SetLatestInstanceFrequency(const char* name, uint32_t frequency) {
	int soundId;
	int newestSequence;
	int instanceIndex;
	int newestIndex;

	if (g_directSound == 0) {
		return 0;
	}
	if (name[0] == '\0') {
		return 0;
	}

	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1) {
		return 0;
	}

	newestSequence = -1;
	instanceIndex = 0;
	newestIndex = -1;
	do {
		if (g_activeSoundInstances[instanceIndex].soundId == soundId &&
			(int)g_activeSoundInstances[instanceIndex].sequence > newestSequence) {
			newestSequence = g_activeSoundInstances[instanceIndex].sequence;
			newestIndex = instanceIndex;
		}
		++instanceIndex;
	} while (instanceIndex < 8);

	if (newestIndex == -1) {
		return 0;
	}

	return g_activeSoundInstances[newestIndex].buffer->lpVtbl->SetFrequency(
			   g_activeSoundInstances[newestIndex].buffer, frequency) == 0;
}

// FUNCTION: XVT 0x42DB50
int Sound_SetEffectCurrentPriority(const char* name, int priority) {
	int soundId;
	int clampedPriority;

	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1) {
		return 0;
	}

	clampedPriority = priority;
	if (clampedPriority > 255) {
		clampedPriority = 255;
	}
	if (clampedPriority < 0) {
		clampedPriority = 0;
	}
	g_soundDefs[soundId].currentPriority = clampedPriority;
	return 1;
}

// FUNCTION: XVT 0x42DBA0
int Sound_GetEffectCurrentPriority(const char* name) {
	int soundId;

	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1) {
		return 0;
	}

	return g_soundDefs[soundId].currentPriority;
}

// FUNCTION: XVT 0x42DBD0
int Sound_CountPlayingInstances(const char* name) {
	int soundId;
	int playingCount;
	int instanceIndex;
	uint32_t status;

	if (g_directSound == 0) {
		return 0;
	}
	if (name[0] == '\0') {
		return 0;
	}

	soundId = Sound_FindLoadedEffectByName(name);
	if (soundId == -1) {
		return 0;
	}

	playingCount = 0;
	instanceIndex = 0;
	do {
		if (g_activeSoundInstances[instanceIndex].soundId == soundId &&
			g_activeSoundInstances[instanceIndex].buffer->lpVtbl->GetStatus(
				g_activeSoundInstances[instanceIndex].buffer, &status) == 0 &&
			((status & 1) != 0 || (status & 4) != 0)) {
			++playingCount;
		}
		++instanceIndex;
	} while (instanceIndex < 8);

	return playingCount;
}

// FUNCTION: XVT 0x42DC60
void Sound_InsertEffectDefSorted(const SoundEffectDef* effect) {
	int insertIndex;
	SoundEffectDef* current;
	const SoundEffectDef* sourceEffect;
	int destinationIndex;
	int remaining;
	int instanceIndex;

	insertIndex = 0;
	if (g_soundCount > 0) {
		current = g_soundDefs;
		sourceEffect = effect;
		do {
			if (strncmp(sourceEffect->name, current->name, sizeof(sourceEffect->name)) < 0)
				break;
			++current;
			++insertIndex;
		} while (g_soundCount > insertIndex);
	} else {
		sourceEffect = effect;
	}
	if (g_soundCount > insertIndex) {
		destinationIndex = g_soundCount;
		remaining = g_soundCount - insertIndex;
		do {
			g_soundDefs[destinationIndex] = g_soundDefs[destinationIndex - 1];
			--destinationIndex;
			--remaining;
		} while (remaining != 0);
	}
	g_soundDefs[insertIndex] = *sourceEffect;
	++g_soundCount;
	instanceIndex = 0;
	do {
		if (insertIndex <= g_activeSoundInstances[instanceIndex].soundId)
			++g_activeSoundInstances[instanceIndex].soundId;
		++instanceIndex;
	} while (instanceIndex < 8);
}

// FUNCTION: XVT 0x42DD20
void Sound_RemoveEffectDef(int soundId) {
	int currentIndex;

	if (soundId < 0 || g_soundCount <= soundId) {
		return;
	}

	currentIndex = soundId;
	if (g_soundCount - 1 > soundId) {
		SoundEffectDef* currentEffect;

		currentEffect = &g_soundDefs[soundId];
		do {
			*currentEffect = currentEffect[1];
			++currentEffect;
			++currentIndex;
		} while (g_soundCount - 1 > currentIndex);
	}

	--g_soundCount;
	{
		int instanceIndex;

		for (instanceIndex = 0; instanceIndex < 8; ++instanceIndex) {
			if (g_activeSoundInstances[instanceIndex].soundId > soundId) {
				--g_activeSoundInstances[instanceIndex].soundId;
			}
		}
	}
}

// FUNCTION: XVT 0x42DDA0
int Sound_FindLoadedEffectByName(const char* name) {
	return Sound_FindEffectByName(g_soundDefs, g_soundCount - 1, name);
}

// FUNCTION: XVT 0x42DDC0
int Sound_FindEffectByName(const SoundEffectDef* records, int lastIndex, const char* name) {
	int searchLastIndex;
	int middle;
	int baseIndex;
	int comparison;
	const SoundEffectDef* middleEffect;

	baseIndex = 0;
	searchLastIndex = lastIndex;
	while (1) {
		if (searchLastIndex < 0) {
			return -1;
		}

		middle = searchLastIndex >> 1;
		middleEffect = &records[middle];
		comparison = strncmp(middleEffect->name, name, sizeof(middleEffect->name));
		if (comparison == 0) {
			return middle + baseIndex;
		}
		if (searchLastIndex <= 0) {
			return -1;
		}

		if (comparison < 0) {
			searchLastIndex -= middle + 1;
			baseIndex += middle + 1;
			records = middleEffect + 1;
		} else {
			searchLastIndex = middle - 1;
		}
	}
}

// FUNCTION: XVT 0x4A9180
int Sound_SetParam(int soundId, int param, int value) {
	if (soundId < 4 || soundId > 837) {
		return 0;
	}
	switch (param) {
		case 0x500:
			return Sound_SetEffectCurrentPriority(g_fsfxSfxNameTable[soundId], value);
		case 0x600:
			return Sound_SetLatestInstanceVolume(g_fsfxSfxNameTable[soundId], value);
		case 0x700:
			return Sound_SetLatestInstancePan(g_fsfxSfxNameTable[soundId], value);
		case 0x777:
			return Sound_SetLatestInstanceFrequency(g_fsfxSfxNameTable[soundId], value);
		default:
			return 0;
	}
}

// FUNCTION: XVT 0x4A9300
int Sound_GetParam(int soundId, int param) {
	if (soundId < 4 || soundId > 837) {
		return 0;
	}
	switch (param) {
		case 0x100:
			return Sound_CountPlayingInstances(g_fsfxSfxNameTable[soundId]);
		case 0x500:
			return Sound_GetEffectCurrentPriority(g_fsfxSfxNameTable[soundId]);
		default:
			return 0;
	}
}

// FUNCTION: XVT 0x4A9370
int Sound_UnusedFourArgStub(int arg1, int arg2, int arg3, int arg4) {
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;

	return 0;
}

// FUNCTION: XVT 0x4A9380
int Sound_StopOldestInstanceById(int soundId) {
	if (soundId < 4 || soundId > 837)
		return 0;
	return Sound_StopOldestInstance(g_fsfxSfxNameTable[soundId]);
}

// FUNCTION: XVT 0x4A93B0
void nullsub_10(void) {}
