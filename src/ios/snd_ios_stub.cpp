// PRE-AVAudioEngine Miles Sound System stub (DEPENDENCY_MAP.md §9).
//
// mss32.dll is a binary-only Win32-x86 dependency with no arm64/iOS build, so
// every AIL_* entry point the engine calls is implemented here as an explicit,
// commented no-op. The design contract:
//
//   * AIL_startup() returns 0 ("failed"), so SND_InitDriver (snd_driver.cpp:43)
//     takes the engine's own failure path — MSS_InitFailed() prints one line,
//     g_snd.Initialized2d stays 0, and every higher-level SND_* call no-ops
//     behind its existing `if (g_snd.Initialized2d)` guards. The game runs
//     silent; nothing below ever dereferences a Miles handle.
//   * Everything else still returns a safe value (NULL handles, SMP_DONE
//     status, zeroed out-params) so any call that slips through a guard is
//     benign rather than undefined.
//
// The AVAudioEngine shim that replaces this file must implement the same
// declarations in src/ios/mss_ios_stub.h (per-channel AVAudioPlayerNode +
// AVAudioUnitTimePitch, AVAudioEnvironmentNode for the 3D channels,
// AVAudioUnitEQ for the milesEq filter — see DEPENDENCY_MAP.md §9).

#ifdef KISAK_IOS

#include <ios/mss_ios_stub.h>
#include <cstring>

// startup / driver ------------------------------------------------------------

S32 AIL_startup(void)
{
    return 0; // "startup failed" -> SND_InitDriver bails via MSS_InitFailed(); sound off until the AVAudioEngine shim
}

void AIL_shutdown(void)
{
    // no-op: nothing was started
}

SINTa AIL_set_preference(U32 number, SINTa value)
{
    (void)number;
    return value; // Miles returns the previous value; pretend it was already set
}

char *AIL_last_error(void)
{
    // AIL_startup() failing routes MSS_Init's Com_PrintError here
    return (char *)"Miles Sound System is not available on iOS (pre-AVAudioEngine stub)";
}

char *AIL_set_redist_directory(char const *dir)
{
    // no-op: .asi/.flt plugin DLL loading is impossible on iOS (unsigned dynamic
    // code); the shim statically links its codecs/DSP instead
    return (char *)dir;
}

void AIL_set_file_callbacks(AIL_file_open_callback opencb,
                            AIL_file_close_callback closecb,
                            AIL_file_seek_callback seekcb,
                            AIL_file_read_callback readcb)
{
    // no-op today, but the FS-callback design (MSS_File*Callback -> FS_*) is
    // portable and the AVAudioEngine shim should store and reuse these
    (void)opencb; (void)closecb; (void)seekcb; (void)readcb;
}

HDIGDRIVER AIL_open_digital_driver(U32 frequency, S32 bits, S32 channel, U32 flags)
{
    (void)frequency; (void)bits; (void)channel; (void)flags;
    return 0; // unreachable while AIL_startup() fails; NULL keeps MSS_Init on its error path regardless
}

S32 AIL_digital_CPU_percent(HDIGDRIVER dig)
{
    (void)dig;
    return 0;
}

MSSVECTOR3D *AIL_speaker_configuration(HDIGDRIVER dig,
                                       S32 *n_physical_channels,
                                       S32 *n_logical_channels,
                                       F32 *falloff_power,
                                       MSS_MC_SPEC *channel_spec)
{
    (void)dig;
    if (n_physical_channels)
        *n_physical_channels = 2; // stereo: keeps milesGlob.isMultiChannel false
    if (n_logical_channels)
        *n_logical_channels = 2;
    if (falloff_power)
        *falloff_power = 0.0f;
    if (channel_spec)
        *channel_spec = MSS_MC_STEREO;
    return 0;
}

void AIL_set_speaker_configuration(HDIGDRIVER dig, MSSVECTOR3D *array, S32 n_channels, F32 falloff_power)
{
    (void)dig; (void)array; (void)n_channels; (void)falloff_power;
}

void AIL_set_3D_distance_factor(HDIGDRIVER dig, F32 factor)
{
    (void)dig; (void)factor;
}

void AIL_set_3D_rolloff_factor(HDIGDRIVER dig, F32 factor)
{
    (void)dig; (void)factor;
}

S32 AIL_set_DirectSound_HWND(HDIGDRIVER dig, HWND wnd)
{
    // no window/DirectSound binding exists on iOS (DEPENDENCY_MAP.md §9,
    // SND_SetHWND row): the shim binds an AVAudioSession instead
    (void)dig; (void)wnd;
    return 0;
}

