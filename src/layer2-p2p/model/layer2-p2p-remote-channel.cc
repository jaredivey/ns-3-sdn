/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
 *
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
 * Author: George Riley <riley@ece.gatech.edu>
 */

#include <iostream>

#include "layer2-p2p-remote-channel.h"
#include "layer2-p2p-net-device.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/mpi-interface.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Layer2P2PRemoteChannel");

NS_OBJECT_ENSURE_REGISTERED (Layer2P2PRemoteChannel);

TypeId
Layer2P2PRemoteChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Layer2P2PRemoteChannel")
    .SetParent<Layer2P2PChannel> ()
    .SetGroupName ("Layer2P2P")
    .AddConstructor<Layer2P2PRemoteChannel> ()
  ;
  return tid;
}

Layer2P2PRemoteChannel::Layer2P2PRemoteChannel ()
{
}

Layer2P2PRemoteChannel::~Layer2P2PRemoteChannel ()
{
}

bool
Layer2P2PRemoteChannel::TransmitStart (
  Ptr<Packet> p,
  Ptr<Layer2P2PNetDevice> src,
  Time txTime)
{
  NS_LOG_FUNCTION (this << p << src);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  IsInitialized ();

  uint32_t wire = src == GetSource (0) ? 0 : 1;
  Ptr<Layer2P2PNetDevice> dst = GetDestination (wire);

#ifdef NS3_MPI
  // Calculate the rxTime (absolute)
  Time rxTime = Simulator::Now () + txTime + GetDelay ();
  MpiInterface::SendPacket (p, rxTime, dst->GetNode ()->GetId (), dst->GetIfIndex ());
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
  return true;
}

} // namespace ns3
