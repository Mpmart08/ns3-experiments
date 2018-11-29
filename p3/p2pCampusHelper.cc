#include "p2pCampusHelper.h"

#include <iostream>

#include <algorithm>
#include <cmath>
#include <sstream>

// ns3 includes
#include "ns3/log.h"
#include "ns3/point-to-point-star.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/constant-position-mobility-model.h"

#include "ns3/node-list.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/vector.h"
#include "ns3/ipv6-address-generator.h"

using namespace ns3;

PointToPointCampusHelper::PointToPointCampusHelper(uint32_t maxInner, uint32_t maxOuter, PointToPointHelper inner, PointToPointHelper outer, Ptr<UniformRandomVariable> rnd, uint32_t systemId) 
{
  //bool flag = false;
	m_hub.Create (1, systemId);
  m_spokes.Create (maxInner, systemId);

  int count_inner = 0, count_outer = 0;

  for (uint32_t i = 0; i < m_spokes.GetN (); ++i)
    {

      double rand_i = rnd->GetValue();
      rand_i = rand_i * 100;
      //std::cout << "the random value selected is  "<<rand_i <<std::endl;

      if(rand_i > 80)
      {
        count_inner ++;
        missing_nodes_inner.push_back(i);
        missing_nodes_outer.push_back(i*(int)maxOuter); 
        missing_nodes_outer.push_back(i*(int)maxOuter + 1); //both the outer nodes of the inner missing node
      }


      NetDeviceContainer nd = inner.Install (m_hub.Get (0), m_spokes.Get (i));
      m_hubDevices.Add (nd.Get (0));
      m_spokeDevices.Add (nd.Get (1));
      m_child[i].Create (maxOuter, systemId);

        for (uint32_t j = 0; j < m_child[i].GetN(); ++j)
        {
          double rand_o = rnd->GetValue();
          rand_o = rand_o * 100;
          //std::cout<< " the random no generated for outer is " <<rand_o<<std::endl;

            NetDeviceContainer nc = outer.Install(m_spokes.Get(i), m_child[i].Get(j));
            m_spokeDevices.Add(nc.Get(0));
            m_childDevices[i].Add(nc.Get(1));

          if(rand_o > 60)
          {
            uint32_t num = i*maxOuter + j;
            if(std::find(missing_nodes_outer.begin(), missing_nodes_outer.end(), num) == missing_nodes_outer.end())
            {
              count_outer++;
              missing_nodes_outer.push_back(num);
            }
          }
        
        }
    }
}

PointToPointCampusHelper::~PointToPointCampusHelper() 
{

}

Ptr<Node>
PointToPointCampusHelper::GetHub () const
{
  return m_hub.Get (0);
}

Ptr<Node>
PointToPointCampusHelper::GetSpokeNode (uint32_t i) const
{
  return m_spokes.Get (i);
}

Ptr<Node>
PointToPointCampusHelper::GetChildNode (uint32_t i, uint32_t j) const
{
  return m_child[i].Get (j);
}

Ipv4Address
PointToPointCampusHelper::GetHubIpv4Address (uint32_t i) const
{
  return m_hubInterfaces.GetAddress (i);
}

Ipv4Address
PointToPointCampusHelper::GetSpokeIpv4Address (uint32_t i) const
{
  return m_spokeInterfaces.GetAddress (i);
}

Ipv4Address
PointToPointCampusHelper::GetChildIpv4Address (uint32_t i, uint32_t j) const
{
  return m_childInterfaces[i].GetAddress (j);
}

Ipv6Address
PointToPointCampusHelper::GetHubIpv6Address (uint32_t i) const
{
  return m_hubInterfaces6.GetAddress (i, 1);
}

Ipv6Address
PointToPointCampusHelper::GetSpokeIpv6Address (uint32_t i) const
{
  return m_spokeInterfaces6.GetAddress (i, 1);
}

uint32_t
PointToPointCampusHelper::SpokeCount () const
{
  return m_spokes.GetN ();
}

void 
PointToPointCampusHelper::InstallStack (InternetStackHelper stack)
{
  stack.Install (m_hub);
  stack.Install (m_spokes);
  for(uint32_t i = 0; i < m_spokes.GetN(); ++i)
  {
      stack.Install (m_child[i]);
  }
}

void 
PointToPointCampusHelper::AssignIpv4Addresses (Ipv4AddressHelper address)
{
  for (uint32_t i = 0; i < m_spokes.GetN(); ++i)
    {
      m_hubInterfaces.Add (address.Assign (m_hubDevices.Get (i)));
      m_spokeInterfaces.Add (address.Assign (m_spokeDevices.Get (i * 3)));
      for(uint32_t j = 0; j < m_child[i].GetN(); ++j)
      {
        m_spokeInterfaces.Add (address.Assign (m_spokeDevices.Get (i * 3 + j + 1)));
        m_childInterfaces[i].Add (address.Assign (m_childDevices[i].Get(j)));
        //GetChildIpv4Address(i, j).Print(std::cout);
        //std::cout << "\n";
      }
      address.NewNetwork ();
    }
    // std::cout << "Hubs:\n";
    // for (uint32_t i = 0; i < m_hubInterfaces.GetN(); ++i)
    // {
    //     m_hubInterfaces.GetAddress(i).Print(std::cout);
    //     std::cout << "\n";
    // }
    // std::cout << "Spokes:\n";
    // for (uint32_t i = 0; i < m_spokeInterfaces.GetN(); ++i)
    // {
    //     m_spokeInterfaces.GetAddress(i).Print(std::cout);
    //     std::cout << "\n";
    // }
    // std::cout << "Children:\n";
    // for (uint32_t i = 0; i < 8; ++i)
    // {
    //     for (uint32_t j = 0; j < m_childInterfaces[0].GetN(); ++j)
    //     {
    //         m_childInterfaces[i].GetAddress(j).Print(std::cout);
    //         std::cout << "\n";
    //     }
    // }
}

std::vector<uint32_t> PointToPointCampusHelper::get_missing_o_nodes()
{
  return missing_nodes_outer;
}

std::vector<uint32_t> PointToPointCampusHelper::get_missing_i_nodes()
{
  return missing_nodes_inner;
}



