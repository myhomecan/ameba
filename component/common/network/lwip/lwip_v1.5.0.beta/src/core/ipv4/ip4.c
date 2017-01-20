/**
 * @file
 * This is the IPv4 layer implementation for incoming and outgoing IP traffic.
 * 
 * @see ip_frag.c
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "lwip/opt.h"
#include "lwip/lwip_ip.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/ip_frag.h"
#include "lwip/inet_chksum.h"
#include "lwip/netif.h"
#include "lwip/icmp.h"
#include "lwip/igmp.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp_impl.h"
#include "lwip/snmp.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/stats.h"
#include "arch/perf.h"

#include <string.h>

/** Set this to 0 in the rare case of wanting to call an extra function to
 * generate the IP checksum (in contrast to calculating it on-the-fly). */
#ifndef LWIP_INLINE_IP_CHKSUM
#define LWIP_INLINE_IP_CHKSUM   1
#endif
#if LWIP_INLINE_IP_CHKSUM && CHECKSUM_GEN_IP
#define CHECKSUM_GEN_IP_INLINE  1
#else
#define CHECKSUM_GEN_IP_INLINE  0
#endif

#if LWIP_DHCP || defined(LWIP_IP_ACCEPT_UDP_PORT)
#define IP_ACCEPT_LINK_LAYER_ADDRESSING 1

/** Some defines for DHCP to let link-layer-addressed packets through while the
 * netif is down.
 * To use this in your own application/protocol, define LWIP_IP_ACCEPT_UDP_PORT
 * to return 1 if the port is accepted and 0 if the port is not accepted.
 */
#if LWIP_DHCP && defined(LWIP_IP_ACCEPT_UDP_PORT)
/* accept DHCP client port and custom port */
#define IP_ACCEPT_LINK_LAYER_ADDRESSED_PORT(port) (((port) == PP_NTOHS(DHCP_CLIENT_PORT)) \
         || (LWIP_IP_ACCEPT_UDP_PORT(port)))
#elif defined(LWIP_IP_ACCEPT_UDP_PORT) /* LWIP_DHCP && defined(LWIP_IP_ACCEPT_UDP_PORT) */
/* accept custom port only */
#define IP_ACCEPT_LINK_LAYER_ADDRESSED_PORT(port) (LWIP_IP_ACCEPT_UDP_PORT(port))
#else /* LWIP_DHCP && defined(LWIP_IP_ACCEPT_UDP_PORT) */
/* accept DHCP client port only */
#define IP_ACCEPT_LINK_LAYER_ADDRESSED_PORT(port) ((port) == PP_NTOHS(DHCP_CLIENT_PORT))
#endif /* LWIP_DHCP && defined(LWIP_IP_ACCEPT_UDP_PORT) */

#else /* LWIP_DHCP */
#define IP_ACCEPT_LINK_LAYER_ADDRESSING 0
#endif /* LWIP_DHCP */

/** Global data for both IPv4 and IPv6 */
struct ip_globals ip_data;

/** The IP header ID of the next outgoing IP packet */
static u16_t ip_id;

/**
 * Finds the appropriate network interface for a given IP address. It
 * searches the list of network interfaces linearly. A match is found
 * if the masked IP address of the network interface equals the masked
 * IP address given to the function.
 *
 * @param dest the destination IP address for which to find the route
 * @return the netif on which to send to reach dest
 */
struct netif *
ip_route(ip_addr_t *dest)
{
  struct netif *netif;

#ifdef LWIP_HOOK_IP4_ROUTE
  netif = LWIP_HOOK_IP4_ROUTE(dest);
  if (netif != NULL) {
    return netif;
  }
#endif

  /* iterate through netifs */
  for (netif = netif_list; netif != NULL; netif = netif->next) {
    /* network mask matches? */
    if ((netif_is_up(netif))
#if LWIP_IPV6
        /* prevent using IPv6-only interfaces */
        && (!ip_addr_isany(&(netif->ip_addr)))
#endif /* LWIP_IPV6 */ 
        ) {
      if (ip_addr_netcmp(dest, &(netif->ip_addr), &(netif->netmask))) {
        /* return netif on which to forward IP packet */
        return netif;
      }
    }
  }
  if ((netif_default == NULL) || (!netif_is_up(netif_default))) {
    LWIP_DEBUGF(IP_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("ip_route: No route to %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
      ip4_addr1_16(dest), ip4_addr2_16(dest), ip4_addr3_16(dest), ip4_addr4_16(dest)));
    IP_STATS_INC(ip.rterr);
    snmp_inc_ipoutnoroutes();
    return NULL;
  }
  /* no matching netif found, use default netif */
  return netif_default;
}

