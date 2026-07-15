// Phase 3 Stage B3: prove the real Netchan/NET initialization and the real
// msg + in-memory loopback path. The marker is formatted only after a packet
// survives encode, queue transfer, and decode with the socket policy intact.

#include <cstdio>
#include <cstring>

#include <qcommon/net_chan_mp.h>

bool Com_iOS_BootNetInitialized();
void NET_Init();
bool NET_iOS_IsLoopbackOnly();
int NET_iOS_GetOpenSocketCount();

extern "C" const char *kisak_boot_net_smoke(void)
{
    static char status[512];
    static const int payloadMagic = 0x42334E45; // "B3NE"
    static const char payloadText[] = "bmk4-b3-loopback";
    uint8_t outgoingData[128];
    uint8_t incomingData[128];
    msg_t outgoing;
    msg_t incoming;
    netadr_t destination = {};
    netadr_t forbiddenDestination = {};
    netadr_t source = {};

    if (!Com_iOS_BootNetInitialized() || !NET_iOS_IsLoopbackOnly())
    {
        snprintf(status, sizeof(status), "network FAIL: B3 init fence/policy");
        return status;
    }

    const int socketCount = NET_iOS_GetOpenSocketCount();
    if (socketCount != 1)
    {
        snprintf(status, sizeof(status),
                 "network FAIL: loopback socket count=%d expected=1", socketCount);
        return status;
    }

    fprintf(stderr, "KISAK_NET_BREADCRUMB msg-init enter\n");
    fflush(stderr);
    MSG_Init(&outgoing, outgoingData, sizeof(outgoingData));
    fprintf(stderr, "KISAK_NET_BREADCRUMB msg-init complete\n");
    fflush(stderr);
    MSG_WriteLong(&outgoing, payloadMagic);
    MSG_WriteString(&outgoing, payloadText);
    if (outgoing.overflowed || outgoing.cursize <= 4)
    {
        snprintf(status, sizeof(status), "network FAIL: msg encode");
        return status;
    }

    // TEST-NET-1 is deliberately non-loopback. The policy must reject it
    // before BSD sendto, proving this CI path cannot emit an outbound packet.
    forbiddenDestination.type = NA_IP;
    forbiddenDestination.ip[0] = 192;
    forbiddenDestination.ip[1] = 0;
    forbiddenDestination.ip[2] = 2;
    forbiddenDestination.ip[3] = 1;
    if (NET_SendPacket(NS_CLIENT1, outgoing.cursize, outgoing.data,
                       forbiddenDestination))
    {
        snprintf(status, sizeof(status), "network FAIL: outbound refusal");
        return status;
    }

    destination.type = NA_LOOPBACK;
    destination.port = NS_CLIENT1;
    if (!NET_SendPacket(NS_CLIENT1, outgoing.cursize, outgoing.data, destination))
    {
        snprintf(status, sizeof(status), "network FAIL: loopback enqueue");
        return status;
    }

    MSG_Init(&incoming, incomingData, sizeof(incomingData));
    if (!NET_GetLoopPacket(NS_SERVER, &source, &incoming))
    {
        snprintf(status, sizeof(status), "network FAIL: loopback receive");
        return status;
    }
    MSG_BeginReading(&incoming);
    const int decodedMagic = MSG_ReadLong(&incoming);
    const char *decodedText = MSG_ReadString(&incoming);
    if (source.type != NA_LOOPBACK || incoming.overflowed
        || decodedMagic != payloadMagic || !decodedText
        || strcmp(decodedText, payloadText) != 0)
    {
        snprintf(status, sizeof(status), "network FAIL: msg decode/readback");
        return status;
    }
    if (NET_GetLoopPacket(NS_SERVER, &source, &incoming))
    {
        snprintf(status, sizeof(status), "network FAIL: loopback queue not drained");
        return status;
    }

    snprintf(status, sizeof(status),
             "Netchan+NET_Init OK — loopback only, %d sockets, msg subsystem up",
             socketCount);
    return status;
}
