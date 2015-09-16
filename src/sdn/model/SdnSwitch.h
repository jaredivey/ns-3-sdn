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
#ifndef SDN_SWITCH_H
#define SDN_SWITCH_H

//Sdn Common library
#include "SdnCommon.h"
#include "SdnFlowTable.h"
#include "SdnConnection.h"
#include "SdnPort.h"
//NS3 objects
#include "ns3/application.h"
#include "ns3/bridge-channel.h"
#include "ns3/socket.h"
#include "ns3/csma-net-device.h"
#include "ns3/csma-channel.h"
//NS3 utilities
#include "ns3/mac48-address.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/node.h"
#include "ns3/enum.h"
#include "ns3/string.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
//libfluid libraries
#include <fluid/of10msg.hh>
#include <fluid/OFServer.hh>
//C++ Libraries
#include <map>
#include <string>
#include <iostream>
#include <sstream>

//Special Openflow Global Constants
#define OFVERSION 0x01 //Openflow version 10
#define OF_DATAPATH_ID_PADDING 0x00
#define OFCONTROLLERPORT 6633

namespace ns3 {

class SdnSwitch;

/**
 * \ingroup sdn
 * \defgroup SdnSwitch
 * This application behaves as a Switch on the data plane
 * in the Software Defined Network model. On application startup, the SdnSwitch
 * tries to connect to all channels sending data into the node, and create a receiving callback
 * to intercept the traffic coming in. Once intercepted, SdnSwitch uses the socket to determine
 * whether we're receiving data from a controller or a switch. If receiving data from an SdnController
 * the switch will read the control plane message and modify it's behavior accordingly. If receiving data
 * from a switch, we consult our SdnFlowTable for routing advice, and if none is found, send the packet to
 * the controller.
 */

class SdnCommon;

class SdnSwitch : public Application
{
public:
    /// \brief A structure to hold the Capabilities of the switch, set as a simple bytemask. See the openflow 1.0.0 specifications. In the NS3 world we can defaultly handle only stat capabilities
  struct OFP_Capabilities
  {
    OFP_Capabilities () : NS3_SWITCH_OFPC_FLOW_STATS (1),
                          NS3_SWITCH_OFPC_TABLE_STATS (1),
                          NS3_SWITCH_OFPC_PORT_STATS (1),
                          NS3_SWITCH_OFPC_STP_STATS (0),
                          NS3_SWITCH_OFPC_RESERVED_STATS (0),
                          NS3_SWITCH_OFPC_QUEUE_STATS (0),
                          NS3_SWITCH_ARP_MATCH_IP (0)
    {
    }
    uint8_t NS3_SWITCH_OFPC_FLOW_STATS;
    uint8_t NS3_SWITCH_OFPC_TABLE_STATS;
    uint8_t NS3_SWITCH_OFPC_PORT_STATS;
    uint8_t NS3_SWITCH_OFPC_STP_STATS;
    uint8_t NS3_SWITCH_OFPC_RESERVED_STATS;
    uint8_t NS3_SWITCH_OFPC_QUEUE_STATS;
    uint8_t NS3_SWITCH_ARP_MATCH_IP;
  };
    /// \brief A structure to hold the Actions that the switch can execute, set as a simple bytemask. See the openflow 1.0.0 specifications. In the NS3 world we can almost always handle all actions
  struct OFP_Actions
  {
    OFP_Actions () : NS3_SWITCH_OFPAT_OUTPUT (1),
                     NS3_SWITCH_OFPAT_SET_VLAN_VID (1),
                     NS3_SWITCH_OFPAT_SET_VLAN_PCP (1),
                     NS3_SWITCH_OFPAT_STRIP_VLAN (1),
                     NS3_SWITCH_OFPAT_SET_DL_SRC (1),
                     NS3_SWITCH_OFPAT_SET_DL_DST (1),
                     NS3_SWITCH_OFPAT_SET_NW_SRC (1),
                     NS3_SWITCH_OFPAT_SET_NW_DST (1),
                     NS3_SWITCH_OFPAT_SET_NW_TOS (1),
                     NS3_SWITCH_OFPAT_SET_TP_SRC (1),
                     NS3_SWITCH_OFPAT_SET_TP_DST (1)
    {
    }
    uint8_t NS3_SWITCH_OFPAT_OUTPUT;
    uint8_t NS3_SWITCH_OFPAT_SET_VLAN_VID;
    uint8_t NS3_SWITCH_OFPAT_SET_VLAN_PCP;
    uint8_t NS3_SWITCH_OFPAT_STRIP_VLAN;
    uint8_t NS3_SWITCH_OFPAT_SET_DL_SRC;
    uint8_t NS3_SWITCH_OFPAT_SET_DL_DST;
    uint8_t NS3_SWITCH_OFPAT_SET_NW_SRC;
    uint8_t NS3_SWITCH_OFPAT_SET_NW_DST;
    uint8_t NS3_SWITCH_OFPAT_SET_NW_TOS;
    uint8_t NS3_SWITCH_OFPAT_SET_TP_SRC;
    uint8_t NS3_SWITCH_OFPAT_SET_TP_DST;
  };
    /// \brief A structures defining all features of the switch. Includes the number of buffers, number of tables, list of capabilities, and list of actions.
  struct Features
  {
    uint32_t n_buffers;
    uint8_t n_tables;
    OFP_Capabilities capabilities;
    OFP_Actions actions;
  };
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief Sends a Flow Removed Message To Controller. Used by the flow table so it's public
   */
  void SendFlowRemovedMessageToController(Flow flow, uint8_t reason);
  SdnSwitch ();
  ~SdnSwitch ();
  static uint32_t TOTAL_SERIAL_NUMBERS; //!< Global counter for all unique switch IDs
  static uint32_t TOTAL_DATAPATH_IDS; //!< Global counter for all Datapath IDs
  SdnFlowTable m_flowTable; //!< The containing flow table for this switch

