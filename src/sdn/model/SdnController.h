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

#ifndef SDN_CONTROLLER_H
#define SDN_CONTROLLER_H

//Sdn Common library
#include "SdnCommon.h"
#include "SdnConnection.h"
#include "SdnListener.h"
#include "SdnSwitch.h"

//NS3 objects
#include "ns3/core-module.h"
#include "ns3/bridge-channel.h"
#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/mac48-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/seq-ts-header.h"
#include "ns3/loopback-net-device.h"
#include "ns3/ipv4.h"

//NS3 utilities
#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/string.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"

//libfluid libraries
#include <fluid/of10/openflow-10.h>
#include <fluid/of10msg.hh>
#include <fluid/OFServer.hh>
#include <fluid/OFConnection.hh>
#include <fluid/OFServerSettings.hh>

//C++ Libraries
#include <map>

//Openflow global definitions
#define OFVERSION 0x01 //Openflow version 10
#define OFCONTROLLERPORT 6633 //Openflow version 10 port
namespace ns3 {

class SdnConnection;
class SdnListener;

/**
 * \ingroup sdn
 * \defgroup SdnController
 *
 * This application is meant to represent a Controller within an Openflow-enabled network
 * Every SdnController must come with an SdnListener to define the behavior of the controller
 */

class SdnController : public Application
{
protected:
  /**
   * \brief Handles the deconstructing inheritance from ns3::object
   */
  virtual void DoDispose (void);
private:
  /**
   * \brief Starts the controller. Involves establishing a connection with all of our SdnSwitch applications
   */
  virtual void StartApplication (void);
  /**
   * \brief Stops the controller. 
   */
  virtual void StopApplication (void);
  /**
   * \brief Given the current state of the SdnConnection, either handle internet processing (OFHandle), or create an event for the SdnListener to handle data from the read.
   * \param socket Socket object we're receiving from
   */
  void HandleRead (Ptr<Socket> socket);
  /**
   * \brief Negotiate the openflow version with the switch, and create a feature request to the switch.
   * \param socket Socket object we're receiving from
   * \param message The hello message we initially received
   * \return 0 if feature request did not send correctly, non-zero otherwise
   */
  int  OFHandle_Hello_Reply (Ptr<Socket> socket, fluid_msg::OFMsg* message);
  /**
   * \brief Negotiate the openflow version with the switch, and create a feature request to the switch.
   * \param socket Socket object we're receiving from
   * \param message The features reply message we initially received
   * \return 0 if feature request did not send correctly, non-zero otherwise
   */
  void OFHandle_Features_Reply (Ptr<Socket> socket, fluid_msg::OFMsg* message);
  /**
   * \brief If a libfluid error message gets sent during handshaking, return an error repsonse
   * \param socket Socket object we're receiving from
   * \param xid The unique id of the original error message
   * \param err_type The type of error received
   * \param code The error code to parse the specific type of error being handled
   * \return 0 if error reponse was not sent, return non-zero otherwise
   */
  int  OFHandle_Errors (Ptr<Socket> socket, int xid, uint16_t err_type, uint16_t code);
  /**
   * \brief A boolean checker to make sure that we have compatible versions of openflow running on both sides
   * \brief message A message from the switch we're nogotiating with. Always carrys the version number within
   * \return True if versions are compatible, false otherwise
   */
  bool NegotiateVersion (fluid_msg::OFMsg* message);
  std::map<Ptr<Socket>, Ptr<SdnConnection> > m_switchMap; //!< A map of socket objects we receive data from to SdnConnections to encapsulate the connection
  fluid_base::OFServerSettings ofsc;
  Ptr<SdnListener> event_listener; //!< The listener that defines the controller behavior when handling most SdnSwitch messages
  
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  SdnController (Ptr<SdnListener> listener);
  SdnController ();
  virtual ~SdnController ();

  void HandleAccept (Ptr<Socket> s, const Address& from);
  void HandlePeerClose (Ptr<Socket> socket);
  void HandlePeerError (Ptr<Socket> socket);
  std::vector< Ptr<Socket> > m_listeningSockets;
};

} //  namespace ns3

#endif
