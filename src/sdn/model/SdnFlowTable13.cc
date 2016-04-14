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

#include "SdnFlowTable13.h"
#include "SdnSwitch13.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SdnFlowTable13");

NS_OBJECT_ENSURE_REGISTERED (SdnFlowTable13);

std::map<uint32_t, std::vector< Ptr<SdnFlowTable13> > > SdnFlowTable13::g_flowTables;

TypeId SdnFlowTable13::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SdnFlowTable13")
    .SetParent<Object> ()
    .AddConstructor<SdnSwitch> ()
  ;
  return tid;
}

SdnFlowTable13::SdnFlowTable13 (Ptr<SdnSwitch13> parentSwitch)
{
  m_parentSwitch = parentSwitch;
  m_max_entries = 0;
  m_active_count = 0;
  m_lookup_count = 0;
  m_matched_count = 0;
  m_tableid = 0;
}

Ptr<SdnFlowTable13>
SdnFlowTable13::addTablesForNewSwitch(Ptr<SdnSwitch13> parent)
{
	uint32_t datapathId = parent->getDatapathID ();
	g_flowTables[datapathId] = std::vector< Ptr<SdnFlowTable13> > ();
	for (uint32_t i = 0; i < 64; ++i)
	{
		Ptr<SdnFlowTable13> newTable = Create<SdnFlowTable13> (parent);
		newTable->setTableID (i);
		g_flowTables[datapathId].push_back (newTable);
	}
	return g_flowTables[datapathId].at(0);
}

std::set<Flow13, cmp_priority13>
SdnFlowTable13::flows ()
{
  return m_flow_table_rules;
}

//Returns the out port. If not, return OFPP_NONE
fluid_msg::ActionSet*
SdnFlowTable13::handlePacket (Ptr<Packet> pkt, fluid_msg::ActionSet *action_set, uint32_t inPort)
{
  DestructHeader (pkt);
  fluid_msg::of13::Match inputFields = getPacketFields (inPort);
  std::set<Flow13, cmp_priority13>::iterator flow_rule = m_flow_table_rules.begin ();
  std::vector<uint32_t> outPorts;
  for (std::set<Flow13, cmp_priority13>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      m_lookup_count++;
      Flow13 tempFlow = *i;
      if (Flow13::pkt_match (tempFlow,inputFields))
        {
          m_matched_count++;
          tempFlow.packet_count_++;
          tempFlow.byte_count_ += pkt->GetSize ();
          std::set<fluid_msg::of13::Instruction*, fluid_msg::of13::comp_inst_set_order> instruction_list = tempFlow.instructions.instruction_set ();
          for (std::set<fluid_msg::of13::Instruction*, fluid_msg::of13::comp_inst_set_order>::iterator j = instruction_list.begin ();
        		  j != instruction_list.end (); j++)
          {
        	  fluid_msg::of13::Instruction* instruction = *j;
        	  // Required by OpenFlow 1.3.0
        	  if (instruction->type () == fluid_msg::of13::OFPIT_GOTO_TABLE)
        	  {
        		  // Try the next specified table
        		  fluid_msg::of13::GoToTable *gotoTable = dynamic_cast<fluid_msg::of13::GoToTable *> (instruction);
        		  return g_flowTables[m_parentSwitch->getDatapathID()].at(gotoTable->table_id ())->handlePacket(pkt, action_set, inPort);
        	  }
        	  // Required by OpenFlow 1.3.0
        	  else if (instruction->type () == fluid_msg::of13::OFPIT_WRITE_ACTIONS)
        	  {
        		  fluid_msg::of13::WriteActions *writeAction = dynamic_cast<fluid_msg::of13::WriteActions *> (instruction);
        		  fluid_msg::ActionSet newActionSet = writeAction->actions();
        		  for (std::set<fluid_msg::Action*, fluid_msg::comp_action_set_order>::iterator k = newActionSet.action_set ().begin ();
        				  k != newActionSet.action_set ().end (); k++)
				  {
        			  fluid_msg::Action* action = *k;
        			  action_set->add_action(action);
				  }
        	  }
        	  // Additional instructions are optional for OpenFlow 1.3.0
        	  else if (instruction->type () == fluid_msg::of13::OFPIT_METER)
        	  {

        	  }
        	  else if (instruction->type () == fluid_msg::of13::OFPIT_APPLY_ACTIONS)
        	  {

        	  }
        	  else if (instruction->type () == fluid_msg::of13::OFPIT_CLEAR_ACTIONS)
        	  {

        	  }
        	  else if (instruction->type () == fluid_msg::of13::OFPIT_WRITE_METADATA)
        	  {

        	  }

          }
          if (tempFlow.idle_timeout_ != 0)
            {
              tempFlow.idle_timeout_event.Cancel ();
              tempFlow.idle_timeout_event = Simulator::Schedule (Seconds (tempFlow.idle_timeout_),&SdnFlowTable13::IdleTimeOutEvent,this,tempFlow);
            }
        }
    }
  RestructHeader (pkt);
  return action_set;
}

