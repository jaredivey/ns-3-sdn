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

#include "SdnConnection.h"
  #include "ns3/packet-bcolor-tag.h"
  #include "ns3/packet-gcolor-tag.h"
  #include "ns3/packet-rcolor-tag.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SdnConnection");

NS_OBJECT_ENSURE_REGISTERED (SdnTimedCallback);
NS_OBJECT_ENSURE_REGISTERED (SdnConnection);

uint32_t SdnConnection::g_idCount = 0;

void
SdnTimedCallback::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  Object::DoDispose ();
}

TypeId
SdnTimedCallback::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SdnTimedCallback")
    .SetParent<Object> ()
    .AddConstructor<SdnTimedCallback> ()
  ;
  return tid;
}

SdnTimedCallback::SdnTimedCallback (void (*cb)(void*), int interval, void* arg)
{
  m_callback = cb;
  m_interval = interval;
  m_arg = arg;
}

SdnTimedCallback::SdnTimedCallback ()
{
  m_callback = 0;
  m_interval = 0;
  m_arg = 0;
}

void
SdnTimedCallback::runTimedCallback (void)
{
  Simulator::Schedule (Seconds (m_interval), m_callback, m_arg);
  Simulator::Schedule (Seconds (m_interval), &SdnTimedCallback::runTimedCallback, this);
}

void
SdnConnection::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  Object::DoDispose ();
}

TypeId
SdnConnection::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SdnConnection")
    .SetParent<Object> ()
    .AddConstructor<SdnConnection> ()
  ;
  return tid;
}

SdnConnection::SdnConnection (Ptr<Socket> socket)
: m_sent (0),
  m_recv (0),
  m_socket (socket),
  m_device(0)
{
  NS_LOG_FUNCTION (this << socket);

  this->m_id = g_idCount++;
  this->set_state (fluid_base::OFConnection::STATE_HANDSHAKE);
  this->set_alive (true);
  this->set_version (0);
  this->m_applicationData = NULL;

  this->m_lastSendTime = Seconds(0.0);
  this->m_consecutivePause = 0;
}

SdnConnection::SdnConnection (Ptr<NetDevice> device,
                              Ptr<Socket> socket)
: m_sent (0),
  m_recv (0),
  m_socket (socket),
  m_device(device)
{
  NS_LOG_FUNCTION (this << socket);

  this->m_id = g_idCount++;
  this->set_state (fluid_base::OFConnection::STATE_HANDSHAKE);
  this->set_alive (true);
  this->set_version (0);
  this->m_applicationData = NULL;

  this->m_lastSendTime = Seconds(0.0);
  this->m_consecutivePause = 0;
}

SdnConnection::SdnConnection ()
: m_sent (0),
  m_recv (0),
  m_socket (0),
  m_device(0)
{
  NS_LOG_FUNCTION (this);

  this->m_id = g_idCount++;
  this->set_state (fluid_base::OFConnection::STATE_HANDSHAKE);
  this->set_alive (true);
  this->set_version (0);
  this->m_applicationData = NULL;

  this->m_lastSendTime = Seconds(0.0);
  this->m_consecutivePause = 0;
}

SdnConnection::~SdnConnection ()
{
  NS_LOG_FUNCTION (this);
}

uint32_t
SdnConnection::get_id ()
{
  NS_LOG_FUNCTION (this);

  return this->m_id;
}

bool
SdnConnection::is_alive ()
{
  NS_LOG_FUNCTION (this);

  return this->m_alive;
}

void
SdnConnection::set_alive (bool alive)
{
  NS_LOG_FUNCTION (this);

  this->m_alive = alive;
}

uint8_t
SdnConnection::get_state ()
{
  NS_LOG_FUNCTION (this);

  return m_state;
}

void
SdnConnection::set_state (fluid_base::OFConnection::State state)
{
  NS_LOG_FUNCTION (this << state);

  this->m_state = state;
}

Ptr<Socket>
SdnConnection::get_socket()
{
  NS_LOG_FUNCTION (this);
  
  return m_socket;
}

void
SdnConnection::set_socket(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this);
  
  this->m_socket = socket;
}

uint8_t
SdnConnection::get_version ()
{
  NS_LOG_FUNCTION (this);

  return this->m_version;
}

void
SdnConnection::set_version (uint8_t version)
{
  NS_LOG_FUNCTION (this << version);

  this->m_version = version;
}

uint32_t
SdnConnection::send (void* data, size_t len)
{
  NS_LOG_FUNCTION (this << data << len);

  Ptr<Packet> packet = Create<Packet> ((uint8_t *)data, len);

  return send (packet);
}

void
SdnConnection::StaggerSend (Ptr<Packet> p)
{
    send (p);
}

uint32_t
SdnConnection::send (fluid_msg::OFMsg* msg)
{
  NS_LOG_FUNCTION (this << msg);

  return send (msg->pack(), msg->length());
}

uint32_t
SdnConnection::send(Ptr<Packet> p)
{
    Time currentTime = Simulator::Now ();
    Ptr<Packet> copyPacket = p->Copy ();
   RColorTag rcolor;
    rcolor.SetRColorValue(255);
    copyPacket->AddPacketTag(rcolor);
    GColorTag gcolor;
    gcolor.SetGColorValue(0);
    copyPacket->AddPacketTag(gcolor);
    BColorTag bcolor;
    bcolor.SetBColorValue(255);
    copyPacket->AddPacketTag(bcolor);
 
    if (currentTime == m_lastSendTime)
      {
        m_consecutivePause++;
        Simulator::ScheduleWithContext (m_id, MicroSeconds (m_consecutivePause),
        	&SdnConnection::StaggerSend, this, copyPacket);
        return copyPacket->GetSize ();
      }
    else
      {
	NS_LOG_DEBUG ("Sending packet on socket of size " << copyPacket->GetSize ()
		<<  " from connection id=" << get_id ());
        m_lastSendTime = Simulator::Now ();
        m_consecutivePause = 0;
        ++m_sent;
        return m_socket->Send(copyPacket);
      }
}

