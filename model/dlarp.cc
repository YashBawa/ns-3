/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "dlarp.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include <algorithm>
#include <limits>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DlarpRoutingProtocol");

NS_OBJECT_ENSURE_REGISTERED (DlarpRoutingProtocol);

// DLARP Packet Types
enum DlarpPacketType
{
  DLARPTYPE_HELLO = 1,
  DLARPTYPE_RREQ  = 2,
  DLARPTYPE_RREP  = 3,
  DLARPTYPE_AGREEMENT = 4
};

// DLARP Packet Header Format
struct DlarpHeader
{
  uint8_t type;          // Packet type
  uint32_t seqNo;        // Sequence number
  uint32_t requestId;    // Request ID for RREQ
  Ipv4Address src;       // Source address
  Ipv4Address dst;       // Destination address
  uint8_t hopCount;      // Hop count
  double metric;         // Route metric

  // Serialization and deserialization methods would go here
};

TypeId
DlarpRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DlarpRoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .SetGroupName ("Dlarp")
    .AddConstructor<DlarpRoutingProtocol> ()
    .AddAttribute ("HelloInterval", "HELLO interval",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&DlarpRoutingProtocol::m_helloInterval),
                   MakeTimeChecker ())
    .AddAttribute ("RouteTimeout", "Route timeout",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&DlarpRoutingProtocol::m_routeTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("NeighborTimeout", "Neighbor timeout",
                   TimeValue (Seconds (10)),
                   MakeTimeAccessor (&DlarpRoutingProtocol::m_neighborTimeout),
                   MakeTimeChecker ());
  return tid;
}

DlarpRoutingProtocol::DlarpRoutingProtocol () :
  m_ipv4 (0),
  m_seqNo (0),
  m_requestId (0)
{
  m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
}

DlarpRoutingProtocol::~DlarpRoutingProtocol ()
{
}

void
DlarpRoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_ASSERT (ipv4 != 0);
  NS_ASSERT (m_ipv4 == 0);
  
  m_ipv4 = ipv4;
  
  // Create the DLARP protocol sockets
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      Ipv4InterfaceAddress iface = m_ipv4->GetAddress (i, 0);
      if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
        continue;
      
      // Create a socket to listen on
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (), tid);
      socket->SetRecvCallback (MakeCallback (&DlarpRoutingProtocol::RecvDlarp, this));
      socket->BindToNetDevice (m_ipv4->GetNetDevice (i));
      socket->Bind (InetSocketAddress (iface.GetLocal (), 654));
      socket->SetAllowBroadcast (true);
      
      m_socketAddresses[socket] = iface;
    }
  
  // Schedule the first Hello message
  m_helloTimer.SetFunction (&DlarpRoutingProtocol::SendHello, this);
  Time jitter = Seconds (m_uniformRandomVariable->GetValue (0, 0.1));
  m_helloTimer.Schedule (m_helloInterval + jitter);
}

void
DlarpRoutingProtocol::NotifyInterfaceUp (uint32_t interface)
{
  // Add sockets for newly-up interfaces
  Ipv4InterfaceAddress iface = m_ipv4->GetAddress (interface, 0);
  if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
    return;
  
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (), tid);
  socket->SetRecvCallback (MakeCallback (&DlarpRoutingProtocol::RecvDlarp, this));
  socket->BindToNetDevice (m_ipv4->GetNetDevice (interface));
  socket->Bind (InetSocketAddress (iface.GetLocal (), 654));
  socket->SetAllowBroadcast (true);
  
  m_socketAddresses[socket] = iface;
}

void
DlarpRoutingProtocol::NotifyInterfaceDown (uint32_t interface)
{
  // Close sockets for down interfaces
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter = m_socketAddresses.begin ();
       iter != m_socketAddresses.end (); )
    {
      if (iter->second.GetLocal () == m_ipv4->GetAddress (interface, 0).GetLocal ())
        {
          iter->first->Close ();
          m_socketAddresses.erase (iter++);
        }
      else
        {
          ++iter;
        }
    }
}

void
DlarpRoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  // Nothing to do here
}

void
DlarpRoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  // Close socket associated with this address
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter = m_socketAddresses.begin ();
       iter != m_socketAddresses.end (); )
    {
      if (iter->second.GetLocal () == address.GetLocal ())
        {
          iter->first->Close ();
          m_socketAddresses.erase (iter++);
        }
      else
        {
          ++iter;
        }
    }
}

