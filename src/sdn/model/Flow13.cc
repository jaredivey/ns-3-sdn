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
#include "Flow13.h"

typedef bool (*match_func)(uint8_t, uint8_t, uint8_t, uint8_t);

static fluid_msg::of13::InPort *ainport = NULL;
static fluid_msg::of13::InPort *binport = NULL;
static fluid_msg::of13::InPhyPort *ainphyport = NULL;
static fluid_msg::of13::InPhyPort *binphyport = NULL;
static fluid_msg::of13::Metadata *ametadata = NULL;
static fluid_msg::of13::Metadata *bmetadata = NULL;
static fluid_msg::of13::EthDst *aethdst = NULL;
static fluid_msg::of13::EthDst *bethdst = NULL;
static fluid_msg::of13::EthSrc *aethsrc = NULL;
static fluid_msg::of13::EthSrc *bethsrc = NULL;
static fluid_msg::of13::EthType *aethtype = NULL;
static fluid_msg::of13::EthType *bethtype = NULL;
static fluid_msg::of13::VLANVid *avlanvid = NULL;
static fluid_msg::of13::VLANVid *bvlanvid = NULL;
static fluid_msg::of13::VLANPcp *avlanpcp = NULL;
static fluid_msg::of13::VLANPcp *bvlanpcp = NULL;
static fluid_msg::of13::IPDSCP *aipdscp = NULL;
static fluid_msg::of13::IPDSCP *bipdscp = NULL;
static fluid_msg::of13::IPECN *aipecn = NULL;
static fluid_msg::of13::IPECN *bipecn = NULL;
static fluid_msg::of13::IPProto *aipproto = NULL;
static fluid_msg::of13::IPProto *bipproto = NULL;
static fluid_msg::of13::IPv4Src *aipv4src = NULL;
static fluid_msg::of13::IPv4Src *bipv4src = NULL;
static fluid_msg::of13::IPv4Dst *aipv4dst = NULL;
static fluid_msg::of13::IPv4Dst *bipv4dst = NULL;
static fluid_msg::of13::TCPSrc *atcpsrc = NULL;
static fluid_msg::of13::TCPSrc *btcpsrc = NULL;
static fluid_msg::of13::TCPDst *atcpdst = NULL;
static fluid_msg::of13::TCPDst *btcpdst = NULL;
static fluid_msg::of13::UDPSrc *audpsrc = NULL;
static fluid_msg::of13::UDPSrc *budpsrc = NULL;
static fluid_msg::of13::UDPDst *audpdst = NULL;
static fluid_msg::of13::UDPDst *budpdst = NULL;
static fluid_msg::of13::SCTPSrc *asctpsrc = NULL;
static fluid_msg::of13::SCTPSrc *bsctpsrc = NULL;
static fluid_msg::of13::SCTPDst *asctpdst = NULL;
static fluid_msg::of13::SCTPDst *bsctpdst = NULL;
static fluid_msg::of13::ICMPv4Type *aicmpv4type = NULL;
static fluid_msg::of13::ICMPv4Type *bicmpv4type = NULL;
static fluid_msg::of13::ICMPv4Code *aicmpv4code = NULL;
static fluid_msg::of13::ICMPv4Code *bicmpv4code = NULL;
static fluid_msg::of13::ARPOp *aarpop = NULL;
static fluid_msg::of13::ARPOp *barpop = NULL;
static fluid_msg::of13::ARPSPA *aarpspa = NULL;
static fluid_msg::of13::ARPSPA *barpspa = NULL;
static fluid_msg::of13::ARPTPA *aarptpa = NULL;
static fluid_msg::of13::ARPTPA *barptpa = NULL;
static fluid_msg::of13::ARPSHA *aarpsha = NULL;
static fluid_msg::of13::ARPSHA *barpsha = NULL;
static fluid_msg::of13::ARPTHA *aarptha = NULL;
static fluid_msg::of13::ARPTHA *barptha = NULL;
static fluid_msg::of13::IPv6Src *aipv6src = NULL;
static fluid_msg::of13::IPv6Src *bipv6src = NULL;
static fluid_msg::of13::IPv6Dst *aipv6dst = NULL;
static fluid_msg::of13::IPv6Dst *bipv6dst = NULL;
static fluid_msg::of13::IPV6Flabel *aipv6flabel = NULL;
static fluid_msg::of13::IPV6Flabel *bipv6flabel = NULL;
static fluid_msg::of13::ICMPv6Type *aicmpv6type = NULL;
static fluid_msg::of13::ICMPv6Type *bicmpv6type = NULL;
static fluid_msg::of13::ICMPv6Code *aicmpv6code = NULL;
static fluid_msg::of13::ICMPv6Code *bicmpv6code = NULL;
static fluid_msg::of13::IPv6NDTarget *aipv6ndtarget = NULL;
static fluid_msg::of13::IPv6NDTarget *bipv6ndtarget = NULL;
static fluid_msg::of13::IPv6NDSLL *aipv6ndsll = NULL;
static fluid_msg::of13::IPv6NDSLL *bipv6ndsll = NULL;
static fluid_msg::of13::IPv6NDTLL *aipv6ndtll = NULL;
static fluid_msg::of13::IPv6NDTLL *bipv6ndtll = NULL;
static fluid_msg::of13::MPLSLabel *amplslabel = NULL;
static fluid_msg::of13::MPLSLabel *bmplslabel = NULL;
static fluid_msg::of13::MPLSTC *amplstc = NULL;
static fluid_msg::of13::MPLSTC *bmplstc = NULL;
static fluid_msg::of13::MPLSBOS *amplsbos = NULL;
static fluid_msg::of13::MPLSBOS *bmplsbos = NULL;
static fluid_msg::of13::PBBIsid *apbbisid = NULL;
static fluid_msg::of13::PBBIsid *bpbbisid = NULL;
static fluid_msg::of13::TUNNELId *atunnelid = NULL;
static fluid_msg::of13::TUNNELId *btunnelid = NULL;
static fluid_msg::of13::IPv6Exthdr *aipv6exthdr = NULL;
static fluid_msg::of13::IPv6Exthdr *bipv6exthdr = NULL;

