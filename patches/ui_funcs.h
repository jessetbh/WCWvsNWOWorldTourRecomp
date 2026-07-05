#ifndef __UI_FUNCS_INTERNAL_H__
#define __UI_FUNCS_INTERNAL_H__

#include "patch_helpers.h"
#include "recompui_event_structs.h"

// recompui calls this (on the game thread) to run queued UI callbacks. Normally a
// recompiled patch function; until WCW has UI patches it's a no-op defined host-side
// in src/main/main.cpp.
DECLARE_FUNC(void, recomp_run_ui_callbacks);

#endif
