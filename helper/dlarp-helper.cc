/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "dlarp-helper.h"
#include "dlarp.h"
#include "ns3/ptr.h"
#include "ns3/names.h"
#include "ns3/node-list.h"

namespace ns3 {

DlarpHelper::DlarpHelper () : 
  Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::DlarpRoutingProtocol");
}

DlarpHelper* 
DlarpHelper::Copy (void) const 
{
  return new DlarpHelper (*this); 
}

Ptr<Ipv4RoutingProtocol> 
DlarpHelper::Create (Ptr<Node> node) const
{
  Ptr<DlarpRoutingProtocol> agent = m_agentFactory.Create<DlarpRoutingProtocol> ();
  node->AggregateObject (agent);
  return agent;
}

void 
DlarpHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

int64_t
DlarpHelper::AssignStreams (NodeContainer c, int64_t stream)
{
  int64_t currentStream = stream;
  Ptr<Node> node;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      node = (*i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      NS_ASSERT_MSG (ipv4, "Ipv4 not installed on node");
      Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
      NS_ASSERT_MSG (proto, "No routing protocol found");
      Ptr<DlarpRoutingProtocol> dlarp = DynamicCast<DlarpRoutingProtocol> (proto);
      if (dlarp)
        {
          // Set stream for random variables - this would need to be implemented in the main protocol
          // currentStream += dlarp->AssignStreams (currentStream);
        }
    }
  return (currentStream - stream);
}

} // namespace ns3