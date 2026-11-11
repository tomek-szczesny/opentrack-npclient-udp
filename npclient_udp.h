#ifndef NPCLIENT_UDP_H
#define NPCLIENT_UDP_H

/*
 * Read the newest OpenTrack UDP pose.
 *
 * pose[0..2] receive translations in centimetres.
 * pose[3..5] receive rotations in radians, in FreeTrack order:
 *           yaw, pitch, roll.
 *
 * The values use the same units and signs that OpenTrack's FreeTrack
 * protocol writes into the shared memory mapping, so the rest of
 * NP_GetData can keep its existing radian-to-degree conversion.
 *
 * The buffer always receives the last valid pose.  When no packet is
 * pending the previous pose is copied again, so callers never see a
 * one-frame snap to zero.  Returns non-zero when at least one new
 * packet was consumed.
 */
int npclient_udp_poll(double pose[6]);

/* Close the socket and release Winsock.  Safe to call more than once. */
void npclient_udp_shutdown(void);

#endif
