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

#include "SdnSwitch13.h"
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

NS_LOG_COMPONENT_DEFINE ("SdnSwitch13");

NS_OBJECT_ENSURE_REGISTERED (SdnSwitch13);

uint32_t SdnSwitch13::TOTAL_SERIAL_NUMBERS = 0;
uint32_t SdnSwitch13::TOTAL_DATAPATH_IDS = 0;

TypeId SdnSwitch13::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SdnSwitch13")
    .SetParent<Application> ()
    .AddConstructor<SdnSwitch13> ()
  ;
  return tid;
}

SdnSwitch13::SdnSwitch13 ()
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

  m_flowTable13 = SdnFlowTable13::addTablesForNewSwitch(this);
  m_kernel = false;
}

SdnSwitch13::~SdnSwitch13 ()
{
  NS_LOG_FUNCTION (this);
}

void SdnSwitch13::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  Application::DoDispose ();
}

void SdnSwitch13::EstablishControllerConnection (Ptr<NetDevice> device,
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
	MakeCallback (&SdnSwitch13::ConnectionSucceeded, this),
	MakeCallback (&SdnSwitch13::ConnectionFailed, this));
  //NS_ASSERT (socket->Connect (b) == 0);
  socket->Connect (b);
  NS_ASSERT (socket->GetErrno() < Socket::ERROR_NOTCONN);

  m_controllerConn = CreateObject<SdnConnection> (device, socket);
}

void SdnSwitch13::EstablishConnection (Ptr<NetDevice> device,
				    Ipv4Address localAddress,
				    Ipv4Address remoteAddress)
{
  NS_LOG_FUNCTION (this << device << localAddress << remoteAddress);
  NS_LOG_INFO (Simulator::Now().GetSeconds());

  //Create a socket to a switch
  uint16_t switchPort = SdnSwitch13::getNewPortNumber ();

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
      l2Device->SetSdnReceiveCallback(MakeCallback(&SdnSwitch13::HandleReadFromNetDevice, this));
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
	      portFeaturesMask |= fluid_msg::of13::OFPPF_10GB_FD;
	    }
	  else if(bitRate >= GB)
	    {
	      portFeaturesMask |= fluid_msg::of13::OFPPF_1GB_FD;
	    }
	  else if(bitRate >= MB * 100)
	    {
	      portFeaturesMask |= fluid_msg::of13::OFPPF_100MB_FD;
	    }
	  else if(bitRate >= MB * 10)
	    {
	      portFeaturesMask |= fluid_msg::of13::OFPPF_10MB_FD;
	    }
	}
      Ptr<SdnPort> p = CreateObject<SdnPort> (device, c, switchPort, 0, 0, portFeaturesMask);
      m_portMap.insert (std::make_pair (p->getPortNumber(),p));
    }
}

void SdnSwitch13::StartApplication (void)
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

void SdnSwitch13::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
}

void
SdnSwitch13::ConnectionSucceeded (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);

    NS_ASSERT (socket == m_controllerConn->get_socket());

    // Set the receive callback and create the SdnConnection
    socket->SetRecvCallback (MakeCallback (&SdnSwitch13::HandleReadController,this));

    NS_LOG_INFO (Simulator::Now().GetSeconds() << " Connection succeeded");
}
void
SdnSwitch13::ConnectionFailed (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    NS_LOG_INFO (Simulator::Now().GetSeconds() << " Connection failed");
}

//Handles a packet from a controller
void SdnSwitch13::HandleReadController (Ptr<Socket> socket)
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
      if (message->type () == fluid_msg::of13::OFPT_HELLO)
        {
          OFHandle_Hello_Request(message);
        }
      if (message->type () == fluid_msg::of13::OFPT_FEATURES_REQUEST)
        {
          OFHandle_Feature_Request(message);
        }
      if (message->type () == fluid_msg::of13::OFPT_GET_CONFIG_REQUEST)
        {
          OFHandle_Get_Config_Request();
        }
      if (message->type () == fluid_msg::of13::OFPT_SET_CONFIG)
        {
          OFHandle_Set_Config(buffer);
        }
      if (message->type () == fluid_msg::of13::OFPT_FLOW_MOD)
        {
          OFHandle_Flow_Mod(buffer);
        }
      if (message->type () == fluid_msg::of13::OFPT_GROUP_MOD)
        {
    	  OFHandle_Group_Mod(buffer);
        }
