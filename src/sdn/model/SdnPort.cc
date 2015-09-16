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

#include "SdnPort.h"

#define DEFAULT_ADVERTISED_FEATURES 0

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("SdnPort");

NS_OBJECT_ENSURE_REGISTERED (SdnPort);

TypeId SdnPort::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SdnPort")
    .SetParent<Object> ()
    .AddConstructor<SdnPort> ()
  ;
  return tid;
}

SdnPort::SdnPort (Ptr<NetDevice> device,
                Ptr<SdnConnection> conn,
                uint16_t port_no,
                uint32_t config,
                uint32_t state,
                uint32_t features)
: m_device (device),
  m_conn (conn)
{
  NS_LOG_FUNCTION (this << device << conn << port_no << config);

  Mac48Address macAddr = Mac48Address::ConvertFrom(m_device->GetAddress());
  uint8_t hwdata[fluid_msg::OFP_ETH_ALEN];
  macAddr.CopyTo(hwdata);
  
  // Create the port since it was not given.
  m_fluidPort = fluid_msg::of10::Port (port_no,
                      fluid_msg::EthAddress(hwdata),
                      "ns-3 Switch",
                      config,
                      state, // state
                      features, // curr
                      features,
                      features, // supported
                      features); // peer
}

SdnPort::SdnPort (Ptr<NetDevice> device, 
                  Ptr<SdnConnection> conn, 
                  fluid_msg::of10::Port p)
: m_device (device),
  m_conn (conn),
  m_fluidPort (p)
{
  NS_LOG_FUNCTION (this << device << conn);
}

SdnPort::SdnPort (void)
:   m_device(),
    m_conn(),
    m_fluidPort()
{
}

SdnPort::~SdnPort ()
{
  NS_LOG_FUNCTION (this);
}

void
SdnPort::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  Object::DoDispose ();
}

fluid_msg::of10::PortStats*
SdnPort::convertToPortStats()
  {
    NS_LOG_FUNCTION (this);
    fluid_msg::of10::PortStats* portStats = new fluid_msg::of10::PortStats(m_fluidPort.port_no(), m_tx_stats, m_err_stats, m_collisions);
    return portStats;
  }

Ptr<NetDevice> SdnPort::getDevice (void)
{
  NS_LOG_FUNCTION (this);

  return m_device;
}

void SdnPort::setDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (this << device);

  m_device = device;
}

Ptr<SdnConnection>
SdnPort::getConn (void)
{
  NS_LOG_FUNCTION (this);

  return m_conn;
}

void
SdnPort::setConn (Ptr<SdnConnection> conn)
{
  NS_LOG_FUNCTION (this << conn);

  m_conn = conn;
}

uint16_t
SdnPort::getPortNumber (void)
{
  NS_LOG_FUNCTION (this);

  return m_fluidPort.port_no();
}

void
SdnPort::setPortNumber (uint16_t portNo)
{
  NS_LOG_FUNCTION (this << portNo);

  m_fluidPort.port_no(portNo);
}

void
SdnPort::setHardwareAddress (uint8_t* hwAddr)
{
  NS_LOG_FUNCTION (this);

  m_fluidPort.hw_addr(hwAddr);
}

uint8_t*
SdnPort::getHardwareAddress (void)
{
  NS_LOG_FUNCTION (this);

  return m_fluidPort.hw_addr().get_data();
}

void
SdnPort::setConfig (uint32_t config)
{
  NS_LOG_FUNCTION (this << config);

  m_fluidPort.config(config);
}

uint32_t
SdnPort::getConfig (void)
{
  NS_LOG_FUNCTION (this);

  return m_fluidPort.config();
}

void
SdnPort::setAdvertised(uint32_t advertised)
{
  NS_LOG_FUNCTION (this << advertised);

  m_fluidPort.advertised(advertised);
}

uint32_t
SdnPort::getAdvertised(void)
{
  NS_LOG_FUNCTION (this);

  return m_fluidPort.advertised();
}

void
SdnPort::setSupported(uint32_t supported)
{
  NS_LOG_FUNCTION (this << supported);

  m_fluidPort.supported(supported);
}

uint32_t
SdnPort::getSupported(void)
{
  NS_LOG_FUNCTION (this);

  return m_fluidPort.supported();
}

void
SdnPort::setPeer(uint32_t peer)
{
  NS_LOG_FUNCTION (this << peer);

  m_fluidPort.peer(peer);
}

uint32_t
SdnPort::getPeer(void)
{
  NS_LOG_FUNCTION (this);

  return m_fluidPort.peer();
}
  
fluid_msg::of10::Port
SdnPort::getFluidPort (void)
{
  NS_LOG_FUNCTION (this);

  return m_fluidPort;
}

uint32_t
SdnPort::sendOnPort (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << packet);

  return m_conn->send (packet, packet->GetSize());
}

uint32_t 
SdnPort::sendOnPort (fluid_msg::OFMsg* msg)
{
  NS_LOG_FUNCTION (this << msg);

  return m_conn->send (msg);
}

uint32_t 
SdnPort::sendOnPort (void* data, size_t len)
{
  NS_LOG_FUNCTION (this << data << len);

  return m_conn->send (data,len);
}

} // End of namespace ns3
