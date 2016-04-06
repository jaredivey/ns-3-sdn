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
#ifndef SDN_SWITCH13_H
#define SDN_SWITCH13_H

//Sdn Common library
#include "SdnCommon.h"
#include "SdnSwitch.h"
#include "SdnFlowTable13.h"
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
#include <fluid/of13msg.hh>
#include <fluid/OFServer.hh>
//C++ Libraries
#include <map>
#include <string>
#include <iostream>
#include <sstream>

namespace ns3 {

class SdnSwitch13;

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

class SdnSwitch13 : public SdnSwitch
{
public:
    /// \brief A structure to hold the Capabilities of the switch, set as a simple bytemask. See the openflow 1.0.0 specifications. In the NS3 world we can defaultly handle only stat capabilities
  struct OFP13_Capabilities
  {
    OFP13_Capabilities () : NS3_SWITCH_OFPC_FLOW_STATS (1),
                          NS3_SWITCH_OFPC_TABLE_STATS (1),
                          NS3_SWITCH_OFPC_PORT_STATS (1),
						  NS3_SWITCH_OFPC_GROUP_STATS (1),
						  NS3_SWITCH_OFPC_IP_REASM (1),
						  NS3_SWITCH_OFPC_QUEUE_STATS (0),
						  NS3_SWITCH_OFPC_PORT_BLOCKED (1)
    {
    }
    uint8_t NS3_SWITCH_OFPC_FLOW_STATS;
    uint8_t NS3_SWITCH_OFPC_TABLE_STATS;
    uint8_t NS3_SWITCH_OFPC_PORT_STATS;
    uint8_t NS3_SWITCH_OFPC_GROUP_STATS;
    uint8_t NS3_SWITCH_OFPC_IP_REASM;
    uint8_t NS3_SWITCH_OFPC_QUEUE_STATS;
    uint8_t NS3_SWITCH_OFPC_PORT_BLOCKED;
  };
    /// \brief A structure to hold the Actions that the switch can execute, set as a simple bytemask. See the openflow 1.0.0 specifications. In the NS3 world we can almost always handle all actions
  struct OFP13_Actions
  {
    OFP13_Actions () : NS3_SWITCH_OFPAT_OUTPUT (1),
    	               NS3_SWITCH_OFPAT_COPY_TTL_OUT (0),
    	               NS3_SWITCH_OFPAT_COPY_TTL_IN (0),
    	               NS3_SWITCH_OFPAT_SET_MPLS_TTL (0),
    	               NS3_SWITCH_OFPAT_DEC_MPLS_TTL (0),
    	               NS3_SWITCH_OFPAT_PUSH_VLAN (0),
    	               NS3_SWITCH_OFPAT_POP_VLAN (0),
    	               NS3_SWITCH_OFPAT_PUSH_MPLS (0),
    	               NS3_SWITCH_OFPAT_POP_MPLS (0),
    	               NS3_SWITCH_OFPAT_SET_QUEUE (0),
    	               NS3_SWITCH_OFPAT_GROUP (1),
    	               NS3_SWITCH_OFPAT_SET_NW_TTL (1),
    	               NS3_SWITCH_OFPAT_DEC_NW_TTL (1),
    	               NS3_SWITCH_OFPAT_SET_FIELD (1),
    	               NS3_SWITCH_OFPAT_PUSH_PBB (0),
    	               NS3_SWITCH_OFPAT_POP_PBB (0),
    	               NS3_SWITCH_OFPAT_EXPERIMENTER (0)
    {
    }
    uint8_t NS3_SWITCH_OFPAT_OUTPUT;
    uint8_t NS3_SWITCH_OFPAT_COPY_TTL_OUT;
    uint8_t NS3_SWITCH_OFPAT_COPY_TTL_IN;
    uint8_t NS3_SWITCH_OFPAT_SET_MPLS_TTL;
    uint8_t NS3_SWITCH_OFPAT_DEC_MPLS_TTL;
    uint8_t NS3_SWITCH_OFPAT_PUSH_VLAN;
    uint8_t NS3_SWITCH_OFPAT_POP_VLAN;
    uint8_t NS3_SWITCH_OFPAT_PUSH_MPLS;
    uint8_t NS3_SWITCH_OFPAT_POP_MPLS;
    uint8_t NS3_SWITCH_OFPAT_SET_QUEUE;
    uint8_t NS3_SWITCH_OFPAT_GROUP;
    uint8_t NS3_SWITCH_OFPAT_SET_NW_TTL;
    uint8_t NS3_SWITCH_OFPAT_DEC_NW_TTL;
    uint8_t NS3_SWITCH_OFPAT_SET_FIELD;
    uint8_t NS3_SWITCH_OFPAT_PUSH_PBB;
    uint8_t NS3_SWITCH_OFPAT_POP_PBB;
    uint8_t NS3_SWITCH_OFPAT_EXPERIMENTER;
  };
    /// \brief A structures defining all features of the switch. Includes the number of buffers, number of tables, list of capabilities, and list of actions.
  struct Features13
  {
    uint32_t n_buffers;
    uint8_t n_tables;
    OFP13_Capabilities capabilities;
    OFP13_Actions actions;
  };
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief Sends a Flow Removed Message To Controller. Used by the flow table so it's public
   */
  void SendFlowRemovedMessageToController(Flow13 flow, uint8_t reason);
  SdnSwitch13 ();
  ~SdnSwitch13 ();
  Ptr<SdnFlowTable13> m_flowTable13; //!< The containing flow table for this switch

