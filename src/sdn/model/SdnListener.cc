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

#include "SdnListener.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SdnListener");

NS_OBJECT_ENSURE_REGISTERED (SdnListener);

TypeId
SdnListener::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SdnListener")
    .SetParent<Object> ()
    .AddConstructor<SdnListener> ()
  ;
  return tid;
}

SdnListener::SdnListener ()
{
  NS_LOG_FUNCTION (this);
}

void
SdnListener::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Object::DoDispose ();
}

NS_OBJECT_ENSURE_REGISTERED (BaseLearningSwitch);

TypeId BaseLearningSwitch::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BaseLearningSwitch")
    .SetParent<SdnListener> ()
    .AddConstructor<BaseLearningSwitch> ()
  ;
  return tid;
}

BaseLearningSwitch::BaseLearningSwitch ()
{
  NS_LOG_FUNCTION (this);
}

BaseLearningSwitch::~BaseLearningSwitch ()
{
  NS_LOG_FUNCTION (this);

  while (!l2tables.empty ())
    {
      L2TABLE* l2table = *l2tables.begin ();
      l2tables.erase (l2tables.begin ());
      delete l2table;
    }
}

void
BaseLearningSwitch::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  SdnListener::DoDispose ();
}

L2TABLE*
BaseLearningSwitch::get_l2table (Ptr<SdnConnection> ofconn)
{
  NS_LOG_FUNCTION (this << ofconn);

  L2TABLE* l2table = (L2TABLE*)ofconn->get_application_data ();

  if (l2table == NULL)
    {
      NS_LOG_DEBUG ("l2table for connection id=" << ofconn->get_id() <<
                    " not initialized. Make sure your application is" <<
                    " listening to EVENT_SWITCH_UP.");
    }

  return l2table;
}

void
BaseLearningSwitch::event_callback (ControllerEvent* ev)
{
  NS_LOG_FUNCTION (this << ev);

  if (ev->get_type () == EVENT_SWITCH_UP)
    {
      L2TABLE* l2table = new L2TABLE ();

      ev->ofconn->set_application_data (l2table);
      l2tables.push_back (l2table);
      NS_LOG_INFO("Adding L2 entries for connection id=" << ev->ofconn->get_id());
    }
  else if (ev->get_type () == EVENT_SWITCH_DOWN)
    {
      L2TABLE* l2table = (L2TABLE*) ev->ofconn->get_application_data ();
      if (l2table != NULL)
        {
          l2table->clear ();
          NS_LOG_INFO("Deleting L2 entries for connection id=" << ev->ofconn->get_id());
        }
    }
}

} // end of namespace ns3
