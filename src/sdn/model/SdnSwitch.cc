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

#include "SdnSwitch.h"
#include "SdnController.h"
#include "SdnConnection.h"
#include "ns3/log.h"
#include "ns3/socket.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/seq-ts-header.h"
#include "ns3/loopback-net-device.h"
#include "ns3/point-to-point-module.h"
#include "ns3/layer2-p2p-module.h"
#include "ns3/ipv4.h"

#include "fluid/util/ethaddr.hh"

namespace ns3 {

#define GB 8000000000
#define Gb 1000000000
#define MB 8000000
#define mb 1000000

#define MAX_BUFFERS 1000000000

#ifndef INT16_MAX
#define INT16_MAX 32767
#endif

NS_LOG_COMPONENT_DEFINE ("SdnSwitch");

NS_OBJECT_ENSURE_REGISTERED (SdnSwitch);

uint32_t SdnSwitch::TOTAL_SERIAL_NUMBERS = 0;
uint32_t SdnSwitch::TOTAL_DATAPATH_IDS = 0;

TypeId SdnSwitch::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SdnSwitch")
    .SetParent<Application> ()
    .AddConstructor<SdnSwitch> ()
    .AddAttribute ("Kernel",
                   "Use the Linux kernel for establishing connections.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SdnSwitch::m_kernel),
                   MakeBooleanChecker ())
  ;
  return tid;
}

SdnSwitch::SdnSwitch () : m_flowTable(this)
{
  NS_LOG_FUNCTION (this);
  PacketMetadata::Enable();
  m_datapathID =  getNewDatapathID ();
  m_vendor = 0xFFFF;
  m_missSendLen = INT16_MAX;
  m_recvEvent = EventId ();
  TOTAL_PORTS = 0;
  m_controllerConn = CreateObject<SdnConnection> ();

  m_bufferIdStream = CreateObject<UniformRandomVariable> ();
  m_bufferIdStream->SetAttribute("Min", DoubleValue(0.0));
  m_bufferIdStream->SetAttribute("Max", DoubleValue(MAX_BUFFERS));
  m_switchFeatures.n_buffers = MAX_BUFFERS;

  m_kernel = false;
}

SdnSwitch::~SdnSwitch ()
{
  NS_LOG_FUNCTION (this);
}

void SdnSwitch::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  Application::DoDispose ();
}

void SdnSwitch::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  uint32_t t_numDevices = GetNode ()->GetNDevices ();
  for (uint32_t i = 0; i < t_numDevices; ++i)
    {
      //Get the Device and the channel on this device
      Ptr<NetDevice> device = GetNode ()->GetDevice (i);
      Ptr<Channel> channel = device->GetChannel ();

      //Try and find the remoteNode on the other side of the channel
      Ptr<Node> remoteNode;
      Ptr<NetDevice> remoteDevice;
      if (!DynamicCast<LoopbackNetDevice> (device))
        {
          if (channel->GetDevice (0) == device)
            {
              remoteDevice = channel->GetDevice (1);
            }
          else
            {
              remoteDevice = channel->GetDevice (0);
            }
          remoteNode = remoteDevice->GetNode ();
          Ptr<Ipv4> remoteIpv4 = remoteNode->GetObject<Ipv4> ();
          Ptr<Ipv4> localIpv4 = GetNode ()->GetObject<Ipv4> ();
          NS_ASSERT_MSG (remoteIpv4 && localIpv4, "SdnSwitch::StartApplication(): NetDevice is associated"
                         " with a node without IPv4 stack installed -> fail "
                         "(maybe need to use InternetStackHelper?)");
          Ipv4InterfaceAddress t_remoteInterfaceAddress =
            remoteIpv4->GetAddress (remoteIpv4->GetInterfaceForDevice (remoteDevice), 0);
          Ipv4Address t_remoteAddress = t_remoteInterfaceAddress.GetLocal ();

          Ipv4InterfaceAddress t_localInterfaceAddress =
            localIpv4->GetAddress (localIpv4->GetInterfaceForDevice (device), 0);
          Ipv4Address t_localAddress = t_localInterfaceAddress.GetLocal ();

          // The following logic is a mix of hack and design. To simplify connectivity between
          // a controller and a switch, the channel used between them should be point-to-point.
          // Anything else that may connect to a switch can likely be considered either an end
          // host or another switch.
          //
          // Previous logic aimed at examining each application installed on an adjacent node
          // to see if one of the applications was either an SndController or an SdnSwitch.
          // This logic was incompatible with distributed simulations (due to ghost nodes)
          // and DCE (due to the fact that controller's running in DCE would definitely not
          // be SdnController ns-3 applications).
          Ptr<PointToPointNetDevice> p2pDevice = DynamicCast<PointToPointNetDevice> (device);
          if (p2pDevice)
            {
              NS_LOG_INFO ("Switch establishing controller connection with " <<
                           t_remoteAddress);
              EstablishControllerConnection (device, t_localAddress, t_remoteAddress);
            }
          else
            {
              NS_LOG_INFO ("Switch establishing non-controller connection with " <<
                           t_remoteAddress);
              EstablishConnection (device, t_localAddress, t_remoteAddress);
            }
        }
    }
}