// samples ---------------------------------------------------------------------

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER dig)
{
    (void)dig;
    // NULL would Com_Error in MSS_InitChannels, but that is only reachable
    // after a successful MSS_Init, which this stub never reports
    return 0;
}

S32 AIL_init_sample(HSAMPLE S, S32 format)
{
    (void)S; (void)format;
    return 0;
}

S32 AIL_set_sample_info(HSAMPLE S, AILSOUNDINFO const *info)
{
    (void)S; (void)info;
    return 0;
}

void AIL_set_sample_volume_levels(HSAMPLE S, F32 left_level, F32 right_level)
{
    (void)S; (void)left_level; (void)right_level;
}

void AIL_sample_volume_levels(HSAMPLE S, F32 *left_level, F32 *right_level)
{
    (void)S;
    if (left_level)
        *left_level = 0.0f;
    if (right_level)
        *right_level = 0.0f;
}

void AIL_set_sample_channel_levels(HSAMPLE S,
                                   MSS_SPEAKER const *source_speaker_indexes,
                                   MSS_SPEAKER const *dest_speaker_indexes,
                                   F32 const *levels,
                                   S32 n_levels)
{
    (void)S; (void)source_speaker_indexes; (void)dest_speaker_indexes; (void)levels; (void)n_levels;
}

void AIL_sample_channel_levels(HSAMPLE S,
                               MSS_SPEAKER const *source_speaker_indexes,
                               MSS_SPEAKER const *dest_speaker_indexes,
                               F32 *levels,
                               S32 n_levels)
{
    (void)S; (void)source_speaker_indexes; (void)dest_speaker_indexes;
    if (levels && n_levels > 0)
        memset(levels, 0, sizeof(F32) * (size_t)n_levels);
}

S32 AIL_sample_channel_count(HSAMPLE S, U32 *mask)
{
    (void)S;
    if (mask)
        *mask = 0;
    return 0;
}

void AIL_sample_volume_pan(HSAMPLE S, F32 *volume, F32 *pan)
{
    (void)S;
    if (volume)
        *volume = 0.0f;
    if (pan)
        *pan = 0.5f; // centered
}

void AIL_set_sample_playback_rate(HSAMPLE S, S32 playback_rate)
{
    (void)S; (void)playback_rate;
}

S32 AIL_sample_playback_rate(HSAMPLE S)
{
    (void)S;
    return 22050; // engine multiplies this by pitch; a sane rate avoids 0*x feedback
}

void AIL_set_sample_loop_count(HSAMPLE S, S32 loop_count)
{
    (void)S; (void)loop_count;
}

void AIL_set_sample_ms_position(HSAMPLE S, S32 milliseconds)
{
    (void)S; (void)milliseconds;
}

void AIL_sample_ms_position(HSAMPLE S, S32 *total_milliseconds, S32 *current_milliseconds)
{
    (void)S;
    if (total_milliseconds)
        *total_milliseconds = 0;
    if (current_milliseconds)
        *current_milliseconds = 0;
}

U32 AIL_sample_status(HSAMPLE S)
{
    (void)S;
    return SMP_DONE; // "finished playing": one-shot channels free immediately
}

void AIL_stop_sample(HSAMPLE S)
{
    (void)S;
}

void AIL_resume_sample(HSAMPLE S)
{
    (void)S;
}

void AIL_end_sample(HSAMPLE S)
{
    (void)S;
}

void AIL_set_sample_reverb_levels(HSAMPLE S, F32 dry_level, F32 wet_level)
{
    (void)S; (void)dry_level; (void)wet_level;
}

HPROVIDER AIL_set_sample_processor(HSAMPLE S, SAMPLESTAGE pipeline_stage, HPROVIDER provider)
{
    (void)S; (void)pipeline_stage; (void)provider;
    return 0; // previous provider: none
}

S32 AIL_sample_stage_property(HSAMPLE S,
                              SAMPLESTAGE stage,
                              C8 const *name,
                              S32 channel,
                              void *before_value,
                              void const *new_value,
                              void *after_value)
{
    (void)S; (void)stage; (void)name; (void)channel;
    (void)before_value; (void)new_value; (void)after_value;
    return 0; // property not found (the eq filter is never "loaded" here)
}

// 3D positioning ----------------------------------------------------------------

void AIL_set_sample_3D_position(HSAMPLE obj, F32 X, F32 Y, F32 Z)
{
    (void)obj; (void)X; (void)Y; (void)Z;
}