//Need to rewrite to make header objects that serialize the iterator, then use their attributes.
fluid_msg::of13::Match
SdnFlowTable13::getPacketFields (uint32_t inPort)
{
 	 fluid_msg::of13::Match match;
 
 	  fluid_msg::of13::InPort *InPort_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_IN_PORT);
 	  InPort_oxmtlv->value(inPort);
 	  match.add_oxm_field(InPort_oxmtlv);
 
 	  if (!m_tcpHeader.isEmpty) //TCP case
 	    {
 	      fluid_msg::of13::TCPDst *Dst_oxmtlv = match.make_oxm_tlv (fluid_msg::of13::OFPXMT_OFB_TCP_DST);
 	      Dst_oxmtlv -> value(m_tcpHeader.header.GetDestinationPort());
 	      match.add_oxm_field(Dst_oxmtlv);
 
 	      fluid_msg::of13::TCPSrc *Src_oxmtlv = match.make_oxm_tlv (fluid_msg::of13::OFPXMT_OFB_TCP_SRC);
 	      Src_oxmtlv -> value(m_tcpHeader.header.GetSourcePort());
 	      match.add_oxm_field(Src_oxmtlv);
 	    }
 	  if (!m_udpHeader.isEmpty) //UDP case
 	    {
 	      fluid_msg::of13::UDPDst *Dst_oxmtlv = match.make_oxm_tlv (fluid_msg::of13::OFPXMT_OFB_UDP_DST);
 	      Dst_oxmtlv -> value(m_udpHeader.header.GetDestinationPort());
 	      match.add_oxm_field(Dst_oxmtlv);
 
 	      fluid_msg::of13::UDPSrc *Src_oxmtlv = match.make_oxm_tlv (fluid_msg::of13::OFPXMT_OFB_UDP_SRC);
 	      Src_oxmtlv -> value(m_udpHeader.header.GetSourcePort());
 	      match.add_oxm_field(Src_oxmtlv);
 	    }
 
 	  if (!m_ipv4Header.isEmpty) //IPv4 case
 	    {
 		  fluid_msg::of13::IPDSCP *Dscp_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_IP_DSCP);
 		  Dscp_oxmtlv->value(m_ipv4Header.header.GetDscp());
 		  match.add_oxm_field(Dscp_oxmtlv);
 
 		  fluid_msg::of13::IPECN *Ecn_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_IP_ECN);
 		  Ecn_oxmtlv->value(m_ipv4Header.header.GetEcn());
 		  match.add_oxm_field(Ecn_oxmtlv);
 
 	      if (!match.ip_proto ())
 	        {
 	    	  fluid_msg::of13::IPProto *IPpro_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_IP_PROTO);
 	    	  Ecn_oxmtlv->value(m_ipv4Header.header.GetProtocol());
 	    	  match.add_oxm_field(IPpro_oxmtlv);
 	        }
 
 		  fluid_msg::of13::IPv4Src *Src_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_IPV4_SRC);
 		  Src_oxmtlv->value(m_ipv4Header.header.GetSource().Get());
 		  match.add_oxm_field(Src_oxmtlv);
 
 		  fluid_msg::of13::IPv4Src *Dst_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_IPV4_DST);
 		  Dst_oxmtlv->value(m_ipv4Header.header.GetDestination().Get());
 		  match.add_oxm_field(Dst_oxmtlv);
 
 
 	      if ((m_ipv4Header.header.GetProtocol () == 1) && (!m_icmpv4Header.isEmpty))
 		    {
 	    	  fluid_msg::of13::ICMPv4Type *Type_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ICMPV4_TYPE);
 	    	  Type_oxmtlv->value(m_icmpv4Header.header.GetType ());
 	    	  match.add_oxm_field(Type_oxmtlv);
 
 	    	  fluid_msg::of13::ICMPv4Code *Code_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ICMPV4_CODE);
 	    	  Type_oxmtlv->value(m_icmpv4Header.header.GetCode ());
 	    	  match.add_oxm_field(Code_oxmtlv);
 		    }
 	    }
 	  if (!m_ipv6Header.isEmpty) //IPv6 case
 	    {
 	      uint8_t sourceBuffer[16];
 	      m_ipv6Header.header.GetSourceAddress ().GetBytes (sourceBuffer);
 		  fluid_msg::of13::IPv6Src *Src_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_IPV6_SRC);
 		  Src_oxmtlv->value(sourceBuffer);
 		  match.add_oxm_field(Src_oxmtlv);
 
 	      uint8_t destinationBuffer[16];
 	      m_ipv6Header.header.GetDestinationAddress ().GetBytes (destinationBuffer);
 		  fluid_msg::of13::IPv6Dst *Dst_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_IPV6_DST);
 		  Dst_oxmtlv->value(destinationBuffer);
 		  match.add_oxm_field(Dst_oxmtlv);
 
 		  fluid_msg::of13::IPV6Flabel *Flabel_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_IPV6_FLABEL);
 		  Flabel_oxmtlv->value(m_ipv6Header.header.GetFlowLabel());
 		  match.add_oxm_field(Flabel_oxmtlv);
 
 
 	    }
 	  if (!m_arpHeader.isEmpty) //Arp case
 	    {
 
 		  fluid_msg::of13::ARPOp *Op_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ARP_OP);
 		  Op_oxmtlv->value((uint16_t)m_arpHeader.header.IsRequest () ? ArpHeader::ARP_TYPE_REQUEST: ArpHeader::ARP_TYPE_REPLY);
 		  match.add_oxm_field(Op_oxmtlv); //????
 
 
 	      //match.nw_proto ((uint8_t)(m_arpHeader.header.IsRequest () ? ArpHeader::ARP_TYPE_REQUEST : ArpHeader::ARP_TYPE_REPLY));
 
 		  fluid_msg::of13::ARPSPA *SPA_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ARP_SPA);
 		  SPA_oxmtlv->value(m_arpHeader.header.GetSourceIpv4Address().Get());
 		  match.add_oxm_field(SPA_oxmtlv);
 
 		  fluid_msg::of13::ARPTPA *TPA_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ARP_TPA);
 		  TPA_oxmtlv->value(m_arpHeader.header.GetDestinationIpv4Address().Get());
 		  match.add_oxm_field(TPA_oxmtlv);
 
 		  uint8_t srcAddr[6];
 		  m_arpHeader.header.GetSourceHardwareAddress().CopyTo(srcAddr);
 		  fluid_msg::of13::ARPSHA *SHA_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ARP_SHA);
 		  SHA_oxmtlv->value(srcAddr);
 		  match.add_oxm_field(SHA_oxmtlv);
 
 		  uint8_t dstAddr[6];
 		  m_arpHeader.header.GetDestinationHardwareAddress().CopyTo(dstAddr);
 		  fluid_msg::of13::ARPTHA *THA_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ARP_THA);
 		  THA_oxmtlv->value(dstAddr);
 		  match.add_oxm_field(THA_oxmtlv);
 
 	    }
 	  if (!m_ethHeader.isEmpty) //Ethernet case
 	    {
 	      uint8_t srcAddr[6];
 	      m_ethHeader.header.GetSource ().CopyTo (srcAddr);
 	      uint8_t dstAddr[6];
 	      m_ethHeader.header.GetDestination ().CopyTo (dstAddr);
 
 		  fluid_msg::of13::EthSrc *Src_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ETH_SRC);
 		  Src_oxmtlv->value(srcAddr);
 		  match.add_oxm_field(Src_oxmtlv);
 
 		  fluid_msg::of13::EthDst *Dst_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ETH_DST);
 		  Dst_oxmtlv->value(dstAddr);
 		  match.add_oxm_field(Dst_oxmtlv);
 
 		  fluid_msg::of13::EthType *Type_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_ETH_TYPE);
 		  Type_oxmtlv->value(m_ethHeader.header.GetLengthType ());
 		  match.add_oxm_field(Type_oxmtlv);
 
 
 	      //No VLAN header accessor yet
 		  fluid_msg::of13::VLANVid *Vid_oxmtlv = match.make_oxm_tlv(fluid_msg::of13::OFPXMT_OFB_VLAN_VID);
 		  Vid_oxmtlv->value(0);
 		  match.add_oxm_field(Vid_oxmtlv);
 
 
 	    }
 	  return match;
}

