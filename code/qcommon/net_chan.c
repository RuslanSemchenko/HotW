#include "q_shared.h"
#include "qcommon.h"

#define MAX_PACKETLEN           1400
#define FRAGMENT_SIZE           (MAX_PACKETLEN - 100)
#define PACKET_HEADER           10
#define FRAGMENT_BIT    (1<<31)

cvar_t* showpackets;
cvar_t* showdrop;
cvar_t* qport_cvar;

static char* netsrcString[2] = {
    "client",
    "server"
};

void Netchan_Init(int port) {
    port &= 0xffff;
    showpackets = Cvar_Get("showpackets", "0", CVAR_TEMP);
    showdrop = Cvar_Get("showdrop", "0", CVAR_TEMP);
    qport_cvar = Cvar_Get("net_qport", va("%i", port), CVAR_INIT);
}

void Netchan_Setup(netsrc_t sock, netchan_t* chan, netadr_t adr, int qport, int challenge, qboolean compat)
{
    Com_Memset(chan, 0, sizeof(*chan));

    chan->sock = sock;
    chan->remoteAddress = adr;
    chan->qport = qport;
    chan->incomingSequence = 0;
    chan->outgoingSequence = 1;
    chan->challenge = challenge;

#ifdef LEGACY_PROTOCOL
    chan->compat = compat;
#endif
}

void Netchan_TransmitNextFragment(netchan_t* chan) {
    msg_t       send;
    byte        send_buf[MAX_PACKETLEN];
    int         fragmentLength;
    int         outgoingSequence;

    MSG_InitOOB(&send, send_buf, sizeof(send_buf));

    outgoingSequence = chan->outgoingSequence | FRAGMENT_BIT;
    MSG_WriteLong(&send, outgoingSequence);

    if (chan->sock == NS_CLIENT) {
        MSG_WriteShort(&send, qport_cvar->integer);
    }

#ifdef LEGACY_PROTOCOL
    if (!chan->compat)
#endif
        MSG_WriteLong(&send, NETCHAN_GENCHECKSUM(chan->challenge, chan->outgoingSequence));

    fragmentLength = FRAGMENT_SIZE;
    if (chan->unsentFragmentStart + fragmentLength > chan->unsentLength) {
        fragmentLength = chan->unsentLength - chan->unsentFragmentStart;
    }

    MSG_WriteShort(&send, chan->unsentFragmentStart);
    MSG_WriteShort(&send, fragmentLength);
    MSG_WriteData(&send, chan->unsentBuffer + chan->unsentFragmentStart, fragmentLength);

    NET_SendPacket(chan->sock, send.cursize, send.data, chan->remoteAddress);

    chan->lastSentTime = Sys_Milliseconds();
    chan->lastSentSize = send.cursize;

    if (showpackets->integer) {
        Com_Printf("%s send %4i : s=%i fragment=%i,%i\n"
            , netsrcString[chan->sock]
            , send.cursize
            , chan->outgoingSequence
            , chan->unsentFragmentStart, fragmentLength);
    }

    chan->unsentFragmentStart += fragmentLength;

    if (chan->unsentFragmentStart == chan->unsentLength && fragmentLength != FRAGMENT_SIZE) {
        chan->outgoingSequence++;
        chan->unsentFragments = qfalse;
    }
}

void Netchan_Transmit(netchan_t* chan, int length, const byte* data) {
    msg_t       send;
    byte        send_buf[MAX_PACKETLEN];

    if (length > MAX_MSGLEN) {
        Com_Error(ERR_DROP, "Netchan_Transmit: length = %i", length);
    }
    chan->unsentFragmentStart = 0;

    if (length >= FRAGMENT_SIZE) {
        chan->unsentFragments = qtrue;
        chan->unsentLength = length;
        Com_Memcpy(chan->unsentBuffer, data, length);

        Netchan_TransmitNextFragment(chan);
        return;
    }

    MSG_InitOOB(&send, send_buf, sizeof(send_buf));

    MSG_WriteLong(&send, chan->outgoingSequence);

    if (chan->sock == NS_CLIENT)
        MSG_WriteShort(&send, qport_cvar->integer);

#ifdef LEGACY_PROTOCOL
    if (!chan->compat)
#endif
        MSG_WriteLong(&send, NETCHAN_GENCHECKSUM(chan->challenge, chan->outgoingSequence));

    chan->outgoingSequence++;

    MSG_WriteData(&send, data, length);

    NET_SendPacket(chan->sock, send.cursize, send.data, chan->remoteAddress);

    chan->lastSentTime = Sys_Milliseconds();
    chan->lastSentSize = send.cursize;

    if (showpackets->integer) {
        Com_Printf("%s send %4i : s=%i ack=%i\n"
            , netsrcString[chan->sock]
            , send.cursize
            , chan->outgoingSequence - 1
            , chan->incomingSequence);
    }
}

