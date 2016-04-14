/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006,2007 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
/*#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <iostream>

using namespace ns3;

// define this class in a public header
class BColorTag : public Tag
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;

  // these are our accessors to our tag structure
  void SetBColorValue (uint8_t value);
  uint8_t GetBColorValue (void) const;
private:
  uint8_t m_BColorValue;
};
*/
#include "packet-bcolor-tag.h"
namespace ns3{
TypeId 
BColorTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BColorTag")
    .SetParent<Tag> ()
    .AddConstructor<BColorTag> ()
    .AddAttribute ("BColorValue",
                   "A BColor value",
                   EmptyAttributeValue (),
                   MakeUintegerAccessor (&BColorTag::GetBColorValue),
                   MakeUintegerChecker<uint8_t> ())
  ;
  return tid;
}
TypeId 
BColorTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t 
BColorTag::GetSerializedSize (void) const
{
  return 1;
}
void 
BColorTag::Serialize (TagBuffer i) const
{
  i.WriteU8 (m_BColorValue);
}
void 
BColorTag::Deserialize (TagBuffer i)
{
  m_BColorValue = i.ReadU8 ();
}
void 
BColorTag::Print (std::ostream &os) const
{
  os << "v=" << (uint32_t)m_BColorValue;
}
void 
BColorTag::SetBColorValue (uint8_t value)
{
  m_BColorValue = value;
}
uint8_t 
BColorTag::GetBColorValue (void) const
{
  return m_BColorValue;
}

}
