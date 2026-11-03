#ifndef XVT_RUNTIME_INPUT_CONTROLLER_MAPPING_H
#define XVT_RUNTIME_INPUT_CONTROLLER_MAPPING_H
#include "xvt_runtime/input/controller_options.h"
void XvtControllerMapping_Init(const XvtControllerOptions* options);
void XvtControllerMapping_SetOptions(const XvtControllerOptions* options);
void XvtControllerMapping_ApplyPending(void);
void XvtControllerMapping_Update(const AeronInputSnapshot* input);
void XvtControllerMapping_Suspend(void);
void XvtControllerMapping_Shutdown(void);
const XvtControllerOptions* XvtControllerMapping_Options(void);
int XvtControllerMapping_Axis(XvtInputAxis axis);
uint16_t XvtControllerMapping_Modifiers(void);
uint16_t XvtControllerMapping_ReadKey(void);
void XvtControllerMapping_ReleaseCommands(void);
const AeronControllerSnapshot* XvtControllerMapping_Resolve(const XvtControllerModel* model,
															const AeronInputSnapshot* input,
															uint32_t preferred);
uint32_t XvtControllerMapping_AnalogInstance(const char* guid);
uint16_t XvtControllerMapping_ThrottlePosition(int16_t raw, AeronControllerKind kind, int source,
											   bool invert);
bool XvtControllerMapping_ThrottleSample(uint16_t* position, uint32_t* generation);
int XvtControllerMapping_Present(void);
int XvtControllerMapping_MenuAxis(XvtInputAxis axis);
uint8_t XvtControllerMapping_MenuButtons(void);
uint8_t XvtControllerMapping_MenuHat(void);
void XvtControllerMapping_ReportAction(XvtInputAction action, int pressed);
#endif
