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

#include "SdnFlowTable.h"
#include "SdnSwitch.h"
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SdnFlowTable");

SdnFlowTable::SdnFlowTable (Ptr<SdnSwitch> parentSwitch)
{
  m_parentSwitch = parentSwitch;
  m_max_entries = 0;
  m_active_count = 0;
  m_lookup_count = 0;
  m_matched_count = 0;

}

std::set<Flow, cmp_priority>
SdnFlowTable::flows ()
{
  return m_flow_table_rules;
}

//Returns the out port. If not, return OFPP_NONE
uint16_t
SdnFlowTable::handlePacket (Ptr<Packet> pkt, uint16_t inPort)
{
  DestructHeader (pkt);
  fluid_msg::of10::Match inputFields = getPacketFields (inPort);
  std::set<Flow, cmp_priority>::iterator flow_rule = m_flow_table_rules.begin ();
  uint16_t outPort = fluid_msg::of10::OFPP_NONE;
  for (std::set<Flow, cmp_priority>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      m_lookup_count++;
      Flow tempFlow = *i;
      if (Flow::pkt_match (tempFlow,inputFields))
        {
          outPort = 0; // Initialize in case of no actions.
          m_matched_count++;
          tempFlow.packet_count_++;
          tempFlow.byte_count_ += pkt->GetSize ();
          std::list<fluid_msg::Action*> action_list = tempFlow.actions.action_list ();
          for (std::list<fluid_msg::Action*>::iterator j = action_list.begin (); j != action_list.end (); j++)
            {
              fluid_msg::Action* action = *j;
              if (action->type () == fluid_msg::of10::OFPAT_OUTPUT)
                {
                  outPort = handleAction (pkt,action);
                }
              else
                {
                  handleAction (pkt,action);
                }
            }
          if (tempFlow.idle_timeout_ != 0)
            {
              tempFlow.idle_timeout_event.Cancel ();
              tempFlow.idle_timeout_event = Simulator::Schedule (Seconds (tempFlow.idle_timeout_),&SdnFlowTable::IdleTimeOutEvent,this,tempFlow);
            }
        }
    }
  RestructHeader (pkt);
  return outPort;
}

//Need to rewrite to make header objects that serialize the iterator, then use their attributes.
fluid_msg::of10::Match
SdnFlowTable::getPacketFields (uint16_t inPort)
{
  fluid_msg::of10::Match match;
  match.in_port (inPort);
  if (!m_tcpHeader.isEmpty) //TCP case
    {
      match.tp_src (m_tcpHeader.header.GetSourcePort ());
      match.tp_dst (m_tcpHeader.header.GetDestinationPort ());
    }
  if (!m_udpHeader.isEmpty) //UDP case
    {
      match.tp_src (m_udpHeader.header.GetSourcePort ());
      match.tp_dst (m_udpHeader.header.GetDestinationPort ());
    }
  if (!m_ipv4Header.isEmpty) //IPv4 case
    {
      match.nw_tos (m_ipv4Header.header.GetTos ()); //zero out the last 2 bits that are ignored by the ToS
      if (!match.nw_proto ())
        {
          match.nw_proto (m_ipv4Header.header.GetProtocol ());
        }
      match.nw_src (fluid_msg::IPAddress (m_ipv4Header.header.GetSource ().Get ()));
      match.nw_dst (fluid_msg::IPAddress (m_ipv4Header.header.GetDestination ().Get ()));

      if ((m_ipv4Header.header.GetProtocol () == 1) && (!m_icmpv4Header.isEmpty))
	{
	  match.tp_src (m_icmpv4Header.header.GetType ());
	  match.tp_dst (m_icmpv4Header.header.GetCode ());
	}
    }
  if (!m_ipv6Header.isEmpty) //IPv6 case
    {
      uint8_t sourceBuffer[16];
      m_ipv6Header.header.GetSourceAddress ().GetBytes (sourceBuffer);
      match.nw_src (fluid_msg::IPAddress (sourceBuffer));
      uint8_t destinationBuffer[16];
      m_ipv6Header.header.GetDestinationAddress ().GetBytes (destinationBuffer);
      match.nw_dst (fluid_msg::IPAddress (destinationBuffer));
    }
  if (!m_arpHeader.isEmpty) //Arp case
    {
      match.nw_proto ((uint8_t)(m_arpHeader.header.IsRequest () ? ArpHeader::ARP_TYPE_REQUEST : ArpHeader::ARP_TYPE_REPLY));

      match.nw_src (fluid_msg::IPAddress (m_arpHeader.header.GetSourceIpv4Address ().Get ()));
      match.nw_dst (fluid_msg::IPAddress (m_arpHeader.header.GetDestinationIpv4Address ().Get ()));
    }
  if (!m_ethHeader.isEmpty) //IPv6 case
    {
      uint8_t srcAddr[6];
      m_ethHeader.header.GetSource ().CopyTo (srcAddr);
      uint8_t dstAddr[6];
      m_ethHeader.header.GetDestination ().CopyTo (dstAddr);
      match.dl_src (fluid_msg::EthAddress (srcAddr));
      match.dl_dst (fluid_msg::EthAddress (dstAddr));
      match.dl_type(m_ethHeader.header.GetLengthType ());

      //No VLAN header accessor yet
      match.dl_vlan (0);
    }
  return match;
}