static bool
match_ (fluid_msg::of13::Match a, fluid_msg::of13::Match b, match_func cur_func)
{
	uint16_t alen = a.oxm_fields_len() - fluid_msg::of13::OFP_OXM_HEADER_LEN;
	uint16_t blen = b.oxm_fields_len() - fluid_msg::of13::OFP_OXM_HEADER_LEN;
	uint8_t aval8, bval8, amask8, bmask8;
	bool matched = true;
	for (uint32_t j = 0; alen > 0 && blen > 0; ++j)
	{
		fluid_msg::of13::OXMTLV *aptr = a.oxm_field(j);
		fluid_msg::of13::OXMTLV *bptr = b.oxm_field(j);
		const fluid_msg::of13::OXMTLV &const_bptr = *(b.oxm_field(j));
		if (aptr)
		{
			alen -= aptr->length ();
			if (bptr)
			{
				blen -= bptr->length ();
				if (aptr->class_ () == fluid_msg::of13::OFPXMC_OPENFLOW_BASIC &&
						bptr->class_ () == fluid_msg::of13::OFPXMC_OPENFLOW_BASIC)
				{
					switch (aptr->field ())
					{
					case fluid_msg::of13::OFPXMT_OFB_IN_PORT: 		//0, /* Switch input port. */
						ainport = dynamic_cast<fluid_msg::of13::InPort *> (aptr);
						binport = dynamic_cast<fluid_msg::of13::InPort *> (bptr);
						matched = matched && (ainport->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_IN_PHY_PORT: 	//1, /* Switch physical input port. */
						ainphyport = dynamic_cast<fluid_msg::of13::InPhyPort *> (aptr);
						binphyport = dynamic_cast<fluid_msg::of13::InPhyPort *> (bptr);
						matched = matched && (ainphyport->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_METADATA: 		//2, /* Metadata passed between tables. */
						ametadata = dynamic_cast<fluid_msg::of13::Metadata *> (aptr);
						bmetadata = dynamic_cast<fluid_msg::of13::Metadata *> (bptr);
						if (ametadata->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_METADATA_LEN; ++i)
							{
								aval8 =  uint8_t((ametadata->value () >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((bmetadata->value () >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((ametadata->mask () >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((bmetadata->mask () >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (ametadata->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_ETH_DST: 		//3, /* Ethernet destination address. */
						aethdst = dynamic_cast<fluid_msg::of13::EthDst *> (aptr);
						bethdst = dynamic_cast<fluid_msg::of13::EthDst *> (bptr);
						if (aethdst->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::OFP_ETH_ALEN; ++i)
							{
								aval8 = aethdst->value ().get_data ()[i];
								bval8 = aethdst->value ().get_data ()[i];
								amask8 = aethdst->value ().get_data ()[i];
								bmask8 = aethdst->value ().get_data ()[i];
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aethdst->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_ETH_SRC: 		//4, /* Ethernet source address. */
						aethsrc = dynamic_cast<fluid_msg::of13::EthSrc *> (aptr);
						bethsrc = dynamic_cast<fluid_msg::of13::EthSrc *> (bptr);
						if (aethsrc->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::OFP_ETH_ALEN; ++i)
							{
								aval8 = aethsrc->value ().get_data ()[i];
								bval8 = aethsrc->value ().get_data ()[i];
								amask8 = aethsrc->value ().get_data ()[i];
								bmask8 = aethsrc->value ().get_data ()[i];
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aethsrc->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_ETH_TYPE: 		//5, /* Ethernet frame type. */
						aethtype = dynamic_cast<fluid_msg::of13::EthType *> (aptr);
						bethtype = dynamic_cast<fluid_msg::of13::EthType *> (bptr);
						matched = matched && (aethtype->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_VLAN_VID: 		//6, /* VLAN id. */
						avlanvid = dynamic_cast<fluid_msg::of13::VLANVid *> (aptr);
						bvlanvid = dynamic_cast<fluid_msg::of13::VLANVid *> (bptr);
						if (avlanvid->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_VLAN_VID_LEN; ++i)
							{
								aval8 = uint8_t((avlanvid->value () >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((bvlanvid->value () >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((avlanvid->mask () >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((bvlanvid->mask () >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (avlanvid->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_VLAN_PCP: 		//7, /* VLAN priority. */
						avlanpcp = dynamic_cast<fluid_msg::of13::VLANPcp *> (aptr);
						bvlanpcp = dynamic_cast<fluid_msg::of13::VLANPcp *> (bptr);
						matched = matched && (avlanpcp->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_IP_DSCP: 		//8, /* IP DSCP (6 bits in ToS field). */
						aipdscp = dynamic_cast<fluid_msg::of13::IPDSCP *> (aptr);
						bipdscp = dynamic_cast<fluid_msg::of13::IPDSCP *> (bptr);
						matched = matched && (aipdscp->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_IP_ECN: 		//9, /* IP ECN (2 bits in ToS field). */
						aipecn = dynamic_cast<fluid_msg::of13::IPECN *> (aptr);
						bipecn = dynamic_cast<fluid_msg::of13::IPECN *> (bptr);
						matched = matched && (aipecn->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_IP_PROTO: 		//10, /* IP protocol. */
						aipproto = dynamic_cast<fluid_msg::of13::IPProto *> (aptr);
						bipproto = dynamic_cast<fluid_msg::of13::IPProto *> (bptr);
						matched = matched && (aipproto->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_IPV4_SRC: 		//11, /* IPv4 source address. */
						aipv4src = dynamic_cast<fluid_msg::of13::IPv4Src *> (aptr);
						bipv4src = dynamic_cast<fluid_msg::of13::IPv4Src *> (bptr);
						if (aipv4src->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_IPV4_LEN; ++i)
							{
								aval8 = uint8_t((aipv4src->value ().getIPv4() >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((aipv4src->value ().getIPv4() >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((aipv4src->mask ().getIPv4() >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((aipv4src->mask ().getIPv4() >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aipv4src->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_IPV4_DST: 		//12, /* IPv4 destination address. */
						aipv4dst = dynamic_cast<fluid_msg::of13::IPv4Dst *> (aptr);
						bipv4dst = dynamic_cast<fluid_msg::of13::IPv4Dst *> (bptr);
						if (aipv4dst->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_IPV4_LEN; ++i)
							{
								aval8 = uint8_t((aipv4src->value ().getIPv4() >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((aipv4src->value ().getIPv4() >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((aipv4src->mask ().getIPv4() >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((aipv4src->mask ().getIPv4() >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aipv4dst->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_TCP_SRC: 		//13, /* TCP source port. */
						atcpsrc = dynamic_cast<fluid_msg::of13::TCPSrc *> (aptr);
						btcpsrc = dynamic_cast<fluid_msg::of13::TCPSrc *> (bptr);
						matched = matched && (atcpsrc->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_TCP_DST: 		//14, /* TCP destination port. */
						atcpdst = dynamic_cast<fluid_msg::of13::TCPDst *> (aptr);
						btcpdst = dynamic_cast<fluid_msg::of13::TCPDst *> (bptr);
						matched = matched && (atcpdst->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_UDP_SRC: 		//15, /* UDP source port. */
						audpsrc = dynamic_cast<fluid_msg::of13::UDPSrc *> (aptr);
						budpsrc = dynamic_cast<fluid_msg::of13::UDPSrc *> (bptr);
						matched = matched && (audpsrc->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_UDP_DST: 		//16, /* UDP destination port. */
						audpdst = dynamic_cast<fluid_msg::of13::UDPDst *> (aptr);
						budpdst = dynamic_cast<fluid_msg::of13::UDPDst *> (bptr);
						matched = matched && (audpdst->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_SCTP_SRC: 		//17, /* SCTP source port. */
						asctpsrc = dynamic_cast<fluid_msg::of13::SCTPSrc *> (aptr);
						bsctpsrc = dynamic_cast<fluid_msg::of13::SCTPSrc *> (bptr);
						matched = matched && (asctpsrc->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_SCTP_DST: 		//18, /* SCTP destination port. */
						asctpdst = dynamic_cast<fluid_msg::of13::SCTPDst *> (aptr);
						bsctpdst = dynamic_cast<fluid_msg::of13::SCTPDst *> (bptr);
						matched = matched && (asctpdst->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_ICMPV4_TYPE: 	//19, /* ICMP type. */
						aicmpv4type = dynamic_cast<fluid_msg::of13::ICMPv4Type *> (aptr);
						bicmpv4type = dynamic_cast<fluid_msg::of13::ICMPv4Type *> (bptr);
						matched = matched && (aicmpv4type->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_ICMPV4_CODE: 	//20, /* ICMP code. */
						aicmpv4code = dynamic_cast<fluid_msg::of13::ICMPv4Code *> (aptr);
						bicmpv4code = dynamic_cast<fluid_msg::of13::ICMPv4Code *> (bptr);
						matched = matched && (aicmpv4code->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_ARP_OP: 		//21, /* ARP opcode. */
						aarpop = dynamic_cast<fluid_msg::of13::ARPOp *> (aptr);
						barpop = dynamic_cast<fluid_msg::of13::ARPOp *> (bptr);
						matched = matched && (aarpop->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_ARP_SPA: 		//22, /* ARP source IPv4 address. */
						aarpspa = dynamic_cast<fluid_msg::of13::ARPSPA *> (aptr);
						barpspa = dynamic_cast<fluid_msg::of13::ARPSPA *> (bptr);
						if (aarpspa->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_IPV4_LEN; ++i)
							{
								aval8 = uint8_t((aarpspa->value ().getIPv4() >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((barpspa->value ().getIPv4() >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((aarpspa->mask ().getIPv4() >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((barpspa->mask ().getIPv4() >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aarpspa->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_ARP_TPA: 		//23, /* ARP target IPv4 address. */
						aarptpa = dynamic_cast<fluid_msg::of13::ARPTPA *> (aptr);
						barptpa = dynamic_cast<fluid_msg::of13::ARPTPA *> (bptr);
						if (aarptpa->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_IPV4_LEN; ++i)
							{
								aval8 = uint8_t((aarptpa->value ().getIPv4() >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((barptpa->value ().getIPv4() >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((aarptpa->mask ().getIPv4() >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((barptpa->mask ().getIPv4() >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aarptpa->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_ARP_SHA: 		//24, /* ARP source hardware address. */
						aarpsha = dynamic_cast<fluid_msg::of13::ARPSHA *> (aptr);
						barpsha = dynamic_cast<fluid_msg::of13::ARPSHA *> (bptr);
						if (aarpsha->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::OFP_ETH_ALEN; ++i)
							{
								aval8 = aarpsha->value ().get_data ()[i];
								bval8 = barpsha->value ().get_data ()[i];
								amask8 = aarpsha->mask ().get_data ()[i];
								bmask8 = barpsha->mask ().get_data ()[i];
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aarpsha->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_ARP_THA: 		//25, /* ARP target hardware address. */
						aarptha = dynamic_cast<fluid_msg::of13::ARPTHA *> (aptr);
						barptha = dynamic_cast<fluid_msg::of13::ARPTHA *> (bptr);
						if (aarptha->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::OFP_ETH_ALEN; ++i)
							{
								aval8 = aarptha->value ().get_data ()[i];
								bval8 = barptha->value ().get_data ()[i];
								amask8 = aarptha->mask ().get_data ()[i];
								bmask8 = barptha->mask ().get_data ()[i];
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aarptha->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_IPV6_SRC: 		//26, /* IPv6 source address. */
						aipv6src = dynamic_cast<fluid_msg::of13::IPv6Src *> (aptr);
						bipv6src = dynamic_cast<fluid_msg::of13::IPv6Src *> (bptr);
						if (aipv6src->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_IPV6_LEN; ++i)
							{
								aval8 = aipv6src->value ().getIPv6()[i];
								bval8 = bipv6src->value ().getIPv6()[i];
								amask8 = aipv6src->mask ().getIPv6()[i];
								bmask8 = bipv6src->mask ().getIPv6()[i];
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aipv6src->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_IPV6_DST: 		//27, /* IPv6 destination address. */
						aipv6dst = dynamic_cast<fluid_msg::of13::IPv6Dst *> (aptr);
						bipv6dst = dynamic_cast<fluid_msg::of13::IPv6Dst *> (bptr);
						if (aipv6dst->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_IPV6_LEN; ++i)
							{
								aval8 = aipv6dst->value ().getIPv6()[i];
								bval8 = bipv6dst->value ().getIPv6()[i];
								amask8 = aipv6dst->mask ().getIPv6()[i];
								bmask8 = bipv6dst->mask ().getIPv6()[i];
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aipv6dst->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_IPV6_FLABEL: 	//28, /* IPv6 Flow Label */
						aipv6flabel = dynamic_cast<fluid_msg::of13::IPV6Flabel *> (aptr);
						bipv6flabel = dynamic_cast<fluid_msg::of13::IPV6Flabel *> (bptr);
						if (aipv6flabel->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_IPV6_FLABEL_LEN; ++i)
							{
								aval8 = uint8_t((aipv6flabel->value () >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((bipv6flabel->value () >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((aipv6flabel->mask () >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((bipv6flabel->mask () >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aipv6flabel->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_ICMPV6_TYPE: 	//29, /* ICMPv6 type. */
						aicmpv6type = dynamic_cast<fluid_msg::of13::ICMPv6Type *> (aptr);
						bicmpv6type = dynamic_cast<fluid_msg::of13::ICMPv6Type *> (bptr);
						matched = matched && (aicmpv6type->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_ICMPV6_CODE: 	//30, /* ICMPv6 code. */
						aicmpv6code = dynamic_cast<fluid_msg::of13::ICMPv6Code *> (aptr);
						bicmpv6code = dynamic_cast<fluid_msg::of13::ICMPv6Code *> (bptr);
						matched = matched && (aicmpv6code->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_IPV6_ND_TARGET: //31, /* Target address for ND. */
						aipv6ndtarget = dynamic_cast<fluid_msg::of13::IPv6NDTarget *> (aptr);
						bipv6ndtarget = dynamic_cast<fluid_msg::of13::IPv6NDTarget *> (bptr);
						matched = matched && (aipv6ndtarget->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_IPV6_ND_SLL: 	//32, /* Source link-layer for ND. */
						aipv6ndsll = dynamic_cast<fluid_msg::of13::IPv6NDSLL *> (aptr);
						bipv6ndsll = dynamic_cast<fluid_msg::of13::IPv6NDSLL *> (bptr);
						matched = matched && (aipv6ndsll->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_IPV6_ND_TLL: 	//33, /* Target link-layer for ND. */
						aipv6ndtll = dynamic_cast<fluid_msg::of13::IPv6NDTLL *> (aptr);
						bipv6ndtll = dynamic_cast<fluid_msg::of13::IPv6NDTLL *> (bptr);
						matched = matched && (aipv6ndtll->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_MPLS_LABEL: 	//34, /* MPLS label. */
						amplslabel = dynamic_cast<fluid_msg::of13::MPLSLabel *> (aptr);
						bmplslabel = dynamic_cast<fluid_msg::of13::MPLSLabel *> (bptr);
						matched = matched && (amplslabel->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_MPLS_TC: 		//35, /* MPLS TC. */
						amplstc = dynamic_cast<fluid_msg::of13::MPLSTC *> (aptr);
						bmplstc = dynamic_cast<fluid_msg::of13::MPLSTC *> (bptr);
						matched = matched && (amplstc->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_MPLS_BOS: 		//36, /* MPLS BoS bit. */
						amplsbos = dynamic_cast<fluid_msg::of13::MPLSBOS *> (aptr);
						bmplsbos = dynamic_cast<fluid_msg::of13::MPLSBOS *> (bptr);
						matched = matched && (amplsbos->equals(const_bptr));
						break;
					case fluid_msg::of13::OFPXMT_OFB_PBB_ISID: 		//37, /* PBB I-SID. */
						apbbisid = dynamic_cast<fluid_msg::of13::PBBIsid *> (aptr);
						bpbbisid = dynamic_cast<fluid_msg::of13::PBBIsid *> (bptr);
						if (apbbisid->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_IPV6_PBB_ISID_LEN; ++i)
							{
								aval8 = uint8_t((apbbisid->value () >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((bpbbisid->value () >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((apbbisid->mask () >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((bpbbisid->mask () >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (apbbisid->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_TUNNEL_ID: 	//38, /* Logical Port Metadata. */
						atunnelid = dynamic_cast<fluid_msg::of13::TUNNELId *> (aptr);
						btunnelid = dynamic_cast<fluid_msg::of13::TUNNELId *> (bptr);
						if (atunnelid->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_TUNNEL_ID_LEN; ++i)
							{
								aval8 = uint8_t((atunnelid->value () >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((btunnelid->value () >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((atunnelid->mask () >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((btunnelid->mask () >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (atunnelid->equals(const_bptr));
						}
						break;
					case fluid_msg::of13::OFPXMT_OFB_IPV6_EXTHDR: 	//39 /* IPv6 Extension Header pseudo-field */
						aipv6exthdr = dynamic_cast<fluid_msg::of13::IPv6Exthdr *> (aptr);
						bipv6exthdr = dynamic_cast<fluid_msg::of13::IPv6Exthdr *> (bptr);
						if (aipv6exthdr->has_mask())
						{
							for (uint8_t i = 0; i < fluid_msg::of13::OFP_OXM_IPV6_EXTHDR_LEN; ++i)
							{
								aval8 = uint8_t((aipv6exthdr->value () >> 8*(7 - i)) & 0xFF);
								bval8 = uint8_t((bipv6exthdr->value () >> 8*(7 - i)) & 0xFF);
								amask8 = uint8_t((aipv6exthdr->mask () >> 8*(7 - i)) & 0xFF);
								bmask8 = uint8_t((bipv6exthdr->mask () >> 8*(7 - i)) & 0xFF);
								matched = matched && cur_func (aval8, bval8, amask8, bmask8);
							}
						}
						else
						{
							matched = matched && (aipv6exthdr->equals(const_bptr));
						}
						break;
					default:
						break;
					}
				}
			}
		}
	}
  return matched;
}

/* Two matches strictly match, if their wildcard fields are the same, and all the
* non-wildcarded fields match on the same exact values.
* NOTE: Handling of bitmasked fields is not specified. In this implementation
* masked fields are checked for equality, and only unmasked bits are compared
* in the field.
*/
static inline bool
strict_mask8 (uint8_t a, uint8_t b, uint8_t am, uint8_t bm)
{
  return (am == bm) && ((a ^ b) & ~am) == 0;
}
static bool
match_std_strict (fluid_msg::of13::Match a, fluid_msg::of13::Match b)
{
  bool matched = match_ (a, b, &strict_mask8);
  return matched;
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
nonstrict_mask8 (uint8_t a, uint8_t b, uint8_t am, uint8_t bm)
{
  return (~am & (~a | ~b | bm) & (a | b | bm)) == 0;
}

static bool match_std_nonstrict
(fluid_msg::of13::Match a, fluid_msg::of13::Match b)
{
  bool matched = match_ (a, b, &nonstrict_mask8);
  return matched;
}

/* A special match, where it is assumed that the wildcards and masks of (b) are
* not used. Specifically used for matching on packets. */
static inline bool
pkt_mask8 (uint8_t a, uint8_t b, uint8_t am, uint8_t bm)
{
  return (~am & (a ^ b)) == 0;
}

static bool
match_std_pkt (fluid_msg::of13::Match a, fluid_msg::of13::Match b)
{
  bool matched = match_ (a, b, &pkt_mask8);
  return matched;
}

bool
Flow13::pkt_match (const Flow13& flow, const fluid_msg::of13::Match& pkt_match)
{
  return match_std_pkt (flow.match, pkt_match);
}

bool
Flow13::pkt_match_strict (const Flow13& flow, const fluid_msg::of13::Match& pkt_match)
{
  return match_std_strict (flow.match, pkt_match);
}

bool
Flow13::strict_match (const Flow13& flow_a, const Flow13& flow_b)
{
  return (flow_a.priority_ == flow_b.priority_) && match_std_strict (flow_a.match, flow_b.match);
}
bool
Flow13::non_strict_match (const Flow13& flow_a, const Flow13& flow_b)
{
  return match_std_nonstrict (flow_a.match, flow_b.match);
  return false;
}

uint64_t
Flow13::getDurationSec ()
{
  return (Simulator::Now ().GetNanoSeconds () - install_time_nsec) / NANOTOSECS;
}
uint64_t
Flow13::getDurationNSec ()
{
  return (Simulator::Now ().GetNanoSeconds () - install_time_nsec) % NANOTOSECS;
}

fluid_msg::of13::FlowStats*
Flow13::convertToFlowStats ()
{
  fluid_msg::of13::FlowStats* flowStats = new fluid_msg::of13::FlowStats (table_id_, getDurationSec (), getDurationNSec (), priority_, idle_timeout_, hard_timeout_, flags_, cookie_, packet_count_, byte_count_);
  return flowStats;
}