  /**
    * \return The 32 DPID number of the switch
    */
  uint32_t getDatapathID () { return m_datapathID; }
protected:
  virtual void DoDispose (void);
private:
  typedef std::map<uint32_t, Ptr<SdnPort> > PortMap; //<!A mapping of virtual port numbers to SdnPort
  /**
   * \brief Establishes a 
   * \param device Netdevice to connect a socket toward
   * \param switchAddress A generic address object for the socket to bind on. Expects an ipv4 Address
   */
  virtual void EstablishControllerConnection (Ptr<NetDevice> device,
		  Ipv4Address localAddress,
		  Ipv4Address remoteAddress);
  /**
   * \brief Overarching receive callback function to handle data from a controller. Calls many other supporting functions
   */
  virtual void HandleReadController (Ptr<Socket> socket);
  /**
   * \brief Overarching receive callback function to handle data from a switch. Calls many other supporting functions
   */
  virtual bool HandleReadFromNetDevice (Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &);
  /**
   * \brief Handle the packet
   */
  virtual bool HandlePacket (Ptr<Packet> packet, uint32_t inPort);
  /**
   * \brief Flood on every port except the one specified by portNum
   * \param packet The packet to flood
   * \param portNum The port we came in from, so avoid flooding on that one
   */
  virtual void Flood(Ptr<Packet> packet, uint32_t portNum);
  /**
   * \brief Flood on every netdevice except the one specified by portNum
   * \param packet The packet to flood
   * \param portNum The device we came in from initially, don't flood on that one
   */
  virtual void Flood(Ptr<Packet> packet, Ptr<NetDevice> netDevice);
  /**
   * \brief send a packet in message to the controller. Usually sent when a packet is not handled in the flow table
   * \param packet The packet to send to the controller
   * \param device The device the original packet was received from
   * \param reason a fluid_msg OFPR reason for sending the packet in to the controller. The default is set to no match
   */
  virtual void SendPacketInToController(Ptr<Packet> packet,Ptr<NetDevice> device, uint8_t reason = fluid_msg::of13::OFPR_NO_MATCH);
  /**
   * \brief send a port status message to the controller. Usually sent when requested a port status message from the controller. This very rarely gets called in ns3 unless you are adding and removing nodes from the program
   * \param port a libfluid port structure defining the port we're concerned with
   * \param reason The reason we're sending a port status message
   */
  void SendPortStatusMessageToController(fluid_msg::of13::Port port, uint8_t reason);
  //ControllerRead Action Utilities
  /**
   * \brief A message handler for dealing with hello requests
   * \param message The original message parsed
   */
  virtual void OFHandle_Hello_Request (fluid_msg::OFMsg* message);
  /**
   * \brief A message handler for dealing with feature requests
   */
  virtual void OFHandle_Feature_Request (fluid_msg::OFMsg* message);
  /**
   * \brief A message handler for dealing with config requests
   */
  virtual void OFHandle_Get_Config_Request ();
  /**
   * \brief A message handler for dealing with set config messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg SetConfig class
   */
  virtual void OFHandle_Set_Config (uint8_t* buffer);
  /**
   * \brief A message handler for dealing with send packet out messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg PacketOut class
   */
  virtual void OFHandle_Packet_Out(uint8_t* buffer);
  /**
   * \brief A message handler for dealing with flow mod messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg FlowMod class
   */
  virtual void OFHandle_Flow_Mod (uint8_t* buffer);
  /**
   * \brief A message handler for dealing with port mod messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg PortMod class
   */
  virtual void OFHandle_Port_Mod (uint8_t* buffer);
  /**
   * \brief A message handler for dealing with status request messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg StatRequest class
   */
  virtual void OFHandle_Stats_Request (uint8_t* buffer) {}
  /**
   * \brief A message handler for dealing with barrier request messages
   * \param buffer a byte buffer of the original message. Converted to a fluid_msg BarrierRequest class
   */
  virtual void OFHandle_Barrier_Request (uint8_t* buffer);
  /**
   * \brief Add a flow to the flow table
   * \param message the original flowmod message. Contains the new flow to add
   */
  void addFlow(fluid_msg::of13::FlowMod* message);
  /**
   * \brief Modify all matching flows
   * \param message the original flowmod message. Contains the modify instructions
   */
  void modifyFlow(fluid_msg::of13::FlowMod* message);
  /**
   * \brief Modify all strictly matching flows
   * \param message the original flowmod message. Contains the modify instructions
   */
  void modifyFlowStrict(fluid_msg::of13::FlowMod* message);
  /**
   * \brief Delete all matching flows
   * \param message the original flowmod message. Contains the delete instructions
   */
  void deleteFlow(fluid_msg::of13::FlowMod* message);
  /**
   * \brief Delete all strictly matching flows
   * \param message the original flowmod message. Contains the delete instructions
   */
  void deleteFlowStrict(fluid_msg::of13::FlowMod* message);
  /**
    * \brief Get the Capabilities of this switch as a bitmask
    * \return A 32 bit number holding the switch capabilities as a bitmask
    */
  virtual uint32_t GetCapabilities ();
  /**
    * \brief Confirms that the version on this message matches the version installed on this switch
    * \param message A message containing a certain version of openflow
    * \return True if the versions are compatible. See the openflow spec 1.0.0 and spec 1.3.0 for version compatibility
    */
  virtual bool NegotiateVersion (fluid_msg::OFMsg* message);

