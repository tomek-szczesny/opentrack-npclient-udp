/*
 * OpenTrack UDP receiver for NPClient64.dll.
 *
 * OpenTrack's UDP output ("UDP over network" protocol) sends one datagram
 * per pose, containing six little-endian doubles:
 *
 *     [ TX, TY, TZ, Yaw, Pitch, Roll ]
 *
 * Translations are in centimetres and rotations in degrees.  This file
 * converts them to the units that OpenTrack's FreeTrack protocol stores in
 * the FT_SharedMem mapping, so the existing NP_GetData code can consume
 * them unchanged.
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <math.h>
#include <string.h>

#include "npclient_udp.h"

/* OpenTrack sends to this fixed port.  Change it here and in OpenTrack. */
#define NPCLIENT_UDP_PORT 4242

#define UDP_PACKET_SIZE (sizeof(double) * 6)

enum udp_state {
    UDP_UNINITIALIZED = 0,
    UDP_READY,
    UDP_FAILED
};

static SRWLOCK g_lock = SRWLOCK_INIT;
static SOCKET g_socket = INVALID_SOCKET;
static int g_state = UDP_UNINITIALIZED;
static int g_wsa_started = 0;
static double g_last_pose[6];

/*
 * Close the socket and reset the state.  WSACleanup is skipped when called
 * from DllMain: Winsock calls are not safe under the loader lock.  At process
 * exit the operating system releases everything anyway.
 */
static void udp_close(int release_wsa)
{
    if (g_socket != INVALID_SOCKET) {
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
    }
    if (release_wsa && g_wsa_started) {
        WSACleanup();
        g_wsa_started = 0;
    }
    g_state = UDP_UNINITIALIZED;
}

static int udp_open(void)
{
    WSADATA wsa;
    struct sockaddr_in addr;
    u_long nonblocking = 1;

    if (g_state == UDP_READY)
        return 1;
    if (g_state == UDP_FAILED)
        return 0;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        g_state = UDP_FAILED;
        return 0;
    }
    g_wsa_started = 1;

    g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_socket == INVALID_SOCKET) {
        udp_close(1);
        g_state = UDP_FAILED;
        return 0;
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(NPCLIENT_UDP_PORT);

    if (bind(g_socket, (struct sockaddr *) &addr, sizeof addr) == SOCKET_ERROR) {
        udp_close(1);
        g_state = UDP_FAILED;
        return 0;
    }

    if (ioctlsocket(g_socket, FIONBIO, &nonblocking) == SOCKET_ERROR) {
        udp_close(1);
        g_state = UDP_FAILED;
        return 0;
    }

    g_state = UDP_READY;
    return 1;
}

static int udp_pose_is_finite(const double raw[6])
{
    int i;

    for (i = 0; i < 6; i++)
        if (!isfinite(raw[i]))
            return 0;
    return 1;
}

static void udp_convert(const double raw[6], double pose[6])
{
    const double d2r = 3.14159265358979323846 / 180.0;

    /* Match proto-ft: translations are scaled by 10, yaw and pitch are
     * negated, and a pitch within 0.15 degrees of +90 is clamped to
     * 89.86 to avoid a discontinuity in some games. */
    const int crossing_90 = fabs(raw[4] - 90.0) < 0.15;

    pose[0] = raw[0] * 10.0;
    pose[1] = raw[1] * 10.0;
    pose[2] = raw[2] * 10.0;
    pose[3] = -raw[3] * d2r;
    pose[4] = -d2r * (crossing_90 ? 89.86 : raw[4]);
    pose[5] = raw[5] * d2r;
}

/* Drain every queued datagram and keep the newest valid one. */
static int udp_drain(double pose[6])
{
    unsigned char buf[512];
    double raw[6];
    int updated = 0;
    int budget = 256;

    while (budget-- > 0) {
        int n = recvfrom(g_socket, (char *) buf, sizeof buf, 0, NULL, NULL);

        if (n == SOCKET_ERROR)
            break;

        if (n != (int) UDP_PACKET_SIZE)
            continue;

        memcpy(raw, buf, sizeof raw);
        if (!udp_pose_is_finite(raw))
            continue;

        udp_convert(raw, pose);
        updated = 1;
    }

    return updated;
}

int npclient_udp_poll(double pose[6])
{
    int updated;

    AcquireSRWLockExclusive(&g_lock);

    if (udp_open())
        updated = udp_drain(g_last_pose);
    else
        updated = 0;

    memcpy(pose, g_last_pose, sizeof g_last_pose);

    ReleaseSRWLockExclusive(&g_lock);
    return updated;
}

void npclient_udp_shutdown(void)
{
    AcquireSRWLockExclusive(&g_lock);
    udp_close(0);
    memset(g_last_pose, 0, sizeof g_last_pose);
    ReleaseSRWLockExclusive(&g_lock);
}
