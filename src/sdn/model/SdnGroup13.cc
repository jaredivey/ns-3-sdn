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

#include "SdnGroup13.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SdnGroup13");

NS_OBJECT_ENSURE_REGISTERED (SdnGroup13);

TypeId SdnGroup13::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SdnGroup13")
    .SetParent<Object> ()
    .AddConstructor<SdnGroup13> ()
  ;
  return tid;
}

SdnGroup13::SdnGroup13()
{
  m_refCount = 0;
  m_packetCount = 0;
  m_byteCount = 0;
  m_durationSec = 0;
  m_durationNanoSec = 0;
}

SdnGroup13::SdnGroup13(fluid_msg::of13::GroupMod *groupMod)
{
  m_refCount = 0;
  m_packetCount = 0;
  m_byteCount = 0;
  m_durationSec = 0;
  m_durationNanoSec = 0;
  m_groupDesc.group_id(groupMod->group_id());
  m_groupDesc.buckets(groupMod->buckets());
}

}