qboolean Netchan_Process(netchan_t* chan, msg_t* msg) {
    int         sequence;
    int         fragmentStart, fragmentLength;
    qboolean    fragmented;

    MSG_BeginReadingOOB(msg);
    sequence = MSG_ReadLong(msg);

    if (sequence & FRAGMENT_BIT) {
        sequence &= ~FRAGMENT_BIT;
        fragmented = qtrue;
    }
    else {
        fragmented = qfalse;
    }

    if (chan->sock == NS_SERVER) {
        MSG_ReadShort(msg);
    }

#ifdef LEGACY_PROTOCOL
    if (!chan->compat)
#endif
    {
        int checksum = MSG_ReadLong(msg);
        if (NETCHAN_GENCHECKSUM(chan->challenge, sequence) != checksum)
            return qfalse;
    }

    if (fragmented) {
        fragmentStart = MSG_ReadShort(msg);
        fragmentLength = MSG_ReadShort(msg);
    }
    else {
        fragmentStart = 0;
        fragmentLength = 0;
    }

    if (showpackets->integer) {
        if (fragmented) {
            Com_Printf("%s recv %4i : s=%i fragment=%i,%i\n"
                , netsrcString[chan->sock]
                , msg->cursize
                , sequence
                , fragmentStart, fragmentLength);
        }
        else {
            Com_Printf("%s recv %4i : s=%i\n"
                , netsrcString[chan->sock]
                , msg->cursize
                , sequence);
        }
    }

    if (sequence <= chan->incomingSequence) {
        if (showdrop->integer || showpackets->integer) {
            Com_Printf("%s:Out of order packet %i at %i\n"
                , NET_AdrToString(chan->remoteAddress)
                , sequence
                , chan->incomingSequence);
        }
        return qfalse;
    }

    chan->dropped = sequence - (chan->incomingSequence + 1);
    if (chan->dropped > 0) {
        if (showdrop->integer || showpackets->integer) {
            Com_Printf("%s:Dropped %i packets at %i\n"
                , NET_AdrToString(chan->remoteAddress)
                , chan->dropped
                , sequence);
        }
    }

    if (fragmented) {
        if (sequence != chan->fragmentSequence) {
            chan->fragmentSequence = sequence;
            chan->fragmentLength = 0;
        }

        if (fragmentStart != chan->fragmentLength) {
            if (showdrop->integer || showpackets->integer) {
                Com_Printf("%s:Dropped a message fragment\n"
                    , NET_AdrToString(chan->remoteAddress));
            }
            return qfalse;
        }

        if (fragmentLength < 0 || msg->readcount + fragmentLength > msg->cursize ||
            chan->fragmentLength + fragmentLength > (int)sizeof(chan->fragmentBuffer)) {
            if (showdrop->integer || showpackets->integer) {
                Com_Printf("%s:illegal fragment length\n"
                    , NET_AdrToString(chan->remoteAddress));
            }
            return qfalse;
        }

        Com_Memcpy(chan->fragmentBuffer + chan->fragmentLength,
            msg->data + msg->readcount, fragmentLength);

        chan->fragmentLength += fragmentLength;

        if (fragmentLength == FRAGMENT_SIZE) {
            return qfalse;
        }

        if (chan->fragmentLength > msg->maxsize) {
            Com_Printf("%s:fragmentLength %i > msg->maxsize\n"
                , NET_AdrToString(chan->remoteAddress),
                chan->fragmentLength);
            return qfalse;
        }

        *(int*)msg->data = LittleLong(sequence);

        Com_Memcpy(msg->data + 4, chan->fragmentBuffer, chan->fragmentLength);
        msg->cursize = chan->fragmentLength + 4;
        chan->fragmentLength = 0;
        msg->readcount = 4;
        msg->bit = 32;

        chan->incomingSequence = sequence;

        return qtrue;
    }

    chan->incomingSequence = sequence;
    return qtrue;
}

//==============================================================================

#define MAX_LOOPBACK    16

typedef struct {
    byte    data[MAX_PACKETLEN];
    int     datalen;
} loopmsg_t;

typedef struct {
    loopmsg_t   msgs[MAX_LOOPBACK];
    int         get, send;
} loopback_t;

loopback_t  loopbacks[2];

qboolean NET_GetLoopPacket(netsrc_t sock, netadr_t* net_from, msg_t* net_message)
{
    int     i;
    loopback_t* loop;

    loop = &loopbacks[sock];

    if (loop->send - loop->get > MAX_LOOPBACK)
        loop->get = loop->send - MAX_LOOPBACK;

    if (loop->get >= loop->send)
        return qfalse;

    i = loop->get & (MAX_LOOPBACK - 1);
    loop->get++;

    Com_Memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
    net_message->cursize = loop->msgs[i].datalen;
    Com_Memset(net_from, 0, sizeof(*net_from));
    net_from->type = NA_LOOPBACK;
    return qtrue;
}

