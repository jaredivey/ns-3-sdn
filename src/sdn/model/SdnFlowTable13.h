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

#ifndef FLOW_TABLE13_H
#define FLOW_TABLE13_H
//Stdlib packages
#include <map>
#include <stack>
//ns3 utilities
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/type-id.h"
#include "ns3/ipv6-header.h"
#include "ns3/ipv6-extension-header.h"
#include "ns3/ipv6-option-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/icmpv6-header.h"
#include "ns3/icmpv4.h"
#include "ns3/arp-header.h"
#include "ns3/ethernet-header.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
//libfluid packages
#include <fluid/of13msg.hh>
#include <fluid/of13/openflow-13.h>
//Sdn classes
#include "Flow13.h"
#include "SdnGroup13.h"
#include "SdnCommon.h"

namespace ns3 {

class SdnSwitch13;
/**
 * A comparators for flows. Sort via priority
 */
struct cmp_priority13
{
  bool operator() (const Flow13 &lhs, const Flow13 &rhs) const
  {
    return lhs.priority_ <= rhs.priority_;
  }
};

/**
 * \ingroup sdn
 * \defgroup SdnFlowTable13
 *
 * This class is a representation of a flow table. It's used exclusively by SdnSwitch objects
 * Every time a packet comes into an SdnSwitch, it must apply that packet to the flowtable to determine actions
 * to take on the packets.
 */

class SdnFlowTable13 : public Object
{
public:
  SdnFlowTable13() {}
  SdnFlowTable13(Ptr<SdnSwitch13> parentSwitch);
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  static Ptr<SdnFlowTable13> addTablesForNewSwitch (Ptr<SdnSwitch13> parent);
  static std::map<uint32_t, std::vector< Ptr<SdnFlowTable13> > > g_flowTables;
  /**
   * \brief reads a packet and commits the actions given the flows in the flow table
   * \param pkt The packet to read
   * \return The ports  the switch must outport the packet from. If no outport is discerned, the vector will be empty
   */
  fluid_msg::ActionSet* handlePacket (Ptr<Packet> pkt, fluid_msg::ActionSet *action_set, uint32_t inPort);
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
   * \brief Creates a tablestats object describing the flow table
   * \return A new table stats object
   */
  fluid_msg::of13::TableStats* convertToTableStats(uint8_t whichTable = 0);
  /**
   * \brief Applies an action to a packet. Main entry point to other more specific action handlers
   * \param pkt The packet to modify
   * \action The action to apply to the packet
   * \return An outport if one is set. OFPP_NONE otherwise
   */
  std::vector<uint32_t> handleActions(Ptr<Packet> pkt,fluid_msg::ActionSet* action_set);
  std::vector<uint32_t> handleActions(Ptr<Packet> pkt,fluid_msg::ActionList* action_list);
  /**
   * \brief Finds a vector of flows in the table that will match to this specific match
   * \param match The match object that describes what we're looking for from the flows
   * \return A vector of all matching flows
   */
  std::vector<Flow13> matchingFlows(fluid_msg::of13::Match match);
  /**
   * \brief A check to find whether a flow will conflict with any allready in the flow table
   * \param flow The possibly offending new flow
   * \return True if the new flow conflicts, false otherwise
   */
  bool conflictingEntry(Flow13 flow);
  /**
   * \brief Adds a new flow into the table
   * \param message The flowmod message that defines the new flow to add
   * \return The newly created flow
   */
  Flow13 addFlow(fluid_msg::of13::FlowMod* message);
  /**
   * \brief Modifies a flow in the table
   * \param message The flowmod message that defines the modified flow
   * \return The modified flow
   */
  Flow13 modifyFlow(fluid_msg::of13::FlowMod* message);
  //Flow modifyFlowStrict(fluid_msg::of10::FlowMod* message);
    /**
   * \brief Deletes a flow in the table
   * \param message The flowmod message that defines the flow to delete
   */
  void deleteFlow(fluid_msg::of13::FlowMod* message);
  //void deleteFlowStrict(fluid_msg::of13::FlowMod* message);
  /**
   * \brief Adds a new group into the table
   * \param message The groupmod message that defines the new group to add
   * \return The newly created group
   */
  Ptr<SdnGroup13> addGroup(fluid_msg::of13::GroupMod* message);
  /**
   * \brief Modifies a group in the table
   * \param message The groupmod message that defines the modified group
   * \return The modified group
   */
  Ptr<SdnGroup13> modifyGroup(fluid_msg::of13::GroupMod* message);
    /**
   * \brief Deletes a group in the table
   * \param message The groupmod message that defines the group to delete
   */
  void deleteGroup(fluid_msg::of13::GroupMod* message);
  /**
   * \brief Getter for all the flows in the table
   * \return m_table_flow_rules
   */
  std::set<Flow13, cmp_priority13> flows();
  //stat entries
  uint32_t m_max_entries;   //!< A count of all flow entries in the table
  uint32_t m_active_count;  //!< A count of all active flow entries in the table 
  uint64_t m_lookup_count;  //!< A count of all lookups done in the table
  uint64_t m_matched_count; //!< A count of all total matches completed in the table
  std::map<uint32_t, Ptr<SdnGroup13> > m_groupTable;
private:
  Ptr<SdnSwitch13> m_parentSwitch;                   //!< The owning SdnSwitch of this table
  std::set<Flow13, cmp_priority13> m_flow_table_rules; //!< The actual set of all flows in the flow table. Sorted by priority
  uint8_t m_tableid;                               //!< Unique ID for flow tables
  template <class T> struct TempHeader { TempHeader() : isEmpty(true), header() {} bool isEmpty; T header; };
  TempHeader<EthernetHeader>  m_ethHeader;         //!< Private EthernetHeader for grabbing information out of the packet
  TempHeader<Ipv4Header> m_ipv4Header;             //!< Private Ipv4Header for grabbing information out of the packet
  TempHeader<Icmpv4Header> m_icmpv4Header;         //!< Private Icmpv4Header for grabbing information out of the packet
  TempHeader<Ipv6Header> m_ipv6Header;             //!< Private Ipv6Header for grabbing information out of the packet
  TempHeader<Ipv6ExtensionHeader> m_ipv6ExtHeader; //!< Private Ipv6Header for grabbing information out of the packet
  TempHeader<Ipv6OptionHeader> m_ipv6OptHeader;    //!< Private Ipv6Header for grabbing information out of the packet
  TempHeader<Icmpv6Header> m_icmpv6Header;         //!< Private Icmpv6Header for grabbing information out of the packet
  TempHeader<ArpHeader>  m_arpHeader;              //!< Private ArpHeader for grabbing information out of the packet
  TempHeader<TcpHeader>  m_tcpHeader;              //!< Private TcpHeader for grabbing information out of the packet
  TempHeader<UdpHeader>  m_udpHeader;              //!< Private UdpHeader for grabbing information out of the packet
public:
  /**
   * \brief Creates a match object from the tempheader information 
   * \param inPort This field needs to go in the match in addition to the packet fields, but it is not
   * 			readily available from the packet information.
   * \return A match object matching whatever information could be discerned from the current packet
   */
  fluid_msg::of13::Match getPacketFields (uint32_t inPort);
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
private:
  /**
   * \brief Action handler for an output action
   * \param pkt The packet being modified from the action
   * \param action The Output action being executed
   */
  uint32_t handleOutputAction (Ptr<Packet> pkt,fluid_msg::of13::OutputAction* action);
  /**
   * \brief Action handler for a group action
   * \param pkt The packet being modified from the action
   * \param action The Group action being executed
   */
  std::vector<uint32_t> handleGroupAction (Ptr<Packet> pkt,fluid_msg::of13::GroupAction* action);

  /**
   * \brief Idle Time Out Event hook. Gets invoked whenever a flow has reached it's idle time without any activity.
   */
  void IdleTimeOutEvent(Flow13 flow);
  /**
   * \brief Hard Time Out Event hook. Gets invoked whenever a flow has reached it's hard time
   */
  void HardTimeOutEvent(Flow13 flow);
  /**
   * \brief Generic timeout. Deletes a flow and asks the owning switch to send a notification to the controller
   */
  void TimeOutEvent(Flow13 flow, uint8_t reason);
};

} //End namespace ns3
#endif