void SdnSwitch::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
}

void SdnSwitch::EstablishControllerConnection (Ptr<NetDevice> device,
	    Ipv4Address localAddress,
	    Ipv4Address remoteAddress)
{
  NS_LOG_FUNCTION (this << device << localAddress << remoteAddress);
  NS_LOG_INFO (Simulator::Now().GetSeconds());

  // Create a socket for this switch.
  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  if (m_kernel)
    {
	  tid = TypeId::LookupByName ("ns3::LinuxTcpSocketFactory");
    }
  Ptr<Socket> socket = Socket::CreateSocket (GetNode (), tid);

  NS_ASSERT_MSG (Ipv4Address::IsMatchingType (remoteAddress),
				 "Found non IPv4 matching NetDevice address " <<
				 remoteAddress << "in a switch application ");

  Address a = InetSocketAddress (localAddress, OFCONTROLLERPORT);
  Address b = InetSocketAddress (remoteAddress, OFCONTROLLERPORT);
  NS_ASSERT (socket->Bind (a) == 0);
  socket->SetConnectCallback (
	MakeCallback (&SdnSwitch::ConnectionSucceeded, this),
	MakeCallback (&SdnSwitch::ConnectionFailed, this));
  //NS_ASSERT (socket->Connect (b) == 0);
  socket->Connect (b);
  NS_ASSERT (socket->GetErrno() < Socket::ERROR_NOTCONN);

  m_controllerConn = CreateObject<SdnConnection> (device, socket);
}

void SdnSwitch::EstablishConnection (Ptr<NetDevice> device,
				    Ipv4Address localAddress,
				    Ipv4Address remoteAddress)
{
  NS_LOG_FUNCTION (this << device << localAddress << remoteAddress);
  NS_LOG_INFO (Simulator::Now().GetSeconds());

  //Create a socket to a switch
  uint16_t switchPort = SdnSwitch::getNewPortNumber ();

  TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");
  if (m_kernel)
    {
	  tid = TypeId::LookupByName ("ns3::LinuxIpv4RawSocketFactory");
    }
  Ptr<Socket> socket = Socket::CreateSocket (GetNode (), tid);
  socket->SetAttribute ("Protocol", UintegerValue (17));

  Address a;
  NS_ASSERT_MSG (Ipv4Address::IsMatchingType (Ipv4Address::ConvertFrom (remoteAddress)),
				 "Found non IPv4 matching NetDevice address " <<
				 Ipv4Address::ConvertFrom (remoteAddress) << "in a switch application ");

  a = InetSocketAddress (Ipv4Address::ConvertFrom (remoteAddress), switchPort);
  socket->Bind ();
  socket->BindToNetDevice (device);
  socket->Connect (a);

  Ptr<Layer2P2PNetDevice> l2Device = DynamicCast<Layer2P2PNetDevice>(device);
  if(l2Device)
    {
      l2Device->SetSdnEnable(true);
      l2Device->SetSdnReceiveCallback(MakeCallback(&SdnSwitch::HandleReadFromNetDevice, this));
      Ptr<SdnConnection> c = CreateObject<SdnConnection> (device, socket);
      Ptr<Layer2P2PChannel> channel = DynamicCast<Layer2P2PChannel>(device->GetChannel());
      uint32_t portFeaturesMask = 0;
      if(channel)
	{
	  DataRateValue deviceDataRate;
	  l2Device->GetAttribute ("DataRate", deviceDataRate);
	  uint64_t bitRate = deviceDataRate.Get ().GetBitRate ();
	  if(bitRate >= 10 * GB)
	    {
	      portFeaturesMask |= fluid_msg::of10::OFPPF_10GB_FD;
	    }
	  else if(bitRate >= GB)
	    {
	      portFeaturesMask |= fluid_msg::of10::OFPPF_1GB_FD;
	    }
	  else if(bitRate >= MB * 100)
	    {
	      portFeaturesMask |= fluid_msg::of10::OFPPF_100MB_FD;
	    }
	  else if(bitRate >= MB * 10)
	    {
	      portFeaturesMask |= fluid_msg::of10::OFPPF_10MB_FD;
	    }
	}
      Ptr<SdnPort> p = CreateObject<SdnPort> (device, c, switchPort, 0, 0, portFeaturesMask);
      m_portMap.insert (std::make_pair (p->getPortNumber(),p));
    }
}

