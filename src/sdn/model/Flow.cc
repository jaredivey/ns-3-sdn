/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Jared Ivey <j.ivey@gatech.edu>
 *          Michael Riley <mriley7@gatech.edu>
 */
#include <stdlib.h>
#include "ns3/ethernet-header.h"
#include "Flow.h"

/* Returns true if the given field is set in the wildcard field */
static inline bool
wc (uint32_t wildcards, uint32_t field)
{
  return (wildcards & field) != 0;
}

/* Two matches strictly match, if their wildcard fields are the same, and all the
* non-wildcarded fields match on the same exact values.
* NOTE: Handling of bitmasked fields is not specified. In this implementation
* masked fields are checked for equality, and only unmasked bits are compared
* in the field.
*/
static inline bool
strict_wild8 (uint8_t a, uint8_t b, uint32_t aw, uint32_t bw, uint32_t f)
{
  return (wc (aw, f) && wc (bw, f)) || (~wc (aw, f) && ~wc (bw, f) && a == b);
}
static inline bool
strict_wild16 (uint16_t a, uint16_t b, uint32_t aw, uint32_t bw, uint32_t f)
{
  return (wc (aw, f) && wc (bw, f)) || (~wc (aw, f) && ~wc (bw, f) && a == b);
}
static inline bool
strict_wild32 (uint32_t a, uint32_t b, uint32_t aw, uint32_t bw, uint32_t f)
{
  return (wc (aw, f) && wc (bw, f)) || (~wc (aw, f) && ~wc (bw, f) && a == b);
}
static inline bool
strict_nw (uint32_t a, uint32_t b, uint32_t am, uint32_t bm)
{
  return (am == bm) && ((a ^ b) & ~am) == 0;
}
static inline bool
strict_dladdr (uint8_t *a, uint8_t *b, uint32_t aw, uint32_t bw, uint32_t f)
{
  return strict_wild32 (*((uint32_t *)a), *((uint32_t *)b), aw, bw, f) && strict_wild16 (*((uint16_t *)(a + 4)), *((uint16_t *)(b + 4)), aw, bw, f);
}

