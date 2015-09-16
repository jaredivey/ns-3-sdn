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
#ifndef SDN_PORT_H
#define SDN_PORT_H

//Sdn Common library
#include "SdnConnection.h"

//NS3 objects
#include "ns3/mac48-address.h"
#include "ns3/packet.h"
#include "ns3/socket.h"

//NS3 utilities
#include "ns3/ptr.h"

//libfluid libraries
#include <fluid/of10msg.hh>

//C++ Libraries
#include <map>

namespace ns3 {

class NetDevice;
class Packet;
class SdnConnection;

/**
 * \ingroup sdn
 * \defgroup SdnPort
 * 
 * An encapsulating port object to define the port for SDN control. Mainly defines the features of switch ports for access level and feature definition
 * Mainly referenced in SdnSwitch.cc as a mapping of port numbers to SdnPort classes
 */
 
class SdnPort : public Object
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
   * \param port_no Associated port number
   * \param config Bitmask of OFPPC flags see Openflow spec 1.0.0
   * \param state Bitmask of OFPPS flags see Openflow spec 1.0.0
   * \param features Bitmask of OFPPF flags see Openflow spec 1.0.0
   */
  SdnPort (Ptr<NetDevice> device, Ptr<SdnConnection> conn, uint16_t port_no, uint32_t config, uint32_t state, uint32_t features);
  /**
   * /param device The specific netdevice we're connecting to
   * /parma conn The SdnConnection object encapsulating the connection
   * \param p the libfluid port structure SdnPort is encapsulating
   */
  SdnPort (Ptr<NetDevice> device, Ptr<SdnConnection> conn, fluid_msg::of10::Port p);
  SdnPort (void);
  ~SdnPort (void);
  /**
   * \brief Getter for the netdevice
   * \return the netdevice
   */
  Ptr<NetDevice> getDevice (void);
  /**
   * \brief Setter for the netdevice
   */
  void setDevice (Ptr<NetDevice> device);
  /**
   * \brief Getter for the SdnConnection
   * \return the SdnConnection
   */
  Ptr<SdnConnection> getConn (void);
  /**
   * \brief Setter for the SdnConnection
   */
  void setConn (Ptr<SdnConnection> conn);
  /**
   * \brief Getter for the port number
   * \return the port number
   */
  uint16_t getPortNumber (void);
  /**
   * \brief Setter for the port number
   */
  void setPortNumber (uint16_t portNo);
  /**
   * \brief Getter for the netdevice hardware address
   * \return the netdevice hardware address
   */
  uint8_t* getHardwareAddress (void);
  /**
   * \brief Setter for the netdevice hardware address
   */
  void setHardwareAddress (uint8_t* hardwareAddress);
  /**
   * \brief Getter for the port configuration
   * \return the port configuration
   */
  uint32_t getConfig (void);
  /**
   * \brief Setter for the port configuration
   */
  void setConfig (uint32_t config);
/**
   * \brief Getter for the port state
   * \return the port state
   */
  uint32_t getState (void);
  /**
   * \brief Setter for the port state
   */
  void setState (uint32_t state);
  /**
   * \brief Getter for the current features
   * \return the current features
   */
  uint32_t getCurr(void);
  /**
   * \brief Setter for the current features
   */
  void setCurr(uint32_t curr);
  /**
   * \brief Getter for the advertised features
   * \return the advertised features
   */
  uint32_t getAdvertised(void);
  /**
   * \brief Setter for the advertised features
   */
  void setAdvertised(uint32_t advertised);
  /**
   * \brief Getter for the supported features
   * \return the supported features
   */
  uint32_t getSupported(void);
  /**
   * \brief Setter for the supported features
   */
  void setSupported(uint32_t supported);
  /**
   * \brief Getter for the peer features
   * \return the peer features
   */
  uint32_t getPeer(void);
  /**
   * \brief Setter for the peer features
   */
  void setPeer(uint32_t peer);
  /**
   * \brief Getter for the libfluid port structure
   * \return A libfluid port object
   */
  fluid_msg::of10::Port getFluidPort (void);
  /**
   * \brief Sends a packet on the port
   * \param packet The packet to send
   * \return The length of data sent. 0 if not sent
   */
  uint32_t sendOnPort (Ptr<Packet> packet);
  /**
   * \brief Sends a libfluid message on the port
   * \param msg A libfluid OFMsg that we want to send
   * \return The length of data sent. 0 if not sent
   */
  uint32_t sendOnPort (fluid_msg::OFMsg* msg);
  /**
   * \brief Sends just data on the port
   * \param data A byte array of data to send
   * \param len A size type of how long the data is
   * \return The length of data sent. 0 if not sent
   */
  uint32_t sendOnPort (void* data, size_t len);
  /**
   * \brief Creates a PortsStats object 
   * \param data A byte array of data to send
   * \param len A size type of how long the data is
   * \return The length of data sent. 0 if not sent
   */
  fluid_msg::of10::PortStats* convertToPortStats (void);

private:
  Ptr<NetDevice> m_device;                //!< The netdevice this port will point to. Held for convience sake
  Ptr<SdnConnection> m_conn;              //!< The SdnConnection encapsulating the connection
  fluid_msg::of10::Port m_fluidPort;      //!< A libfluid representation of the port
  uint16_t m_portNumber;                  //!< Port number
  uint32_t m_config;                      //!< Configuration state as defined in openflow
  uint32_t m_advertised;                  //!< The advertised capabilities as defined in openflow
  fluid_msg::port_rx_tx_stats m_tx_stats; //!< Total bytes transmitted
  fluid_msg::port_err_stats m_err_stats;  //!< Total bytes errored
  uint64_t m_collisions;                  //!< Number of collisions
};

} // end of namespace ns3

#endif
