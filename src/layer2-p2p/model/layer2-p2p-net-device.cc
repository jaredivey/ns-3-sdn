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
 */

#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/ethernet-header.h"
#include "ns3/ethernet-trailer.h"
#include "ns3/mac48-address.h"
#include "ns3/llc-snap-header.h"
#include "ns3/error-model.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/enum.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "layer2-p2p-net-device.h"
#include "layer2-p2p-channel.h"

namespace ns3 {

#define LLDP_DISCOVERY_ADDRESS "01:80:c2:00:00:0e"
#define BPDU_STP_ADDRESS1 "01:80:c2:00:00:00"
#define BPDU_STP_ADDRESS2 "01:00:c2:cc:cc:cd"

NS_LOG_COMPONENT_DEFINE ("Layer2P2PNetDevice");

NS_OBJECT_ENSURE_REGISTERED (Layer2P2PNetDevice);

TypeId 
Layer2P2PNetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Layer2P2PNetDevice")
    .SetParent<NetDevice> ()
    .SetGroupName ("Layer2P2P")
    .AddConstructor<Layer2P2PNetDevice> ()
    .AddAttribute ("Mtu", "The MAC-level Maximum Transmission Unit",
                   UintegerValue (DEFAULT_MTU),
                   MakeUintegerAccessor (&Layer2P2PNetDevice::SetMtu,
                                         &Layer2P2PNetDevice::GetMtu),
                   MakeUintegerChecker<uint16_t> ())
	.AddAttribute ("EncapsulationMode",
				   "The link-layer encapsulation type to use.",
				   EnumValue (DIX),
				   MakeEnumAccessor (&Layer2P2PNetDevice::SetEncapsulationMode),
				   MakeEnumChecker (DIX, "Dix",
									LLC, "Llc"))
    .AddAttribute ("Address", 
                   "The MAC address of this device.",
                   Mac48AddressValue (Mac48Address ("ff:ff:ff:ff:ff:ff")),
                   MakeMac48AddressAccessor (&Layer2P2PNetDevice::m_address),
                   MakeMac48AddressChecker ())
    .AddAttribute ("DataRate", 
                   "The default data rate for point to point links",
                   DataRateValue (DataRate ("32768b/s")),
                   MakeDataRateAccessor (&Layer2P2PNetDevice::m_bps),
                   MakeDataRateChecker ())
    .AddAttribute ("ReceiveErrorModel", 
                   "The receiver error model used to simulate packet loss",
                   PointerValue (),
                   MakePointerAccessor (&Layer2P2PNetDevice::m_receiveErrorModel),
                   MakePointerChecker<ErrorModel> ())
    .AddAttribute ("InterframeGap", 
                   "The time to wait between packet (frame) transmissions",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&Layer2P2PNetDevice::m_tInterframeGap),
                   MakeTimeChecker ())
	.AddAttribute ("SdnEnable",
		"Enable or disable whether to override receive handling with sdn control.",
		BooleanValue (false),
		MakeBooleanAccessor (&Layer2P2PNetDevice::m_sdnEnable),
		MakeBooleanChecker ())

    //
    // Transmit queueing discipline for the device which includes its own set
    // of trace hooks.
    //
    .AddAttribute ("TxQueue", 
                   "A queue to use as the transmit queue in the device.",
                   PointerValue (),
                   MakePointerAccessor (&Layer2P2PNetDevice::m_queue),
                   MakePointerChecker<Queue> ())

    //
    // Trace sources at the "top" of the net device, where packets transition
    // to/from higher layers.
    //
    .AddTraceSource ("MacTx", 
                     "Trace source indicating a packet has arrived for transmission by this device",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_macTxTrace))
    .AddTraceSource ("MacTxDrop", 
                     "Trace source indicating a packet has been dropped by the device before transmission",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_macTxDropTrace))
    .AddTraceSource ("MacPromiscRx", 
                     "A packet has been received by this device, has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  This is a promiscuous trace,",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_macPromiscRxTrace))
    .AddTraceSource ("MacRx", 
                     "A packet has been received by this device, has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  This is a non-promiscuous trace,",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_macRxTrace))
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("MacRxDrop", 
                     "Trace source indicating a packet was dropped before being forwarded up the stack",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_macRxDropTrace))
