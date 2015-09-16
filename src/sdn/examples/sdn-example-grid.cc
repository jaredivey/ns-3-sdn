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
 */

/* Network topology
 *
 *  h00      h01            h0n
 *    \       |             /
 *     s00---s01---...---s0n
 *      |     |           |
 * h10-s10---s11---...---s1n-h1n
 *      |     |           |
 *      .     .           .
 *      .     .           .
 *      .     .           .
 *      |     |           |
 *     sn0---sn1---...---snn
 *    /       |             \
 *  hn0      hn1            hnn
 *
 */
#include <string>
#include <fstream>
#include <vector>
#include <sys/time.h>
#include <sys/types.h>

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/layer2-p2p-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

#include "ns3/SdnController.h"
#include "ns3/SdnSwitch.h"
#include "ns3/SdnListener.h"

#include "MsgApps.hh"
#include "FirewallApps.hh"
#include "STPApps.hh"

using namespace ns3;

#define PING_DELAY_TIME 10
#define REALLY_BIG_TIME 1000000
#define SIMULATOR_STOP_TIME 100000

typedef struct timeval TIMER_TYPE;
#define TIMER_NOW(_t) gettimeofday (&_t,NULL);
#define TIMER_SECONDS(_t) ((double)(_t).tv_sec + (_t).tv_usec * 1e-6)
#define TIMER_DIFF(_t1, _t2) (TIMER_SECONDS (_t1) - TIMER_SECONDS (_t2))

NS_LOG_COMPONENT_DEFINE ("sdn-example-dumbbell");

enum APPCHOICE
{
  PING,
  ON_OFF,
} APPCHOICE;

enum ControllerApplication
{
  STP_APPS,
  MSG_APPS,
} ControllerApplication;

int
main (int argc, char *argv[])
{
  bool tracing = false;
  uint32_t maxBytes = 0;
  bool verbose = false;
  uint32_t appChoice = PING;
  uint32_t controllerApplication = STP_APPS;

  uint32_t numHosts    = 1;
  uint32_t numGrid     = 2;

  const std::string animFile = "grid.xml";

  std::ostringstream oss;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("appChoice",
                "Application to use: (0) Ping; (1) On Off", appChoice);
  cmd.AddValue ("controllerApplication",
                "Controller application that defined behavior: (0) MsgApps; (1) STPApps", controllerApplication);
  cmd.AddValue ("numGrid", "Length/Width of Grid", numGrid);
  cmd.AddValue ("numHosts", "Number of hosts per edge switch (currently assumes only 1", numHosts);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("sdn-example-grid", LOG_LEVEL_INFO);
      
      LogComponentEnable ("SdnController", LOG_LEVEL_INFO);
      LogComponentEnable ("SdnSwitch", LOG_LEVEL_INFO);
      LogComponentEnable ("SdnListener", LOG_LEVEL_INFO);
      LogComponentEnable ("SdnConnection", LOG_LEVEL_INFO);
      LogComponentEnable ("SdnFlowTable", LOG_LEVEL_DEBUG);
      LogComponentEnable ("SdnFlowTable", LOG_LEVEL_DEBUG);

      LogComponentEnable ("CsmaNetDevice", LOG_LEVEL_INFO);

      LogComponentEnable ("V4Ping", LOG_LEVEL_INFO);
      LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
      
      LogComponentEnable("STPApps", LOG_LEVEL_INFO);
    }

  TIMER_TYPE t0, t1, t2;
  TIMER_NOW (t0);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer controllerNode, switchNodes, edgeNodes;
  controllerNode.Create (1);
  switchNodes.Create (numGrid*numGrid);
  edgeNodes.Create (1*4*(numGrid-1)); // Only one host per edge switch

  NS_LOG_INFO ("Create channels.");
  Layer2P2PHelper layer2P2P;
  layer2P2P.SetChannelAttribute ("DataRate", DataRateValue (10000000));
  layer2P2P.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (5)));
  
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  std::vector<NetDeviceContainer> edgeHostToSwitchContainers;
  std::vector<NetDeviceContainer> switchToSwitchContainers;
  std::vector<NetDeviceContainer> switchToControllerContainers;
  
  // Host to Switch connections
  uint32_t n = 0;
  for (uint32_t i = 0; i < numGrid; ++i)
    {
      for (uint32_t j = 0; j < numGrid; ++j)
        {
          // Only install hosts on edges.
          if (i == 0 || i == numGrid - 1)
            {
              edgeHostToSwitchContainers.push_back (layer2P2P.Install (
                                      NodeContainer(edgeNodes.Get (n),
                                                    switchNodes.Get (i*numGrid + j))));
              ++n;
            }
          else if (j == 0 || j == numGrid - 1)
            {
              edgeHostToSwitchContainers.push_back (layer2P2P.Install (
                                      NodeContainer(edgeNodes.Get (n),
                                                    switchNodes.Get (i*numGrid + j))));
              ++n;
            }
        }
    }

  // Switch to Switch connections
  for (uint32_t i = 0; i < numGrid; ++i)
    {
      for (uint32_t j = 0; j < numGrid; ++j)
         {
           uint32_t switchOffset = i * numGrid + j;
           if (i > 0)
             {
               switchToSwitchContainers.push_back (layer2P2P.Install (
                                     NodeContainer(switchNodes.Get (switchOffset - numGrid),
                                                   switchNodes.Get (switchOffset))));
             }
           if (j > 0)
             {
               switchToSwitchContainers.push_back (layer2P2P.Install (
                                     NodeContainer(switchNodes.Get (switchOffset - 1),
                                                   switchNodes.Get (switchOffset))));
             }
         }
    }
  
  // Switch to Controller connections
  for (uint32_t j = 0; j < numGrid*numGrid; ++j)
     {
       switchToControllerContainers.push_back (pointToPoint.Install (
                                 NodeContainer(switchNodes.Get (j),
                                               controllerNode.Get (0))));

     }