#if IP_FORWARD
/**
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 * @param p the packet to forward
 * @param dest the destination IP address
 * @return 1: can forward 0: discard
 */
static int
ip_canforward(struct pbuf *p)
{
  u32_t addr = htonl(ip4_addr_get_u32(ip_current_dest_addr()));

  if (p->flags & PBUF_FLAG_LLBCAST) {
    /* don't route link-layer broadcasts */
    return 0;
  }
  if ((p->flags & PBUF_FLAG_LLMCAST) && !IP_MULTICAST(addr)) {
    /* don't route link-layer multicasts unless the destination address is an IP
       multicast address */
    return 0;
  }
  if (IP_EXPERIMENTAL(addr)) {
    return 0;
  }
  if (IP_CLASSA(addr)) {
    u32_t net = addr & IP_CLASSA_NET;
    if ((net == 0) || (net == ((u32_t)IP_LOOPBACKNET << IP_CLASSA_NSHIFT))) {
      /* don't route loopback packets */
      return 0;
    }
  }
  return 1;
}

/**
 * Forwards an IP packet. It finds an appropriate route for the
 * packet, decrements the TTL value of the packet, adjusts the
 * checksum and outputs the packet on the appropriate interface.
 *
 * @param p the packet to forward (p->payload points to IP header)
 * @param iphdr the IP header of the input packet
 * @param inp the netif on which this packet was received
 */
static void
ip_forward(struct pbuf *p, struct ip_hdr *iphdr, struct netif *inp)
{
  struct netif *netif;

  PERF_START;

  if (!ip_canforward(p)) {
    goto return_noroute;
  }

  /* RFC3927 2.7: do not forward link-local addresses */
  if (ip_addr_islinklocal(ip_current_dest_addr())) {
    LWIP_DEBUGF(IP_DEBUG, ("ip_forward: not forwarding LLA %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
      ip4_addr1_16(ip_current_dest_addr()), ip4_addr2_16(ip_current_dest_addr()),
      ip4_addr3_16(ip_current_dest_addr()), ip4_addr4_16(ip_current_dest_addr())));
    goto return_noroute;
  }

  /* Find network interface where to forward this IP packet to. */
  netif = ip_route(ip_current_dest_addr());
  if (netif == NULL) {
    LWIP_DEBUGF(IP_DEBUG, ("ip_forward: no forwarding route for %"U16_F".%"U16_F".%"U16_F".%"U16_F" found\n",
      ip4_addr1_16(ip_current_dest_addr()), ip4_addr2_16(ip_current_dest_addr()),
      ip4_addr3_16(ip_current_dest_addr()), ip4_addr4_16(ip_current_dest_addr())));
    /* @todo: send ICMP_DUR_NET? */
    goto return_noroute;
  }
#if !IP_FORWARD_ALLOW_TX_ON_RX_NETIF
  /* Do not forward packets onto the same network interface on which
   * they arrived. */
  if (netif == inp) {
    LWIP_DEBUGF(IP_DEBUG, ("ip_forward: not bouncing packets back on incoming interface.\n"));
    goto return_noroute;
  }
#endif /* IP_FORWARD_ALLOW_TX_ON_RX_NETIF */

  /* decrement TTL */
  IPH_TTL_SET(iphdr, IPH_TTL(iphdr) - 1);
  /* send ICMP if TTL == 0 */
  if (IPH_TTL(iphdr) == 0) {
    snmp_inc_ipinhdrerrors();
#if LWIP_ICMP
    /* Don't send ICMP messages in response to ICMP messages */
    if (IPH_PROTO(ip