#endif
    //
    // Trace souces at the "bottom" of the net device, where packets transition
    // to/from the channel.
    //
    .AddTraceSource ("PhyTxBegin", 
                     "Trace source indicating a packet has begun transmitting over the channel",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_phyTxBeginTrace))
    .AddTraceSource ("PhyTxEnd", 
                     "Trace source indicating a packet has been completely transmitted over the channel",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_phyTxEndTrace))
    .AddTraceSource ("PhyTxDrop", 
                     "Trace source indicating a packet has been dropped by the device during transmission",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_phyTxDropTrace))
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("PhyRxBegin", 
                     "Trace source indicating a packet has begun being received by the device",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_phyRxBeginTrace))
#endif
    .AddTraceSource ("PhyRxEnd", 
                     "Trace source indicating a packet has been completely received by the device",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_phyRxEndTrace))
    .AddTraceSource ("PhyRxDrop", 
                     "Trace source indicating a packet has been dropped by the device during reception",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_phyRxDropTrace))

    //
    // Trace sources designed to simulate a packet sniffer facility (tcpdump).
    // Note that there is really no difference between promiscuous and 
    // non-promiscuous traces in a point-to-point link.
    //
    .AddTraceSource ("Sniffer", 
                     "Trace source simulating a non-promiscuous packet sniffer attached to the device",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_snifferTrace))
    .AddTraceSource ("PromiscSniffer", 
                     "Trace source simulating a promiscuous packet sniffer attached to the device",
                     MakeTraceSourceAccessor (&Layer2P2PNetDevice::m_promiscSnifferTrace))
  ;
  return tid;
}


Layer2P2PNetDevice::Layer2P2PNetDevice ()
  :
    m_txMachineState (READY),
	m_encapMode (DIX),
    m_channel (0),
    m_linkUp (false),
    m_currentPkt (0)
{
  NS_LOG_FUNCTION (this);
}

Layer2P2PNetDevice::~Layer2P2PNetDevice ()
{
  NS_LOG_FUNCTION (this);
}

void
Layer2P2PNetDevice::AddHeader (Ptr<Packet> p,   Mac48Address source,  Mac48Address dest,  uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (p << source << dest << protocolNumber);

  EthernetHeader header (false);
  header.SetSource (source);
  header.SetDestination (dest);

  EthernetTrailer trailer;

  NS_LOG_LOGIC ("p->GetSize () = " << p->GetSize ());
  NS_LOG_LOGIC ("m_encapMode = " << m_encapMode);
  NS_LOG_LOGIC ("m_mtu = " << m_mtu);

  uint16_t lengthType = 0;
  switch (m_encapMode)
    {
    case DIX:
      NS_LOG_LOGIC ("Encapsulating packet as DIX (type interpretation)");
      //
      // This corresponds to the type interpretation of the lengthType field as
      // in the old Ethernet Blue Book.
      //
      lengthType = protocolNumber;

      //
      // All Ethernet frames must carry a minimum payload of 46 bytes.  We need
      // to pad out if we don't have enough bytes.  These must be real bytes
      // since they will be written to pcap files and compared in regression
      // trace files.
      //
      if (p->GetSize () < 46)
        {
          uint8_t buffer[46];
          memset (buffer, 0, 46);
          Ptr<Packet> padd = Create<Packet> (buffer, 46 - p->GetSize ());
          p->AddAtEnd (padd);
        }
      break;
    case LLC:
      {
        NS_LOG_LOGIC ("Encapsulating packet as LLC (length interpretation)");

        LlcSnapHeader llc;
        llc.SetType (protocolNumber);
        p->AddHeader (llc);

        //
        // This corresponds to the length interpretation of the lengthType
        // field but with an LLC/SNAP header added to the payload as in
        // IEEE 802.2
        //
        lengthType = p->GetSize ();

        //
        // All Ethernet frames must carry a minimum payload of 46 bytes.  The
        // LLC SNAP header counts as part of this payload.  We need to padd out
        // if we don't have enough bytes.  These must be real bytes since they
        // will be written to pcap files and compared in regression trace files.
        //
        if (p->GetSize () < 46)
          {
            uint8_t buffer[46];
            memset (buffer, 0, 46);
            Ptr<Packet> padd = Create<Packet> (buffer, 46 - p->GetSize ());
            p->AddAtEnd (padd);
          }

        NS_ASSERT_MSG (p->GetSize () <= GetMtu (),
                       "CsmaNetDevice::AddHeader(): 802.3 Length/Type field with LLC/SNAP: "
                       "length interpretation must not exceed device frame size minus overhead");
      }
      break;
    case ILLEGAL:
    default:
      NS_FATAL_ERROR ("CsmaNetDevice::AddHeader(): Unknown packet encapsulation mode");
      break;
    }

  NS_LOG_LOGIC ("header.SetLengthType (" << lengthType << ")");
  header.SetLengthType (lengthType);
  p->AddHeader (header);

  if (Node::ChecksumEnabled ())
    {
      trailer.EnableFcs (true);
    }
  trailer.CalcFcs (p);
  p->AddTrailer (trailer);
}

