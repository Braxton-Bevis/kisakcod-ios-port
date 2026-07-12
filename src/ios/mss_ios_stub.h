#pragma once
// iOS replacement for <msslib/mss.h> (Miles Sound System 7.2e — binary-only Win32-x86
// dependency, no arm64/iOS library exists; DEPENDENCY_MAP.md §9).
//
// This is the PRE-AVAudioEngine typed stub: it re-declares only the Miles types and
// the AIL_* entry points the engine actually names (src/sound/*, snd.cpp reaches 3
// more via _AILMIXINFO, r_cinematic.cpp needs HDIGDRIVER), with LP64-correct scalar
// widths, so the sound layer compiles unmodified. The matching implementations in
// src/ios/snd_ios_stub.cpp are explicit no-ops: AIL_startup() reports failure, so
// SND_InitDriver() bails out through the engine's own MSS_InitFailed() path and the
// game runs silent. Replace snd_ios_stub.cpp with the AVAudioEngine shim; this
// header's declarations are the contract that shim must implement.

#ifndef KISAK_IOS
#error mss_ios_stub.h is the KISAK_IOS replacement for msslib/mss.h; win32 builds use the real header
#endif

#include <cstdint>
#include <ios/win32_tags.h> // DXVK windows_base.h typedefs (HWND = void*)

// --- calling-convention / storage decorations (all empty on arm64) ----------
#ifndef FAR
#define FAR
#endif
#define AILCALL
#define AILEXPORT
#define AILCALLBACK
#define DXDEC extern

// --- Miles scalar types (mss.h:1128-1319, LP64-corrected) -------------------
// mss.h:1278 defines UINTa as MSVC `__w64 unsigned long` (pointer-width int);
// on LP64 Darwin that is uintptr_t (DEPENDENCY_MAP.md §12).
typedef char      C8;
typedef int8_t    S8;
typedef uint8_t   U8;
typedef int16_t   S16;
typedef uint16_t  U16;
typedef int32_t   S32;
typedef uint32_t  U32;
typedef float     F32;
typedef double    F64;
typedef uintptr_t UINTa;
typedef intptr_t  SINTa;

// --- handle types (mss.h:2540-2550,5286,1966-1994) --------------------------
// The engine only ever passes _SAMPLE/_STREAM/_DIG_DRIVER around as pointers
// (snd_local.h MssLocal, HSTREAM casts in snd_driver.cpp), so the structs stay
// opaque here; the stub .cpp never dereferences them either.
typedef struct _DIG_DRIVER FAR *HDIGDRIVER; // Handle to digital driver
typedef struct _SAMPLE FAR *HSAMPLE;        // Handle to sample
typedef struct _STREAM FAR *HSTREAM;        // Handle to stream
typedef S32 HTIMER;                         // Handle to timer
typedef U32 HPROVIDER;                      // RIB provider handle (AIL_find_filter target)
typedef UINTa HPROPERTY;
typedef U32 HPROENUM;
#define HPROENUM_FIRST 0
typedef SINTa HDRIVERSTATE;                 // AIL_open_filter result

// --- sample/stream status values (mss.h:1045-1054) --------------------------
// snd_driver.cpp compares AIL_sample_status()/AIL_stream_status() against the
// literal 2 (SMP_DONE); the stub returns SMP_DONE so one-shots free immediately.
#define SMP_FREE               0x0001 // Sample is available for allocation
#define SMP_DONE               0x0002 // Sample has finished playing, or has never been started
#define SMP_PLAYING            0x0004 // Sample is playing
#define SMP_STOPPED            0x0008 // Sample has been stopped
#define SMP_PLAYINGBUTRELEASED 0x0010 // Sample is playing, but digital handle released

// --- multichannel output spec (mss.h:3406-3426) -----------------------------
typedef enum
{
    MSS_MC_INVALID             = 0,    // Used for configuration-function errors
    MSS_MC_MONO                = 1,    // For compatibility with S32 channel param
    MSS_MC_STEREO              = 2,
    MSS_MC_USE_SYSTEM_CONFIG   = 0x10,
    MSS_MC_HEADPHONES          = 0x20,
    MSS_MC_DOLBY_SURROUND      = 0x30,
    MSS_MC_SRS_CIRCLE_SURROUND = 0x40,
    MSS_MC_40_DTS              = 0x48,
    MSS_MC_40_DISCRETE         = 0x50,
    MSS_MC_51_DTS              = 0x58,
    MSS_MC_51_DISCRETE         = 0x60,
    MSS_MC_61_DISCRETE         = 0x70,
    MSS_MC_71_DISCRETE         = 0x80,
    MSS_MC_81_DISCRETE         = 0x90,
    MSS_MC_DIRECTSOUND3D       = 0xA0,
    MSS_MC_EAX2                = 0xC0,
    MSS_MC_EAX3                = 0xD0,
    MSS_MC_EAX4                = 0xE0
} MSS_MC_SPEC;

