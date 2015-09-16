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

#include "SdnController.h"
#include "ns3/mpi-interface.h"

#define MAX_CONNECTION_DELAY 3
#define SWITCH_RAND_VARIATION_LIMITER 1000
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SdnController");

NS_OBJECT_ENSURE_REGISTERED (SdnController);

TypeId
SdnController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SdnController")
    .SetParent<Application> ()
    .AddConstructor<SdnController> ()
  ;
  return tid;
}

SdnController::SdnController (Ptr<SdnListener> listener)
: ofsc (fluid_base::OFServerSettings ()),
  event_listener (listener)
{
  NS_LOG_FUNCTION (this);
}

SdnController::SdnController ()
: ofsc (fluid_base::OFServerSettings ())
{
  NS_LOG_FUNCTION (this);
  event_listener = CreateObject<BaseLearningSwitch> ();
}

SdnController::~SdnController ()
{
  NS_LOG_FUNCTION (this);
}

void
SdnController::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  if (MpiInterface::IsEnabled() && MpiInterface::GetSystemId () != GetNode ()->GetSystemId ())
    {
      std::cerr << MpiInterface::GetSystemId () << " " << GetNode ()->GetSystemId ()
	      << " Not my system ID so no dispose yet" << std::endl;
      return;
    }
  Application::DoDispose ();
}

void
SdnController::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  //Create a listening socket for this application.
  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  Ptr<Socket> socket = Socket::CreateSocket (GetNode (), tid);

  Address a = InetSocketAddress (Ipv4Address::GetAny (), OFCONTROLLERPORT);
  NS_ASSERT (socket->Bind (a) == 0);

  socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&SdnController::HandleAccept, this));
  socket->SetCloseCallbacks (
    MakeCallback (&SdnController::HandlePeerClose, this),
    MakeCallback (&SdnController::HandlePeerError, this));
  NS_ASSERT (socket->Listen() == 0);
}

void
SdnController::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
}

void
SdnController::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  if (InetSocketAddress::IsMatchingType (from))
    {
      s->SetRecvCallback (MakeCallback (&SdnController::HandleRead, this));

      Ptr<SdnConnection> c = CreateObject<SdnConnection> (s);
      m_switchMap[s] = c;

      m_switchMap[s]->set_state (fluid_base::OFConnection::STATE_HANDSHAKE);
      fluid_msg::of10::Hello* helloMessage = new fluid_msg::of10::Hello (SdnCommon::GenerateXId());
      if (helloMessage)
      	{
	  NS_LOG_INFO ("Controller sending Hello message");
	  m_switchMap[s]->send (helloMessage);
	  NS_LOG_INFO (Simulator::Now().GetSeconds() << " Connection accepted");
      	}
    }
}

void SdnController::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_INFO (Simulator::Now().GetSeconds() << " Peer closed");
}

void SdnController::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_INFO (Simulator::Now().GetSeconds() << " Peer error " << socket->GetErrno ());
}

