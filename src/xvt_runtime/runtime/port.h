#ifndef XVT_RUNTIME_PORT_H
#define XVT_RUNTIME_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Main-thread contract. Aeron and render snapshot storage must outlive the port. */
int XvtPort_Init(void);
void XvtPort_SetSkipIntro(int skip_intro);
int XvtPort_IsInitialized(void);
void XvtPort_Tick(int32_t delta_us);
void XvtPort_PausedFrame(void);
void XvtPort_SetSettingsOpen(int open);
/* Nonzero freezes the simulation the same way an open settings menu does. */
void XvtPort_SetSimulationHeld(int held);
void XvtPort_RequestSettings(void);
int XvtPort_ConsumeSettingsRequest(void);
int XvtPort_NetworkRequiresProgress(void);
int XvtPort_ShouldQuit(void);
int XvtPort_GetExitCode(void);
/* Relative to Aeron_BeginFrame, UINT64_MAX when no task has a deadline. */
uint64_t XvtPort_NextWakeDelayUs(void);
void XvtPort_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