//      if (message->type () == fluid_msg::of10::OFPT_PORT_STATUS)
//        {
//          OFHandle_Port_Status(buffer);
//        }
      if (message->type () == fluid_msg::of13::OFPT_PACKET_OUT)
        {
          OFHandle_Packet_Out(buffer);
        }
      if (message->type () == fluid_msg::of13::OFPT_PORT_MOD)
        {
          OFHandle_Port_Mod(buffer);
        }
      if (message->type () == fluid_msg::of13::OFPT_BARRIER_REQUEST)
        {
          OFHandle_Barrier_Request(buffer);
        }
      if (message->type() == fluid_msg::of13::OFPT_MULTIPART_REQUEST)
       {
    	  OFHandle_Multipart_Request(buffer);
       }
      delete (message);
    }
}

//Handles a packet from a non-controller
bool SdnSwitch13::HandleReadFromNetDevice (Ptr<NetDevice> device, Ptr<const Packet> originalPacket, uint16_t protocol, const Address &source )
{
  NS_LOG_FUNCTION (this << device << originalPacket << protocol << source);

  if(m_controllerConn->get_state() != fluid_base::OFConnection::STATE_RUNNING)
    {
      NS_LOG_WARN ("Received a packet on a switch but we're not in running mode yet! Dropping packet");
      return 0;
    }
  Ptr<Packet> packet = originalPacket->Copy();

  uint32_t inPort = 0;
  for(std::map<uint32_t,Ptr<SdnPort> >::iterator portIterator = m_portMap.begin(); portIterator != m_portMap.end(); ++portIterator)
    {
      if(portIterator->second && portIterator->second->getDevice() == device)
        {
          inPort = portIterator->first;
          break;
        }
    }

  return HandlePacket (packet, inPort);
}

bool SdnSwitch13::HandlePacket (Ptr<Packet>packet, uint32_t inPort)
{
  NS_LOG_FUNCTION (this << packet << inPort);
  fluid_msg::ActionSet *action_set = new fluid_msg::ActionSet ();
  std::vector<uint32_t> outPorts;
  fluid_msg::ActionSet *resulting_action_set = m_flowTable13->handlePacket (packet, action_set, inPort);

  // Process action set instead of sending out port vector (need to send reason along with messages to controller
  outPorts = m_flowTable13->handleActions (packet, resulting_action_set);

  if (!outPorts.empty())
  {
	  HandlePorts (packet, outPorts, inPort);
  }

  return 0;
}

void SdnSwitch13::HandlePorts (Ptr<Packet> packet, std::vector<uint32_t> outPorts, uint32_t inPort)
{
	  for (std::vector<uint32_t>::iterator outPort = outPorts.begin(); outPort != outPorts.end(); ++outPort)
	    {
		  if (*outPort > fluid_msg::of13::OFPP_MAX)
		    {
			  if (*outPort == fluid_msg::of13::OFPP_IN_PORT)
			    {
				  Ptr<SdnPort> outputPort = m_portMap[inPort];
				  outputPort->getConn()->sendOnNetDevice(packet);
			    }
			  else if (*outPort == fluid_msg::of13::OFPP_TABLE)
				{
				  HandlePacket (packet, inPort);
				}
			  else if (*outPort == fluid_msg::of13::OFPP_NORMAL)
			    {
				  // Normal L2/L3 switching?
			    }
			  else if(*outPort == fluid_msg::of13::OFPP_FLOOD)
				{
				  Flood(packet, inPort);
				}
			  else if(*outPort == fluid_msg::of13::OFPP_ALL)
				{
				  OutputAll(packet, inPort);
				}
			  else if (*outPort == fluid_msg::of13::OFPP_CONTROLLER)
			    {
				  // Send to controller with a reason
				  uint8_t reason = (outPort + 1 == outPorts.end() ? fluid_msg::of13::OFPR_NO_MATCH : fluid_msg::of13::OFPR_ACTION);
			      SendPacketInToController(packet, m_portMap[inPort]->getDevice (), reason);
			    }
			  else if (*outPort == fluid_msg::of13::OFPP_LOCAL)
			    {
				  // Local openflow port?
			    }
			  else if (*outPort == fluid_msg::of13::OFPP_ANY)
				{
				  // Should never be used outside flow mods and flow stats requests
				}
			  continue;
		    }
		  if (m_portMap.count(*outPort) != 0)
			{
			  Ptr<SdnPort> outputPort = m_portMap[*outPort]; //Might be wrong?
			  outputPort->getConn()->sendOnNetDevice(packet);
			}
	    }
}