void
DlarpRoutingProtocol::SendHello ()
{
  NS_LOG_FUNCTION (this);
  
  // Prepare a HELLO packet
  DlarpHeader helloHeader;
  helloHeader.type = DLARPTYPE_HELLO;
  helloHeader.seqNo = ++m_seqNo;
  
  // Send HELLO over all interfaces
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator i = m_socketAddresses.begin ();
       i != m_socketAddresses.end (); ++i)
    {
      Ptr<Socket> socket = i->first;
      Ipv4InterfaceAddress iface = i->second;
      
      helloHeader.src = iface.GetLocal ();
      
      // Create and send packet
      Ptr<Packet> packet = Create<Packet> ();
      // Add DlarpHeader (would need proper serialization in real implementation)
      
      socket->SendTo (packet, 0, InetSocketAddress (Ipv4Address ("255.255.255.255"), 654));
    }
  
  // Schedule next HELLO
  Time jitter = Seconds (m_uniformRandomVariable->GetValue (0, 0.1));
  m_helloTimer.Schedule (m_helloInterval + jitter);
}

void
DlarpRoutingProtocol::RecvDlarp (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  
  Ptr<Packet> packet;
  Address sourceAddress;
  
  while ((packet = socket->RecvFrom (sourceAddress)))
    {
      InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
      Ipv4Address sender = inetSourceAddr.GetIpv4 ();
      
      // Extract header (would need proper deserialization in real implementation)
      DlarpHeader header;
      // packet->RemoveHeader (header)
      
      // Process based on packet type
      switch (header.type)
        {
        case DLARPTYPE_HELLO:
          // Update neighbor table
          m_neighborTable[header.src] = Simulator::Now () + m_neighborTimeout;
          break;
          
        case DLARPTYPE_RREQ:
          // Process route request
          // Implement route request handling
          break;
          
        case DLARPTYPE_RREP:
          // Process route reply
          // Implement route reply handling
          break;
          
        case DLARPTYPE_AGREEMENT:
          // Process local agreement message
          // Implement agreement handling
          break;
          
        default:
          NS_LOG_WARN ("Unknown DLARP packet type received");
          break;
        }
    }
}

Ptr<Ipv4Route>
DlarpRoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header);
  
  Ipv4Address dst = header.GetDestination ();
  
  // Check if we have a route to the destination
  std::map<Ipv4Address, std::vector<DlarpRoutingTableEntry> >::iterator it = m_routingTable.find (dst);
  if (it != m_routingTable.end () && !it->second.empty ())
    {
      // Sort by metric to get the best route
      std::sort (it->second.begin (), it->second.end (),
                 [](const DlarpRoutingTableEntry &a, const DlarpRoutingTableEntry &b) {
                   return a.GetMetric () < b.GetMetric ();
                 });
      
      DlarpRoutingTableEntry entry = it->second.front ();
      if (entry.GetLifeTime () > Simulator::Now ())
        {
          // Valid route exists
          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination (dst);
          route->SetGateway (entry.GetNextHop ());
          route->SetOutputDevice (m_ipv4->GetNetDevice (entry.GetInterface ()));
          route->SetSource (m_ipv4->GetAddress (entry.GetInterface (), 0).GetLocal ());
          return route;
        }
    }
  
  // No route found, initiate route discovery
  SendRouteRequest (dst);
  
  // Return error for now
  sockerr = Socket::ERROR_NOROUTETOHOST;
  return NULL;
}

bool
DlarpRoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                                 UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                 LocalDeliverCallback lcb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header << idev);
  
  Ipv4Address dst = header.GetDestination ();
  Ipv4Address src = header.GetSource ();
  
  // If the packet is destined for this node, deliver locally
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      if (m_ipv4->GetAddress (i, 0).GetLocal () == dst)
        {
          return lcb (p, header, 0);
        }
    }
  
  // Check if we have a route to forward the packet
  std::map<Ipv4Address, std::vector<DlarpRoutingTableEntry> >::iterator it = m_routingTable.find (dst);
  if (it != m_routingTable.end () && !it->second.empty ())
    {
      // Sort by metric to get the best route
      std::sort (it->second.begin (), it->second.end (),
                 [](const DlarpRoutingTableEntry &a, const DlarpRoutingTableEntry &b) {
                   return a.GetMetric () < b.GetMetric ();
                 });
      
      DlarpRoutingTableEntry entry = it->second.front ();
      if (entry.GetLifeTime () > Simulator::Now ())
        {
          // Valid route exists, forward the packet
          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination (dst);
          route->SetGateway (entry.GetNextHop ());
          route->SetOutputDevice (m_ipv4->GetNetDevice (entry.GetInterface ()));
          route->SetSource (m_ipv4->GetAddress (entry.GetInterface (), 0).GetLocal ());
          ucb (route, p, header);
          return true;
        }
    }
  
  // No route found, drop the packet
  return ecb (p, header, Socket::ERROR_NOROUTETOHOST);
}

