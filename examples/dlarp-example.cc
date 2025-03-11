/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/dlarp-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DlarpExample");

int main (int argc, char *argv[])
{
  // Set simulation parameters
  uint32_t nNodes = 20;
  double simTime = 200.0;
  double nodeSpeed = 5.0;  // m/s
  double nodePause = 0.0;  // pause time
  double pktInterval = 1.0;  // seconds
  uint32_t packetSize = 1024;  // bytes
  std::string phyMode = "DsssRate1Mbps";
  bool enableFlowMonitor = true;
  
  // Parse command line arguments
  CommandLine cmd;
  cmd.AddValue ("nNodes", "Number of nodes", nNodes);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("nodeSpeed", "Node maximum speed in m/s", nodeSpeed);
  cmd.AddValue ("packetSize", "UDP packet size in bytes", packetSize);
  cmd.AddValue ("pktInterval", "Packet interval in seconds", pktInterval);
  cmd.Parse (argc, argv);
  
  // Enable logging
  LogComponentEnable ("DlarpRoutingProtocol", LOG_LEVEL_INFO);
  LogComponentEnable ("DlarpExample", LOG_LEVEL_INFO);
  
  // Create nodes
  NS_LOG_INFO ("Creating " << nNodes << " nodes...");
  NodeContainer nodes;
  nodes.Create (nNodes);
  
  // Configure wireless network
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  
  // Set wifi MAC layer
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);
  
  // Set node mobility
  MobilityHelper mobility;
  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  
  Ptr<PositionAllocator> positionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  
  mobility.SetPositionAllocator (positionAlloc);
  
  std::stringstream ssSpeed;
  ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
  std::stringstream ssPause;
  ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
  
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue (ssSpeed.str ()),
                            "Pause", StringValue (ssPause.str ()),
                            "PositionAllocator", PointerValue (positionAlloc));
  
  mobility.Install (nodes);
  
  // Install Internet stack with DLARP
  InternetStackHelper internet;
  DlarpHelper dlarp;
  internet.SetRoutingHelper (dlarp);
  internet.Install (nodes);
  
  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);
  
  // Set up application
  uint16_t port = 9;
  
  // Create a UDP server on node 0
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime));
  
  // Create UDP clients on all other nodes sending to node 0
  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t (simTime / pktInterval)));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (pktInterval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  
  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nNodes; i++)
    {
      clientApps.Add (echoClient.Install (nodes.Get (i)));
    }
  
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simTime));
  
  // Enable NetAnim animation
  AnimationInterface anim ("dlarp-animation.xml");
  anim.EnablePacketMetadata (true);
  
  // Setup flow monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  if (enableFlowMonitor)
    {
      flowMonitor = flowHelper.InstallAll ();
    }
  
  // Run simulation
  NS_LOG_INFO ("Starting simulation for " << simTime << " s ...");
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  
  // Print statistics
  if (enableFlowMonitor)
    {
      flowMonitor->CheckForLostPackets ();
      
      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
      std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
      
      double totalThroughput = 0.0;
      uint32_t totalPacketsSent = 0;
      uint32_t totalPacketsReceived = 0;
      
      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          
          totalPacketsSent += i->second.txPackets;
          totalPacketsReceived += i->second.rxPackets;
          
          double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000;
          totalThroughput += throughput;
          
          NS_LOG_INFO ("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n"
                      << "  Tx Packets: " << i->second.txPackets << "\n"
                      << "  Rx Packets: " << i->second.rxPackets << "\n"
                      << "  Throughput: " << throughput << " kbps\n"
                      << "  Packet Delivery Ratio: " << ((double)i->second.rxPackets / i->second.txPackets * 100) << "%");
        }
      
      NS_LOG_INFO ("Total statistics:");
      NS_LOG_INFO ("  Total Tx Packets: " << totalPacketsSent);
      NS_LOG_INFO ("  Total Rx Packets: " << totalPacketsReceived);
      NS_LOG_INFO ("  Packet Delivery Ratio: " << ((double)totalPacketsReceived / totalPacketsSent * 100) << "%");
      NS_LOG_INFO ("  Average Throughput: " << (totalThroughput / stats.size ()) << " kbps");
      
      flowMonitor->SerializeToXmlFile ("dlarp-flowmon.xml", true, true);
    }
  
  Simulator::Destroy ();
  
  return 0;
}