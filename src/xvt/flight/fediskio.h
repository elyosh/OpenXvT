#ifndef XVT_FLIGHT_FEDISKIO_H
#define XVT_FLIGHT_FEDISKIO_H

#include "xvt/assets/file.h"
#include "xvt/assets/object_type.h"
#include "xvt/xvt_typedefs.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum DiskIoStringId {
	DISK_IO_STR_ENTERING_COMBAT = 0x0,
	DISK_IO_STR_ENTERING_MELEE = 0x1,
	DISK_IO_STR_ENTERING_TRAINING = 0x2,
	DISK_IO_STR_ENTERING_CAMPAIGN = 0x3,
	DISK_IO_STR_UNUSED = 0x4,
	DISK_IO_STR_RES_320_NOT_SUPPORTED = 0x5,
	DISK_IO_STR_RES_512_NOT_SUPPORTED = 0x6,
	DISK_IO_STR_RES_640_NOT_SUPPORTED = 0x7,
	DISK_IO_STR_RES_NOT_SUPPORTED = 0x8,
	DISK_IO_STR_RES_320_USED_INSTEAD = 0x9,
	DISK_IO_STR_RES_512_USED_INSTEAD = 0xA,
	DISK_IO_STR_RES_640_USED_INSTEAD = 0xB,
	DISK_IO_STR_NEXT_RES_USED_INSTEAD = 0xC,
	DISK_IO_STR_USING_8BPP = 0xD,
	DISK_IO_STR_USING_16BPP = 0xE,
	DISK_IO_STR_HARDWARE_3D_NOT_SUPPORTED = 0xF,
	DISK_IO_STR_COM_FAILURE_RECEIVING = 0x10,
	DISK_IO_STR_COM_FAILURE_SENDING = 0x11,
	DISK_IO_STR_COM_FAILURE_WAITING = 0x12,
	DISK_IO_STR_RECOVERING = 0x13,
	DISK_IO_STR_RECOVERING_WAIT = 0x14,
	DISK_IO_STR_RESENDING_PACKET = 0x15,
	DISK_IO_STR_RESENDING_PACKET_WAIT = 0x16,
	DISK_IO_STR_ESC_BOOT_PLAYER = 0x17,
	DISK_IO_STR_ESC_DISCONNECT = 0x18,
	DISK_IO_STR_DISCONNECT_COUNTDOWN = 0x19,
	DISK_IO_STR_WAITING_FOR_OTHER_PLAYERS = 0x1A,
	DISK_IO_STR_OTHER_PLAYERS_STILL_LOADING = 0x1B,
	DISK_IO_STR_PLAYER_STILL_LOADING_MINUS = 0x1C,
	DISK_IO_STR_PLAYER_STILL_LOADING_PLUS = 0x1D,
	DISK_IO_STR_NO_CDROM = 0x1E,
	DISK_IO_STR_RETRY_FAIL = 0x1F,
} DiskIoStringId;

extern char g_fileName[256];
extern char* g_strDiskIoMessages[32];
extern uint8_t* g_flightLog1Buffer;
extern uint8_t* g_flightAuxBufferMirror;
extern uint16_t g_fileReadAbortFlag;
extern XvtFile* g_stream;
extern unsigned int g_paletteGenerationEnabled;
extern char g_flightPaletteResourceFileName[12];
extern uint8_t g_rgb565ToPaletteIndexLut[UINT16_MAX + 1u];
extern uint16_t g_warheadGuidancePoolHandle;
extern uint16_t g_craftDataPoolHandle;
extern uint16_t g_mobileObjectPoolHandle;
extern uint16_t g_flightAuxBufferHandle;
extern uint16_t g_mobileObjectCharDataHandle;
extern uint16_t g_objectTableHandle;
extern uint16_t g_stringDataHandle;
extern uint16_t g_visibleObjectsHandle;
extern uint16_t g_flightLog1BufferHandle;
extern uint16_t g_flightTinyFontHandle;
extern uint16_t g_flightOffscreenBufferHandle;
extern uint16_t g_flightMicroFontHandle;
extern uint16_t g_flightSmallFontHandle;
extern const int g_pilotKillScoreBaseByAiLevel[7];
extern const int g_pilotRatingPromotionPointThresholds[25];
extern const uint8_t g_placementAwardLevels[24];
extern const int g_missionAwardWinThresholds[5];
extern const int g_missionAwardScoreMarginThresholds[3];

int16_t FeDiskIo_CommitFlightResults(int arg1, int arg2);
uint16_t FeDiskIo_ReadAllBytesOrFatal(const char* fileName, void* dst);
void FeDiskIo_InitGlobalBuffers(void);
void FeDiskIo_UnlockGlobalBuffers(void);
void FeDiskIo_LockGlobalBuffers(void);
void FeDiskIo_FreeModelResources(void);
void FeDiskIo_LoadResources(void);
unsigned int FeDiskIo_InitResources(void);
void FeDiskIo_BuildModelDef(uint8_t modelDefIndex, ObjectTypeId objectType);
#ifndef XVT_MODERN
char FeDiskIo_ShowRetryFailPrompt(void);
int FeDiskIo_ShowFatalErrorMessageAndWaitKey(const char* message);
#endif
int File_OpenGlobalStream(const char* fileName, const char* mode, int promptOnFail, int locationMode);
int16_t FeDiskIo_CloseGlobalStream(int16_t removeFileOnError);
size_t FeDiskIo_ReadWithRetryPrompt(void* dst, size_t elemSize, size_t elemCount, XvtFile* stream);
void FeDiskIo_FatalError(FileErrorStringId errorCode);
void File_PrintFatalMessageAndExit(const char* message, int exitCode);

#ifdef __cplusplus
}
#endif

#endif
