/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef DLARP_HELPER_H
#define DLARP_HELPER_H

#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"

namespace ns3 {

/**
 * \ingroup dlarp
 * \brief Helper class that adds DLARP routing to nodes.
 */
class DlarpHelper : public Ipv4RoutingHelper
{
public:
  DlarpHelper ();
  
  /**
   * \returns pointer to clone of this DlarpHelper
   */
  DlarpHelper* Copy (void) const;
  
  /**
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;
  
  /**
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set.
   */
  void Set (std::string name, const AttributeValue &value);
  
  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model. Return the number of streams (possibly zero) that
   * have been assigned.
   *
   * \param stream first stream index to use
   * \param c NodeContainer of the set of nodes for which DLARP routing
   *          should be modified to use a fixed stream
   * \return the number of stream indices assigned by this helper
   */
  int64_t AssignStreams (NodeContainer c, int64_t stream);
  
private:
  ObjectFactory m_agentFactory; //!< Object factory
};

} // namespace ns3

#endif /* DLARP_HELPER_H */