void SdnSwitch13::OFHandle_Hello_Request (fluid_msg::OFMsg* message)
{
  NS_LOG_FUNCTION (this << message);
  fluid_msg::of13::Hello* helloMessage = new fluid_msg::of13::Hello(message->xid());
  if (NegotiateVersion (message))
    {
      m_controllerConn->send (helloMessage);
      m_controllerConn->set_state(fluid_base::OFConnection::STATE_RUNNING);
      return;
    }
  else
    {
      m_controllerConn->set_state(fluid_base::OFConnection::STATE_FAILED);
      fluid_msg::of13::Error* errorMessage =
        new fluid_msg::of13::Error (
          SdnCommon::GenerateXId(),
          fluid_msg::of13::OFPET_HELLO_FAILED,
          fluid_msg::of13::OFPHFC_INCOMPATIBLE);
      m_controllerConn->send(errorMessage);
      return;
    }
}

void SdnSwitch13::OFHandle_Feature_Request (fluid_msg::OFMsg* message)
{
  NS_LOG_FUNCTION (this << message);

  fluid_msg::of13::FeaturesReply* featuresReply =
    new fluid_msg::of13::FeaturesReply (message->xid (),
					((uint64_t)((uint64_t)OF_DATAPATH_ID_PADDING << 48) | GetMacAddress ()),
                                        m_switchFeatures.n_buffers,
                                        m_switchFeatures.n_tables,
										0,
                                        GetCapabilities ());
  m_controllerConn->send (featuresReply);
  return;
}

void SdnSwitch13::OFHandle_Get_Config_Request()
{
  NS_LOG_FUNCTION (this);
  fluid_msg::of10::GetConfigReply* getConfigReply =
    new fluid_msg::of10::GetConfigReply (SdnCommon::GenerateXId(),
                                         0, //Flags of 0 for fragmentation
                                         m_missSendLen);
   m_controllerConn->send (getConfigReply);
}

void SdnSwitch13::OFHandle_Set_Config(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of10::SetConfig* setConfig = new fluid_msg::of10::SetConfig();
  setConfig->unpack(buffer);
  if (setConfig)
    {
      m_missSendLen = setConfig->miss_send_len();
    }
}

void SdnSwitch13::OFHandle_Flow_Mod(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of13::FlowMod* flowMod = new fluid_msg::of13::FlowMod();
  flowMod->unpack(buffer);
  if (flowMod)
    {
      switch (flowMod->command())
        {
          case fluid_msg::of13::OFPFC_ADD:
            addFlow(flowMod);
            break;
          case fluid_msg::of13::OFPFC_MODIFY:
            modifyFlow(flowMod);
            break;
          case fluid_msg::of13::OFPFC_MODIFY_STRICT:
            modifyFlowStrict(flowMod);
            break;
          case fluid_msg::of13::OFPFC_DELETE:
            deleteFlow(flowMod);
            break;
          case fluid_msg::of13::OFPFC_DELETE_STRICT:
            deleteFlowStrict(flowMod);
            break;
        }

		if (flowMod->command () != fluid_msg::of13::OFPFC_DELETE &&
				flowMod->command () != fluid_msg::of13::OFPFC_DELETE_STRICT)
		{
			if ((flowMod->buffer_id () != (uint32_t)(-1)) &&
			(m_packetBuffers.count (flowMod->buffer_id ())))
			{
			  Ptr<Packet> copyPacket = m_packetBuffers[flowMod->buffer_id ()]->Copy ();
			  HandlePacket (copyPacket, flowMod->match ().in_port ()->value ());
			}
		}
    }
}

