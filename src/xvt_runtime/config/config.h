#ifndef XVT_RUNTIME_CONFIG_H
#define XVT_RUNTIME_CONFIG_H
#include "aeron/config_file.h"
#include "xvt/frontend/config.h"
#include "xvt_runtime/config/settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Read views remain valid until the next successful update or shutdown. */
int XvtConfig_Load(AeronVfs* vfs, char* error, size_t capacity);
int XvtConfig_Replace(char* error, size_t capacity);
void XvtConfig_Shutdown(void);
const AeronConfigFile* XvtConfig_UserDocument(void);
/* Merged game/user tree. Typed settings also include the Aeron scene baseline. */
const AeronConfigFile* XvtConfig_ResolvedDocument(void);
const XvtSettings* XvtConfig_Settings(void);
const XvtSettings* XvtConfig_DefaultSettings(void);
uint64_t XvtConfig_Generation(void);
int XvtConfig_CanReplace(void);
/* Copies and validates overrides before optional atomic USER/config.yaml save. */
int XvtConfig_UpdateUser(const AeronConfigFile* candidate, int save, char* error, size_t capacity);
bool XvtConfig_SetController(const XvtControllerOptions* options, char* error, size_t capacity);
bool XvtConfig_SetKeyboard(const XvtKeyboardBindings* bindings, char* error, size_t capacity);
bool XvtConfig_RestoreKeyboard(char* error, size_t capacity);
int XvtConfig_SetGameData(const char* path, int save, char* error, size_t capacity);
/* Updates the preference consumed at the next mission start; Save persists it. */
bool XvtConfig_SetFlightRate(bool unlocked, char* error, size_t capacity);
/* Updates the preference consumed at the next launch; Save persists it. */
bool XvtConfig_SetSkipIntro(bool enabled, char* error, size_t capacity);
bool XvtConfig_SetTouchControls(int mode, char* error, size_t capacity);
/* 0 restores the shipped solid look. Values above 1 are rejected. */
bool XvtConfig_SetTouchTransparency(float transparency, char* error, size_t capacity);
/* action == XVT_INPUT_ACTION_NONE restores that button's shipped command. */
bool XvtConfig_SetTouchButton(int index, int action, char* error, size_t capacity);
bool XvtConfig_ClearTouchButtons(char* error, size_t capacity);
/* x or y below zero restores that control's shipped position. */
bool XvtConfig_SetTouchPlaces(const float* x, const float* y, char* error, size_t capacity);
bool XvtConfig_ClearTouchPlaces(char* error, size_t capacity);
int XvtConfig_Apply(GameConfig* game, char* error, size_t capacity);
int XvtConfig_Write(const GameConfig* game, char* error, size_t capacity);
int XvtConfig_Save(char* error, size_t capacity);
int XvtConfig_Import(const char* path, char* error, size_t capacity);
#ifdef __cplusplus
}
#endif

#endif
