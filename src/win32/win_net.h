#pragma once

#ifdef KISAK_MP
#include <qcommon/net_chan_mp.h>
#elif KISAK_SP
#include <qcommon/net_chan.h>
#endif

#include <universal/q_shared.h>

void		NET_Init(void);
void		NET_Shutdown(void);
void		NET_Restart(void);
void		NET_Config(bool enableNetworking);
void		NET_SendPacket(netsrc_t sock, int length, const void* data, netadr_t to);
const char* NET_ErrorString(void);
void		NET_Sleep(int msec);

#ifdef KISAK_IOS
// Temporary Stage B3 policy: the headless no-assets lane owns one loopback
// UDP socket and refuses every non-loopback send. Later LAN work must opt out
// explicitly rather than silently broadening CI's network surface.
void NET_iOS_SetLoopbackOnly(bool enabled);
bool NET_iOS_IsLoopbackOnly(void);
int NET_iOS_GetOpenSocketCount(void);
#endif

uint32_t __cdecl NET_TCPIPSocket(const char *net_interface, int port, int type);

qboolean Sys_StringToAdr(const char *s, netadr_t *a);
char __cdecl Sys_SendPacket(int length, unsigned __int8 *data, netadr_t to);
