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

#ifndef SDN_CONNECTION_H
#define SDN_CONNECTION_H

//NS3 objects
#include "ns3/core-module.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/net-device.h"
#include "ns3/ethernet-header.h"
#include "ns3/ethernet-trailer.h"
#include "ns3/llc-snap-header.h"

//NS3 utilities
#include "ns3/log.h"

//libfluid libraries
#include <fluid/of10msg.hh>
#include <fluid/OFConnection.hh>

//C++ Libraries
#include <vector>
#include <map>

//Openflow global definitions
#define OFVERSION 0x01 //Openflow version 10

namespace ns3 {

class Socket;
class Packet;
class NetDevice;

class SdnTimedCallback : public Object {
    /**
     * \brief Handles the deconstructing inheritance from ns3::object
     */
    virtual void DoDispose (void);

public:

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId (void);

    SdnTimedCallback (void (*cb)(void*), int interval, void* arg);
    SdnTimedCallback ();

    void runTimedCallback (void);

    int m_interval;
    void (*m_callback) (void*);
    void *m_arg;
};

/**
 * \ingroup sdn
 * \defgroup SdnConnection
 * 
 * An encapsulating connection object to formally define the controller-switch connection
 * Controller oriented, used exclusivly on the controller side. Switches use traditional ns3 sockets
 */

class SdnConnection : public Object
{
protected:
  /**
   * \brief Handles the deconstructing inheritance from ns3::object
   */
  virtual void DoDispose (void);

public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \param socket Base socket to encapsulate
   */
  SdnConnection (Ptr<Socket> socket);
  /**
   * \param device Switch device we're connecting to directly
   * \param socket Base socket to encapsulate
   */
  SdnConnection (Ptr<NetDevice> device, Ptr<Socket> socket);
  SdnConnection ();
  ~SdnConnection ();

  static uint32_t g_idCount; //!< Unique ID counter for our SdnConnections
  /**
   * \brief Getter for the unique ID
   * \return current ID
   */
  uint32_t get_id ();
  /**
   * \brief Getter for the live state of the connection
   * \return boolean of alive
   */
  bool is_alive ();
  /**
   * \brief Setter for the live state of the connection
   */
  void set_alive (bool alive);

  /**
   * \brief Get the connection state. See #fluid_base::#OFConnection::State.
   */
  uint8_t get_state ();

  /**
   * \brief Set the connection state. See #fluid_base::#OFConnection::State.
   */
  void set_state (fluid_base::OFConnection::State state);
  
  /**
   * \brief Socket getter
   * \return The socket
   */
  Ptr<Socket> get_socket();
  
  /**
   * \brief Socket setter
   */
  void set_socket (Ptr<Socket> socket);

  /**
  * \brief Get the negotiated OpenFlow version for the connection (OpenFlow protocol
  version number). Note that this is not an OFVersion value. It is the value
  that goes into the OpenFlow header (e.g.: 4 for OpenFlow 1.3).
  * \return version number
  */
  uint8_t get_version ();

  /**
   * \brief Set a negotiated version for the connection. (OpenFlow protocol version
  number). Note that this is not an OFVersion value. It is the value
  that goes into the OpenFlow header (e.g.: 4 for OpenFlow 1.3).
   * \param version an OpenFlow version number
   */
  void set_version (uint8_t version);
  
  /**
   * \brief Device getter
   * \return The device
   */
  
  Ptr<NetDevice> get_device();

  /**
  * \brief Send data to through the connection/Net Device. This data gets encapsulated into an OFMsg before being sent
  * \param data the binary data to send
  * \param len length of the binary data (in bytes)
  * \return Number of bytes sent. See ns3::Socket
  */
  uint32_t send (void* data, size_t len);