//
// Install the internet stack on the nodes
//
  InternetStackHelper internet;
  internet.Install (edgeNodes);
  internet.Install (switchNodes);
  internet.Install (controllerNode);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> edgeHostToSwitchIPContainers;

  oss.str ("");
  oss << "10.0.0.0";
  ipv4.SetBase (oss.str ().c_str (), "255.255.255.0");

  for (uint32_t i=0; i < edgeHostToSwitchContainers.size(); ++i)
    {
      edgeHostToSwitchIPContainers.push_back(ipv4.Assign (edgeHostToSwitchContainers[i]));

      NS_LOG_INFO ("Host " << i << " address: " << edgeHostToSwitchIPContainers[i].GetAddress(0));
    }

  std::vector<Ipv4InterfaceContainer> switchToSwitchIPContainers;
  for (uint32_t i=0; i < switchToSwitchContainers.size(); ++i)
    {
      switchToSwitchIPContainers.push_back(ipv4.Assign (switchToSwitchContainers[i]));
    }

  std::vector<Ipv4InterfaceContainer> switchToControllerIPContainers;
  for (uint32_t i=0; i < switchToControllerContainers.size(); ++i)
    {
      oss.str ("");
      oss << "192.168." << i+1 << ".0";
      ipv4.SetBase (oss.str ().c_str (), "255.255.255.0");
      switchToControllerIPContainers.push_back(ipv4.Assign (switchToControllerContainers[i]));
    }

  NS_LOG_INFO ("Create Applications.");
  //Create the Application
  ApplicationContainer sourceApps;
  uint16_t port = 0;
  //Ping example
  Ptr<UniformRandomVariable> randTime = CreateObject<UniformRandomVariable>();
  randTime->SetAttribute ("Min", DoubleValue(numGrid*2));
  randTime->SetAttribute ("Max", DoubleValue(numGrid*2+1*2)); // Assumes one host per edge switch

  if (appChoice == PING)
    {
      for (uint32_t i = 0; i < edgeNodes.GetN (); ++i)
        {
          for (uint32_t j = 0; j < edgeNodes.GetN(); ++j)
            {
              if (i != j)
                {
                  NS_LOG_INFO ("PING app to address: " << edgeHostToSwitchIPContainers[j].GetAddress(0));
                  V4PingHelper source1 (edgeHostToSwitchIPContainers[j].GetAddress(0));
                  source1.SetAttribute ("Verbose", BooleanValue (false));
                  source1.SetAttribute ("PingAll", BooleanValue (true));
                  source1.SetAttribute ("Count", UintegerValue (2));

                  ApplicationContainer sourceApp;
                  sourceApp = source1.Install (edgeNodes.Get (i));
                  if ((i == 0) && (j == 1))
                    {
                      sourceApp.Start (Seconds (PING_DELAY_TIME));
                      sourceApps.Add(sourceApp);
                    }
                  else
                    {
                      sourceApp.Start (Seconds (REALLY_BIG_TIME));
                      sourceApps.Add(sourceApp);
                    }
                }
            }
        }
    }
  //ON-OFF example
  else if (appChoice == ON_OFF)
    {
      ApplicationContainer sourceApp;
      port = 50000;

      OnOffHelper source ("ns3::TcpSocketFactory",
                          InetSocketAddress (edgeHostToSwitchIPContainers[numGrid*numGrid-1].GetAddress(0), port));
      source.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      source.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      source.SetAttribute ("MaxBytes", UintegerValue (51200));

      sourceApp = source.Install (edgeNodes.Get (0));
      sourceApp.Start (Seconds (randTime->GetValue()));
      sourceApps.Add(sourceApp);
    }

//
// Create a PacketSinkApplication and install it on host nodes
//
  ApplicationContainer sinkApps;
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  for (uint32_t i = 0; i < edgeNodes.GetN (); ++i)
    {
      sinkApps.Add(sink.Install (edgeNodes.Get (i)));
    }
  sinkApps.Start (Seconds (0.0));