uint32_t
SdnConnection::sendOnNetDevice (void* data, size_t len)
{
  NS_LOG_FUNCTION (this << data << len);
  
  Ptr<Packet> packet = Create<Packet> ((uint8_t*)data, len, true);
  return sendOnNetDevice(packet);
}

uint32_t
SdnConnection::sendOnNetDevice (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  ++m_sent;

  NS_LOG_DEBUG ("Sending packet on netdevice of size " << p->GetSize() << 
                " from connection id=" << get_id ());

  Address src, dst;
  uint16_t protocol_number;
  Ptr<Packet> pktToSend = filter (p, &src, &dst, &protocol_number);
  
  NS_ASSERT_MSG (get_device(), "Attempting to send on NetDevice without a NetDevice");

  Time currentTime = Simulator::Now ();
  if (currentTime == m_lastSendTime)
    {
      m_consecutivePause++;
      Simulator::ScheduleWithContext (m_id, TimeStep (m_consecutivePause),
	      &SdnConnection::StaggerSendOnNetDevice, this, p);
      return p->GetSize();
    }
  else
    {
      m_lastSendTime = Simulator::Now ();
      m_consecutivePause = 0;
      return get_device()->SendFrom(pktToSend, src, dst, protocol_number);
    }
}

void
SdnConnection::StaggerSendOnNetDevice (Ptr<Packet> p)
{
    sendOnNetDevice (p);
}

Ptr<Packet>
SdnConnection::filter (Ptr<Packet> p, Address *src, Address *dst, uint16_t *type)
{
  NS_LOG_FUNCTION (p);
  uint32_t pktSize;
  Ptr<Packet> copyPacket = p->Copy ();

  //
  // We have a candidate packet for injection into ns-3.  We expect that since
  // it came over a socket that provides Ethernet packets, it should be big 
  // enough to hold an EthernetHeader.  If it can't, we signify the packet 
  // should be filtered out by returning 0.
  //
  pktSize = copyPacket->GetSize ();
  EthernetHeader header (false);
  EthernetTrailer trailer;
  if (pktSize < header.GetSerializedSize () + trailer.GetSerializedSize())
    {
      return 0;
    }

  copyPacket->RemoveHeader (header);

  PacketMetadata metadata (copyPacket->GetUid (),copyPacket->GetSize ());
  PacketMetadata::ItemIterator iterator = copyPacket->BeginItem ();
  PacketMetadata::Item item;
  bool has_trailer = false;
  while (iterator.HasNext ())
    {
	    item = iterator.Next ();
	    if (item.type == PacketMetadata::Item::TRAILER)
	      {
		      has_trailer = true;
	 	      break;
	      }
    }
  if (has_trailer)
    {
	    copyPacket->RemoveTrailer (trailer);
    }

  NS_LOG_LOGIC ("Pkt source is " << header.GetSource ());
  NS_LOG_LOGIC ("Pkt destination is " << header.GetDestination ());
  NS_LOG_LOGIC ("Pkt LengthType is " << header.GetLengthType ());

  //
  // If the length/type is less than 1500, it corresponds to a length 
  // interpretation packet.  In this case, it is an 802.3 packet and 
  // will also have an 802.2 LLC header.  If greater than 1500, we
  // find the protocol number (Ethernet type) directly.
  //
//  if (header.GetLengthType () <= 1500)
//    {
//      *src = header.GetSource ();
//      *dst = header.GetDestination ();
//
//      pktSize = copyPacket->GetSize ();
//      LlcSnapHeader llc;
//      if (pktSize < llc.GetSerializedSize ())
//        {
//          return 0;
//        }
//
//      copyPacket->RemoveHeader (llc);
//      *type = llc.GetType ();
//    }
//  else
//    {
      *src = header.GetSource ();
      *dst = header.GetDestination ();
      *type = header.GetLengthType ();
//    }

  //
  // What we give back is a packet without the Ethernet header (nor the 
  // possible llc/snap header) on it.  We think it is ready to send on
  // out the bridged net device.
  //
  return copyPacket;
}

Ptr<NetDevice>
SdnConnection::get_device()
{
  return m_device;
}

void
SdnConnection::add_timed_callback (void (*cb)(void*),
                                        int interval,
                                        void* arg)
{
  NS_LOG_FUNCTION (this << cb << interval << arg);
  // Add code to set up a callback on this SdnConnection.
  Ptr<SdnTimedCallback> thisCB = CreateObject<SdnTimedCallback> (cb, interval, arg);
  thisCB->runTimedCallback ();
  m_timedCallbacks.push_back (thisCB);
}

void* SdnConnection::get_application_data ()
{
  NS_LOG_FUNCTION (this);

  return this->m_applicationData;
}

void SdnConnection::set_application_data (void* data)
{
  NS_LOG_FUNCTION (this << data);

  this->m_applicationData = data;
}

void
SdnConnection::close ()
{
  NS_LOG_FUNCTION (this);

  set_state (fluid_base::OFConnection::STATE_DOWN);
}

} // End of namespace ns3
