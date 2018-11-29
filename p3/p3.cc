#include <iostream>
#include <string>
#include <algorithm>
#include "p2pCampusHelper.h"

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"

#include "ns3/node-list.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/vector.h"
#include "ns3/ipv6-address-generator.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/global-router-interface.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/mpi-interface.h"
#include "ns3/udp-client-server-helper.h"
#include "worm.h"
#include <time.h>
#include <sys/time.h>

using namespace ns3;

std::vector<uint32_t> fake_outer[4];
std::vector<uint32_t> fake_inner[4];
bool nix = true;
uint32_t exist_nodes;
ApplicationContainer apps;
uint32_t a=0;

NS_LOG_COMPONENT_DEFINE ("p3");

double get_wall_time(){
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        //  Handle error
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

int main(int argc, char** argv) 
{
  //double wall0 = get_wall_time();

  uint32_t inner = 8;
	uint32_t outer = 2;
  uint32_t scanRate = 5;
  std::string scanPattern = "Uniform";
  std::string syncType = "Yawns";
	uint32_t delay = 10;
	PointToPointCampusHelper* topology[4];
	CommandLine cmd;

	cmd.AddValue("BackboneDelay", "enter the delay for this simulation", delay);
  cmd.AddValue("ScanRate", "enter the scan rate for this simulation", scanRate);
  cmd.AddValue("ScanPattern", "enter the scan pattern for this simulation", scanPattern);
  cmd.AddValue("SyncType", "enter the sync type for this simulation", syncType);

  uint32_t int_scanPattern = (scanPattern == "Uniform") ? 0 : (scanPattern == "Local") ? 1 : 2;

	PointToPointHelper innerp2p;
	innerp2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
	innerp2p.SetChannelAttribute ("Delay", StringValue ("5ms"));	

	PointToPointHelper outerp2p;
	outerp2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
	outerp2p.SetChannelAttribute ("Delay", StringValue ("8ms"));

	if(syncType == "Null") 
  {
    GlobalValue::Bind ("SimulatorImplementationType",
                       StringValue ("ns3::NullMessageSimulatorImpl"));
  } 
  else 
  {
    GlobalValue::Bind ("SimulatorImplementationType",
                       StringValue ("ns3::DistributedSimulatorImpl"));
  }

	MpiInterface::Enable (&argc, &argv);

	LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

	uint32_t systemId = MpiInterface::GetSystemId ();
	uint32_t systemCount = MpiInterface::GetSize ();

	srand(time(NULL));
	RngSeedManager::SetSeed (rand());	
	Ptr<UniformRandomVariable> rand1 = CreateObject<UniformRandomVariable> ();	
	//Ptr<UniformRandomVariable> rand2 = CreateObject<UniformRandomVariable> ();
	//Ptr<UniformRandomVariable> rand3 = CreateObject<UniformRandomVariable> ();
	//Ptr<UniformRandomVariable> rand4 = CreateObject<UniformRandomVariable> ();


	switch (systemCount)
	{
		case 1:
		
			for(uint32_t i = 0; i < 4; i++)
			{
				topology[i] = new PointToPointCampusHelper(inner, outer, innerp2p, outerp2p, rand1, 0);
			}
		break;	
		

		case 2:
		
			// for(uint32_t i = 0; i < 2; i++)
			// {
			// 	topology[i] = new PointToPointCampusHelper(inner, outer, innerp2p, outerp2p, rand1, 0);
			// 	topology[i+1] = new PointToPointCampusHelper(inner, outer, innerp2p, outerp2p, rand1, 1);
			// }
            for(uint32_t i = 0; i < 4; i++)
            {
                topology[i] = new PointToPointCampusHelper(inner, outer, innerp2p, outerp2p, rand1, i / 2);
            }
		break;	
		
		case 4:
		
			for(uint32_t i = 0; i < 4; i++)
			{
				topology[i] = new PointToPointCampusHelper(inner, outer, innerp2p, outerp2p, rand1, i);
			}
		break;	

	}
    
	//PointToPointCampusHelper topology1(inner, outer, innerp2p, outerp2p, rand1, 0);
	//PointToPointCampusHelper topology2(inner, outer, innerp2p, outerp2p, rand2);
	

	NodeContainer hubs;
	for(uint32_t i = 0; i < 4; i++)
	{
		hubs.Add(topology[i]->GetHub());
	}


	PointToPointHelper h2h;
	h2h.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
	h2h.SetChannelAttribute ("Delay", StringValue (std::to_string(delay) + std::string("ms")));

	NetDeviceContainer hubDevices;
	// hubDevices = h2h.Install(hubs.Get(0), hubs.Get(1));
	// hubDevices = h2h.Install(hubs.Get(1), hubs.Get(2));
	// hubDevices = h2h.Install(hubs.Get(2), hubs.Get(3));
	// hubDevices = h2h.Install(hubs.Get(3), hubs.Get(0));
    hubDevices.Add(h2h.Install(hubs.Get(0), hubs.Get(1)));
    hubDevices.Add(h2h.Install(hubs.Get(1), hubs.Get(2)));
    hubDevices.Add(h2h.Install(hubs.Get(2), hubs.Get(3)));
    hubDevices.Add(h2h.Install(hubs.Get(3), hubs.Get(0)));


	InternetStackHelper stack;
  	if (nix)
    {
      Ipv4NixVectorHelper nixRouting;
      stack.SetRoutingHelper (nixRouting); // has effect on the next Install ()
    }

    stack.InstallAll ();

	Ipv4AddressHelper address;
  	address.SetBase("14.1.1.0","255.255.255.0");
  	Ipv4InterfaceContainer interfaces = address.Assign(hubDevices);

	//InternetStackHelper internet;
    //topology1.InstallStack (internet);
  	//topology2.InstallStack (internet);
  	//internet.Install(hubs);

  	topology[0]->AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"));
  	topology[1]->AssignIpv4Addresses (Ipv4AddressHelper ("11.1.1.0", "255.255.255.0"));
  	topology[2]->AssignIpv4Addresses (Ipv4AddressHelper ("12.1.1.0", "255.255.255.0"));
  	topology[3]->AssignIpv4Addresses (Ipv4AddressHelper ("13.1.1.0", "255.255.255.0"));

  int count = 0;
  ApplicationContainer sinkApps;
  uint32_t iterations;
  uint32_t start;

  switch (systemCount)
  {
    case 1:
      iterations = 4;
      start = 0;
      break;
    case 2:
      iterations = 2;
      start = systemId * 2;
      break;
    case 4:
      iterations = 1;
      start = systemId;
      break;
  }
  
  //std::cout << iterations << ", " << start << ", " << systemId << ", " << systemCount << std::endl;

  for(uint32_t q = start; q < (start + iterations); q++)
  {
  	fake_outer[q] = topology[q]->get_missing_o_nodes();
  	fake_inner[q] = topology[q]->get_missing_i_nodes();
  	
  	for(uint32_t i = 0; i < inner; i++)
  	{
      bool fakeSpoke = false;
  		for(uint32_t j = 0; j < fake_inner[q].size(); j++)
  		{
  			if(i == fake_inner[q].at(j))
  			{
          fakeSpoke = true;
          UdpServerHelper sink(100);
          Ptr<Node> node = topology[q]->GetSpokeNode(i);
          if (node->GetSystemId() == systemId) sinkApps.Add(sink.Install(node));

          node = topology[q]->GetChildNode(i, 1);
          if (node->GetSystemId() == systemId) sinkApps.Add(sink.Install (node));

          node = topology[q]->GetChildNode(i, 0);
          if (node->GetSystemId() == systemId) sinkApps.Add(sink.Install (node));
  			}
  		}

      if(!fakeSpoke)
      {
        for(uint32_t k = 0; k < outer; k++)
        {
          bool fake = false;
          for(uint32_t l = 0; l < fake_outer[q].size(); l++)
          {
            if((i*outer + k) == fake_outer[q].at(l))
            {
              fake = true;
              UdpServerHelper sink1(100);
              Ptr<Node> node = topology[q]->GetChildNode(i, k);
              if (node->GetSystemId() == systemId) sinkApps.Add(sink1.Install(node));
            }
          }
          
          if(!fake) 
          {
            ObjectFactory factory;
            factory.SetTypeId(Worm::GetTypeId());
            factory.Set("ScanPattern", UintegerValue(int_scanPattern));
            factory.Set("Interval", TimeValue(Seconds(1.0 / (scanRate * 1000))));
            if(a == 0 && systemId == 0)
            {
              Ptr<Node> node = topology[q]->GetChildNode(i, k);
              if (node->GetSystemId() == systemId)
              {
                factory.Set("Infected", BooleanValue(true));
                Ptr<Worm> worm = factory.Create<Worm>();
                node->AddApplication(worm);
                apps.Add(worm);
                factory.Set("Infected", BooleanValue(false));
                a++;
                count++;
              }
            }
            else
            {
              Ptr<Node> node = topology[q]->GetChildNode(i, k);
              if (node->GetSystemId() == systemId)
              {
                Ptr<Worm> worm = factory.Create<Worm>();
                node->AddApplication(worm);
                apps.Add(worm);
                count++;
              }
            }
          }
        }
      }
  	}
  }

sinkApps.Start(Seconds(0.0));
apps.Start (Seconds (0.0));
NS_LOG_INFO ("Run Simulation");

exist_nodes = (64 / systemCount) - (fake_outer[0].size()+fake_outer[1].size()+fake_outer[2].size()+fake_outer[3].size());
//std::cout << exist_nodes << std::endl;
//std::cout << std::endl << std::endl;
//std::cout << count << std::endl;

// std::cout << topology[0]->get_missing_i_nodes().size() << std::endl;
// std::cout << topology[1]->get_missing_i_nodes().size() << std::endl;
// std::cout << topology[2]->get_missing_i_nodes().size() << std::endl;
// std::cout << topology[3]->get_missing_i_nodes().size() << std::endl;

// std::cout << fake_outer[0].size() << std::endl;
// std::cout << fake_outer[1].size() << std::endl;
// std::cout << fake_outer[2].size() << std::endl;
// std::cout << fake_outer[3].size() << std::endl;

Simulator::Run();
	//PointToPointCampusHelper topology3(inner, outer, innerp2p, outerp2p, rand3);
	//PointToPointCampusHelper topology4(inner, outer, innerp2p, outerp2p, rand4);

	//AnimationInterface anim ("p3.xml");
	//delete[] topology;
  Simulator::Destroy();
  MpiInterface::Disable ();

  //std::cout << get_wall_time() - wall0 << std::endl;

  return 0;

}