void
Layer2P2PNetDevice::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_node = 0;
  m_channel = 0;
  m_receiveErrorModel = 0;
  m_currentPkt = 0;
  NetDevice::DoDispose ();
}

void
Layer2P2PNetDevice::SetEncapsulationMode (Layer2P2PNetDevice::EncapsulationMode mode)
{
  NS_LOG_FUNCTION (mode);

  m_encapMode = mode;

  NS_LOG_LOGIC ("m_encapMode = " << m_encapMode);
  NS_LOG_LOGIC ("m_mtu = " << m_mtu);
}

Layer2P2PNetDevice::EncapsulationMode
Layer2P2PNetDevice::GetEncapsulationMode (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_encapMode;
}

void
Layer2P2PNetDevice::SetDataRate (DataRate bps)
{
  NS_LOG_FUNCTION (this);
  m_bps = bps;
}

void
Layer2P2PNetDevice::SetInterframeGap (Time t)
{
  NS_LOG_FUNCTION (this << t.GetSeconds ());
  m_tInterframeGap = t;
}

bool
Layer2P2PNetDevice::TransmitStart (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  //
  // This function is called to start the process of transmitting a packet.
  // We need to tell the channel that we've started wiggling the wire and
  // schedule an event that will be executed when the transmission is complete.
  //
  NS_ASSERT_MSG (m_txMachineState == READY, "Must be READY to transmit");
  m_txMachineState = BUSY;
  m_currentPkt = p;
  m_phyTxBeginTrace (m_currentPkt);

  Time txTime = Seconds (m_bps.CalculateTxTime (p->GetSize ()));
  Time txCompleteTime = txTime + m_tInterframeGap;

  NS_LOG_LOGIC ("Schedule TransmitCompleteEvent in " << txCompleteTime.GetSeconds () << "sec");
  Simulator::Schedule (txCompleteTime, &Layer2P2PNetDevice::TransmitComplete, this);

  bool result = m_channel->TransmitStart (p, this, txTime);
  if (result == false)
    {
      m_phyTxDropTrace (p);
    }
  return result;
}

