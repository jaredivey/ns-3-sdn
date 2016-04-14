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
 */

#ifndef SDNGROUP13_H
#define SDNGROUP13_H

//ns3 utilities
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/type-id.h"
//libfluid packages
#include <fluid/of13msg.hh>
#include <fluid/of13/of13common.hh>
#include <fluid/of13/openflow-13.h>
#include "SdnCommon.h"

namespace ns3 {

/**
 * \ingroup sdn
 * \defgroup SdnGroup13
 *
 */

class SdnGroup13 : public Object
{
public:
	SdnGroup13();

	SdnGroup13(fluid_msg::of13::GroupMod *groupMod);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  fluid_msg::of13::GroupDesc m_groupDesc;
  uint32_t m_refCount;       //!< Number of flows or groups that directly forward to this group
  uint64_t m_packetCount;    //!< Number of packets processed by group
  uint64_t m_byteCount;      //!< Number of bytes processed by group
  uint32_t m_durationSec;    //!< Time group has been alive in seconds
  uint32_t m_durationNanoSec;//!< Time group has been alive in nanoseconds beyond m_durationSec

private:
};

} //End namespace ns3
#endif