  /**
  * \brief Stagger sending of packets to account for difference in real time versus simulation time in controller applications
  * \param p Smart pointer to packet
  */
  void StaggerSend (Ptr<Packet> p);
  /**
  * \brief Send an OFMsg to through the connection/Net Device.
  * \param msg A libfluid defined OFMsg that is predefined
  * \return Number of bytes sent. See ns3::Socket
  */
  uint32_t send (fluid_msg::OFMsg* msg);
  /**
  * \brief Send an OFMsg to through the connection/Net Device.
  * \param msg A libfluid defined OFMsg that is predefined
  * \return Number of bytes sent. See ns3::Socket
  */
  uint32_t send (Ptr<Packet> p);
  /**
  * \brief Send data directly to a netdevice, bypassing the socket. This chain calls into sendOnNetDevice(packet)
  * \param p Smart pointer to packet
  * \return 0 if sent successfully. See ns3::NetDevice
  */
  uint32_t sendOnNetDevice (void* data, size_t len);
  /**
  * \brief Send data directly from a netdevice, bypassing the socket.
  * \param packet Packet to send
  \return 0 if sent successfully. See ns3::NetDevice
  */
  uint32_t sendOnNetDevice (Ptr<Packet> p);
  
  /**
  * \brief Stagger sending of packets to account for simultaneous packet sends
  * \param packet Packet to send
  */
  void StaggerSendOnNetDevice (Ptr<Packet> p);

  /**
   * At times, the controller may want switches to send packets that it
   * has specially created (LLDP for spanning tree, for one example). As such,
   * we need to make sure that the packet has been properly conditioned prior
   * to introduction in ns-3 from the libfluid environment.
   *
   * \param packet The packet we received from the host, and which we need 
   *               to check.
   * \param src    A pointer to the data structure that will get the source
   *               MAC address of the packet (extracted from the packet Ethernet
   *               header).
   * \param dst    A pointer to the data structure that will get the destination
   *               MAC address of the packet (extracted from the packet Ethernet 
   *               header).
   * \param type   A pointer to the variable that will get the packet type from 
   *               either the Ethernet header in the case of type interpretation
   *               (DIX framing) or from the 802.2 LLC header in the case of 
   *               length interpretation (802.3 framing).
   */
  Ptr<Packet> filter (Ptr<Packet> p, Address *src, Address *dst, uint16_t *type);
  
  /**
   * Set up a function to be called forever with an argument at a regular
   * interval. This is a utility function provided for no specific use case, but
   * rather because it is frequently needed.
   *
   *  \param cb the callback function. It should accept a void* argument and
   *         return a void*. Cannot return void* due to ns-3 Schedule
   *         limitation. Also, limited to a single controller
   *  \param interval interval in milisseconds
   *  \param arg an argument to the callback function
   */
  void add_timed_callback (void (*cb)(void*), int interval, void* arg);

  /**
   * Get application data. This data is any piece of data you might want to
   * associated with this OFConnection object.
   */
  void* get_application_data ();

  /**
   * Set application data.
   *
   *  \param data a pointer to application data
   */
  void set_application_data (void* data);

  /**
   * Close the connection.
   * This will not trigger OFServer::connection_callback.
   */
  void close ();

  uint32_t m_sent; //!< amount of packets sent on connection
  uint32_t m_recv; //!< amount of packets receieved on connection

private:
  Ptr<Socket> m_socket; //!< IPv4 Socket
  Ptr<NetDevice> m_device;

  uint32_t m_id; //!< Unique ID for this connection
  fluid_base::OFConnection::State m_state; //!< libfluid connection state
  uint8_t m_version; //!< Version of openflow this connection uses
  bool m_alive; //!< Bool status of if the connection is active or not
  void* m_applicationData; //!< Generic data for use by the SDN application

  std::vector< Ptr<SdnTimedCallback> > m_timedCallbacks;

  Time		m_lastSendTime; //!< Last time a packet was sent from this connection
  uint64_t	m_consecutivePause; //!< Consecutive timestep stagger for simultaneous message sends
};

} // namespace ns3

#endif