void
Layer2P2PNetDevice::TransmitComplete (void)
{
  NS_LOG_FUNCTION (this);

  //
  // This function is called to when we're all done transmitting a packet.
  // We try and pull another packet off of the transmit queue.  If the queue
  // is empty, we are done, otherwise we need to start transmitting the
  // next packet.
  //
  NS_ASSERT_MSG (m_txMachineState == BUSY, "Must be BUSY if transmitting");
  m_txMachineState = READY;

  NS_ASSERT_MSG (m_currentPkt != 0, "Layer2P2PNetDevice::TransmitComplete(): m_currentPkt zero");

  m_phyTxEndTrace (m_currentPkt);
  m_currentPkt = 0;

  Ptr<Packet> p = m_queue->Dequeue ();
  if (p == 0)
    {
      //
      // No packet was on the queue, so we just exit.
      //
      return;
    }

  //
  // Got another packet off of the queue, so start the transmit process agin.
  //
  m_snifferTrace (p);
  m_promiscSnifferTrace (p);
  TransmitStart (p);
}

bool
Layer2P2PNetDevice::Attach (Ptr<Layer2P2PChannel> ch)
{
  NS_LOG_FUNCTION (this << &ch);

  m_channel = ch;

  m_channel->Attach (this);

  //
  // This device is up whenever it is attached to a channel.  A better plan
  // would be to have the link come up when both devices are attached, but this
  // is not done for now.
  //
  NotifyLinkUp ();
  return true;
}

void
Layer2P2PNetDevice::SetQueue (Ptr<Queue> q)
{
  NS_LOG_FUNCTION (this << q);
  m_queue = q;
}

void
Layer2P2PNetDevice::SetReceiveErrorModel (Ptr<ErrorModel> em)
{
  NS_LOG_FUNCTION (this << em);
  m_receiveErrorModel = em;
}

void
Layer2P2PNetDevice::Receive (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << packet);
  NS_LOG_LOGIC ("UID is " << packet->GetUid ());

  //
  // Hit the trace hook.  This trace will fire on all packets received from the
  // channel except those originated by this device.
  //
  m_phyRxEndTrace (packet);


  if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt (packet) )
    {
      NS_LOG_LOGIC ("Dropping pkt due to error model ");
      m_phyRxDropTrace (packet);
      return;
    }

  //
  // Trace sinks will expect complete packets, not packets without some of the
  // headers.
  //
  Ptr<Packet> originalPacket = packet->Copy ();
  Ptr<Packet> copyPacket = packet->Copy ();

  EthernetHeader header (false);
  copyPacket->RemoveHeader (header);

  EthernetTrailer trailer;
  copyPacket->RemoveTrailer (trailer);
  if (Node::ChecksumEnabled ())
    {
      trailer.EnableFcs (true);
    }

  bool crcGood = trailer.CheckFcs (copyPacket);
  if (!crcGood)
    {
      NS_LOG_INFO ("CRC error on Packet " << copyPacket);
      m_phyRxDropTrace (copyPacket);
      return;
    }

  NS_LOG_LOGIC ("Pkt source is " << header.GetSource ());
  NS_LOG_LOGIC ("Pkt destination is " << header.GetDestination ());

  if(IsSdnEnabled())
    {
      if (!m_sdnRxCallback.IsNull ())
        {
          m_sdnRxCallback (this, originalPacket, header.GetLengthType (), header.GetSource ());
        }
      return;
    }

  // Block BPDU packets for non-SDN net devices
  Mac48Address bpduStpAddress1(BPDU_STP_ADDRESS1);
  Mac48Address bpduStpAddress2(BPDU_STP_ADDRESS2);
  if (header.GetDestination () == bpduStpAddress1 || header.GetDestination () == bpduStpAddress2)
    {
      NS_LOG_LOGIC ("BPDU packet received but dropped for STP non-participant.");
      return;
    }

  uint16_t protocol;
  //
  // If the length/type is less than 1500, it corresponds to a length
  // interpretation packet.  In this case, it is an 802.3 packet and
  // will also have an 802.2 LLC header.  If greater than 1500, we
  // find the protocol number (Ethernet type) directly.
  //
  if (header.GetLengthType () <= 1500)
    {
      NS_ASSERT (copyPacket->GetSize () >= header.GetLengthType ());
      uint32_t padlen = copyPacket->GetSize () - header.GetLengthType ();
      NS_ASSERT (padlen <= 46);
      if (padlen > 0)
        {
          copyPacket->RemoveAtEnd (padlen);
        }

      LlcSnapHeader llc;
      copyPacket->RemoveHeader (llc);
      protocol = llc.GetType ();
    }
  else
    {
      protocol = header.GetLengthType ();
    }

  //
  // Classify the packet based on its destination.
  //
  PacketType packetType;

  if (header.GetDestination ().IsBroadcast ())
    {
      packetType = PACKET_BROADCAST;
    }
  else if (header.GetDestination ().IsGroup ())
    {
      packetType = PACKET_MULTICAST;
    }
  else if (header.GetDestination () == m_address)
    {
      packetType = PACKET_HOST;
    }
  else
    {
      packetType = PACKET_OTHERHOST;
    }

  //
  // For all kinds of packetType we receive, we hit the promiscuous sniffer
  // hook and pass a copy up to the promiscuous callback.  Pass a copy to
  // make sure that nobody messes with our packet.
  //
  m_promiscSnifferTrace (originalPacket);
  if (!m_promiscCallback.IsNull ())
    {
      m_macPromiscRxTrace (originalPacket);
      m_promiscCallback (this, copyPacket, protocol, header.GetSource (), header.GetDestination (), packetType);
    }

  //
  // If this packet is not destined for some other host, it must be for us
  // as either a broadcast, multicast or unicast.  We need to hit the mac
  // packet received trace hook and forward the packet up the stack.
  //
  Mac48Address lldpAddress(LLDP_DISCOVERY_ADDRESS);
  if (packetType != PACKET_OTHERHOST && header.GetDestination() != lldpAddress)
    {
      m_snifferTrace (originalPacket);
      m_macRxTrace (originalPacket);
      m_rxCallback (this, copyPacket, protocol, header.GetSource ());
    }
}

