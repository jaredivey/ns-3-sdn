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

#ifndef SDN_LISTENER_H
#define SDN_LISTENER_H

//Sdn Common library
#include "SdnConnection.h"

//NS3 objects
#include "ns3/ptr.h"

//NS3 utilities
#include "ns3/log.h"
#include <fluid/of10msg.hh>
#include <fluid/of13msg.hh>

//C++ libraries
#include <map>

namespace ns3 {

#define EVENT_PACKET_IN    0
#define EVENT_SWITCH_DOWN  1
#define EVENT_SWITCH_UP    2
#define EVENT_FLOW_REMOVED 3
#define EVENT_PORT_STATUS  4
#define EVENT_STATS_REPLY  5

class SdnConnection;

typedef std::map<uint64_t, uint16_t> L2TABLE;

/**
 * \ingroup sdn
 * \defgroup ControllerEvent
 * Generic controller event interface. Contains the SdnConnection that was used to send the event, and the type of event sent
 */
class ControllerEvent
{
public:
  ControllerEvent (Ptr<SdnConnection> ofconn, uint32_t type)
  {
    this->ofconn = ofconn;
    this->type = type;
  }
  virtual ~ControllerEvent ()
  {
  }
  /**
   * \brief Get the type of event
   * \return the event type
   */
  virtual int get_type ()
  {
    return this->type;
  }

  Ptr<SdnConnection> ofconn; //!< SdnConnection used to send event

private:
  int type; //!< Type of event sent
};
/**
 * \ingroup ControllerEvent
 * \defgroup PacketInEvent
 * PacketInEvent. These should be the majority of events handled
 */
class PacketInEvent : public ControllerEvent
{
public:
  PacketInEvent (Ptr<SdnConnection> ofconn, void* data, uint32_t len)
    : ControllerEvent (ofconn, EVENT_PACKET_IN)
  {
    this->data = (uint8_t*) data;
    this->len = len;
  }

  virtual ~PacketInEvent ()
  {
  }

  uint8_t* data; //!< Compact data of PacketInEvent
  size_t len;    //!< Size of data
};

/**
 * \ingroup ControllerEvent
 * \defgroup SwitchUpEvent
 * Switch Up Event. Occur when a switch finishes the handshake with the controller
 */
 
class SwitchUpEvent : public ControllerEvent
{
public:
  SwitchUpEvent (Ptr<SdnConnection> ofconn, void* data, uint32_t len)
    : ControllerEvent (ofconn, EVENT_SWITCH_UP)
  {
    this->data = (uint8_t*) data;
    this->len = len;
  }

  virtual ~SwitchUpEvent ()
  {
  }

  uint8_t* data; //!< Compact data of SwitchUpEvent
  size_t len;    //!< Size of data
};

/**
 * \ingroup ControllerEvent
 * \defgroup SwitchDownEvent
 * Switch Down Event. Occur when a switch is unresponsive, or manually tells the controller to exit
 */
 
class SwitchDownEvent : public ControllerEvent
{
public:
  SwitchDownEvent (Ptr<SdnConnection> ofconn)
    : ControllerEvent (ofconn, EVENT_SWITCH_DOWN)
  {
  }
};
/**
 * \ingroup ControllerEvent
 * \defgroup FlowRemovedEvent
 * Flow Removed Event. Occur when a flow has an idle or a hard time out reached
 */
class FlowRemovedEvent : public ControllerEvent
{
public:
  FlowRemovedEvent (Ptr<SdnConnection> ofconn, void* data, uint32_t len)
    : ControllerEvent (ofconn, EVENT_FLOW_REMOVED)
  {
    this->data = (uint8_t*) data;
    this->len = len;
  }

  virtual ~FlowRemovedEvent ()
  {
  }

  uint8_t* data; //!< Compact data of FlowRemovedEvent
  size_t len;    //!< Size of data
};

/**
 * \ingroup ControllerEvent
 * \defgroup PortStatusEvent
 * Port Status Event. Occur when a port state has changed. Should not occur in most NS3 simulations
 */
class PortStatusEvent : public ControllerEvent
{
public:
  PortStatusEvent (Ptr<SdnConnection> ofconn, void* data, uint32_t len)
    : ControllerEvent (ofconn, EVENT_PORT_STATUS)
  {
    this->data = (uint8_t*) data;
    this->len = len;
  }

  virtual ~PortStatusEvent ()
  {
  }

  uint8_t* data; //!< Compact data of PortStatusEvent
  size_t len;    //!< Size of data
};
/**
 * \ingroup ControllerEvent
 * \defgroup StatsReplyEvent
 * Stats Reply Event. Occur when a stats request has been acknowledged
 */
class StatsReplyEvent : public ControllerEvent
{
public:
  StatsReplyEvent (Ptr<SdnConnection> ofconn, void* data, uint32_t len)
    : ControllerEvent (ofconn, EVENT_STATS_REPLY)
  {
    this->data = (uint8_t*) data;
    this->len = len;
  }

  virtual ~StatsReplyEvent ()
  {
  }

  uint8_t* data; //!< Compact data of StatsReplyEvent
  size_t len;    //!< Size of data
};
/**
 * \ingroup sdn
 * \defgroup SdnListener
 * The major interface between outside behavior and ns3 applications.
 * Every SdnController must have an SdnListener
 * SdnListeners handle ControllerEvents, and those handlers define Controller behavior
 * BaseLearningSwitch extends SdnListener and is used as the default Controller behavior in most examples
 * See sdn/examples for *.hh files that define custom Controllers
 */
class SdnListener : public Object
{
protected:
  virtual void DoDispose (void);

public:
  static TypeId GetTypeId (void);
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  SdnListener (void);
  /**
   * \brief Inheritable function that handles our variety of controller events. The main reason to inherit SdnListener
   */
  virtual void event_callback (ControllerEvent* ev) { }
};
/**
 * \ingroup SdnListener
 * \defgroup BaseLearningSwitch
 * The default SdnListener
 * Most examples extend off of this controller behavior
 * The base learning switch tries to remember all L2 routes as a map of MAC address to portnums
 * No routing actually occurs with the BaseLearningSwitch
 * See sdn/examples for *.hh files that define more concrete custom Controllers
 */
class BaseLearningSwitch : public SdnListener
{
protected:
  virtual void DoDispose (void);

private:
  std::list<L2TABLE*> l2tables; //!< The 

public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  BaseLearningSwitch (void);
  ~BaseLearningSwitch (void);
  /**
   * \brief Get an L2Table based on the SdnConnection we're receiving data from
   * \return A L2Table for any given SdnSwitch
   */
  L2TABLE* get_l2table (Ptr<SdnConnection> ofconn);
  /**
   * \brief SdnListener defined behavior. Create an L2Table when a switch goes up, and remove it when it goes doewn
   */
  virtual void event_callback (ControllerEvent* ev);
};

} // End of namespace ns3

#endif