std::vector<Flow13>
SdnFlowTable13::matchingFlows (fluid_msg::of13::Match match)
{
  std::vector<Flow13> matchingFlows;
  for (std::set<Flow13, cmp_priority13>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      Flow13 tempFlow = *i;
      if (Flow13::pkt_match (tempFlow,match))
        {
          matchingFlows.push_back (tempFlow);
        }
    }
  return matchingFlows;
}

//ACTION HANDLERS

std::vector<uint32_t>
SdnFlowTable13::handleActions (Ptr<Packet> pkt,fluid_msg::ActionSet* action_set)
{
  std::vector<uint32_t> outPorts;
  for (std::set<fluid_msg::Action*, fluid_msg::comp_action_set_order>::iterator i = action_set->action_set ().begin ();
		  i != action_set->action_set ().end (); i++)
    {
	  fluid_msg::Action *action = *i;
	  switch ((uint32_t)action->type ())
		{
			case fluid_msg::of13::OFPAT_OUTPUT:
			{
				fluid_msg::of13::OutputAction* outputAction = dynamic_cast<fluid_msg::of13::OutputAction*>(action);
				outPorts.push_back(handleOutputAction (pkt,outputAction));
				break;
			}
			case fluid_msg::of13::OFPAT_GROUP:
			{
				fluid_msg::of13::GroupAction* groupAction = dynamic_cast<fluid_msg::of13::GroupAction*>(action);
				outPorts.push_back(handleGroupAction (pkt,groupAction));
				break;
			}
			default:
				break;
		}
    }
  return outPorts;
}