void SdnSwitch13::OFHandle_Group_Mod(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of13::GroupMod* groupMod = new fluid_msg::of13::GroupMod();
  groupMod->unpack(buffer);
  if (groupMod)
    {
      switch (groupMod->command())
        {
          case fluid_msg::of13::OFPGC_ADD:
            addGroup(groupMod);
            break;
          case fluid_msg::of13::OFPGC_MODIFY:
            modifyGroup(groupMod);
            break;
          case fluid_msg::of13::OFPGC_DELETE:
            deleteGroup(groupMod);
            break;
        }
    }
}

void SdnSwitch13::OFHandle_Multipart_Request(uint8_t* buffer)
{
	  NS_LOG_FUNCTION (this << buffer);
	  fluid_msg::of13::MultipartRequest* multipartRequest = new fluid_msg::of13::MultipartRequest ();
	  multipartRequest->unpack(buffer);
	  if (multipartRequest->mpart_type()==fluid_msg::of13::OFPMP_PORT_DESC)
	  {
		  std::vector<fluid_msg::of13::Port> fluidPorts;
		  for(std::map<uint32_t, Ptr<SdnPort> >::iterator i = m_portMap.begin(); i != m_portMap.end(); ++i)
		  {
			  fluid_msg::of10::Port curPort (i->second->getFluidPort());
			  uint32_t curr_speed =0;
			  uint32_t max_speed=0;
			  std::cout<< curPort.port_no()<<std::endl;
			  fluid_msg::of13::Port makePort (
					  (uint32_t)curPort.port_no(),curPort.hw_addr(), curPort.name(), curPort.config(), curPort.state(),
					  curPort.curr(), curPort.advertised(), curPort.supported(), curPort.peer(), curr_speed, max_speed );
			  std::cout<<makePort.port_no()<<std::endl;
		      fluidPorts.push_back(makePort);
		  }
	      fluid_msg::of13::MultipartReplyPortDescription* multipartReplyPortDesc =
			  new fluid_msg::of13::MultipartReplyPortDescription(multipartRequest ->xid(),multipartRequest->flags(),fluidPorts);
	      m_controllerConn->send (multipartReplyPortDesc);
	  }
}

void SdnSwitch13::addFlow(fluid_msg::of13::FlowMod* message)
{
  NS_LOG_FUNCTION (this << message);
  Flow13 flow = m_flowTable13->addFlow(message);
  
  if(flow.cookie_ != message->cookie())
    {
    
      fluid_msg::of13::Error* errorMessage = new fluid_msg::of13::Error(SdnCommon::GenerateXId(),
	      fluid_msg::of13::OFPET_FLOW_MOD_FAILED,fluid_msg::of13::OFPFMFC_OVERLAP);
      m_controllerConn->send(errorMessage);
      return;
    }
}

void SdnSwitch13::modifyFlow(fluid_msg::of13::FlowMod* message)
{
  NS_LOG_FUNCTION (this << message);
  m_flowTable13->modifyFlow(message);
}

void SdnSwitch13::modifyFlowStrict(fluid_msg::of13::FlowMod* message)
{
// HACK FOR COMPILE
  NS_LOG_FUNCTION (this << message);
  m_flowTable13->modifyFlow(message);
}

void SdnSwitch13::deleteFlow(fluid_msg::of13::FlowMod* message)
{
  NS_LOG_FUNCTION (this << message);
  m_flowTable13->deleteFlow(message);
}

void SdnSwitch13::deleteFlowStrict(fluid_msg::of13::FlowMod* message)
{
// HACK FOR COMPILE
  NS_LOG_FUNCTION (this << message);
  m_flowTable13->deleteFlow(message);
}

void SdnSwitch13::addGroup(fluid_msg::of13::GroupMod* message)
{
  NS_LOG_FUNCTION (this << message);
  Ptr<SdnGroup13> group = m_flowTable13->addGroup(message);

  if(group)
    {

      fluid_msg::of13::Error* errorMessage = new fluid_msg::of13::Error(SdnCommon::GenerateXId(),
	      fluid_msg::of13::OFPET_GROUP_MOD_FAILED,fluid_msg::of13::OFPGMFC_GROUP_EXISTS);
      m_controllerConn->send(errorMessage);
      return;
    }
}