void
SdnController::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  Ptr<SdnConnection> c = m_switchMap[socket];
  while ((packet = socket->RecvFrom (from)))
    {
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO (Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }

      packet->RemoveAllPacketTags ();
      packet->RemoveAllByteTags ();

      NS_LOG_DEBUG ("Controller recv " << packet->GetSize () << " bytes");

      uint8_t buffer[packet->GetSize ()];
      packet->CopyData (buffer,packet->GetSize ());
      fluid_msg::OFMsg* message = new fluid_msg::OFMsg (buffer);
      if (c->get_state () == fluid_base::OFConnection::STATE_HANDSHAKE)
        {
          if (ofsc.handshake () && message->type () == fluid_msg::of10::OFPT_HELLO)
            {
              fluid_msg::of10::Hello* helloMessage = new fluid_msg::of10::Hello ();
              helloMessage->unpack(buffer);
              
              if (OFHandle_Hello_Reply (socket, message)) return;
            }
          else if (message->type () == fluid_msg::of10::OFPT_FEATURES_REPLY)
            {
              fluid_msg::of10::FeaturesReply* featuresReply = new fluid_msg::of10::FeaturesReply ();
              featuresReply->unpack(buffer);
              OFHandle_Features_Reply (socket, featuresReply);

              // With the connection established, report SwitchUpEvent to the SdnListener.
              std::cout << Simulator::Now ().GetSeconds () << " SWITCH_UP_EVENT" << std::endl;
              SwitchUpEvent* sue = new SwitchUpEvent (c, (void*)featuresReply->pack(), featuresReply->length());
              Simulator::ScheduleNow ( &SdnListener::event_callback, event_listener, sue);
            }
          else
            {
              if ( OFHandle_Errors (socket,
                                    message->xid(),
                                    fluid_msg::of10::OFPET_HELLO_FAILED,
                                    fluid_msg::of10::OFPHFC_INCOMPATIBLE) ) return;
            }
        }
      else if (c->get_state () == fluid_base::OFConnection::STATE_RUNNING)
        {
          if (message->type () == fluid_msg::of10::OFPT_PACKET_IN)
            {
              fluid_msg::of10::PacketIn* packetIn = new fluid_msg::of10::PacketIn ();
              packetIn->unpack(buffer);
              PacketInEvent* pie = new PacketInEvent (c, (void*)packetIn->pack(), packetIn->length());
              Simulator::ScheduleNow ( &SdnListener::event_callback, event_listener, pie);
            }
          else if (message->type () == fluid_msg::of10::OFPT_FLOW_REMOVED)
            {
              fluid_msg::of10::FlowRemoved* flowRemoved = new fluid_msg::of10::FlowRemoved ();
              flowRemoved->unpack(buffer);
              FlowRemovedEvent* fre = new FlowRemovedEvent (c, (void*)flowRemoved->pack(), flowRemoved->length());
              Simulator::ScheduleNow ( &SdnListener::event_callback, event_listener, fre);
            }
          else if (message->type () == fluid_msg::of10::OFPT_PORT_STATUS)
            {
              fluid_msg::of10::PortStatus* portStat = new fluid_msg::of10::PortStatus ();
              portStat->unpack(buffer);
              PortStatusEvent* pse = new PortStatusEvent (c, (void*)portStat->pack(), portStat->length());
              Simulator::ScheduleNow ( &SdnListener::event_callback, event_listener, pse);
            }
          else if (message->type () == fluid_msg::of10::OFPT_STATS_REPLY)
            {
              fluid_msg::of10::StatsReply* statsReply = new fluid_msg::of10::StatsReply ();
              statsReply->unpack(buffer);
              StatsReplyEvent* sre = new StatsReplyEvent (c, (void*)statsReply->pack(), statsReply->length());
              Simulator::ScheduleNow ( &SdnListener::event_callback, event_listener, sre);
            }
        }
      else
        {
          c->set_state (fluid_base::OFConnection::STATE_DOWN);
        }
      delete (message);
    }
}

int
SdnController::OFHandle_Hello_Reply (Ptr<Socket> socket, fluid_msg::OFMsg* message)
{
  NS_LOG_FUNCTION (this << socket << message);

  Ptr<SdnConnection> c = m_switchMap[socket];
  fluid_msg::of10::Hello* helloMessage = static_cast<fluid_msg::of10::Hello*> (message);

  if (helloMessage && NegotiateVersion (helloMessage))
    {
      fluid_msg::of10::FeaturesRequest* featureRequest = new fluid_msg::of10::FeaturesRequest (SdnCommon::GenerateXId());

      if (featureRequest)
        {
          return c->send(featureRequest);
        }
    }
  return 0;
}

void
SdnController::OFHandle_Features_Reply (Ptr<Socket> socket, fluid_msg::OFMsg* message)
{
  NS_LOG_FUNCTION (this << socket << message);

  Ptr<SdnConnection> c = m_switchMap[socket];

  c->set_version (message->version ());
  c->set_state (fluid_base::OFConnection::STATE_RUNNING);
  if (ofsc.liveness_check ())
    {
      // Schedule echo request; format: c->add_timed_callback (send_echo, ofsc.echo_interval() * 1000);
    }

  NS_LOG_DEBUG ("Connection id=" << c->get_id () << " established");
}

int
SdnController::OFHandle_Errors (Ptr<Socket> socket, int xid, uint16_t err_type, uint16_t code)
{
  NS_LOG_FUNCTION (this << socket << err_type << code);

  Ptr<SdnConnection> c = m_switchMap[socket];

  c->set_state (fluid_base::OFConnection::STATE_FAILED);
  fluid_msg::of10::Error* errorResponse = new fluid_msg::of10::Error (xid,
                                                                      err_type,
                                                                      code);
  if (errorResponse)
    {
      return c->send(errorResponse);
    }
  return 0;
}

bool
SdnController::NegotiateVersion (fluid_msg::OFMsg* msg)
{
  NS_LOG_FUNCTION (this << msg);

  return msg->version () == fluid_msg::of10::OFP_VERSION;
}

} // ns3 namespace
