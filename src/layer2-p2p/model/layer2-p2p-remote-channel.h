/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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

// This object connects two point-to-point net devices where at least one
// is not local to this simulator object.  It simply over-rides the transmit
// method and uses an MPI Send operation instead.

#ifndef LAYER2_P2P_REMOTE_CHANNEL_H
#define LAYER2_P2P_REMOTE_CHANNEL_H

#include "layer2-p2p-channel.h"

namespace ns3 {

/**
 * \ingroup layer2-p2p
 *
 * \brief A Remote Point-To-Point Channel
 * 
 * This object connects two point-to-point net devices where at least one
 * is not local to this simulator object. It simply override the transmit
 * method and uses an MPI Send operation instead.
 */
class Layer2P2PRemoteChannel : public Layer2P2PChannel
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /** 
   * \brief Constructor
   */
  Layer2P2PRemoteChannel ();

  /** 
   * \brief Deconstructor
   */
  ~Layer2P2PRemoteChannel ();

  /**
   * \brief Transmit the packet
   *
   * \param p Packet to transmit
   * \param src Source Layer2P2PNetDevice
   * \param txTime Transmit time to apply
   * \returns true if successful (currently always true)
   */
  virtual bool TransmitStart (Ptr<Packet> p, Ptr<Layer2P2PNetDevice> src,
                              Time txTime);
};

} // namespace ns3

#endif