void SdnSwitch13::modifyGroup(fluid_msg::of13::GroupMod* message)
{
  NS_LOG_FUNCTION (this << message);
  m_flowTable13->modifyGroup(message);
}

void SdnSwitch13::deleteGroup(fluid_msg::of13::GroupMod* message)
{
  NS_LOG_FUNCTION (this << message);
  m_flowTable13->deleteGroup(message);
}

void SdnSwitch13::OFHandle_Port_Mod (uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of13::PortMod* portMod = new fluid_msg::of13::PortMod();
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

void SdnSwitch13::OFHandle_Barrier_Request(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of13::BarrierRequest* barrierRequest = new fluid_msg::of13::BarrierRequest ();
  barrierRequest->unpack(buffer);
  //Finish computing all other messages we've queued. Whatever that means for us.
  fluid_msg::of13::BarrierReply* barrierReply = new fluid_msg::of13::BarrierReply(barrierRequest->xid ());
  m_controllerConn->send(barrierReply);
}

void SdnSwitch13::OFHandle_Packet_Out(uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);
  fluid_msg::of13::PacketOut* packetOut = new fluid_msg::of13::PacketOut ();
  fluid_msg::of_error status = packetOut->unpack(buffer);
  NS_ASSERT (status != fluid_msg::OF_ERROR);

  size_t structSize = sizeof(struct fluid_msg::of13::ofp_packet_out);
  uint8_t *testP = buffer + structSize;

  fluid_msg::ActionList testActions;
  testActions.length(packetOut->actions_len());
  testActions.unpack13(testP);

  Ptr<Packet> packet;
  uint32_t inPort = packetOut->in_port();
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
  std::vector<uint32_t> outPorts;
  
  fluid_msg::ActionList action_list = testActions;
  outPorts = m_flowTable13->handleActions (packet, &action_list);

  if (!outPorts.empty())
  {
	  HandlePorts (packet, outPorts, inPort);
  }
}

void SdnSwitch13::SendPacketInToController(Ptr<Packet> packet, Ptr<NetDevice> device, uint8_t reason)
{
  NS_LOG_FUNCTION (this << packet << device << reason);
  Ptr<SdnPort> port;
  for(std::map<uint32_t,Ptr<SdnPort> >::iterator portIterator = m_portMap.begin(); portIterator != m_portMap.end(); ++portIterator)
    {
      if(portIterator->second && portIterator->second->getDevice() == device)
        {
          port = portIterator->second;
          break;
        }
    }
  
  // While the maximum number of buffered packets has not been exceeded, add the
  // packet to the buffer before sending
  fluid_msg::of13::PacketIn* packetIn;
  if (m_packetBuffers.size () <= MAX_BUFFERS)
    {
      uint32_t newId = 0;
      do
	{
	  newId = m_bufferIdStream->GetInteger ();
	} while (m_packetBuffers.count (newId) == 1);
      std::cout<<"newId is "<<newId<<std::endl;

      m_packetBuffers[newId] = packet->Copy ();
      packetIn = new fluid_msg::of13::PacketIn(SdnCommon::GenerateXId(),
    	  newId, packet->GetSize(), reason, 0, 0); // Last 2 are table ID and cookie.
    }
  else
    {
      packetIn = new fluid_msg::of13::PacketIn(SdnCommon::GenerateXId(),
    	  -1, packet->GetSize(), reason, 0, 0);
    }

  m_flowTable13->DestructHeader(packet);
  fluid_msg::of13::Match match = m_flowTable13->getPacketFields((uint32_t)port->getFluidPort().port_no());
  m_flowTable13->RestructHeader(packet);

  for (uint8_t i=0; i<OXM_NUM; ++i){
	  if (match.oxm_field(i))
	    packetIn->add_oxm_field(match.oxm_field(i));

  }

  uint8_t buffer[packet->GetSize ()];
  packet->CopyData (buffer,packet->GetSize ());
  //packetIn->unpack(buffer);
  packetIn->data(buffer,42);

  if (packetIn) m_controllerConn->send(packetIn);
 // delete(packetIn);
}