std::vector<Flow>
SdnFlowTable::matchingFlows (fluid_msg::of10::Match match)
{
  std::vector<Flow> matchingFlows;
  for (std::set<Flow, cmp_priority>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      Flow tempFlow = *i;
      if (Flow::pkt_match (tempFlow,match))
        {
          matchingFlows.push_back (tempFlow);
        }
    }
  return matchingFlows;
}

//ACTION HANDLERS

uint16_t
SdnFlowTable::handleAction (Ptr<Packet> pkt,fluid_msg::Action* action)
{
  uint16_t outPort = fluid_msg::of10::OFPP_NONE;
  switch ((uint32_t)action->type ())
    {
    case fluid_msg::of10::OFPAT_OUTPUT:
      {
        fluid_msg::of10::OutputAction* specAction = (fluid_msg::of10::OutputAction*)action;
        outPort = handleOutputAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_SET_VLAN_VID:
      {
        fluid_msg::of10::SetVLANVIDAction* specAction = (fluid_msg::of10::SetVLANVIDAction*)action;
        handleSetVlanIDAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_SET_VLAN_PCP:
      {
        fluid_msg::of10::SetVLANPCPAction* specAction = (fluid_msg::of10::SetVLANPCPAction*)action;
        handleSetVlanPCPAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_STRIP_VLAN:
      {
        fluid_msg::of10::StripVLANAction* specAction = (fluid_msg::of10::StripVLANAction*)action;
        handleStripVLANAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_SET_DL_SRC:
      {
        fluid_msg::of10::SetDLSrcAction* specAction = (fluid_msg::of10::SetDLSrcAction*)action;
        handleSetDLSrcAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_SET_DL_DST:
      {
        fluid_msg::of10::SetDLDstAction* specAction = (fluid_msg::of10::SetDLDstAction*)action;
        handleSetDLDstAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_SET_NW_SRC:
      {
        fluid_msg::of10::SetNWSrcAction* specAction = (fluid_msg::of10::SetNWSrcAction*)action;
        handleSetNWSrcAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_SET_NW_DST:
      {
        fluid_msg::of10::SetNWDstAction* specAction = (fluid_msg::of10::SetNWDstAction*)action;
        handleSetNWDstAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_SET_NW_TOS:
      {
        fluid_msg::of10::SetNWTOSAction* specAction = (fluid_msg::of10::SetNWTOSAction*)action;
        handleSetNWTOSAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_SET_TP_SRC:
      {
        fluid_msg::of10::SetTPSrcAction* specAction = (fluid_msg::of10::SetTPSrcAction*)action;
        handleSetTPSrcAction (pkt,specAction);
        break;
      }
    case fluid_msg::of10::OFPAT_SET_TP_DST:
      {
        fluid_msg::of10::SetTPDstAction* specAction = (fluid_msg::of10::SetTPDstAction*)action;
        handleSetTPDstAction (pkt,specAction);
        break;
      }
    }
  return outPort;
}

uint16_t
SdnFlowTable::handleOutputAction (Ptr<Packet> pkt,fluid_msg::of10::OutputAction* action)
{
  if (action->max_len () < pkt->GetSize ()) //Only send max_len_ worth of data, fragment the packet.
    {
      pkt = pkt->CreateFragment (0,action->max_len ());
    }
  return action->port ();
}

void
SdnFlowTable::handleSetVlanIDAction (Ptr<Packet>pkt,fluid_msg::of10::SetVLANVIDAction* action)
{
  //
}

void
SdnFlowTable::handleSetVlanPCPAction (Ptr<Packet>pkt,fluid_msg::of10::SetVLANPCPAction* action)
{
  //
}

void
SdnFlowTable::handleStripVLANAction (Ptr<Packet>pkt,fluid_msg::of10::StripVLANAction* action)
{
  //
}

void
SdnFlowTable::handleSetDLSrcAction (Ptr<Packet>pkt,fluid_msg::of10::SetDLSrcAction* action)
{
  m_ethHeader.header.SetSource (Mac48Address (action->dl_addr ().to_string ().c_str ()));
}

void
SdnFlowTable::handleSetDLDstAction (Ptr<Packet>pkt,fluid_msg::of10::SetDLDstAction* action)
{
  m_ethHeader.header.SetDestination (Mac48Address (action->dl_addr ().to_string ().c_str ()));
}

void
SdnFlowTable::handleSetNWSrcAction (Ptr<Packet>pkt,fluid_msg::of10::SetNWSrcAction* action)
{
  if (!m_ipv4Header.isEmpty)
    {
      m_ipv4Header.header.SetSource (Ipv4Address (action->nw_addr ().getIPv4 ()));
    }
  if (!m_ipv6Header.isEmpty)
    {
      m_ipv6Header.header.SetSourceAddress (Ipv6Address (action->nw_addr ().getIPv6 ()));
    }
}

void
SdnFlowTable::handleSetNWDstAction (Ptr<Packet>pkt,fluid_msg::of10::SetNWDstAction* action)
{
  if (!m_ipv4Header.isEmpty)
    {
      m_ipv4Header.header.SetDestination (Ipv4Address (action->nw_addr ().getIPv4 ()));
    }
  if (!m_ipv6Header.isEmpty)
    {
      m_ipv6Header.header.SetDestinationAddress (Ipv6Address (action->nw_addr ().getIPv6 ()));
    }
}

void
SdnFlowTable::handleSetNWTOSAction (Ptr<Packet>pkt,fluid_msg::of10::SetNWTOSAction* action)
{
  if (!m_ipv4Header.isEmpty)
    {
      m_ipv4Header.header.SetTos (action->nw_tos ());
    }
}

void
SdnFlowTable::handleSetTPSrcAction (Ptr<Packet>pkt,fluid_msg::of10::SetTPSrcAction* action)
{
  if (!m_tcpHeader.isEmpty)
    {
      m_tcpHeader.header.SetSourcePort (action->tp_port ()); //Seems broken here. Check the fluid_msg action
    }
  if (!m_udpHeader.isEmpty)
    {
      m_udpHeader.header.SetSourcePort (action->tp_port ()); //Seems broken here. Check the fluid_msg action
    }
}

void
SdnFlowTable::handleSetTPDstAction (Ptr<Packet>pkt,fluid_msg::of10::SetTPDstAction* action)
{
  if (!m_tcpHeader.isEmpty)
    {
      m_tcpHeader.header.SetDestinationPort (action->tp_port ()); //Seems broken here. Check the fluid_msg action
    }
  if (!m_udpHeader.isEmpty)
    {
      m_udpHeader.header.SetDestinationPort (action->tp_port ()); //Seems broken here. Check the fluid_msg action
    }
}

//Utility methods to deconstruct/reconstruct headers
Ptr<Packet>
SdnFlowTable::DestructHeader (Ptr<Packet> pkt)
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
SdnFlowTable::RestructHeader (Ptr<Packet> pkt)
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

fluid_msg::of10::TableStats*
SdnFlowTable::convertToTableStats ()
{
  std::stringstream ss;
  ss << m_tableid;
  return new fluid_msg::of10::TableStats (m_tableid,ss.str ().c_str (),m_wildcards,m_max_entries,m_active_count,m_lookup_count,m_matched_count);
}

//Checks if this flow will conflict with a current flow of the same priority
bool
SdnFlowTable::conflictingEntry (Flow flow)
{
  for (std::set<Flow, cmp_priority>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      if (Flow::strict_match (flow,*i))
        {
          return true;
        }
    }
  return false;
}

Flow
SdnFlowTable::addFlow (fluid_msg::of10::FlowMod* message)
{
  NS_LOG_DEBUG ("Adding new flow on switch at time" << Simulator::Now ().GetSeconds ());
  Flow newFlow = Flow ();
  newFlow.length_ = message->length ();
  newFlow.table_id_ = getTableId ();
  newFlow.priority_ = message->priority ();
  newFlow.idle_timeout_ = message->idle_timeout ();
  newFlow.hard_timeout_ = message->hard_timeout ();
  newFlow.cookie_ = message->cookie ();
  newFlow.packet_count_ = 0;
  newFlow.byte_count_ = 0;

  newFlow.match = message->match ();
  newFlow.match.dl_vlan (0); // Hack since ns-3 doesn't currently support VLAN
  newFlow.nw_src_mask = 0xFFFFFFF >> (32 - ((newFlow.match.wildcards () & fluid_msg::of10::OFPFW_NW_SRC_MASK) >> fluid_msg::of10::OFPFW_NW_SRC_SHIFT) - 1);
  newFlow.nw_dst_mask = 0xFFFFFFF >> (32 - ((newFlow.match.wildcards () & fluid_msg::of10::OFPFW_NW_DST_MASK) >> fluid_msg::of10::OFPFW_NW_DST_SHIFT) - 1);
  newFlow.actions = message->actions ();

  if ((message->flags () & fluid_msg::of10::OFPFF_CHECK_OVERLAP) && conflictingEntry (newFlow))
    {
      return Flow ();
    }
  if (newFlow.idle_timeout_ != 0)
    {
      newFlow.idle_timeout_event = Simulator::Schedule (Seconds (newFlow.idle_timeout_),&SdnFlowTable::IdleTimeOutEvent,this,newFlow);
    }
  if (newFlow.hard_timeout_ != 0)
    {
      newFlow.hard_timeout_event = Simulator::Schedule (Seconds (newFlow.hard_timeout_),&SdnFlowTable::HardTimeOutEvent,this,newFlow);
    }
  m_flow_table_rules.insert (newFlow);
  m_active_count++;
  return newFlow;
}

Flow
SdnFlowTable::modifyFlow (fluid_msg::of10::FlowMod* message)
{
  NS_LOG_DEBUG ("Modifying flow on switch at time" << Simulator::Now ().GetSeconds ());
  for (std::set<Flow, cmp_priority>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      Flow flow = *i;
      if (flow.priority_ == message->priority () && Flow::pkt_match (flow,message->match ()))
        {
          flow.actions = message->actions ();
          flow.cookie_ = message->cookie ();
          if (flow.idle_timeout_ != 0)
            {
              flow.idle_timeout_event.Cancel ();
              flow.idle_timeout_event = Simulator::Schedule (Seconds (flow.idle_timeout_),&SdnFlowTable::IdleTimeOutEvent,this,flow);
            }
          if (flow.hard_timeout_ != 0)
            {
              flow.hard_timeout_event.Cancel ();
              flow.hard_timeout_event = Simulator::Schedule (Seconds (flow.hard_timeout_),&SdnFlowTable::HardTimeOutEvent,this,flow);
            }
          return flow;
        }
    }
  return Flow ();
}

void
SdnFlowTable::deleteFlow (fluid_msg::of10::FlowMod* message)
{
  NS_LOG_DEBUG ("Deleting flow on switch at time" << Simulator::Now ().GetSeconds ());
  for (std::set<Flow, cmp_priority>::iterator i = m_flow_table_rules.begin (); i != m_flow_table_rules.end (); i++)
    {
      Flow flow = *i;
      if (flow.priority_ == message->priority () && Flow::pkt_match (flow,message->match ()))
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
SdnFlowTable::IdleTimeOutEvent (Flow flow)
{
  NS_LOG_DEBUG ("Idle timeout event of flow at time " << Simulator::Now ().GetSeconds ());
  TimeOutEvent (flow, fluid_msg::of10::OFPRR_IDLE_TIMEOUT);
}

void
SdnFlowTable::HardTimeOutEvent (Flow flow)
{
  NS_LOG_DEBUG ("Hard timeout event of flow at time " << Simulator::Now ().GetSeconds ());
  TimeOutEvent (flow, fluid_msg::of10::OFPRR_HARD_TIMEOUT);
}

void
SdnFlowTable::TimeOutEvent (Flow flow, uint8_t reason)
{
  m_flow_table_rules.erase (flow);
  m_parentSwitch->SendFlowRemovedMessageToController (flow,reason);
  //Send a flow removed message back to controller
}

} //End ns3 namespace
