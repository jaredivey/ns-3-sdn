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

#ifndef FLOW_TABLE_H
#define FLOW_TABLE_H
//Stdlib packages
#include <map>
#include <stack>
//ns3 utilities
#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/type-id.h"
#include "ns3/ipv6-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/icmpv6-header.h"
#include "ns3/icmpv4.h"
#include "ns3/arp-header.h"
#include "ns3/ethernet-header.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
//libfluid packages
#include <fluid/of10msg.hh>
//Sdn classes
#include "Flow.h"
#include "SdnCommon.h"

namespace ns3 {

class SdnSwitch;
/**
 * A comparators for flows. Sort via priority
 */
struct cmp_priority
{
  bool operator() (const Flow &lhs, const Flow &rhs) const
  {
    return lhs.priority_ <= rhs.priority_;
  }
};

/**
 * \ingroup sdn
 * \defgroup SdnFlowTable
 *
 * This class is a representation of a flow table. It's used exclusively by SdnSwitch objects
 * Every time a packet comes into an SdnSwitch, it must apply that packet to the flowtable to determine actions
 * to take on the packets.
 */

class SdnFlowTable
{
public:
  SdnFlowTable(Ptr<SdnSwitch> parentSwitch);
  /**
   * \brief reads a packet and commits the actions given the flows in the flow table
   * \param pkt The packet to read
   * \return The port number the switch must outport the packet from. If no outport is discerned, a value of OFPP_NONE is returned
   */
  uint16_t handlePacket (Ptr<Packet> pkt, uint16_t inPort);
  /**
   * \brief Getter for tableID
   * \return tableID
   */
  uint8_t getTableId (void) { return m_tableid; }
  /**
   * \brief Setter for tableID
   * \param tid TableID to set to
   */
  void setTableID (uint32_t tid) { m_tableid = tid; }
  /**
   * \brief Getter for table wildcards
   * \return wildcard
   */
  uint32_t getWildcards (void) { return m_wildcards; }
  /**
   * \brief Setter for table wildcards
   * \param wc Wildcard to set to
   */
  void setWildcards (uint32_t wc) { m_wildcards = wc; }
  /**
   * \brief Creates a tablestats object describing the flow table
   * \return A new table stats object
   */
  fluid_msg::of10::TableStats* convertToTableStats();
  /**
   * \brief Applies an action to a packet. Main entry point to other more specific action handlers
   * \param pkt The packet to modify
   * \action The action to apply to the packet
   * \return An outport if one is set. OFPP_NONE otherwise
   */
  uint16_t handleAction(Ptr<Packet> pkt,fluid_msg::Action* action);
  /**
   * \brief Finds a vector of flows in the table that will match to this specific match
   * \param match The match object that describes what we're looking for from the flows
   * \return A vector of all matching flows
   */
  std::vector<Flow> matchingFlows(fluid_msg::of10::Match match);
  /**
   * \brief A check to find whether a flow will conflict with any allready in the flow table
   * \param flow The possibly offending new flow
   * \return True if the new flow conflicts, false otherwise
   */
  bool conflictingEntry(Flow flow);
  /**
   * \brief Adds a new flow into the table
   * \param message The flowmod message that defines the new flow to add
   * \return The newly created flow
   */
  Flow addFlow(fluid_msg::of10::FlowMod* message);
  /**
   * \brief Modifies a flow in the table
   * \param message The flowmod message that defines the modified flow
   * \return The modified flow
   */
  Flow modifyFlow(fluid_msg::of10::FlowMod* message);
  //Flow modifyFlowStrict(fluid_msg::of10::FlowMod* message);
    /**
   * \brief Deletes a flow in the table
   * \param message The flowmod message that defines the flow to delete
   */
  void deleteFlow(fluid_msg::of10::FlowMod* message);
  //void deleteFlowStrict(fluid_msg::of10::FlowMod* message);
  /**
   * \brief Getter for all the flows in the table
   * \return m_table_flow_rules
   */
  std::set<Flow, cmp_priority> flows();
  //stat entries
  uint32_t m_max_entries;   //!< A count of all flow entries in the table
  uint32_t m_active_count;  //!< A count of all active flow entries in the table 
  uint64_t m_lookup_count;  //!< A count of all lookups done in the table
  uint64_t m_matched_count; //!< A count of all total matches completed in the table
private:
  Ptr<SdnSwitch> m_parentSwitch;                   //!< The owning SdnSwitch of this table     
  std::set<Flow, cmp_priority> m_flow_table_rules; //!< The actual set of all flows in the flow table. Sorted by priority
  uint8_t m_tableid;                               //!< Unique ID for flow tables
  uint32_t m_wildcards;                            //!< Wildcard rules for matches to ignore. NOT IMPLEMENTED
  template <class T> struct TempHeader { TempHeader() : isEmpty(true), header() {} bool isEmpty = true; T header; };
  TempHeader<EthernetHeader>  m_ethHeader;         //!< Private EthernetHeader for grabbing information out of the packet
  TempHeader<Ipv4Header> m_ipv4Header;             //!< Private Ipv4Header for grabbing information out of the packet
  TempHeader<Icmpv4Header> m_icmpv4Header;         //!< Private Icmpv4Header for grabbing information out of the packet
  TempHeader<Ipv6Header> m_ipv6Header;             //!< Private Ipv6Header for grabbing information out of the packet
  TempHeader<Icmpv6Header> m_icmpv6Header;         //!< Private Icmpv6Header for grabbing information out of the packet
  TempHeader<ArpHeader>  m_arpHeader;              //!< Private ArpHeader for grabbing information out of the packet
  TempHeader<TcpHeader>  m_tcpHeader;              //!< Private TcpHeader for grabbing information out of the packet
  TempHeader<UdpHeader>  m_udpHeader;              //!< Private UdpHeader for grabbing information out of the packet
  /**
   * \brief Creates a match object from the tempheader information 
   * \param inPort This field needs to go in the match in addition to the packet fields, but it is not
   * 			readily available from the packet information.
   * \return A match object matching whatever information could be discerned from the current packet
   */
  fluid_msg::of10::Match getPacketFields (uint16_t inPort);
  /**
   * \brief Action handler for an output action
   * \param pkt The packet being modified from the action
   * \param action The Output action being executed
   */
  uint16_t handleOutputAction (Ptr<Packet> pkt,fluid_msg::of10::OutputAction*    action);
  /**
   * \brief Action handler for a Set VLAN ID action
   * \param pkt The packet being modified from the action
   * \param action The Set VLAN ID action being executed
   */
  void handleSetVlanIDAction (Ptr<Packet>pkt,fluid_msg::of10::SetVLANVIDAction* action);
  /**
   * \brief Action handler for a Set VLAN PCP action
   * \param pkt The packet being modified from the action
   * \param action The Set VLAN PC action being executed
   */
  void handleSetVlanPCPAction (Ptr<Packet>pkt,fluid_msg::of10::SetVLANPCPAction* action);
  /**
   * \brief Action handler for a Strip VLAN action
   * \param pkt The packet being modified from the action
   * \param action The Strip VLAN action being executed
   */
  void handleStripVLANAction (Ptr<Packet>pkt,fluid_msg::of10::StripVLANAction*  action);
  /**
   * \brief Action handler for a Set DL Source action
   * \param pkt The packet being modified from the action
   * \param action The Set DL Source action being executed
   */
  void handleSetDLSrcAction  (Ptr<Packet>pkt,fluid_msg::of10::SetDLSrcAction*   action);
  /**
   * \brief Action handler for a Set DL Destination action
   * \param pkt The packet being modified from the action
   * \param action The Set DL Destination action being executed
   */
  void handleSetDLDstAction  (Ptr<Packet>pkt,fluid_msg::of10::SetDLDstAction*   action);
  /**
   * \brief Action handler for a Set Network Source action
   * \param pkt The packet being modified from the action
   * \param action The Set Network Source action being executed
   */
  void handleSetNWSrcAction  (Ptr<Packet>pkt,fluid_msg::of10::SetNWSrcAction*   action);
  /**
   * \brief Action handler for a Set Network Destination action
   * \param pkt The packet being modified from the action
   * \param action The Set Network Destination action being executed
   */
  void handleSetNWDstAction  (Ptr<Packet>pkt,fluid_msg::of10::SetNWDstAction*   action);
  /**
   * \brief Action handler for a Set Network Terms Of Service action
   * \param pkt The packet being modified from the action
   * \param action The Set Network Terms Of Service action being executed
   */
  void handleSetNWTOSAction  (Ptr<Packet>pkt,fluid_msg::of10::SetNWTOSAction*   action);
  /**
   * \brief Action handler for a Set Transport Source action
   * \param pkt The packet being modified from the action
   * \param action The Set Transport Source action being executed
   */
  void handleSetTPSrcAction  (Ptr<Packet>pkt,fluid_msg::of10::SetTPSrcAction*   action);
  /**
   * \brief Action handler for a Set Transport Destination action
   * \param pkt The packet being modified from the action
   * \param action The Set Transport Destination action being executed
   */
  void handleSetTPDstAction  (Ptr<Packet>pkt,fluid_msg::of10::SetTPDstAction*   action);
  /**
   * \brief Removes the TempHeaders from an actual packet so we can use them for analysis
   * \param pkt The packet containing the headers we're interested in
   * \return The Packet with the relevant headers removed
   */
  Ptr<Packet> DestructHeader (Ptr<Packet> pkt);
  /**
   * \brief Add the TempHeaders back onto a packet. Should always be called soon after DestructHeader
   * \param pkt The destructed packet ready to be reconstructed
   */
  void RestructHeader (Ptr<Packet> pkt);
  /**
   * \brief Idle Time Out Event hook. Gets invoked whenever a flow has reached it's idle time without any activity.
   */
  void IdleTimeOutEvent(Flow flow);
  /**
   * \brief Hard Time Out Event hook. Gets invoked whenever a flow has reached it's hard time
   */
  void HardTimeOutEvent(Flow flow);
  /**
   * \brief Generic timeout. Deletes a flow and asks the owning switch to send a notification to the controller
   */
  void TimeOutEvent(Flow flow, uint8_t reason);
};

} //End namespace ns3
#endif