  /**
    * \return The 32 DPID number of the switch
    */
  uint32_t getDatapathID () { return m_datapathID; }
protected:
  virtual void DoDispose (void);
private:
  typedef std::map<uint16_t, Ptr<SdnPort> > PortMap; //<!A mapping of virtual port numbers to SdnPort 
  virtual void StartApplication (void);  // Called at time specified by Start
  virtual void StopApplication (void);   // Called at time specified by Stop
  /**
   * \brief Establishes a 
   * \param device Netdevice to connect a socket toward
   * \param switchAddress A generic address object for the socket to bind on. Expects an ipv4 Address
   */
  void EstablishControllerConnection (Ptr<NetDevice> device,
		  Ipv4Address localAddress,
		  Ipv4Address remoteAddress);
  /**
   * \brief Establishes a
   * \param device Netdevice to connect a socket toward
   * \param switchAddress A generic address object for the socket to bind on. Expects an ipv4 Address
   */
  void EstablishConnection (Ptr<NetDevice> device,
		  Ipv4Address localAddress,
		  Ipv4Address remoteAddress);
  /**
   * \brief Overarching receive callback function to handle data from a controller. Calls many other supporting functions
   */
  void HandleReadController (Ptr<Socket> socket);
  /**
   * \brief Overarching receive callback function to handle data from a switch. Calls many other supporting functions
   */
  bool HandleReadFromNetDevice (Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &);
  /**
   * \brief Handle the packet
   */
  bool HandlePacket (Ptr<Packet> packet, uint16_t inPort);
  /**
   * \brief Flood on every port except the one specified by portNum
   * \param packet The packet to flood
   * \param portNum The port we came in from, so avoid flooding on that one
   */
  void Flood(Ptr<Packet> packet, uint16_t portNum);
  /**
   * \brief Flood on every netdevice except the one specified by portNum
   * \param packet The packet to flood
   * \param portNum The device we came in from initially, don't flood on that one
   */
  void Flood(Ptr<Packet> packet, Ptr<NetDevice> netDevice);
  /**
   * \brief send a packet in message to the controller. Usually sent when a packet is not handled in the flow table
   * \param packet The packet to send to the controller
   * \param device The device the original packet was received from
   * \param reason a fluid_msg OFPR reason for sending the packet in to the controller. The default is set to no match
   */
  void SendPacketInToController(Ptr<Packet> packet,Ptr<NetDevice> device, uint8_t reason = fluid_msg::of10::OFPR_NO_MATCH);
  /**
   * \brief send a port status message to the controller. Usually sent when requested a port status message from the controller. This very rarely gets called in ns3 unless you are adding and removing nodes from the program
   * \param port a libfluid port structure defining the port we're concerned with
   * \param reason The reason we're sending a port status message
   */
  void SendPortStatusMessageToController(fluid_msg::of10::Port port, uint8_t reason);
  //ControllerRead Action Utilities
  /**
   * \brief A message handler for dealing with hello requests
   * \param message The original message parsed
   */
  void OFHandle_Hello_Request (fluid_msg::OFMsg* message);
  /**
   * \brief A message handler for dealing with feature requests
   */
  void OFHandle_Feature_Request (fluid_msg::OFMsg* message);
  /**
   * \brief A message handler for dealing with config requests
   */
  void OFHandle_Get_Config_Request ();
  /**
   * \brief A message handler for dealing with set config messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg SetConfig class
   */
  void OFHandle_Set_Config (uint8_t* buffer);
  /**
   * \brief A message handler for dealing with send packet out messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg PacketOut class
   */
  void OFHandle_Packet_Out(uint8_t* buffer);
  /**
   * \brief A message handler for dealing with flow mod messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg FlowMod class
   */
  void OFHandle_Flow_Mod (uint8_t* buffer);
  /**
   * \brief A message handler for dealing with port mod messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg PortMod class
   */
  void OFHandle_Port_Mod (uint8_t* buffer);
  /**
   * \brief A message handler for dealing with status request messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg StatRequest class
   */
  void OFHandle_Stats_Request (uint8_t* buffer);
  /**
   * \brief A message handler for dealing with barrier request messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg BarrierRequest class
   */
  void OFHandle_Barrier_Request (uint8_t* buffer);
  /**
   * \brief Add a flow to the flow table
   * \param message the original flowmod message. Contains the new flow to add
   */
  void addFlow(fluid_msg::of10::FlowMod* message);
  /**
   * \brief Modify all matching flows
   * \param message the original flowmod message. Contains the modify instructions
   */
  void modifyFlow(fluid_msg::of10::FlowMod* message);
  /**
   * \brief Modify all strictly matching flows
   * \param message the original flowmod message. Contains the modify instructions
   */
  void modifyFlowStrict(fluid_msg::of10::FlowMod* message);
  /**
   * \brief Delete all matching flows
   * \param message the original flowmod message. Contains the delete instructions
   */
  void deleteFlow(fluid_msg::of10::FlowMod* message);
  /**
   * \brief Delete all strictly matching flows
   * \param message the original flowmod message. Contains the delete instructions
   */
  void deleteFlowStrict(fluid_msg::of10::FlowMod* message);
  //Stat utility functions
  /**
   * \brief Create a stats reply of the switch's descriptions
   * \return A StatsReplyDesc describing this switch
   */
  fluid_msg::of10::StatsReplyDesc* replyWithDescription();
  /**
   * \brief Create a stats reply of a flow on the switch
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg StatsRequestFlow
   * \return A StatsReplyFlow describing a flow on the switch
   */
  fluid_msg::of10::StatsReplyFlow* replyWithFlow(uint8_t* buffer);
  /**
   * \brief Create a stats reply of an aggregate of all the flows on the switch
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg StatsRequestAggregate
   * \return A StatsReplyAggregate describing all flows on the switch
   */
  fluid_msg::of10::StatsReplyAggregate* replyWithAggregate(uint8_t* buffer);
  /**
   * \brief Create a stats reply of a table on the switch. Note that in NS-SDN we only allow one table on the switch
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg StatsRequestTable
   * \return A StatsReplyTable describing the flow table
   */
  fluid_msg::of10::StatsReplyTable* replyWithTable(uint8_t* buffer);
  /**
   * \brief Create a stats reply of a port on the switch.
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg StatsRequestPort
   * \return A StatsReplyPort describing a port on this switch
   */
  fluid_msg::of10::StatsReplyPort* replyWithPort(uint8_t* buffer);
  /**
   * \brief UNUSED Create a stats reply of a queue on the switch. We don't use queues within ns-SDN so this is a prop function
   * \return A StatsReplyPort describing a *UNUSED* queue on this switch
   */
  fluid_msg::of10::StatsReplyQueue* replyWithQueue();
  /**
   * \brief UNUSED Create a stats reply set by the vendor of this switch. ns-SDN switches don't have vendors, so this function is unused
   * \return A StatsReplyVendor describing a *UNUSED* vendor on this switch
   */
  fluid_msg::of10::StatsReplyVendor* replyWithVendor();
  /**
   * \brief Utility function for logging to NS_LOG_DEBUG information of a message
   * \param msg The ofMsg to log
   */
  void LogOFMsg (fluid_msg::OFMsg* msg);
   /**
    * \brief Utility function to grab the mac address from a socket
    * \return A 64 bit number holding the macaddress
    */
  uint64_t GetMacAddress ();
  /**
    * \brief Get the Capabilities of this switch as a bitmask
    * \return A 32 bit number holding the switch capabilities as a bitmask
    */
  uint32_t GetCapabilities ();
  /**
    * \brief Get the enabled actions of this switch as a bitmask
    * \return A 32 bit number holding the enabled actions as a bitmask
    */
  uint32_t GetActions ();
  /**
    * \brief Create a match object to strictly match the fields from a packet
    * \param pkt The packet to strictly match to
    * \return A match object that matches the input packet
    */
  fluid_msg::of10::Match getPacketFields (Ptr<Packet> pkt);
  /**
    * \brief Confirms that the version on this message matches the version installed on this switch
    * \param message A message containing a certain version of openflow
    * \return True if the versions are compatible. See the openflow spec 1.0.0 and spec 1.3.0 for version compatibility
    */
  bool NegotiateVersion (fluid_msg::OFMsg* message);
  /**
    * \brief Generates a new port number
    * \return A new 16 bit virtual port number
    */
  uint16_t getNewPortNumber ();
  std::string getNewSerialNumber ();
  /**
    * \brief Generates a new Datapath Id. Datapath IDs are unique across all switches
    * \return A new 32 DPID number
    */
  static uint32_t getNewDatapathID ();
  //Descriptions
  uint32_t m_datapathID; //!< Unique Datapath ID number
  uint32_t m_vendor; //!< Vendor value set by vendor. For ns-SDN we set all of 0xFFFF (invalid)
  uint16_t m_missSendLen; //!< A value defining how many byte of a packet to send to the controller in packet in messages
  fluid_msg::SwitchDesc m_switchDescription; //!< A SwitchDesc statically describing this switch
  EventId m_sendEvent; //!< A temporary EventId held when a new sendEvent is generated from this switch
  EventId m_recvEvent; //!< A temportary EventId held when a new revEvent is generated for this switch
  Ptr<SdnConnection> m_controllerConn;
  Features m_switchFeatures; //!<Features on the switch that we need to return when asked from the controller
  PortMap m_portMap; //!<Making a mapping of all devices to the virtual ports for the flow table to use

  static uint32_t xid; //!< A Unique XID for the switch. XIDs are global unique identifiers for messages/siwtches/controllers within sdn
  uint16_t TOTAL_PORTS; //!< A counter counting the total number of ports ever used by this switch

  Ptr<UniformRandomVariable> m_bufferIdStream;
  std::map< uint32_t, Ptr<Packet> > m_packetBuffers;

  void ConnectionSucceeded (Ptr<Socket> socket);
  void ConnectionFailed (Ptr<Socket> socket);
};

} //End namespace ns3
#endif
