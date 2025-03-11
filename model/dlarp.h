/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef DLARP_H
#define DLARP_H

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/timer.h"
#include <map>
#include <vector>
#include <set>

namespace ns3 {

class Packet;
class NetDevice;
class Ipv4Interface;
class Ipv4Address;
class Ipv4Header;
class DlarpRoutingTableEntry;
class Ipv4Route;
class Socket;
class Ipv4EndPoint;

/**
 * \ingroup dlarp
 * \brief DLARP routing protocol.
 */
class DlarpRoutingProtocol : public Ipv4RoutingProtocol
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  
  DlarpRoutingProtocol ();
  virtual ~DlarpRoutingProtocol ();

  // Inherited methods from Ipv4RoutingProtocol
  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                   UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                   LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;

private:
  // DLARP-specific methods and members
  /**
   * \brief Starts the DLARP routing protocol
   */
  void Start ();
  
  /**
   * \brief Processes a received DLARP packet
   */
  void RecvDlarp (Ptr<Socket> socket);
  
  /**
   * \brief Sends a DLARP route discovery packet
   */
  void SendRouteRequest (Ipv4Address dst);
  
  /**
   * \brief Sends a DLARP route reply packet
   */
  void SendRouteReply (Ipv4Address src, Ipv4Address dst, uint32_t seqNo);
  
  /**
   * \brief Performs the local agreement phase of DLARP
   */
  void PerformLocalAgreement (Ipv4Address dst);
  
  /**
   * \brief Checks and updates the routing table based on local agreement
   */
  bool UpdateRouteByLocalAgreement (Ipv4Address dst);
  
  /**
   * \brief Sends periodic hello messages to discover neighbors
   */
  void SendHello ();
  
  /**
   * \brief Handle hello timeout (neighbor expiry)
   */
  void HelloTimerExpire ();

  // Data structures and variables
  Ptr<Ipv4> m_ipv4;                       //!< IPv4 reference
  Ptr<UniformRandomVariable> m_uniformRandomVariable;  //!< Used for random jitter
  Time m_helloInterval;                    //!< Interval between hello messages
  Time m_routeTimeout;                     //!< Route validity timeout
  Time m_neighborTimeout;                  //!< Neighbor validity timeout
  Timer m_helloTimer;                      //!< Timer for sending hello messages
  
  // Routing table and neighbor information
  std::map<Ipv4Address, std::vector<DlarpRoutingTableEntry> > m_routingTable;
  std::map<Ipv4Address, Time> m_neighborTable;
  
  // Sockets for sending and receiving DLARP packets
  std::map<Ptr<Socket>, Ipv4InterfaceAddress> m_socketAddresses;
  
  uint32_t m_seqNo;                        //!< Current sequence number
  uint32_t m_requestId;                    //!< Current request ID
};

// Routing table entry for DLARP
class DlarpRoutingTableEntry
{
public:
  DlarpRoutingTableEntry ();
  
  // Constructors
  DlarpRoutingTableEntry (Ipv4Address dst, Ipv4Address nextHop, uint32_t interface, uint32_t seqNo);
  
  // Getters and setters
  Ipv4Address GetDestination () const;
  Ipv4Address GetNextHop () const;
  uint32_t GetInterface () const;
  uint32_t GetSeqNo () const;
  Time GetLifeTime () const;
  double GetMetric () const;
  
  void SetLifeTime (Time lifeTime);
  void SetMetric (double metric);
  void SetNextHop (Ipv4Address nextHop);
  void SetInterface (uint32_t interface);
  void SetSeqNo (uint32_t seqNo);
  
private:
  Ipv4Address m_destination;    //!< Destination address
  Ipv4Address m_nextHop;        //!< Next hop address
  uint32_t m_interface;         //!< Output interface
  uint32_t m_seqNo;             //!< Sequence number
  Time m_lifeTime;              //!< Expiration time
  double m_metric;              //!< Route metric
};

} // namespace ns3

#endif /* DLARP_H */