static bool
match_std_strict (fluid_msg::of10::Match a, fluid_msg::of10::Match b, uint32_t nw_src_mask_a, uint32_t nw_src_mask_b, uint32_t nw_dst_mask_a, uint32_t nw_dst_mask_b)
{
  return strict_wild16 (a.in_port (), b.in_port (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_IN_PORT)
         && strict_dladdr (a.dl_src ().get_data (), b.dl_src ().get_data (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_SRC)
         && strict_dladdr (a.dl_dst ().get_data (), b.dl_dst ().get_data (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_DST)
         && strict_wild16 (a.dl_vlan (), b.dl_vlan (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_VLAN)
         && strict_wild16 (a.dl_vlan_pcp (), b.dl_vlan_pcp (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_VLAN_PCP)
         && strict_wild16 (a.dl_type (), b.dl_type (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_TYPE)
         && strict_wild8 (a.nw_tos (), b.nw_tos (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_NW_TOS)
         && strict_wild8 (a.nw_proto (), b.nw_proto (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_NW_PROTO)
         && strict_nw (a.nw_src ().getIPv4 (), b.nw_src ().getIPv4 (), nw_src_mask_a, nw_src_mask_b)
         && strict_nw (a.nw_dst ().getIPv4 (), b.nw_dst ().getIPv4 (), nw_dst_mask_a, nw_dst_mask_b)
         && strict_wild16 (a.tp_src (), b.tp_src (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_TP_SRC)
         && strict_wild16 (a.tp_dst (), b.tp_dst (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_TP_DST);
}

/* A match (a) non-strictly matches match (b), if for each field they are both
* wildcarded, or (a) is wildcarded, and (b) isn't, or if neither is wildcarded
* and they match on the same value.
* NOTE: Handling of bitmasked fields is not specified. In this implementation
* a masked field of (a) matches the field of (b) if all masked bits of (b) are
* also masked in (a), and for each unmasked bits of (b) , the bit is either
* masked in (a), or is set to the same value in both matches.
*/
static inline bool
nonstrict_wild8 (uint8_t a, uint8_t b, uint32_t aw, uint32_t bw, uint32_t f)
{
  return (wc (bw, f) && wc (aw, f)) || (~wc (bw, f) && (wc (aw, f) || a == b));
}
static inline bool
nonstrict_wild16 (uint16_t a, uint16_t b, uint32_t aw, uint32_t bw, uint32_t f)
{
  return (wc (bw, f) && wc (aw, f)) ||  (~wc (bw, f) && (wc (aw, f) || a == b));
}
static inline bool
nonstrict_wild32 (uint32_t a, uint32_t b, uint32_t aw, uint32_t bw, uint32_t f)
{
  return (wc (bw, f) && wc (aw, f)) || (~wc (bw, f) && (wc (aw, f) || a == b));
}
static inline bool
nonstrict_nw (uint32_t a, uint32_t b, uint32_t am, uint32_t bm)
{
  return (~am & (~a | ~b | bm) & (a | b | bm)) == 0;
}
static inline bool
nonstrict_dladdr (uint8_t *a, uint8_t *b, uint32_t aw, uint32_t bw, uint32_t f)
{
  return nonstrict_wild32 (*((uint32_t *)a), *((uint32_t *)b), aw, bw, f) && nonstrict_wild16 (*((uint16_t *)(a + 4)), *((uint16_t *)(b + 4)), aw, bw, f);
}

static bool match_std_nonstrict
(fluid_msg::of10::Match a, fluid_msg::of10::Match b, uint32_t nw_src_mask_a, uint32_t nw_src_mask_b, uint32_t nw_dst_mask_a, uint32_t nw_dst_mask_b)
{
  return nonstrict_wild16 (a.in_port (), b.in_port (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_IN_PORT)
         && nonstrict_dladdr (a.dl_src ().get_data (), b.dl_src ().get_data (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_SRC)
         && nonstrict_dladdr (a.dl_dst ().get_data (), b.dl_dst ().get_data (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_DST)
         && nonstrict_wild16 (a.dl_vlan (), b.dl_vlan (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_VLAN)
         && nonstrict_wild8 (a.dl_vlan_pcp (), b.dl_vlan_pcp (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_VLAN_PCP)
         && nonstrict_wild16 (a.dl_type (), b.dl_type (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_DL_TYPE)
         && nonstrict_wild8 (a.nw_tos (), b.nw_tos (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_NW_TOS)
         && nonstrict_wild8 (a.nw_proto (), b.nw_proto (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_NW_PROTO)
         && nonstrict_nw (a.nw_src ().getIPv4 (), b.nw_src ().getIPv4 (), nw_src_mask_a, nw_src_mask_b)
         && nonstrict_nw (a.nw_dst ().getIPv4 (), b.nw_dst ().getIPv4 (), nw_dst_mask_a, nw_dst_mask_b)
         && nonstrict_wild16 (a.tp_src (), b.tp_src (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_TP_SRC)
         && nonstrict_wild16 (a.tp_dst (), b.tp_dst (), a.wildcards (), b.wildcards (), fluid_msg::of10::OFPFW_TP_DST);
}

/* A special match, where it is assumed that the wildcards and masks of (b) are
* not used. Specifically used for matching on packets. */
static inline bool
pkt_wild8 (uint8_t a, uint8_t b, uint32_t aw, uint32_t f)
{
  return wc (aw, f) || a == b;
}
static inline bool
pkt_wild16 (uint16_t a, uint16_t b, uint32_t aw, uint32_t f)
{
  return wc (aw, f) || a == b;
}
static inline bool
pkt_wild32 (uint32_t a, uint32_t b, uint32_t aw, uint32_t f)
{
  return wc (aw, f) || a == b;
}
static inline bool
pkt_nw (uint32_t a, uint32_t b, uint32_t am)
{
  return (~am & (a ^ b)) == 0;
}
static inline bool
pkt_dladdr (uint8_t *a, uint8_t *b, uint32_t am, uint32_t f)
{
  return pkt_wild32 (*((uint32_t *)a), *((uint32_t *)b), am, f) && pkt_wild16 (*((uint16_t *)(a + 4)), *((uint16_t *)(b + 4)), am, f);
}

static bool
match_std_pkt (fluid_msg::of10::Match a, fluid_msg::of10::Match b, uint32_t nw_src_mask, uint32_t nw_dst_mask)
{
  return pkt_wild16 (a.in_port (), b.in_port (), a.wildcards (), fluid_msg::of10::OFPFW_IN_PORT)
         && pkt_dladdr (a.dl_src ().get_data (), b.dl_src ().get_data (), a.wildcards (), fluid_msg::of10::OFPFW_DL_SRC)
         && pkt_dladdr (a.dl_dst ().get_data (), b.dl_dst ().get_data (), a.wildcards (), fluid_msg::of10::OFPFW_DL_DST)
         && pkt_wild16 (a.dl_vlan (), b.dl_vlan (), a.wildcards (), fluid_msg::of10::OFPFW_DL_VLAN)
         && pkt_wild8 (a.dl_vlan_pcp (), b.dl_vlan_pcp (), a.wildcards (), fluid_msg::of10::OFPFW_DL_VLAN_PCP)
         && pkt_wild16 (a.dl_type (), b.dl_type (), a.wildcards (), fluid_msg::of10::OFPFW_DL_TYPE)
         && pkt_wild16 (a.tp_src (), b.tp_src (), a.wildcards (), fluid_msg::of10::OFPFW_TP_SRC)
         && pkt_wild16 (a.tp_dst (), b.tp_dst (), a.wildcards (), fluid_msg::of10::OFPFW_TP_DST)
         && pkt_wild8 (a.nw_tos (), b.nw_tos (), a.wildcards (), fluid_msg::of10::OFPFW_NW_TOS)
         && pkt_wild8 (a.nw_proto (), b.nw_proto (), a.wildcards (), fluid_msg::of10::OFPFW_NW_PROTO)
         && pkt_nw (a.nw_src ().getIPv4 (), b.nw_src ().getIPv4 (), nw_src_mask)
         && pkt_nw (a.nw_dst ().getIPv4 (), b.nw_dst ().getIPv4 (), nw_dst_mask);
}

bool
Flow::pkt_match (const Flow& flow, const fluid_msg::of10::Match& pkt_match)
{
  return match_std_pkt (flow.match, pkt_match,flow.nw_src_mask, flow.nw_dst_mask);
}

bool
Flow::pkt_match_strict (const Flow& flow, const fluid_msg::of10::Match& pkt_match)
{
  return match_std_strict (flow.match, pkt_match,flow.nw_src_mask, fluid_msg::of10::OFPFW_NW_SRC_MASK, flow.nw_dst_mask, fluid_msg::of10::OFPFW_NW_DST_MASK);
  return false;
}

bool
Flow::strict_match (const Flow& flow_a, const Flow& flow_b)
{
  return (flow_a.priority_ == flow_b.priority_) && match_std_strict (flow_a.match, flow_b.match,flow_a.nw_src_mask, flow_b.nw_src_mask, flow_a.nw_dst_mask,flow_b.nw_dst_mask);
}
bool
Flow::non_strict_match (const Flow& flow_a, const Flow& flow_b)
{
  return match_std_nonstrict (flow_a.match, flow_b.match,flow_a.nw_src_mask, flow_b.nw_src_mask, flow_a.nw_dst_mask,flow_b.nw_dst_mask);
  return false;
}

uint64_t
Flow::getDurationSec ()
{
  return (Simulator::Now ().GetNanoSeconds () - install_time_nsec) / NANOTOSECS;
}
uint64_t
Flow::getDurationNSec ()
{
  return (Simulator::Now ().GetNanoSeconds () - install_time_nsec) % NANOTOSECS;
}

fluid_msg::of10::FlowStats*
Flow::convertToFlowStats ()
{
  fluid_msg::of10::FlowStats* flowStats = new fluid_msg::of10::FlowStats (table_id_, getDurationSec (), getDurationNSec (), priority_, idle_timeout_, hard_timeout_, cookie_, packet_count_, byte_count_);
  return flowStats;
}
