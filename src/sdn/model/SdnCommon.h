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

#ifndef SDN_COMMON_H
#define SDN_COMMON_H

//NS3 objects
#include "ns3/ptr.h"
#include "ns3/bridge-channel.h"
#include "ns3/application.h"
#include "ns3/ipv4-address.h"
#include "ns3/mac48-address.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/socket.h"

//NS3 utilities
#include "ns3/enum.h"
#include "ns3/string.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"

//NS Logging
#include "ns3/log.h"

//libfluid libraries
#include <fluid/of10msg.hh>
//C++ Libraries
#include <map>
namespace ns3 {

class Socket;
class Packet;

struct CommonConnection
{
  CommonConnection ()
  : s_port (0),
    s_sent (0), 
    s_recv (0)
  {
  }
  uint16_t s_port;       //!< Port on which we listen for incoming packets.
  uint32_t s_sent;       //!< amount of packets sent on connection
  uint32_t s_recv;       //!< amount of packets receieved on connection
  Ptr<Socket> s_socket;  //!< IPv4 Socket
  Address s_peerAddress; //!< Remote peer address
};

class SdnCommon{

static int xIDs; //!< xID generator for openflow messages

public:
  /**
   * \brief Generate a new xID
   * \return a new unique xID within int form
  */
  static int GenerateXId (void);

};

} //  namespace ns3

#endif
