#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <iomanip>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Dumbbell");

// The times
double global_start_time;
double global_stop_time;
double sink_start_time;
double sink_stop_time;
double client_start_time;
double client_stop_time;

int
main (int argc, char *argv[])
{

  uint32_t redTest = 1;
  global_start_time = 0.0;
  global_stop_time = 320.0; 
  sink_start_time = global_start_time;
  sink_stop_time = global_stop_time + 3.0;
  client_start_time = sink_start_time + 0.2;
  client_stop_time = global_stop_time - 2.0;

  std::string animFile = "dumbbell-animation.xml" ;  // Name of file for animation output

  uint32_t    nLeaf = 30; 
  uint32_t    maxPackets = 100;
  uint32_t    modeBytes  = 0;
  uint32_t    queueDiscLimitPackets = 120;
  double      minTh = 30;
  double      maxTh = 90;
  uint32_t    pktSize = 1024;
  std::string appDataRate = "10Mbps";
  std::string queueDiscType = "FIFO";
  std::string bottleNeckLinkBw = "1Mbps";
  std::string bottleNeckLinkDelay = "100ms";

  CommandLine cmd;
  cmd.AddValue ("testNumber", "Run test 1", redTest);
  cmd.AddValue ("nLeaf",     "Number of left and right side leaf nodes", nLeaf);
  cmd.AddValue ("maxPackets","Max Packets allowed in the device queue", maxPackets);
  cmd.AddValue ("queueDiscLimitPackets","Max Packets allowed in the queue disc", queueDiscLimitPackets);
  cmd.AddValue ("queueDiscType", "Set QueueDisc type to FIFO", queueDiscType);
  cmd.AddValue ("appPktSize", "Set OnOff App Packet Size", pktSize);
  cmd.AddValue ("appDataRate", "Set OnOff App DataRate", appDataRate);
  cmd.AddValue ("modeBytes", "Set QueueDisc mode to Packets <0> or bytes <1>", modeBytes);

  cmd.AddValue ("redMinTh", "RED queue minimum threshold", minTh);
  cmd.AddValue ("redMaxTh", "RED queue maximum threshold", maxTh);
  cmd.Parse (argc,argv);

  //Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (pktSize));
  //Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (appDataRate));
  Config::SetDefault ("ns3::QueueBase::MaxSize", StringValue (std::to_string (maxPackets) + "p"));
  //Config::SetDefault ("ns3:QueueBase::MaxSize", StringValue ("100p"));
  //Config::SetDefault ("ns3::QueueBase::Mode", StringValue ("QUEUE_MODE_PACKETS"));
  //Config::SetDefault ("ns3::QueueBase::MaxPackets", UintegerValue (maxPackets));
  
    if (!modeBytes)
    {
      Config::SetDefault ("ns3::PfifoFastQueueDisc::MaxSize",
                          QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueDiscLimitPackets)));
    }


  // Create the point-to-point link helpers
  PointToPointHelper bottleNeckLink;
  bottleNeckLink.SetDeviceAttribute  ("DataRate", StringValue (bottleNeckLinkBw));
  bottleNeckLink.SetChannelAttribute ("Delay", StringValue (bottleNeckLinkDelay));

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("10Mbps"));
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("10ms"));

  PointToPointDumbbellHelper d (nLeaf, pointToPointLeaf,
                                nLeaf, pointToPointLeaf,
                                bottleNeckLink);

  // Install Stack
  InternetStackHelper stack;
  for (uint32_t i = 0; i < d.LeftCount (); ++i)
    {
      stack.Install (d.GetLeft (i));
    }
  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      stack.Install (d.GetRight (i));
    }

  stack.Install (d.GetLeft ());
  stack.Install (d.GetRight ());
  TrafficControlHelper tchBottleneck;
  tchBottleneck.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  QueueDiscContainer queueDiscs =  tchBottleneck.Install (d.GetLeft ()->GetDevice (0));
  tchBottleneck.Install (d.GetRight ()->GetDevice (0));

  // Assign IP Addresses
  d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

  // Install on/off app on all right side nodes
  uint16_t port = 5001;
  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", Address ());
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps;
  /*for (uint32_t i = 0; i < d.LeftCount (); ++i)
    {
      sinkApps.Add (packetSinkHelper.Install (d.GetLeft (i)));
    }*/
  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      sinkApps.Add (packetSinkHelper.Install (d.GetRight (i)));
    }
  sinkApps.Start (Seconds (sink_start_time));
  sinkApps.Stop (Seconds (sink_stop_time));

  ApplicationContainer clientApps;
  /*for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      // Create an on/off app sending packets to the left side
      AddressValue remoteAddress (InetSocketAddress (d.GetLeftIpv4Address (i), port));
      clientHelper.SetAttribute ("Remote", remoteAddress);
      clientApps.Add (clientHelper.Install (d.GetRight (i)));
    }*/
  for (uint32_t i = 0; i < d.LeftCount (); ++i)
    {
      // Create an on/off app sending packets to the Right side
      AddressValue remoteAddress (InetSocketAddress (d.GetRightIpv4Address (i), port));
      clientHelper.SetAttribute ("Remote", remoteAddress);
      clientApps.Add (clientHelper.Install (d.GetLeft (i)));
    }
  clientApps.Start (Seconds (client_start_time));
  clientApps.Stop (Seconds (client_stop_time));

  // Set the bounding box for animation
  //d.BoundingBox (1, 1, 100, 100);

  // Create the animation object and configure for specified output
  //AnimationInterface anim (animFile);
  //anim.EnablePacketMetadata (); // Optional
  //anim.EnableIpv4L3ProtocolCounters (Seconds (0), Seconds (client_stop_time)); // Optional

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //FlowMonitorHelper flowmon;
  //Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  AsciiTraceHelper ascii;
  //PcapHelper pcap;
  bottleNeckLink.EnableAscii (ascii.CreateFileStream ("drop-vs-red.tr"), 0, 0);
  //bottleNeckLink.EnableAsciiAll (ascii.CreateFileStream ("dumbbellAll10-80.tr"));
  //bottleNeckLink.EnablePcapAll ("dumbbell30-90.pcap");

  std::cout << "Running the simulation" << std::endl;
  Simulator::Stop (Seconds (sink_stop_time + 5));
  Simulator::Run ();

  /*Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  double totalRx = 0;
  double Rx = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
     for (uint32_t i = 0; i < nLeaf; ++i)
      {
      if ((t.sourceAddress == Ipv4Address(d.GetLeftIpv4Address (i)) && t.destinationAddress == Ipv4Address(d.GetRightIpv4Address (i))))
        {
          Rx = iter->second.rxBytes;
          totalRx = totalRx + Rx;
    	  //NS_LOG_UNCOND("Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
    	  //NS_LOG_UNCOND("Tx Packets = " << iter->second.txPackets);
    	  //NS_LOG_UNCOND("Rx Packets = " << iter->second.rxPackets);
    	  std:: cout << "Throughput: " << totalRx * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps" << std::endl;
        }
       }
      //NS_LOG_UNCOND("Throughput: " << totalRx * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Kbps");
    }*/
  uint32_t totalRxBytesCounter = 0;
  for (uint32_t i = 0; i < sinkApps.GetN (); i++)
    {
      Ptr <Application> app = sinkApps.Get (i);
      Ptr <PacketSink> pktSink = DynamicCast <PacketSink> (app);
      totalRxBytesCounter += pktSink->GetTotalRx ();
     }
  std::cout << "\nGoodput MBit/sec:" << std::endl;
  std::cout << (totalRxBytesCounter * 8/ 1e6)/Simulator::Now ().GetSeconds () << std::endl;

  QueueDisc::Stats st = queueDiscs.Get (0)->GetStats ();
  std::cout << "*** RED stats from Node left queue disc ***" << std::endl;
  std::cout << st << std::endl;

  std::cout << "Destroying the simulation" << std::endl;
  Simulator::Destroy ();
  return 0;

}