// --- speaker indexes (mss.h:902-924) -----------------------------------------
// snd_driver.cpp's SND_ApplyChannelMap builds MSS_SPEAKER_FRONT_CENTER lists.
typedef enum
{
    MSS_SPEAKER_FRONT_LEFT            = 0,
    MSS_SPEAKER_FRONT_RIGHT           = 1,
    MSS_SPEAKER_FRONT_CENTER          = 2,
    MSS_SPEAKER_LOW_FREQUENCY         = 3,
    MSS_SPEAKER_BACK_LEFT             = 4,
    MSS_SPEAKER_BACK_RIGHT            = 5,
    MSS_SPEAKER_FRONT_LEFT_OF_CENTER  = 6,
    MSS_SPEAKER_FRONT_RIGHT_OF_CENTER = 7,
    MSS_SPEAKER_BACK_CENTER           = 8,
    MSS_SPEAKER_SIDE_LEFT             = 9,
    MSS_SPEAKER_SIDE_RIGHT            = 10,
    MSS_SPEAKER_TOP_CENTER            = 11,
    MSS_SPEAKER_TOP_FRONT_LEFT        = 12,
    MSS_SPEAKER_TOP_FRONT_CENTER      = 13,
    MSS_SPEAKER_TOP_FRONT_RIGHT       = 14,
    MSS_SPEAKER_TOP_BACK_LEFT         = 15,
    MSS_SPEAKER_TOP_BACK_CENTER       = 16,
    MSS_SPEAKER_TOP_BACK_RIGHT        = 17,
    MSS_SPEAKER_MAX_INDEX             = 17
} MSS_SPEAKER;

// --- sample pipeline stages (mss.h:2734-2751) --------------------------------
typedef enum
{
    SP_ASI_DECODER = 0,          // Must be "ASI codec stream" provider
    SP_FILTER,                   // Must be "MSS pipeline filter" provider
    SP_FILTER_0 = SP_FILTER,
    SP_FILTER_1,
    SP_FILTER_2,
    SP_FILTER_3,
    SP_FILTER_4,
    SP_FILTER_5,
    SP_FILTER_6,
    SP_FILTER_7,
    SP_MERGE,                    // Must be "MSS mixer" provider
    N_SAMPLE_STAGES,
    SP_OUTPUT = N_SAMPLE_STAGES,
    SAMPLE_ALL_STAGES
} SAMPLESTAGE;

// --- 3D vector (mss.h:2996-3001) ---------------------------------------------
typedef struct _MSSVECTOR3D
{
    F32 x;
    F32 y;
    F32 z;
} MSSVECTOR3D;

// --- sound-data descriptors (mss.h:1753-1764,2499-2513,3230-3236) ------------
// snd.cpp:SND_SetData fills _AILMIXINFO.Info field-by-field and calls
// AIL_size/process_digital_audio; snd_driver*.cpp stack-allocates _AILSOUNDINFO.
typedef struct _AILSOUNDINFO
{
    S32 format;
    void const FAR *data_ptr;
    U32 data_len;
    U32 rate;
    S32 bits;
    S32 channels;
    U32 channel_mask;
    U32 samples;
    U32 block_size;
    void const FAR *initial_ptr;
} AILSOUNDINFO;

typedef struct _ADPCMDATATAG
{
    U32   blocksize;
    U32   extrasamples;
    U32   blockleft;
    U32   step;
    UINTa savesrc;
    U32   sample;
    UINTa destend;
    UINTa srcend;
    U32   samplesL;
    U32   samplesR;
    U16   moresamples[16];
} ADPCMDATA;

typedef struct _AILMIXINFO
{
    AILSOUNDINFO Info;
    ADPCMDATA mss_adpcm;
    U32 src_fract;
    S32 left_val;
    S32 right_val;
} AILMIXINFO;

// --- engine file callbacks (mss.h:5416-5448) ---------------------------------
typedef char MSS_FILE;

