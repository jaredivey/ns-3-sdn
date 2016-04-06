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

// Model definition of a flow. Imported from libfluid to customize for our use

#ifndef FLOW13_H
#define FLOW13_H


//Libfluid classes
#include <fluid/of13/of13common.hh>
#include <fluid/of13/of13match.hh>
#include <fluid/of13/of13action.hh>

//NS3 Classes
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/event-id.h"

using namespace ns3;

#define NANOTOSECS 1000000000

/**
 * \ingroup sdn
 * \defgroup Flow
 * 
 * \brief Internal Model representing the Flow rule
 *
 * Provides the basic components (Matches, Actions, masks, etc.) for defining
 * a Flow rule
 */
class Flow13
{
public:
 
/**
 * \brief Constructor
 *
 */
  Flow13 ()
  : nw_src_mask (0xffffffff),
    nw_dst_mask (0xffffffff),
    match (),
    actions ()
  {
    install_time_nsec = Simulator::Now().GetNanoSeconds();
  }
  /**
  * \brief Grabs the total time alive in seconds
  * \return a uint64 representation of time
  */
  uint64_t getDurationSec();
  /**
  * \brief Grabs the total time alive in nanoseconds
  * \return a uint64 representation of time
  */
  uint64_t getDurationNSec();
  /**
  * \brief Creates a flowstats object from the flow
  * \return Pointer to a FlowStats object. Defined in libfluid library
  */
  fluid_msg::of13::FlowStats* convertToFlowStats(); //Conversion function to make stats
  /**
  * \brief Non-Strict match on a packet
  * \return boolean match
  */
  static bool pkt_match (const Flow13& flow, const fluid_msg::of13::Match &pkt_match);
  /**
  * \brief Strict match on a packet
  * \return boolean match
  */
  static bool pkt_match_strict (const Flow13& flow, const fluid_msg::of13::Match &pkt_match);
  /**
  * \brief Non-Strict match on another flow
  * \return boolean match
  */
  static bool non_strict_match (const Flow13& flow_a, const Flow13& flow_b);
  /**
  * \brief Strict match on another flow
  * \return boolean match
  */
  static bool strict_match (const Flow13& flow_a, const Flow13& flow_b);

  uint16_t length_;              //!< Total length in bytes
  uint8_t  table_id_;            //!< Id of containing flow table
  uint64_t duration_sec_;        //!< Seconds component of the duration to live
  uint64_t duration_nsec_;       //!< NanoSeconds component of the duration to live
  uint64_t install_time_nsec;    //!< Simulator time the flow is install in nanoseconds
  uint16_t priority_;            //!< Priority level within the flow table for rule resolution
  uint16_t flags_;               //!< Flags
  uint16_t idle_timeout_;        //!< Inactivity timeout in seconds
  uint16_t hard_timeout_;        //!< Hard garenteed timeout in seconds
  uint64_t cookie_;              //!< A controller specified cookie unique per each flow
  uint64_t packet_count_;        //!< A count of packets handled by this flow
  uint64_t byte_count_;          //!< A count of bytes in the packets handled by this flow
  uint32_t nw_src_mask;          //!< A subnet mask of source IP addresses. Specified as 111 ----- 100
  uint32_t nw_dst_mask;          //!< A subnet mask of destination IP addresses. Specified as 111 ----- 100
  EventId idle_timeout_event;    //!< An NS3 event to fire a timeout if the idle time is reached
  EventId hard_timeout_event;    //!< An NS3 event to fire a timeout if the hard time is reached
  fluid_msg::of13::Match match;  //!< A libfluid match object of packets features we match
  fluid_msg::of13::InstructionSet instructions;
  fluid_msg::ActionList actions; //!< A libfluid ActionList (vector of actions) to apply to a packet. Only applies if the match is correct
  

};
#endif /* FLOW13_H */