std::vector<uint32_t>
SdnFlowTable13::handleActions (Ptr<Packet> pkt,fluid_msg::ActionList* action_list)
{
  std::vector<uint32_t> outPorts;
  for (std::list<fluid_msg::Action*>::iterator i = action_list->action_list ().begin ();
		  i != action_list->action_list ().end (); i++)
    {
	  fluid_msg::Action *action = *i;
	  switch ((uint32_t)action->type ())
		{
			case fluid_msg::of13::OFPAT_OUTPUT:
			{
				fluid_msg::of13::OutputAction* outputAction = dynamic_cast<fluid_msg::of13::OutputAction*>(action);
				outPorts.push_back(handleOutputAction (pkt,outputAction));
				break;
			}
			case fluid_msg::of13::OFPAT_GROUP:
			{
				fluid_msg::of13::GroupAction* groupAction = dynamic_cast<fluid_msg::of13::GroupAction*>(action);
				outPorts.push_back(handleGroupAction (pkt,groupAction));
				break;
			}
			default:
				break;
		}
    }
  return outPorts;
}

uint32_t
SdnFlowTable13::handleOutputAction (Ptr<Packet> pkt,fluid_msg::of13::OutputAction* action)
{
  if (action->max_len () < pkt->GetSize ()) //Only send max_len_ worth of data, fragment the packet.
    {
      pkt = pkt->CreateFragment (0,action->max_len ());
    }
  return action->port ();
}

uint32_t
SdnFlowTable13::handleGroupAction (Ptr<Packet> pkt,fluid_msg::of13::GroupAction* action)
{
  // Figure out which group table to gather action buckets from. action->group_id ();
  return 0;
}