typedef U32  (AILCALLBACK FAR *AIL_file_open_callback)(MSS_FILE const FAR *Filename, UINTa FAR *FileHandle);
typedef void (AILCALLBACK FAR *AIL_file_close_callback)(UINTa FileHandle);
typedef S32  (AILCALLBACK FAR *AIL_file_seek_callback)(UINTa FileHandle, S32 offset, U32 type);
typedef U32  (AILCALLBACK FAR *AIL_file_read_callback)(UINTa FileHandle, void FAR *Buffer, U32 Bytes);

// -----------------------------------------------------------------------------
// AIL_* entry points the engine calls (54 across src/sound + snd.cpp; prototypes
// mirror mss.h with FAR/AILCALL flattened). Implemented as no-ops in
// src/ios/snd_ios_stub.cpp until the AVAudioEngine shim lands.
// -----------------------------------------------------------------------------

// startup / driver (mss.h:4334-4453,4599-4622)
DXDEC S32          AILCALL AIL_startup(void);
DXDEC void         AILCALL AIL_shutdown(void);
DXDEC SINTa        AILCALL AIL_set_preference(U32 number, SINTa value);
DXDEC char FAR *   AILCALL AIL_last_error(void);
DXDEC char FAR *   AILCALL AIL_set_redist_directory(char const FAR *dir);
DXDEC void         AILCALL AIL_set_file_callbacks(AIL_file_open_callback opencb,
                                                  AIL_file_close_callback closecb,
                                                  AIL_file_seek_callback seekcb,
                                                  AIL_file_read_callback readcb);
DXDEC HDIGDRIVER   AILCALL AIL_open_digital_driver(U32 frequency, S32 bits, S32 channel, U32 flags);
DXDEC S32          AILCALL AIL_digital_CPU_percent(HDIGDRIVER dig);
DXDEC MSSVECTOR3D FAR *AILCALL AIL_speaker_configuration(HDIGDRIVER dig,
                                                         S32 FAR *n_physical_channels,
                                                         S32 FAR *n_logical_channels,
                                                         F32 FAR *falloff_power,
                                                         MSS_MC_SPEC FAR *channel_spec);
DXDEC void         AILCALL AIL_set_speaker_configuration(HDIGDRIVER dig,
                                                         MSSVECTOR3D FAR *array,
                                                         S32 n_channels,
                                                         F32 falloff_power);
DXDEC void         AILCALL AIL_set_3D_distance_factor(HDIGDRIVER dig, F32 factor);
DXDEC void         AILCALL AIL_set_3D_rolloff_factor(HDIGDRIVER dig, F32 factor);
DXDEC S32          AILCALL AIL_set_DirectSound_HWND(HDIGDRIVER dig, HWND wnd);

// samples (mss.h:4607-4790,4955-4960)
DXDEC HSAMPLE      AILCALL AIL_allocate_sample_handle(HDIGDRIVER dig);
DXDEC S32          AILCALL AIL_init_sample(HSAMPLE S, S32 format);
DXDEC S32          AILCALL AIL_set_sample_info(HSAMPLE S, AILSOUNDINFO const FAR *info);
DXDEC void         AILCALL AIL_set_sample_volume_levels(HSAMPLE S, F32 left_level, F32 right_level);
DXDEC void         AILCALL AIL_sample_volume_levels(HSAMPLE S, F32 FAR *left_level, F32 FAR *right_level);
DXDEC void         AILCALL AIL_set_sample_channel_levels(HSAMPLE S,
                                                         MSS_SPEAKER FAR const *source_speaker_indexes,
                                                         MSS_SPEAKER FAR const *dest_speaker_indexes,
                                                         F32 FAR const *levels,
                                                         S32 n_levels);
DXDEC void         AILCALL AIL_sample_channel_levels(HSAMPLE S,
                                                     MSS_SPEAKER FAR const *source_speaker_indexes,
                                                     MSS_SPEAKER FAR const *dest_speaker_indexes,
                                                     F32 FAR *levels,
                                                     S32 n_levels);
DXDEC S32          AILCALL AIL_sample_channel_count(HSAMPLE S, U32 *mask);
DXDEC void         AILCALL AIL_sample_volume_pan(HSAMPLE S, F32 FAR *volume, F32 FAR *pan);
DXDEC void         AILCALL AIL_set_sample_playback_rate(HSAMPLE S, S32 playback_rate);
DXDEC S32          AILCALL AIL_sample_playback_rate(HSAMPLE S);
DXDEC void         AILCALL AIL_set_sample_loop_count(HSAMPLE S, S32 loop_count);
DXDEC void         AILCALL AIL_set_sample_ms_position(HSAMPLE S, S32 milliseconds);
DXDEC void         AILCALL AIL_sample_ms_position(HSAMPLE S,
                                                  S32 FAR *total_milliseconds,
                                                  S32 FAR *current_milliseconds);