// Implementation of DLARP-specific methods

void
DlarpRoutingProtocol::SendRouteRequest (Ipv4Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  
  // Prepare a RREQ packet
  DlarpHeader rreqHeader;
  rreqHeader.type = DLARPTYPE_RREQ;
  rreqHeader.seqNo = ++m_seqNo;
  rreqHeader.requestId = ++m_requestId;
  rreqHeader.dst = dst;
  rreqHeader.hopCount = 0;
  
  // Broadcast RREQ over all interfaces
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator i = m_socketAddresses.begin ();
       i != m_socketAddresses.end (); ++i)
    {
      Ptr<Socket> socket = i->first;
      Ipv4InterfaceAddress iface = i->second;
      
      rreqHeader.src = iface.GetLocal ();
      
      // Create and send packet
      Ptr<Packet> packet = Create<Packet> ();
      // Add DlarpHeader (would need proper serialization in real implementation)
      
      socket->SendTo (packet, 0, InetSocketAddress (Ipv4Address ("255.255.255.255"), 654));
    }
}

void
DlarpRoutingProtocol::PerformLocalAgreement (Ipv4Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  
  // Implementation of DLARP's local agreement phase
  // This is where the core of DLARP's algorithm would go
  
  // For example:
  // 1. Collect routing information from neighbors
  // 2. Apply consensus algorithm to determine best path
  // 3. Update routing table based on agreement
}

void
DlarpRoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream () << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
                        << ", DLARP Routing table:" << std::endl;
                        
  *stream->GetStream () << "Destination\tNextHop\tInterface\tSeqNo\tMetric\tLifetime" << std::endl;
  
  for (std::map<Ipv4Address, std::vector<DlarpRoutingTableEntry> >::const_iterator i = m_routingTable.begin ();
       i != m_routingTable.end (); ++i)
    {
      for (std::vector<DlarpRoutingTableEntry>::const_iterator j = i->second.begin ();
           j != i->second.end (); ++j)
        {
          *stream->GetStream () << i->first << "\t"
                                << j->GetNextHop () << "\t"
                                << j->GetInterface () << "\t"
                                << j->GetSeqNo () << "\t"
                                << j->GetMetric () << "\t"
                                << (j->GetLifeTime () - Simulator::Now ()).As (unit) << std::endl;
        }
    }
}

// RoutingTableEntry implementation

DlarpRoutingTableEntry::DlarpRoutingTableEntry () :
  m_seqNo (0),
  m_metric (0)
{
}

DlarpRoutingTableEntry::DlarpRoutingTableEntry (Ipv4Address dst, Ipv4Address nextHop, uint32_t interface, uint32_t seqNo) :
  m_destination (dst),
  m_nextHop (nextHop),
  m_interface (interface),
  m_seqNo (seqNo),
  m_lifeTime (Simulator::Now ()),
  m_metric (0)
{
}

Ipv4Address
DlarpRoutingTableEntry::GetDestination () const
{
  return m_destination;
}

Ipv4Address
DlarpRoutingTableEntry::GetNextHop () const
{
  return m_nextHop;
}

uint32_t
DlarpRoutingTableEntry::GetInterface () const
{
  return m_interface;
}

uint32_t
DlarpRoutingTableEntry::GetSeqNo () const
{
  return m_seqNo;
}

Time
DlarpRoutingTableEntry::GetLifeTime () const
{
  return m_lifeTime;
}

double
DlarpRoutingTableEntry::GetMetric () const
{
  return m_metric;
}

void
DlarpRoutingTableEntry::SetLifeTime (Time lifeTime)
{
  m_lifeTime = lifeTime;
}

void
DlarpRoutingTableEntry::SetMetric (double metric)
{
  m_metric = metric;
}

void
DlarpRoutingTableEntry::SetNextHop (Ipv4Address nextHop)
{
  m_nextHop = nextHop;
}

void
DlarpRoutingTableEntry::SetInterface (uint32_t interface)
{
  m_interface = interface;
}

void
DlarpRoutingTableEntry::SetSeqNo (uint32_t seqNo)
{
  m_seqNo = seqNo;
}

} // namespace ns3