void
SdnSwitch::ConnectionSucceeded (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);

    NS_ASSERT (socket == m_controllerConn->get_socket());

    // Set the receive callback and create the SdnConnection
    socket->SetRecvCallback (MakeCallback (&SdnSwitch::HandleReadController,this));

    NS_LOG_INFO (Simulator::Now().GetSeconds() << " Connection succeeded");
}

void
SdnSwitch::ConnectionFailed (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    NS_LOG_INFO (Simulator::Now().GetSeconds() << " Connection failed");
}

//Handles a packet from a controller
void SdnSwitch::HandleReadController (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_INFO (Simulator::Now().GetSeconds());
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }

      NS_ASSERT (m_controllerConn->get_socket () == socket);

      packet->RemoveAllPacketTags ();
      packet->RemoveAllByteTags ();

      NS_LOG_DEBUG ("Switch recv " << packet->GetSize () << " bytes");

      uint8_t buffer[packet->GetSize ()];
      packet->CopyData (buffer,packet->GetSize ());
      fluid_msg::OFMsg* message = new fluid_msg::OFMsg (buffer);

      //These handlers should return number of bytes sent
      if (message->type () == fluid_msg::of10::OFPT_HELLO)
        {
          OFHandle_Hello_Request(message);
        }
      if (message->type () == fluid_msg::of10::OFPT_FEATURES_REQUEST)
        {
          OFHandle_Feature_Request(message);
        }
      if (message->type () == fluid_msg::of10::OFPT_GET_CONFIG_REQUEST)
        {
          OFHandle_Get_Config_Request();
        }
      if (message->type () == fluid_msg::of10::OFPT_SET_CONFIG)
        {
          OFHandle_Set_Config(buffer);
        }
      if (message->type () == fluid_msg::of10::OFPT_FLOW_MOD)
        {
          OFHandle_Flow_Mod(buffer);
        }
//      if (message->type () == fluid_msg::of10::OFPT_PORT_STATUS)
//        {
//          OFHandle_Port_Status(buffer);
//        }
      if (message->type () == fluid_msg::of10::OFPT_STATS_REQUEST)
        {
          OFHandle_Stats_Request(buffer);
        }
      if (message->type () == fluid_msg::of10::OFPT_PACKET_OUT)
        {
          OFHandle_Packet_Out(buffer);
        }
      if (message->type () == fluid_msg::of10::OFPT_PORT_MOD)
        {
          OFHandle_Port_Mod(buffer);
        }
      if (message->type () == fluid_msg::of10::OFPT_BARRIER_REQUEST)
        {
          OFHandle_Barrier_Request(buffer);
        }
      delete (message);
    }
}

//Handles a packet from a non-controller
bool SdnSwitch::HandleReadFromNetDevice (Ptr<NetDevice> device, Ptr<const Packet> originalPacket, uint16_t protocol, const Address &source )
{
  NS_LOG_FUNCTION (this << device << originalPacket << protocol << source);

  if(m_controllerConn->get_state() != fluid_base::OFConnection::STATE_RUNNING)
    {
      NS_LOG_WARN ("Received a packet on a switch but we're not in running mode yet! Dropping packet");
      return 0;
    }
  Ptr<Packet> packet = originalPacket->Copy();

  uint16_t inPort = 0;
  for(std::map<uint16_t,Ptr<SdnPort> >::iterator portIterator = m_portMap.begin(); portIterator != m_portMap.end(); ++portIterator)
    {
      if(portIterator->second && portIterator->second->getDevice() == device)
        {
          inPort = portIterator->first;
          break;
        }
    }

  return HandlePacket (packet, inPort);
}