//
// Install Controller.
//
  Ptr<SdnListener> sdnListener;
  if(controllerApplication == STP_APPS)
    {
      sdnListener = CreateObject<STPApps> ();
    }
  if(controllerApplication == MSG_APPS)
    {
      sdnListener = CreateObject<MultiLearningSwitch> ();
    }
  //  Ptr<MultiLearningSwitch> sdnListener = CreateObject<MultiLearningSwitch> ();
  //  Ptr<Firewall> sdnListener = CreateObject<Firewall> (firewallText);
  Ptr<SdnController> sdnC = CreateObject<SdnController> (sdnListener);
  sdnC->SetStartTime (Seconds (0.0));
  controllerNode.Get(0)->AddApplication (sdnC);
  
//
// Install Switch. 
//
  ApplicationContainer sdnSwitchApps;
  for (uint32_t i = 0; i < switchNodes.GetN (); ++i)
    {
      Ptr<SdnSwitch> sdnS = CreateObject<SdnSwitch> ();
      sdnS->SetStartTime (Seconds (0.0));
      switchNodes.Get (i)->AddApplication (sdnS);
      sdnSwitchApps.Add(sdnS);
    }

//
// Set up tracing if enabled
//
  if (tracing)
    {
      AsciiTraceHelper ascii;
      pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("sdn-example-grid.tr"));
    }

  MobilityHelper mobility;

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (controllerNode);
  mobility.Install (switchNodes);
  mobility.Install (edgeNodes);

  AnimationInterface anim (animFile);

  anim.SetConstantPosition (controllerNode.Get(0), 150, 0);
  double mainOffset = 97.5;
  double minorOffset = mainOffset / 3.0;
  for (uint32_t i = 0; i < numGrid; ++i)
    {
      for (uint32_t j = 0; j < numGrid; ++j)
        {
          double xPos = minorOffset*(double)i + mainOffset*j/((double)numGrid - 1.0);
          double yPos = mainOffset*(double)(i + 1)/((double)numGrid - 1.0);
          anim.SetConstantPosition (switchNodes.Get(i*numGrid + j),
                                    xPos,
                                    yPos);
        }
    }
//
// Set netanim node positions
//
  n = 0;
  for (uint32_t i = 0; i < numGrid; ++i)
    {
      for (uint32_t j = 0; j < numGrid; ++j)
        {
          double xPos = 0.0;
          double yPos = 0.0;
          if (i == 0)
            {
              xPos = minorOffset*(double)i + mainOffset*j/((double)numGrid - 1.0);
              yPos = 0.0;
              anim.SetConstantPosition(edgeNodes.Get(n),xPos,yPos);
              ++n;
            }
          else if (i == numGrid - 1)
            {
              xPos = minorOffset*(double)i + mainOffset*j/((double)numGrid - 1.0);
              yPos = mainOffset*(double)(numGrid + 1)/((double)numGrid - 1.0);
              anim.SetConstantPosition(edgeNodes.Get(n),xPos,yPos);
              ++n;
            }
          else if (j == 0)
            {
              xPos = minorOffset*(double)i + mainOffset*j/((double)numGrid - 1.0) - minorOffset;
              yPos = mainOffset*(double)(i + 1)/((double)numGrid - 1.0);
              anim.SetConstantPosition(edgeNodes.Get(n),xPos,yPos);
              ++n;
            }
          else if (j == numGrid - 1)
            {
              xPos = minorOffset*(double)i + mainOffset*numGrid/((double)numGrid - 1.0);
              yPos = mainOffset*(double)(i + 1)/((double)numGrid - 1.0);
              anim.SetConstantPosition(edgeNodes.Get(n),xPos,yPos);
              ++n;
            }
        }
    }

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (SIMULATOR_STOP_TIME));
  TIMER_NOW (t1);
  Simulator::Run ();
  TIMER_NOW (t2);
//
// Grab the stats
//
  uint32_t totalRx = 0;
  for (uint32_t i = 0; i < sinkApps.GetN(); ++i)
    {
      Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (i));
      if (sink1)
        {
          totalRx += sink1->GetTotalRx();
        }
    }

  double simTime = Simulator::Now().GetSeconds();
  double d1 = TIMER_DIFF (t1, t0) + TIMER_DIFF (t2, t1);
  uint64_t initializationTime = DynamicCast<STPApps>(sdnListener)->getInitializationTime();
  uint32_t totalDropped = (V4Ping::m_totalSend - V4Ping::m_totalRecv);
  std::cout << "Grid Size: "              << numGrid                          << "\t"
            << "NumHosts: "               << numHosts                         << "\t"
            << "simTime: "                << simTime                          << "\t"
            << "TimeDiff: "               << d1                               << "\t"
            << "TotalRx: "                << totalRx                          << "\t"
            << "TotalDropped: "           << totalDropped                     << "\t"
            << "TotalSent: "              << V4Ping::m_totalSend              << "\t"
            << "TotalReceive: "           << V4Ping::m_totalRecv              << "\t"
            << "initializationDuration: " << initializationTime               << "\t"
            << std::endl;

  Simulator::Destroy ();
  return 0;
}
