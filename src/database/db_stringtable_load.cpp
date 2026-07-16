#include "database.h"
#ifdef BMK4_ORACLE1
#include <bmk4_oracle1_instr.h>
#endif
#line 2

void __cdecl Load_ScriptStringCustom(uint16_t *var)
{
#ifdef BMK4_ORACLE1
    const uint16_t bmk4Or1IndexBefore = *var;
#endif
#line 5
    *var = (uint16_t)varXAssetList->stringList.strings[*var];
#ifdef BMK4_ORACLE1
    Bmk4Or1_ScriptStringRemap(bmk4Or1IndexBefore, *var);
#endif
#line 6
}

void __cdecl Mark_ScriptStringCustom(uint16_t *var)
{
    if (*var)
        SL_AddUser(*var, 4u);
}