bool SdnSwitch::HandlePacket (Ptr<Packet>packet, uint16_t inPort)
{
  NS_LOG_FUNCTION (this << packet << inPort);
  std::vector<uint16_t> outPorts = m_flowTable.handlePacket (packet, inPort);

  //Handle packet in message
  if (outPorts.empty() && m_portMap.count(inPort))
    {
      SendPacketInToController(packet, m_portMap[inPort]->getDevice (), fluid_msg::of10::OFPR_NO_MATCH);
      return 1;
    }
  //Send out on port assuming it's enabled
  for (std::vector<uint16_t>::iterator outPort = outPorts.begin(); outPort != outPorts.end(); ++outPort)
  {
    if ((*outPort == fluid_msg::of10::OFPP_CONTROLLER) && m_portMap.count(inPort))
      {
        SendPacketInToController(packet, m_portMap[inPort]->getDevice (), fluid_msg::of10::OFPR_ACTION);
        return 1;
      }
    //Handle flooding
    if(*outPort == fluid_msg::of10::OFPP_FLOOD)
      {
        Flood(packet, inPort);
        return 1;
      }
    //Handle packet output
    if (m_portMap.count (*outPort) != 0)
      {
        Ptr<SdnPort> portStruct = m_portMap[*outPort];
        if(portStruct && !(portStruct->getConfig() & (fluid_msg::of10::OFPPC_PORT_DOWN | fluid_msg::of10::OFPPC_NO_RECV | fluid_msg::of10::OFPPC_NO_FWD)))
          {
            portStruct->getConn()->sendOnNetDevice (packet);
          }
      }
  }
  return 0;
}

void SdnSwitch::OFHandle_Hello_Request (fluid_msg::OFMsg* message)
{
  NS_LOG_FUNCTION (this << message);
  fluid_msg::of10::Hello* helloMessage = new fluid_msg::of10::Hello(message->xid());
  if (NegotiateVersion (message))
    {
      m_controllerConn->send (helloMessage);
      m_controllerConn->set_state(fluid_base::OFConnection::STATE_RUNNING);
      return;
    }
  else
    {
      m_controllerConn->set_state(fluid_base::OFConnection::STATE_FAILED);
      fluid_msg::of10::Error* errorMessage =
        new fluid_msg::of10::Error (
          SdnCommon::GenerateXId(),
          fluid_msg::of10::OFPET_HELLO_FAILED,
          fluid_msg::of10::OFPHFC_INCOMPATIBLE);
      m_controllerConn->send(errorMessage);
      return;
    }
}

void SdnSwitch::OFHandle_Feature_Request (fluid_msg::OFMsg* message)
{
  NS_LOG_FUNCTION (this << message);
  std::vector<fluid_msg::of10::Port> fluidPorts;
  for(std::map<uint16_t, Ptr<SdnPort> >::iterator i = m_portMap.begin(); i != m_portMap.end(); ++i)
  {
    fluidPorts.push_back(i->second->getFluidPort());
  }

  fluid_msg::of10::FeaturesReply* featuresReply =
    new fluid_msg::of10::FeaturesReply (message->xid (),
					((uint64_t)((uint64_t)OF_DATAPATH_ID_PADDING << 48) | GetMacAddress ()),
                                        m_switchFeatures.n_buffers,
                                        m_switchFeatures.n_tables,
                                        GetCapabilities (),
                                        GetActions (),
                                        fluidPorts);
  m_controllerConn->send (featuresReply);
  return;
}

void SdnSwitch::OFHandle_Get_Config_Request()
{
  NS_LOG_FUNCTION (this);
  fluid_msg::of10::GetConfigReply* getConfigReply =
    new fluid_msg::of10::GetConfigReply (SdnCommon::GenerateXId(),
                                         0, //Flags of 0 for fragmentation
                                         m_missSendLen);
   m_controllerConn->send (getConfigReply);
}

void SdnSwitch::OFHandle_Set_Config(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::SetConfig* setConfig = new fluid_msg::of10::SetConfig();
  setConfig->unpack(buffer);
  if (setConfig)
    {
      m_missSendLen = setConfig->miss_send_len();
    }
}