  //Descriptions
  uint32_t m_datapathID; //!< Unique Datapath ID number
  uint32_t m_vendor; //!< Vendor value set by vendor. For ns-SDN we set all of 0xFFFF (invalid)
  uint16_t m_missSendLen; //!< A value defining how many byte of a packet to send to the controller in packet in messages
  fluid_msg::SwitchDesc m_switchDescription; //!< A SwitchDesc statically describing this switch
  EventId m_sendEvent; //!< A temporary EventId held when a new sendEvent is generated from this switch
  EventId m_recvEvent; //!< A temportary EventId held when a new revEvent is generated for this switch
  Ptr<SdnConnection> m_controllerConn;
  Features13 m_switchFeatures; //!<Features on the switch that we need to return when asked from the controller
  PortMap m_portMap; //!<Making a mapping of all devices to the virtual ports for the flow table to use

  static uint32_t xid; //!< A Unique XID for the switch. XIDs are global unique identifiers for messages/siwtches/controllers within sdn
  uint32_t TOTAL_PORTS; //!< A counter counting the total number of ports ever used by this switch

  Ptr<UniformRandomVariable> m_bufferIdStream;
  std::map< uint32_t, Ptr<Packet> > m_packetBuffers;

  bool m_kernel; //!< Use the Linux kernel stack (DCE-only)

  virtual void ConnectionSucceeded (Ptr<Socket> socket);
};

} //End namespace ns3
#endif