S32 AIL_sample_3D_position(HSAMPLE obj, F32 *X, F32 *Y, F32 *Z)
{
    (void)obj;
    if (X)
        *X = 0.0f;
    if (Y)
        *Y = 0.0f;
    if (Z)
        *Z = 0.0f;
    return 0;
}

void AIL_set_sample_3D_distances(HSAMPLE S, F32 max_dist, F32 min_dist, S32 auto_3D_wet_atten)
{
    (void)S; (void)max_dist; (void)min_dist; (void)auto_3D_wet_atten;
}

// reverb / room -------------------------------------------------------------------

void AIL_set_room_type(HDIGDRIVER dig, S32 room_type)
{
    (void)dig; (void)room_type;
}

void AIL_set_digital_master_reverb_levels(HDIGDRIVER dig, F32 dry_level, F32 wet_level)
{
    (void)dig; (void)dry_level; (void)wet_level;
}

// pipeline filters ------------------------------------------------------------------

S32 AIL_enumerate_filters(HPROENUM *next, HPROVIDER *dest, C8 **name)
{
    // no filter providers exist: .flt plugins cannot load on iOS. Returning 0
    // ends MSS_InitEq's enumeration loop immediately.
    (void)next; (void)dest; (void)name;
    return 0;
}

S32 AIL_find_filter(C8 const *name, HPROVIDER *ret)
{
    // "3 Band Parm Eq" (milesEq.flt) is not available; MSS_InitEq logs one
    // error and runs without eq. The shim replaces this with AVAudioUnitEQ or
    // milesEq.cpp's biquads compiled in statically.
    (void)name;
    if (ret)
        *ret = 0;
    return 0;
}

HDRIVERSTATE AIL_open_filter(HPROVIDER lib, HDIGDRIVER dig)
{
    (void)lib; (void)dig;
    return 0;
}

// streams -----------------------------------------------------------------------------

HSTREAM AIL_open_stream(HDIGDRIVER dig, char const *filename, S32 stream_mem)
{
    (void)dig; (void)filename; (void)stream_mem;
    return 0; // stream open fails; SND_StartAliasStreamOnChannel handles NULL
}

void AIL_close_stream(HSTREAM stream)
{
    (void)stream;
}

void AIL_pause_stream(HSTREAM stream, S32 onoff)
{
    (void)stream; (void)onoff;
}

S32 AIL_stream_status(HSTREAM stream)
{
    (void)stream;
    return SMP_DONE; // snd_driver.cpp:1840 compares against 2 (SMP_DONE)
}

void AIL_stream_info(HSTREAM stream, S32 *datarate, S32 *sndtype, S32 *length, S32 *memory)
{
    (void)stream;
    if (datarate)
        *datarate = 0;
    if (sndtype)
        *sndtype = 0;
    if (length)
        *length = 0;
    if (memory)
        *memory = 0;
}

HSAMPLE AIL_stream_sample_handle(HSTREAM stream)
{
    (void)stream;
    return 0;
}

void AIL_stream_ms_position(HSTREAM S, S32 *total_milliseconds, S32 *current_milliseconds)
{
    (void)S;
    if (total_milliseconds)
        *total_milliseconds = 0;
    if (current_milliseconds)
        *current_milliseconds = 0;
}

void AIL_set_stream_ms_position(HSTREAM S, S32 milliseconds)
{
    (void)S; (void)milliseconds;
}

void AIL_set_stream_loop_count(HSTREAM stream, S32 count)
{
    (void)stream; (void)count;
}

// wave parsing / format conversion ------------------------------------------------------

S32 AIL_WAV_info(void const *data, AILSOUNDINFO *info)
{
    // no parsing: report failure so LoadObj falls back to its error path
    (void)data;
    if (info)
        memset(info, 0, sizeof(*info));
    return 0;
}

S32 AIL_size_processed_digital_audio(U32 dest_rate, U32 dest_format, S32 num_srcs, AILMIXINFO const *src)
{
    (void)dest_rate; (void)dest_format; (void)num_srcs; (void)src;
    return 0; // 0 bytes: SND_SetData allocates nothing extra and copies nothing
}

S32 AIL_process_digital_audio(void *dest_buffer,
                              S32 dest_buffer_size,
                              U32 dest_rate,
                              U32 dest_format,
                              S32 num_srcs,
                              AILMIXINFO *src)
{
    (void)dest_buffer; (void)dest_buffer_size; (void)dest_rate;
    (void)dest_format; (void)num_srcs; (void)src;
    return 0;
}

#endif // KISAK_IOS