void SdnSwitch::OFHandle_Flow_Mod(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::FlowMod* flowMod = new fluid_msg::of10::FlowMod();
  flowMod->unpack(buffer);
  if (flowMod)
    {
      switch (flowMod->command())
        {
          case fluid_msg::of10::OFPFC_ADD:
            addFlow(flowMod);
            break;
          case fluid_msg::of10::OFPFC_MODIFY:
            modifyFlow(flowMod);
            break;
          case fluid_msg::of10::OFPFC_MODIFY_STRICT:
            modifyFlowStrict(flowMod);
            break;
          case fluid_msg::of10::OFPFC_DELETE:
            deleteFlow(flowMod);
            break;
          case fluid_msg::of10::OFPFC_DELETE_STRICT:
            deleteFlowStrict(flowMod);
            break;
        }

      if (flowMod->command () != fluid_msg::of10::OFPFC_DELETE &&
	      flowMod->command () != fluid_msg::of10::OFPFC_DELETE_STRICT)
	{
          if ((flowMod->buffer_id () != (uint32_t)(-1)) &&
      	    (m_packetBuffers.count (flowMod->buffer_id ())))
            {
              Ptr<Packet> copyPacket = m_packetBuffers[flowMod->buffer_id ()]->Copy ();
              HandlePacket (copyPacket, flowMod->match ().in_port ());
            }
	}
    }
}

void SdnSwitch::addFlow(fluid_msg::of10::FlowMod* message)
{
  NS_LOG_FUNCTION (this << message);
  Flow flow = m_flowTable.addFlow(message);
  
  if(flow.cookie_ != message->cookie())
    {
    
      fluid_msg::of10::Error* errorMessage = new fluid_msg::of10::Error(SdnCommon::GenerateXId(),
	      fluid_msg::of10::OFPET_FLOW_MOD_FAILED,fluid_msg::of10::OFPFMFC_OVERLAP);
      m_controllerConn->send(errorMessage);
      return;
    }
}

void SdnSwitch::modifyFlow(fluid_msg::of10::FlowMod* message)
{
  NS_LOG_FUNCTION (this << message);
  m_flowTable.modifyFlow(message);
}

void SdnSwitch::modifyFlowStrict(fluid_msg::of10::FlowMod* message)
{
// HACK FOR COMPILE
  NS_LOG_FUNCTION (this << message);
  m_flowTable.modifyFlow(message);
}

void SdnSwitch::deleteFlow(fluid_msg::of10::FlowMod* message)
{
  NS_LOG_FUNCTION (this << message);
  m_flowTable.deleteFlow(message);
}

void SdnSwitch::deleteFlowStrict(fluid_msg::of10::FlowMod* message)
{
// HACK FOR COMPILE
  NS_LOG_FUNCTION (this << message);
  m_flowTable.deleteFlow(message);
}

void SdnSwitch::OFHandle_Port_Mod (uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::PortMod* portMod = new fluid_msg::of10::PortMod();
  portMod->unpack(buffer);
  if (portMod)
  {
    if (m_portMap.count(portMod->port_no()) != 0)
      {
        Ptr<SdnPort> port = m_portMap[portMod->port_no()];
        uint32_t newConfig = ((portMod->config() & portMod->mask()) | (port->getConfig() & ~portMod->mask())); // & port->getAdvertised();
        port->setConfig(newConfig);
      }
  }
}

//Need some more groundwork in here
void SdnSwitch::OFHandle_Stats_Request(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::StatsRequest* statsRequest = new fluid_msg::of10::StatsRequest();
  statsRequest->unpack(buffer);
  fluid_msg::of10::StatsReply* statsReply = new fluid_msg::of10::StatsReply();
  if (statsRequest)
    {
      switch (statsRequest->stats_type())
        {
          case fluid_msg::of10::OFPST_DESC:
            statsReply = replyWithDescription();
            break;
          case fluid_msg::of10::OFPST_FLOW:
            statsReply = replyWithFlow(buffer);
            break;
          case fluid_msg::of10::OFPST_AGGREGATE:
            statsReply = replyWithAggregate(buffer);
            break;
          case fluid_msg::of10::OFPST_TABLE:
            statsReply = replyWithTable(buffer);
            break;
          case fluid_msg::of10::OFPST_PORT:
            statsReply = replyWithPort(buffer);
            break;
          case fluid_msg::of10::OFPST_QUEUE:
            statsReply = replyWithQueue();
            break;
          case fluid_msg::of10::OFPST_VENDOR:
            statsReply = replyWithVendor();
            break;
        }
    }
  //Establish statsReply here
  m_controllerConn->send(statsReply);
  return;
}

//stat utility functions
fluid_msg::of10::StatsReplyDesc* SdnSwitch::replyWithDescription()
{
  NS_LOG_FUNCTION (this);
  fluid_msg::of10::StatsReplyDesc* statsReply = new fluid_msg::of10::StatsReplyDesc(SdnCommon::GenerateXId(), 0, m_switchDescription); //0 flags because 1.0.0 hasn't defined it yet
  return statsReply;
}

