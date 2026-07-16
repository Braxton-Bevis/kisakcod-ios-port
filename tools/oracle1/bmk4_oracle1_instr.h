// BMK4 Oracle 1 instrumentation hook surface.
//
// Included by engine database TUs ONLY inside `#ifdef BMK4_ORACLE1` guards;
// shipping targets (KisakCOD-sp/mp/dedi) never define BMK4_ORACLE1 and never
// see this file. Deliberately header-light: plain types only, no engine
// headers, so the guarded include cannot perturb engine translation.
//
// Implementations live in tools/oracle1/oracle1_trace.cpp, which reads the
// real db_stream globals (g_streamPos, g_streamPosIndex, g_streamZoneMem)
// to attribute every event to a (block, offset) pair. No raw pointer value
// ever reaches the trace.

#pragma once

// db_stream.cpp
void Bmk4Or1_StreamPush(unsigned int requestedIndex);
void Bmk4Or1_StreamPop();
void Bmk4Or1_Alloc(int alignment);
void Bmk4Or1_Inc(int size);
void Bmk4Or1_InsertPointer(const void **slot);

// db_stream_load.cpp
void Bmk4Or1_Fill(const unsigned char *ptr, int size);
void Bmk4Or1_PtrOffset(unsigned int token);
void Bmk4Or1_PtrAlias(unsigned int token);

// db_file_load.cpp
void Bmk4Or1_XFileData(const unsigned char *pos, unsigned int size);
void Bmk4Or1_XFileHeader(const void *xfile);
void Bmk4Or1_AssetList(const void *assetList);

// db_load.cpp
void Bmk4Or1_AssetDispatch(int type);

// db_stringtable_load.cpp
void Bmk4Or1_ScriptStringRemap(unsigned int indexBefore, unsigned int handleAfter);

// db_registry.cpp
void Bmk4Or1_AssetInsert(int type, const void *loadedData, const void *linkedAsset);

// tools/oracle1 internal (scaffold + driver side)
void Bmk4Or1_SlIntern(unsigned int handle, const char *text);
void Bmk4Or1_Error(const char *kind, const char *detail);
