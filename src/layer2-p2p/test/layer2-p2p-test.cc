#include "ns3/test.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/simulator.h"
#include "ns3/layer2-p2p-net-device.h"
#include "ns3/layer2-p2p-channel.h"

using namespace ns3;

/**
 * \brief Test class for Layer2P2P model
 *
 * It tries to send one packet from one NetDevice to another, over a
 * Layer2P2PChannel.
 */
class Layer2P2PTest : public TestCase
{
public:
  /**
   * \brief Create the test
   */
  Layer2P2PTest ();

  /**
   * \brief Run the test
   */
  virtual void DoRun (void);

private:
  /**
   * \brief Send one packet to the device specified
   *
   * \param device NetDevice to send to
   */
  void SendOnePacket (Ptr<Layer2P2PNetDevice> device);
};

Layer2P2PTest::Layer2P2PTest ()
  : TestCase ("Layer2P2P")
{
}

void
Layer2P2PTest::SendOnePacket (Ptr<Layer2P2PNetDevice> device)
{
  Ptr<Packet> p = Create<Packet> ();
  device->Send (p, device->GetBroadcast (), 0x800);
}


void
Layer2P2PTest::DoRun (void)
{
  Ptr<Node> a = CreateObject<Node> ();
  Ptr<Node> b = CreateObject<Node> ();
  Ptr<Layer2P2PNetDevice> devA = CreateObject<Layer2P2PNetDevice> ();
  Ptr<Layer2P2PNetDevice> devB = CreateObject<Layer2P2PNetDevice> ();
  Ptr<Layer2P2PChannel> channel = CreateObject<Layer2P2PChannel> ();

  devA->Attach (channel);
  devA->SetAddress (Mac48Address::Allocate ());
  devA->SetQueue (CreateObject<DropTailQueue> ());
  devB->Attach (channel);
  devB->SetAddress (Mac48Address::Allocate ());
  devB->SetQueue (CreateObject<DropTailQueue> ());

  a->AddDevice (devA);
  b->AddDevice (devB);

  Simulator::Schedule (Seconds (1.0), &Layer2P2PTest::SendOnePacket, this, devA);

  Simulator::Run ();

  Simulator::Destroy ();
}

/**
 * \brief TestSuite for Layer2P2P module
 */
class Layer2P2PTestSuite : public TestSuite
{
public:
  /**
   * \brief Constructor
   */
  Layer2P2PTestSuite ();
};

Layer2P2PTestSuite::Layer2P2PTestSuite ()
  : TestSuite ("devices-point-to-point", UNIT)
{
  AddTestCase (new Layer2P2PTest, TestCase::QUICK);
}

static Layer2P2PTestSuite g_pointToPointTestSuite; //!< The testsuite