DXDEC U32          AILCALL AIL_sample_status(HSAMPLE S);
DXDEC void         AILCALL AIL_stop_sample(HSAMPLE S);
DXDEC void         AILCALL AIL_resume_sample(HSAMPLE S);
DXDEC void         AILCALL AIL_end_sample(HSAMPLE S);
DXDEC void         AILCALL AIL_set_sample_reverb_levels(HSAMPLE S, F32 dry_level, F32 wet_level);
DXDEC HPROVIDER    AILCALL AIL_set_sample_processor(HSAMPLE S, SAMPLESTAGE pipeline_stage, HPROVIDER provider);
DXDEC S32          AILCALL AIL_sample_stage_property(HSAMPLE S,
                                                     SAMPLESTAGE stage,
                                                     C8 const FAR *name,
                                                     S32 channel,
                                                     void FAR *before_value,
                                                     void const FAR *new_value,
                                                     void FAR *after_value);

// 3D positioning (mss.h:6082-6130)
DXDEC void         AILCALL AIL_set_sample_3D_position(HSAMPLE obj, F32 X, F32 Y, F32 Z);
DXDEC S32          AILCALL AIL_sample_3D_position(HSAMPLE obj, F32 FAR *X, F32 FAR *Y, F32 FAR *Z);
DXDEC void         AILCALL AIL_set_sample_3D_distances(HSAMPLE S, F32 max_dist, F32 min_dist, S32 auto_3D_wet_atten);

// reverb / room (mss.h:4855,6049)
DXDEC void         AILCALL AIL_set_room_type(HDIGDRIVER dig, S32 room_type);
DXDEC void         AILCALL AIL_set_digital_master_reverb_levels(HDIGDRIVER dig, F32 dry_level, F32 wet_level);

// pipeline filters (mss.h:6319-6328)
DXDEC S32          AILCALL AIL_enumerate_filters(HPROENUM FAR *next, HPROVIDER FAR *dest, C8 FAR * FAR *name);
DXDEC S32          AILCALL AIL_find_filter(C8 const *name, HPROVIDER *ret);
DXDEC HDRIVERSTATE AILCALL AIL_open_filter(HPROVIDER lib, HDIGDRIVER dig);

// streams (mss.h:5368-5412)
DXDEC HSTREAM      AILCALL AIL_open_stream(HDIGDRIVER dig, char const FAR *filename, S32 stream_mem);
DXDEC void         AILCALL AIL_close_stream(HSTREAM stream);
DXDEC void         AILCALL AIL_pause_stream(HSTREAM stream, S32 onoff);
DXDEC S32          AILCALL AIL_stream_status(HSTREAM stream);
DXDEC void         AILCALL AIL_stream_info(HSTREAM stream, S32 FAR *datarate, S32 FAR *sndtype, S32 FAR *length, S32 FAR *memory);
DXDEC HSAMPLE      AILCALL AIL_stream_sample_handle(HSTREAM stream);
DXDEC void         AILCALL AIL_stream_ms_position(HSTREAM S,
                                                  S32 FAR *total_milliseconds,
                                                  S32 FAR *current_milliseconds);
DXDEC void         AILCALL AIL_set_stream_ms_position(HSTREAM S, S32 milliseconds);
DXDEC void         AILCALL AIL_set_stream_loop_count(HSTREAM stream, S32 count);

// wave parsing / format conversion (mss.h:5734-5748)
DXDEC S32          AILCALL AIL_WAV_info(void const FAR *data, AILSOUNDINFO FAR *info);
DXDEC S32          AILCALL AIL_size_processed_digital_audio(U32 dest_rate,
                                                            U32 dest_format,
                                                            S32 num_srcs,
                                                            AILMIXINFO const FAR *src);
DXDEC S32          AILCALL AIL_process_digital_audio(void FAR *dest_buffer,
                                                     S32 dest_buffer_size,
                                                     U32 dest_rate,
                                                     U32 dest_format,
                                                     S32 num_srcs,
                                                     AILMIXINFO FAR *src);