Ptr<Queue>
Layer2P2PNetDevice::GetQueue (void) const
{ 
  NS_LOG_FUNCTION (this);
  return m_queue;
}

void
Layer2P2PNetDevice::NotifyLinkUp (void)
{
  NS_LOG_FUNCTION (this);
  m_linkUp = true;
  m_linkChangeCallbacks ();
}

void
Layer2P2PNetDevice::SetIfIndex (const uint32_t index)
{
  NS_LOG_FUNCTION (this);
  m_ifIndex = index;
}

uint32_t
Layer2P2PNetDevice::GetIfIndex (void) const
{
  return m_ifIndex;
}

Ptr<Channel>
Layer2P2PNetDevice::GetChannel (void) const
{
  return m_channel;
}

//
// This is a point-to-point device, so we really don't need any kind of address
// information.  However, the base class NetDevice wants us to define the
// methods to get and set the address.  Rather than be rude and assert, we let
// clients get and set the address, but simply ignore them.

void
Layer2P2PNetDevice::SetAddress (Address address)
{
  NS_LOG_FUNCTION (this << address);
  m_address = Mac48Address::ConvertFrom (address);
}

Address
Layer2P2PNetDevice::GetAddress (void) const
{
  return m_address;
}

bool
Layer2P2PNetDevice::IsLinkUp (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkUp;
}

void
Layer2P2PNetDevice::AddLinkChangeCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION (this);
  m_linkChangeCallbacks.ConnectWithoutContext (callback);
}

