#ifndef NETCMDS_H
#define NETCMDS_H

/*
 * netcmds.h – High-level network commands for JupiterOS
 */

/* ping <ip> [count]          – ICMP echo request/reply */
void cmd_ping(int argc, char *args[]);

/* ifconfig                   – show NIC configuration  */
void cmd_ifconfig(void);

/* arp                        – show ARP cache           */
void cmd_arp(int argc, char *args[]);

/* net send <ip> <message>    – send a UDP datagram      */
void cmd_netsend(int argc, char *args[]);

#endif /* NETCMDS_H */