fluid_msg::of10::StatsReplyFlow* SdnSwitch::replyWithFlow(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::StatsRequestFlow* flowRequest = new fluid_msg::of10::StatsRequestFlow();
  flowRequest->unpack(buffer);
  std::vector<Flow> matchingFlows = m_flowTable.matchingFlows(flowRequest->match());
  std::vector<fluid_msg::of10::FlowStats> matchFlowStats;
  for(std::vector<Flow>::iterator flow = matchingFlows.begin(); flow != matchingFlows.end(); ++flow)
    {
      fluid_msg::of10::FlowStats* flowStat = (*flow).convertToFlowStats();
      matchFlowStats.push_back(*flowStat);
    }
  fluid_msg::of10::StatsReplyFlow* flowReply = new fluid_msg::of10::StatsReplyFlow(SdnCommon::GenerateXId (), 0, matchFlowStats); //0 flags because 1.0.0 hasn't defined it yet
  return flowReply;
}

fluid_msg::of10::StatsReplyAggregate* SdnSwitch::replyWithAggregate(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::StatsRequestAggregate* aggregateRequest = new fluid_msg::of10::StatsRequestAggregate();
 aggregateRequest->unpack(buffer);
  uint64_t packet_count = 0;
  uint64_t byte_count = 0;
  uint32_t flow_count = 0;
  std::vector<Flow> matchingFlows = m_flowTable.matchingFlows(aggregateRequest->match());
  for(std::vector<Flow>::iterator flow = matchingFlows.begin(); flow != matchingFlows.end(); ++flow)
    {
      packet_count += (*flow).packet_count_;
      byte_count += (*flow).byte_count_;
      flow_count++;
    }
  fluid_msg::of10::StatsReplyAggregate* aggregateReply  = new fluid_msg::of10::StatsReplyAggregate(SdnCommon::GenerateXId(), 0, packet_count, byte_count, flow_count);
  return aggregateReply;
}

fluid_msg::of10::StatsReplyTable* SdnSwitch::replyWithTable(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::StatsRequestTable* tableRequest = new fluid_msg::of10::StatsRequestTable();
  tableRequest->unpack(buffer);
  std::vector<fluid_msg::of10::TableStats> tableStats;
  tableStats.push_back(*(m_flowTable.convertToTableStats()));
  fluid_msg::of10::StatsReplyTable* tableReply = new fluid_msg::of10::StatsReplyTable(SdnCommon::GenerateXId(), 0, tableStats);
  return tableReply;
}

fluid_msg::of10::StatsReplyPort* SdnSwitch::replyWithPort(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::StatsRequestPort* portRequest = new fluid_msg::of10::StatsRequestPort();
  portRequest->unpack(buffer);
  std::vector<fluid_msg::of10::PortStats> portStats;
  if(portRequest->port_no() == fluid_msg::of10::OFPP_NONE)
  {
    for(std::map<uint16_t,Ptr<SdnPort> >::iterator port = m_portMap.begin(); port != m_portMap.end(); ++port)
    {
      portStats.push_back(*(port->second->convertToPortStats()));
    }
  }
  else if (m_portMap.count(portRequest->port_no()) != 0)
  {
    portStats.push_back(*(m_portMap[portRequest->port_no()]->convertToPortStats()));
  }
  fluid_msg::of10::StatsReplyPort* portReply = new fluid_msg::of10::StatsReplyPort(SdnCommon::GenerateXId(), 0, portStats);
  return portReply;
}

fluid_msg::of10::StatsReplyQueue* SdnSwitch::replyWithQueue()
{
  NS_LOG_FUNCTION (this);
  return new fluid_msg::of10::StatsReplyQueue(SdnCommon::GenerateXId(), 0); //We don't handle queues anyway
}

fluid_msg::of10::StatsReplyVendor* SdnSwitch::replyWithVendor()
{
  NS_LOG_FUNCTION (this);
  return new fluid_msg::of10::StatsReplyVendor(); //We don't handle vendor stats
}

void SdnSwitch::OFHandle_Barrier_Request(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::BarrierRequest* barrierRequest = new fluid_msg::of10::BarrierRequest ();
  barrierRequest->unpack(buffer);
  //Finish computing all other messages we've queued. Whatever that means for us.
  fluid_msg::of10::BarrierReply* barrierReply = new fluid_msg::of10::BarrierReply(barrierRequest->xid ());
  m_controllerConn->send(barrierReply);
}