//
// This is a point-to-point device, so every transmission is a broadcast to
// all of the devices on the network.
//
bool
Layer2P2PNetDevice::IsBroadcast (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

//
// We don't really need any addressing information since this is a 
// point-to-point device.  The base class NetDevice wants us to return a
// broadcast address, so we make up something reasonable.
//
Address
Layer2P2PNetDevice::GetBroadcast (void) const
{
  NS_LOG_FUNCTION (this);
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool
Layer2P2PNetDevice::IsMulticast (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

Address
Layer2P2PNetDevice::GetMulticast (Ipv4Address multicastGroup) const
{
  NS_LOG_FUNCTION (this);
  return Mac48Address ("01:00:5e:00:00:00");
}

Address
Layer2P2PNetDevice::GetMulticast (Ipv6Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
  return Mac48Address ("33:33:00:00:00:00");
}

bool
Layer2P2PNetDevice::IsPointToPoint (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

bool
Layer2P2PNetDevice::IsBridge (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

bool
Layer2P2PNetDevice::Send (Ptr<Packet> packet,const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (packet << dest << protocolNumber);
  return SendFrom (packet, m_address, dest, protocolNumber);
}

bool
Layer2P2PNetDevice::SendFrom (Ptr<Packet> packet, const Address& src, const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (packet << src << dest << protocolNumber);
  NS_LOG_LOGIC ("packet =" << packet);
  NS_LOG_LOGIC ("UID is " << packet->GetUid () << ")");

  NS_ASSERT (IsLinkUp ());

  Ptr<Packet> copyPacket = packet->Copy ();

  Mac48Address destination = Mac48Address::ConvertFrom (dest);
  Mac48Address source = Mac48Address::ConvertFrom (src);
  AddHeader (copyPacket, source, destination, protocolNumber);

  m_macTxTrace (copyPacket);

  //
  // Place the packet to be sent on the send queue.  Note that the
  // queue may fire a drop trace, but we will too.
  //
  if (m_queue->Enqueue (copyPacket) == false)
    {
      m_macTxDropTrace (copyPacket);
      return false;
    }

  //
  // If the device is idle, we need to start a transmission. Otherwise,
  // the transmission will be started when the current packet finished
  // transmission (see TransmitCompleteEvent)
  //
  if (m_txMachineState == READY)
    {
      if (m_queue->IsEmpty () == false)
        {
          copyPacket = m_queue->Dequeue ();
          m_snifferTrace (copyPacket);
          m_promiscSnifferTrace (copyPacket);
          return TransmitStart (copyPacket);
        }
    }
  return true;
}

Ptr<Node>
Layer2P2PNetDevice::GetNode (void) const
{
  return m_node;
}

void
Layer2P2PNetDevice::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this);
  m_node = node;
}

bool
Layer2P2PNetDevice::NeedsArp (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

void
Layer2P2PNetDevice::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  m_rxCallback = cb;
}

void
Layer2P2PNetDevice::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  m_promiscCallback = cb;
}

void
Layer2P2PNetDevice::SetSdnReceiveCallback (NetDevice::ReceiveCallback cb)
{
  NS_LOG_FUNCTION (&cb);
  m_sdnRxCallback = cb;
}

bool
Layer2P2PNetDevice::SupportsSendFrom (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

Address 
Layer2P2PNetDevice::GetRemote (void) const
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_channel->GetNDevices () == 2);
  for (uint32_t i = 0; i < m_channel->GetNDevices (); ++i)
    {
      Ptr<NetDevice> tmp = m_channel->GetDevice (i);
      if (tmp != this)
        {
          return tmp->GetAddress ();
        }
    }
  NS_ASSERT (false);
  // quiet compiler.
  return Address ();
}

bool
Layer2P2PNetDevice::SetMtu (uint16_t mtu)
{
  NS_LOG_FUNCTION (this << mtu);
  m_mtu = mtu;

  NS_LOG_LOGIC ("m_encapMode = " << m_encapMode);
  NS_LOG_LOGIC ("m_mtu = " << m_mtu);
  return true;
}

uint16_t
Layer2P2PNetDevice::GetMtu (void) const
{
  NS_LOG_FUNCTION (this);
  return m_mtu;
}

void
Layer2P2PNetDevice::SetSdnEnable (bool sdnEnable)
{
  NS_LOG_FUNCTION (sdnEnable);
  m_sdnEnable = sdnEnable;
}

bool
Layer2P2PNetDevice::IsSdnEnabled (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_sdnEnable;
}

} // namespace ns3