//Utility methods to deconstruct/reconstruct headers
Ptr<Packet>
SdnFlowTable13::DestructHeader (Ptr<Packet> pkt)
{
  PacketMetadata metadata (pkt->GetUid (),pkt->GetSize ());
  PacketMetadata::ItemIterator iterator = pkt->BeginItem ();
  PacketMetadata::Item item;
  while (iterator.HasNext ())
    {
      item = iterator.Next ();
      if (item.type != PacketMetadata::Item::HEADER)
        {
          continue;
        }
      else if (item.tid.GetName () == "ns3::TcpHeader" && pkt->PeekHeader (m_tcpHeader.header))
        {
          pkt->RemoveHeader (m_tcpHeader.header);
          m_tcpHeader.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::UdpHeader" && pkt->PeekHeader (m_udpHeader.header))
        {
          pkt->RemoveHeader (m_udpHeader.header);
          m_udpHeader.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::Ipv4Header" && pkt->PeekHeader (m_ipv4Header.header))
        {
          pkt->RemoveHeader (m_ipv4Header.header);
          m_ipv4Header.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::Icmpv4Header" && pkt->PeekHeader (m_icmpv4Header.header))
        {
          pkt->RemoveHeader (m_icmpv4Header.header);
          m_icmpv4Header.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::Ipv6Header" && pkt->PeekHeader (m_ipv6Header.header))
        {
          pkt->RemoveHeader (m_ipv6Header.header);
          m_ipv6Header.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::Ipv6ExtensionHeader" && pkt->PeekHeader (m_ipv6ExtHeader.header))
        {
          pkt->RemoveHeader (m_ipv6ExtHeader.header);
          m_ipv6ExtHeader.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::Ipv6OptionHeader" && pkt->PeekHeader (m_ipv6OptHeader.header))
        {
          pkt->RemoveHeader (m_ipv6OptHeader.header);
          m_ipv6OptHeader.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::Icmpv6Header" && pkt->PeekHeader (m_icmpv6Header.header))
        {
          pkt->RemoveHeader (m_icmpv6Header.header);
          m_icmpv6Header.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::ArpHeader"  && pkt->PeekHeader (m_arpHeader.header))
        {
          pkt->RemoveHeader (m_arpHeader.header);
          m_arpHeader.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::EthernetHeader" && pkt->PeekHeader (m_ethHeader.header))
        {
          pkt->RemoveHeader (m_ethHeader.header);
          m_ethHeader.isEmpty = false;
        }
      else if (item.tid.GetName () == "ns3::LlcSnapHeader")
        {
        }
    }
  return pkt;
}

//Does the opposite, re-adds the headers we need
void
SdnFlowTable13::RestructHeader (Ptr<Packet> pkt)
{
  if (!m_udpHeader.isEmpty)
    {
      pkt->AddHeader (m_udpHeader.header);
    }
  if (!m_tcpHeader.isEmpty)
    {
      pkt->AddHeader (m_tcpHeader.header);
    }
  if (!m_icmpv6Header.isEmpty)
    {
      pkt->AddHeader (m_icmpv6Header.header);
    }
  if (!m_ipv6OptHeader.isEmpty)
    {
      pkt->AddHeader (m_ipv6OptHeader.header);
    }
  if (!m_ipv6ExtHeader.isEmpty)
    {
      pkt->AddHeader (m_ipv6ExtHeader.header);
    }
  if (!m_icmpv4Header.isEmpty)
    {
      pkt->AddHeader (m_icmpv4Header.header);
    }
  if (!m_ipv6Header.isEmpty)
    {
      pkt->AddHeader (m_ipv6Header.header);
    }
  if (!m_ipv4Header.isEmpty)
    {
      pkt->AddHeader (m_ipv4Header.header);
    }
  if (!m_arpHeader.isEmpty)
    {
      pkt->AddHeader (m_arpHeader.header);
    }
  if (!m_ethHeader.isEmpty)
    {
      pkt->AddHeader (m_ethHeader.header);
    }
  //Clear out headers so they're useful for the next read
  m_ethHeader.header = EthernetHeader ();
  m_ethHeader.isEmpty = true;
  m_ipv4Header.header = Ipv4Header ();
  m_ipv4Header.isEmpty = true;
  m_ipv6Header.header = Ipv6Header ();
  m_ipv6Header.isEmpty = true;
  m_arpHeader.header = ArpHeader ();
  m_arpHeader.isEmpty = true;
  m_tcpHeader.header = TcpHeader ();
  m_tcpHeader.isEmpty = true;
  m_udpHeader.header = UdpHeader ();
  m_udpHeader.isEmpty = true;
}

fluid_msg::of13::TableStats*
SdnFlowTable13::convertToTableStats ()
{
  std::stringstream ss;
  ss << m_tableid;
  return new fluid_msg::of13::TableStats (m_tableid,m_active_count,m_lookup_count,m_matched_count);
}

//Checks if this flow will conflict with a current flow of the same priority
bool
SdnFlowTable13::conflictingEntry (Flow13 flow)
{
  for (std::set<Flow13, cmp_priority13>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      if (Flow13::strict_match (flow,*i))
        {
          return true;
        }
    }
  return false;
}

Flow13
SdnFlowTable13::addFlow (fluid_msg::of13::FlowMod* message)
{
  NS_LOG_DEBUG ("Adding new flow on switch at time" << Simulator::Now ().GetSeconds ());
  Flow13 newFlow = Flow13 ();
  newFlow.length_ = message->length ();
  newFlow.table_id_ = message->table_id ();
  newFlow.priority_ = message->priority ();
  newFlow.idle_timeout_ = message->idle_timeout ();
  newFlow.hard_timeout_ = message->hard_timeout ();
  newFlow.cookie_ = message->cookie ();
  newFlow.packet_count_ = 0;
  newFlow.byte_count_ = 0;

  newFlow.match = message->match ();
  newFlow.instructions = message->instructions ();
  //newFlow.match.dl_vlan (0); // Hack since ns-3 doesn't currently support VLAN
  //newFlow.actions = message->actions ();

  if ((message->flags () & fluid_msg::of13::OFPFF_CHECK_OVERLAP) && conflictingEntry (newFlow))
    {
      return Flow13 ();
    }
  if (!(message->flags () & fluid_msg::of13::OFPFF_CHECK_OVERLAP) && conflictingEntry (newFlow))
    {
      deleteFlow (message);
    }
  if (newFlow.idle_timeout_ != 0)
    {
      newFlow.idle_timeout_event = Simulator::Schedule (Seconds (newFlow.idle_timeout_),&SdnFlowTable13::IdleTimeOutEvent,this,newFlow);
    }
  if (newFlow.hard_timeout_ != 0)
    {
      newFlow.hard_timeout_event = Simulator::Schedule (Seconds (newFlow.hard_timeout_),&SdnFlowTable13::HardTimeOutEvent,this,newFlow);
    }
  m_flow_table_rules.insert (newFlow);
  m_active_count++;
  return newFlow;
}

Flow13
SdnFlowTable13::modifyFlow (fluid_msg::of13::FlowMod* message)
{
  NS_LOG_DEBUG ("Modifying flow on switch at time" << Simulator::Now ().GetSeconds ());
  for (std::set<Flow13, cmp_priority13>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      Flow13 flow = *i;
      if (flow.priority_ == message->priority () && Flow13::pkt_match (flow,message->match ()))
        {
          flow.instructions = message->instructions ();
          flow.cookie_ = message->cookie ();
          if (flow.idle_timeout_ != 0)
            {
              flow.idle_timeout_event.Cancel ();
              flow.idle_timeout_event = Simulator::Schedule (Seconds (flow.idle_timeout_),&SdnFlowTable13::IdleTimeOutEvent,this,flow);
            }
          if (flow.hard_timeout_ != 0)
            {
              flow.hard_timeout_event.Cancel ();
              flow.hard_timeout_event = Simulator::Schedule (Seconds (flow.hard_timeout_),&SdnFlowTable13::HardTimeOutEvent,this,flow);
            }
          return flow;
        }
    }
  return Flow13 ();
}

void
SdnFlowTable13::deleteFlow (fluid_msg::of13::FlowMod* message)
{
  NS_LOG_DEBUG ("Deleting flow on switch at time" << Simulator::Now ().GetSeconds ());
  for (std::set<Flow13, cmp_priority>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      Flow13 flow = *i;
      if (flow.priority_ == message->priority () && Flow13::pkt_match (flow,message->match ()))
        {
          if (flow.idle_timeout_ != 0)
            {
              flow.idle_timeout_event.Cancel ();
            }
          if (flow.hard_timeout_ != 0)
            {
              flow.hard_timeout_event.Cancel ();
            }
          m_flow_table_rules.erase (flow);
        }
    }
  m_active_count--;
}

void
SdnFlowTable13::IdleTimeOutEvent (Flow13 flow)
{
  NS_LOG_DEBUG ("Idle timeout event of flow at time " << Simulator::Now ().GetSeconds ());
  TimeOutEvent (flow, fluid_msg::of13::OFPRR_IDLE_TIMEOUT);
}

void
SdnFlowTable13::HardTimeOutEvent (Flow13 flow)
{
  NS_LOG_DEBUG ("Hard timeout event of flow at time " << Simulator::Now ().GetSeconds ());
  TimeOutEvent (flow, fluid_msg::of13::OFPRR_HARD_TIMEOUT);
}

void
SdnFlowTable13::TimeOutEvent (Flow13 flow, uint8_t reason)
{
  m_flow_table_rules.erase (flow);
  m_parentSwitch->SendFlowRemovedMessageToController (flow,reason);
  //Send a flow removed message back to controller
}

} //End ns3 namespace