void SdnSwitch::OFHandle_Packet_Out(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::PacketOut* packetOut = new fluid_msg::of10::PacketOut ();
  fluid_msg::of_error status = packetOut->unpack(buffer);
  NS_ASSERT (status != fluid_msg::OF_ERROR);

  Ptr<Packet> packet;
  // Buffer ID == -1 => PacketOut should contain packet.
  if (packetOut->buffer_id () == (uint32_t)(-1))
    {
      uint32_t dataSize = (uint32_t)packetOut->data_len();
      uint8_t* dataBuffer;
      dataBuffer = (uint8_t*)packetOut->data();
      packet = Create<Packet>(dataBuffer, dataSize);
    }
  // Buffer ID matches one from packet buffers.
  else if (m_packetBuffers.count (packetOut->buffer_id ()))
    {
      packet = m_packetBuffers[packetOut->buffer_id ()]->Copy ();
//      m_packetBuffers.erase (packetOut->buffer_id ());
    }
  else
    {
      NS_LOG_WARN ("Received buffer id without an associated packet");
      return;
    }
  uint16_t outPort = fluid_msg::of10::OFPP_NONE;
  
  std::list<fluid_msg::Action*> action_list = packetOut->actions().action_list();

  fluid_msg::Action* action;
  for (std::list<fluid_msg::Action*>::iterator j = action_list.begin (); j != action_list.end(); ++j)
    {
      action = *j;
      if(action->type() == fluid_msg::of10::OFPAT_OUTPUT)
        {
          outPort = m_flowTable.handleAction(packet,action);
        }
      else
        {
          m_flowTable.handleAction(packet,action);
        }
    }
  if(outPort == fluid_msg::of10::OFPP_FLOOD)
    {
      Flood(packet, packetOut->in_port());
      return;
    }
  if (outPort == fluid_msg::of10::OFPP_TABLE)
    {
	  HandlePacket (packet, packetOut->in_port());
	  return;
    }
  if (m_portMap.count(outPort) != 0)
    {
      Ptr<SdnPort> outputPort = m_portMap[outPort]; //Might be wrong?
      outputPort->getConn()->sendOnNetDevice(packet);
    }
}

void SdnSwitch::SendPacketInToController(Ptr<Packet> packet, Ptr<NetDevice> device, uint8_t reason)
{
  NS_LOG_FUNCTION (this << packet << device << reason);
  Ptr<SdnPort> port;
  for(std::map<uint16_t,Ptr<SdnPort> >::iterator portIterator = m_portMap.begin(); portIterator != m_portMap.end(); ++portIterator)
    {
      if(portIterator->second && portIterator->second->getDevice() == device)
        {
          port = portIterator->second;
          break;
        }
    }
  
  // While the maximum number of buffered packets has not been exceeded, add the
  // packet to the buffer before sending
  fluid_msg::of10::PacketIn* packetIn;
  if (m_packetBuffers.size () <= MAX_BUFFERS)
    {
      uint32_t newId = 0;
      do
	{
	  newId = m_bufferIdStream->GetInteger ();
	} while (m_packetBuffers.count (newId) == 1);

      m_packetBuffers[newId] = packet->Copy ();
      packetIn = new fluid_msg::of10::PacketIn(SdnCommon::GenerateXId(),
    	  newId, port->getPortNumber(), packet->GetSize(), reason);
    }
  else
    {
      packetIn = new fluid_msg::of10::PacketIn(SdnCommon::GenerateXId(),
    	  -1, port->getPortNumber(), packet->GetSize(), reason);
    }
  uint8_t buffer[packet->GetSize ()];
  packet->CopyData (buffer,packet->GetSize ());
  packetIn->data(buffer,packet->GetSize());

  if (packetIn) m_controllerConn->send(packetIn);
}

void SdnSwitch::SendFlowRemovedMessageToController(Flow flow, uint8_t reason)
{
  NS_LOG_FUNCTION (this << reason);
  fluid_msg::of10::FlowRemoved* flowRemoved = new fluid_msg::of10::FlowRemoved(
	  SdnCommon::GenerateXId(), flow.cookie_, flow.priority_, reason, flow.duration_sec_,
	  flow.duration_nsec_, flow.idle_timeout_, flow.packet_count_, flow.byte_count_);
  m_controllerConn->send(flowRemoved);
}

void SdnSwitch::SendPortStatusMessageToController(fluid_msg::of10::Port port, uint8_t reason)
{
  NS_LOG_FUNCTION (this << reason);
  fluid_msg::of10::PortStatus* portStatus = new fluid_msg::of10::PortStatus(SdnCommon::GenerateXId(), reason, port);  
  m_controllerConn->send(portStatus);
}

