#ifndef POINT_TO_POINT_CAMPUS_HELPER_H
#define POINT_TO_POINT_CAMPUS_HELPER_H


#pragma once

#include <string>
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;


class PointToPointCampusHelper {
public:

	PointToPointCampusHelper(uint32_t maxInner, uint32_t maxOuter, PointToPointHelper inner, PointToPointHelper outer, Ptr<UniformRandomVariable> rnd, uint32_t systemId);

	~PointToPointCampusHelper();

public:
  /**
   * \returns a node pointer to the hub node in the
   *          star, i.e., the center node
   */
  Ptr<Node> GetHub () const;

  /**
   * \param i an index into the spokes of the star
   *
   * \returns a node pointer to the node at the indexed spoke
   */
  Ptr<Node> GetSpokeNode (uint32_t i) const;

  Ptr<Node> GetChildNode (uint32_t i, uint32_t j) const;  

  /**
   * \param i index into the hub interfaces
   *
   * \returns Ipv4Address according to indexed hub interface
   */
  Ipv4Address GetHubIpv4Address (uint32_t i) const;

  /**
   * \param i index into the spoke interfaces
   *
   * \returns Ipv4Address according to indexed spoke interface
   */
  Ipv4Address GetSpokeIpv4Address (uint32_t i) const;

  Ipv4Address GetChildIpv4Address (uint32_t i, uint32_t j) const;

  /**
   * \param i index into the hub interfaces
   *
   * \returns Ipv6Address according to indexed hub interface
   */
  Ipv6Address GetHubIpv6Address (uint32_t i) const;

  /**
   * \param i index into the spoke interfaces
   *
   * \returns Ipv6Address according to indexed spoke interface
   */
  Ipv6Address GetSpokeIpv6Address (uint32_t i) const;

  /**
   * \returns the total number of spokes in the star
   */
  uint32_t SpokeCount () const;

  /**
   * \param stack an InternetStackHelper which is used to install 
   *              on every node in the star
   */
  void InstallStack (InternetStackHelper stack);

  /**
   * \param address an Ipv4AddressHelper which is used to install 
   *                Ipv4 addresses on all the node interfaces in 
   *                the star
   */
  void AssignIpv4Addresses (Ipv4AddressHelper address);

  std::vector<uint32_t> get_missing_o_nodes();
  std::vector<uint32_t> get_missing_i_nodes();


private:
  NodeContainer m_hub;              //!< Hub node
  NetDeviceContainer m_hubDevices;  //!< Hub node NetDevices
  NodeContainer m_spokes;                     //!< Spoke nodes
  NetDeviceContainer m_spokeDevices;          //!< Spoke nodes NetDevices
  NodeContainer m_child[8];
  NetDeviceContainer m_childDevices[8];
  Ipv4InterfaceContainer m_hubInterfaces;     //!< IPv4 hub interfaces
  Ipv4InterfaceContainer m_spokeInterfaces;   //!< IPv4 spoke nodes interfaces
  Ipv4InterfaceContainer m_childInterfaces[8];   //!< IPv4 child nodes interfaces
  Ipv6InterfaceContainer m_hubInterfaces6;    //!< IPv6 hub interfaces
  Ipv6InterfaceContainer m_spokeInterfaces6;  //!< IPv6 spoke nodes interfaces

  std::vector<uint32_t> missing_nodes_inner;
  std::vector<uint32_t> missing_nodes_outer;

};

#endif