void SdnSwitch13::SendFlowRemovedMessageToController(Flow13 flow, uint8_t reason)
{
  NS_LOG_FUNCTION (this << reason);
  fluid_msg::of13::FlowRemoved* flowRemoved = new fluid_msg::of13::FlowRemoved(
	  SdnCommon::GenerateXId(), flow.cookie_, flow.priority_, reason, flow.table_id_, flow.duration_sec_,
	  flow.duration_nsec_, flow.idle_timeout_, flow.hard_timeout_, flow.packet_count_, flow.byte_count_);
  m_controllerConn->send(flowRemoved);
}

void SdnSwitch13::SendPortStatusMessageToController(fluid_msg::of13::Port port, uint8_t reason)
{
  NS_LOG_FUNCTION (this << reason);
  fluid_msg::of13::PortStatus* portStatus = new fluid_msg::of13::PortStatus(SdnCommon::GenerateXId(), reason, port);
  m_controllerConn->send(portStatus);
}

void SdnSwitch13::Flood(Ptr<Packet> packet, uint32_t portNum)
{
  NS_LOG_FUNCTION (this << packet << portNum);
 for(std::map<uint32_t,Ptr<SdnPort> >::iterator port = m_portMap.begin(); port != m_portMap.end(); ++port)
  {
    if((port->first != portNum) && (port->second) &&
    		!(port->second->getConfig() & (fluid_msg::of13::OFPPC_NO_FWD | fluid_msg::of13::OFPPC_PORT_DOWN)))
      {
        Ptr<Packet> copyPacket = packet->Copy ();
        port->second->getConn()->sendOnNetDevice(copyPacket);
      }
  }
}

void SdnSwitch13::Flood(Ptr<Packet> packet, Ptr<NetDevice> netDevice)
{
  NS_LOG_FUNCTION (this << packet << netDevice);
  for(std::map<uint32_t,Ptr<SdnPort> >::iterator port = m_portMap.begin(); port != m_portMap.end(); ++port)
    {
      if((port->second) && !(port->second->getConfig() & (fluid_msg::of13::OFPPC_NO_FWD | fluid_msg::of13::OFPPC_PORT_DOWN)) &&
    		  (port->second->getDevice() != netDevice))
        {
          port->second->getConn()->sendOnNetDevice(packet);
        }
    }
}

void SdnSwitch13::OutputAll(Ptr<Packet> packet, uint32_t portNum)
{
  NS_LOG_FUNCTION (this << packet << portNum);
 for(std::map<uint32_t,Ptr<SdnPort> >::iterator port = m_portMap.begin(); port != m_portMap.end(); ++port)
  {
    if((port->first != portNum) && (port->second))
      {
        Ptr<Packet> copyPacket = packet->Copy ();
        port->second->getConn()->sendOnNetDevice(copyPacket);
      }
  }
}

uint64_t SdnSwitch13::GetMacAddress ()
{
  NS_LOG_FUNCTION (this);
  uint8_t buffer[6];
  m_controllerConn->get_device ()->GetAddress ().CopyTo (buffer);
  uint64_t returnWord = 0;
  memcpy (&returnWord,buffer,6);
  return returnWord;
}

uint32_t SdnSwitch13::GetCapabilities ()
{
  NS_LOG_FUNCTION (this);
  return (uint32_t) m_switchFeatures.capabilities.NS3_SWITCH_OFPC_FLOW_STATS          |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_TABLE_STATS    << 1 |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_PORT_STATS     << 2 |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_GROUP_STATS    << 3 |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_IP_REASM       << 5 |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_QUEUE_STATS    << 5 |
         m_switchFeatures.capabilities.NS3_SWITCH_OFPC_PORT_BLOCKED   << 8;
}

bool SdnSwitch13::NegotiateVersion (fluid_msg::OFMsg* msg)
{
  NS_LOG_FUNCTION (this << msg);
  return msg->version () == fluid_msg::of13::OFP_VERSION;
}

uint16_t SdnSwitch13::getNewPortNumber ()
{
  NS_LOG_FUNCTION (this);
  TOTAL_PORTS++;
  return TOTAL_PORTS == OFCONTROLLERPORT ? ++TOTAL_PORTS : TOTAL_PORTS;
}

uint32_t SdnSwitch13::getNewDatapathID ()
{
  ++TOTAL_DATAPATH_IDS;
  return TOTAL_DATAPATH_IDS;
}
}
