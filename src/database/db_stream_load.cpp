#include "database.h"
#ifdef BMK4_ORACLE1
#include <bmk4_oracle1_instr.h>
#endif
#line 2


void __cdecl Load_Stream(bool atStreamStart, uint8_t *ptr, int32_t size)
{
    iassert(atStreamStart == (ptr == DB_GetStreamPos()));
    if (atStreamStart && size)
    {
#ifdef BMK4_ORACLE1
        Bmk4Or1_Fill(ptr, size);
#endif
#line 9
        if (g_streamPosIndex - 1 < 3)
        {
            if (g_streamPosIndex == 1)
            {
                memset(ptr, 0, size);
            }
            else
            {
                if (g_streamDelayIndex >= 0x1000)
                    MyAssertHandler(
                        ".\\database\\db_stream_load.cpp",
                        33,
                        0,
                        "g_streamDelayIndex doesn't index ARRAY_COUNT( g_streamDelayArray )\n\t%i not in [0, %i)",
                        g_streamDelayIndex,
                        4096);
                g_streamDelayArray[g_streamDelayIndex].ptr = ptr;
                g_streamDelayArray[g_streamDelayIndex++].size = size;
            }
        }
        else
        {
            DB_LoadXFileData(ptr, size);
        }
        DB_IncStreamPos(size);
    }
}

void __cdecl Load_DelayStream()
{
    uint32_t index; // [esp+4h] [ebp-8h]

    for (index = 0; index < g_streamDelayIndex; ++index)
        DB_LoadXFileData((unsigned char*)g_streamDelayArray[index].ptr, g_streamDelayArray[index].size);
}

void __cdecl DB_ConvertOffsetToAlias(uint32_t *data)
{
    uint32_t offset; // [esp+0h] [ebp-8h]

    offset = *data;
#ifdef BMK4_ORACLE1
    Bmk4Or1_PtrAlias(offset);
#endif
#line 50
    iassert((offset && (offset != -1) && (offset != -2)));
    *data = *(uint32_t *)&g_streamZoneMem->blocks[(offset - 1) >> 28].data[(offset - 1) & 0xFFFFFFF];
}

void __cdecl DB_ConvertOffsetToPointer(uint32_t *data)
{
#ifdef BMK4_ORACLE1
    Bmk4Or1_PtrOffset(*data);
#endif
#line 56
    *data = (uint32_t)&g_streamZoneMem->blocks[(uint32_t)(*data - 1) >> 28].data[(*data - 1) & 0xFFFFFFF];
}

void __cdecl Load_XStringCustom(char **str)
{
    uint8_t *pos; // [esp+0h] [ebp-8h]
    char *s; // [esp+4h] [ebp-4h]

    s = *str;
    for (pos = (uint8_t *)*str; ; ++pos)
    {
        DB_LoadXFileData(pos, 1u);
        if (!*pos)
            break;
    }
    DB_IncStreamPos(pos - (uint8_t *)s + 1);
}

void __cdecl Load_TempStringCustom(char **str)
{
    const char * string; // [esp+0h] [ebp-4h]

    Load_XStringCustom(str);
    if (*str)
        string = (const char*)SL_GetString(*str, 4u); // KISAKTODO: this seems way wrong but it's what the decomp is showing
    else
        string= 0;
    *str = (char *)string;
}