void NET_SendLoopPacket(netsrc_t sock, int length, const void* data, netadr_t to)
{
    int     i;
    loopback_t* loop;

    loop = &loopbacks[sock ^ 1];

    i = loop->send & (MAX_LOOPBACK - 1);
    loop->send++;

    Com_Memcpy(loop->msgs[i].data, data, length);
    loop->msgs[i].datalen = length;
}

//=============================================================================

typedef struct packetQueue_s {
    struct packetQueue_s* next;
    int length;
    byte* data;
    netadr_t to;
    int release;
} packetQueue_t;

packetQueue_t* packetQueue = NULL;

static void NET_QueuePacket(int length, const void* data, netadr_t to, int offset)
{
    packetQueue_t* new, * next = packetQueue;

    if (offset > 999)
        offset = 999;

    new = S_Malloc(sizeof(packetQueue_t));
    new->data = S_Malloc(length);
    Com_Memcpy(new->data, data, length);
    new->length = length;
    new->to = to;
    new->release = Sys_Milliseconds() + (int)((float)offset / com_timescale->value);
    new->next = NULL;

    if (!packetQueue) {
        packetQueue = new;
        return;
    }
    while (next) {
        if (!next->next) {
            next->next = new;
            return;
        }
        next = next->next;
    }
}

void NET_FlushPacketQueue(void)
{
    packetQueue_t* last;
    int now;

    while (packetQueue) {
        now = Sys_Milliseconds();
        if (packetQueue->release >= now)
            break;
        Sys_SendPacket(packetQueue->length, packetQueue->data, packetQueue->to);
        last = packetQueue;
        packetQueue = packetQueue->next;
        Z_Free(last->data);
        Z_Free(last);
    }
}

void NET_SendPacket(netsrc_t sock, int length, const void* data, netadr_t to) {

    if (showpackets->integer && *(int*)data == -1) {
        Com_Printf("send packet %4i\n", length);
    }

    if (to.type == NA_LOOPBACK) {
        NET_SendLoopPacket(sock, length, data, to);
        return;
    }
    if (to.type == NA_BOT) {
        return;
    }
    if (to.type == NA_BAD) {
        return;
    }

    if (sock == NS_CLIENT && cl_packetdelay->integer > 0) {
        NET_QueuePacket(length, data, to, cl_packetdelay->integer);
    }
    else if (sock == NS_SERVER && sv_packetdelay->integer > 0) {
        NET_QueuePacket(length, data, to, sv_packetdelay->integer);
    }
    else {
        Sys_SendPacket(length, data, to);
    }
}

void QDECL NET_OutOfBandPrint(netsrc_t sock, netadr_t adr, const char* format, ...) {
    va_list     argptr;
    char        string[MAX_MSGLEN];

    string[0] = -1;
    string[1] = -1;
    string[2] = -1;
    string[3] = -1;

    va_start(argptr, format);
    Q_vsnprintf(string + 4, sizeof(string) - 4, format, argptr);
    va_end(argptr);

    NET_SendPacket(sock, (int)strlen(string), string, adr);
}

void QDECL NET_OutOfBandData(netsrc_t sock, netadr_t adr, byte* format, int len) {
    byte        string[MAX_MSGLEN * 2];
    int         i;
    msg_t       mbuf;

    string[0] = 0xff;
    string[1] = 0xff;
    string[2] = 0xff;
    string[3] = 0xff;

    for (i = 0; i < len; i++) {
        string[i + 4] = format[i];
    }

    mbuf.data = string;
    mbuf.cursize = len + 4;
    Huff_Compress(&mbuf, 12);
    NET_SendPacket(sock, mbuf.cursize, mbuf.data, adr);
}

int NET_StringToAdr(const char* s, netadr_t* a, netadrtype_t family)
{
    char    base[MAX_STRING_CHARS], * search;
    char* port = NULL;

    if (!strcmp(s, "localhost")) {
        Com_Memset(a, 0, sizeof(*a));
        a->type = NA_LOOPBACK;
        return 1;
    }

    Q_strncpyz(base, s, sizeof(base));

    if (*base == '[' || Q_CountChar(base, ':') > 1)
    {
        search = strchr(base, ']');
        if (search)
        {
            *search = '\0';
            search++;

            if (*search == ':')
                port = search + 1;
        }

        if (*base == '[')
            search = base + 1;
        else
            search = base;
    }
    else
    {
        port = strchr(base, ':');

        if (port) {
            *port = '\0';
            port++;
        }

        search = base;
    }

    if (!Sys_StringToAdr(search, a, family))
    {
        a->type = NA_BAD;
        return 0;
    }

    if (port)
    {
        a->port = BigShort((short)atoi(port));
        return 1;
    }
    else
    {
        a->port = BigShort(PORT_SERVER);
        return 2;
    }
}