void SdnSwitch::Flood(Ptr<Packet> packet, uint16_t portNum)
{
  NS_LOG_FUNCTION (this << packet << portNum);
 for(std::map<uint16_t,Ptr<SdnPort> >::iterator port = m_portMap.begin(); port != m_portMap.end(); ++port)
  {
    if((port->first != portNum) && (port->second) && !(port->second->getConfig() & fluid_msg::of10::OFPPC_NO_FLOOD))
      {
        Ptr<Packet> copyPacket = packet->Copy ();
        port->second->getConn()->sendOnNetDevice(copyPacket);
      }
  }
}

void SdnSwitch::Flood(Ptr<Packet> packet, Ptr<NetDevice> netDevice)
{
  NS_LOG_FUNCTION (this << packet << netDevice);
  for(std::map<uint16_t,Ptr<SdnPort> >::iterator port = m_portMap.begin(); port != m_portMap.end(); ++port)
    {
      if((port->second) && !(port->second->getConfig() & fluid_msg::of10::OFPPC_NO_FLOOD) && (port->second->getDevice() != netDevice))
        {
          port->second->getConn()->sendOnNetDevice(packet);
        }
    }
}

void SdnSwitch::LogOFMsg (fluid_msg::OFMsg* message)
{
  NS_LOG_FUNCTION (this << message);
  NS_LOG_DEBUG ("OFMsg Log:");
  NS_LOG_DEBUG ("\t type:" << message->type ());
  NS_LOG_DEBUG ("\t length:" << message->length ());
  NS_LOG_DEBUG ("\t xid:" << message->xid ());
}

uint64_t SdnSwitch::GetMacAddress ()
{
  NS_LOG_FUNCTION (this);
  uint8_t buffer[6];
  m_controllerConn->get_device ()->GetAddress ().CopyTo (buffer);
  uint64_t returnWord = 0;
  memcpy (&returnWord,buffer,6);
  return returnWord;
}

uint32_t SdnSwitch::GetCapabilities ()
{
  NS_LOG_FUNCTION (this);
  return (uint32_t) m_switchFeatures.capabilities.NS3_SWITCH_OFPC_FLOW_STATS          |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_TABLE_STATS    << 1 |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_PORT_STATS     << 2 |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_STP_STATS      << 3 |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_RESERVED_STATS << 4 |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_QUEUE_STATS    << 5 |
         m_switchFeatures.capabilities.NS3_SWITCH_ARP_MATCH_IP        << 6;
}

uint32_t SdnSwitch::GetActions ()
{
  NS_LOG_FUNCTION (this);
  return (uint32_t) m_switchFeatures.actions.NS3_SWITCH_OFPAT_OUTPUT               |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_SET_VLAN_VID   << 1  |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_SET_VLAN_PCP   << 2  |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_STRIP_VLAN     << 3  |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_SET_DL_SRC     << 4  |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_SET_DL_DST     << 5  |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_SET_NW_SRC     << 6  |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_SET_NW_DST     << 7  |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_SET_NW_TOS     << 8  |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_SET_TP_SRC     << 9  |
         m_switchFeatures.actions.NS3_SWITCH_OFPAT_SET_TP_DST     << 10;
}

bool SdnSwitch::NegotiateVersion (fluid_msg::OFMsg* msg)
{
  NS_LOG_FUNCTION (this << msg);
  return msg->version () == fluid_msg::of10::OFP_VERSION;
}

//Static ID generators

uint16_t SdnSwitch::getNewPortNumber ()
{
  NS_LOG_FUNCTION (this);
  TOTAL_PORTS++;
  return TOTAL_PORTS == OFCONTROLLERPORT ? ++TOTAL_PORTS : TOTAL_PORTS;
}

std::string SdnSwitch::getNewSerialNumber ()
{
  NS_LOG_FUNCTION (this);
  TOTAL_SERIAL_NUMBERS++;
  std::stringstream ss;
  ss << TOTAL_SERIAL_NUMBERS;
  std::string returnString = ss.str();
  while(returnString.size() != fluid_msg::SERIAL_NUM_LEN - 3)
    {
      returnString.append("0");
    }
  returnString.append("NS3");
  return returnString;
}

uint32_t SdnSwitch::getNewDatapathID ()
{
  ++TOTAL_DATAPATH_IDS;
  return TOTAL_DATAPATH_IDS;
}

}
