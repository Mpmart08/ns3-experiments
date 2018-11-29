#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/flow-monitor-module.h"
#include <sstream>
#include "sac-queue-disc.h"

using namespace ns3;

using string = std::string;
using stringstream = std::stringstream;

stringstream filePlotQueue;
stringstream filePlotQueueAvg;
string filePlotSource[4] = {"./source1.plot", "./source2.plot", "./source3.plot", "./source4.plot"};

uint32_t checkTimes;
double avgQueueSize;

int s[] = {0, 0, 0, 0};

NS_LOG_COMPONENT_DEFINE ("P2A");

void EnqueueAtRed(Ptr<const QueueItem> item)
{
    //std::cout << "enq" << std::endl;
    UdpHeader tcp;
    Ptr<Packet> pkt = item->GetPacket();
    pkt->PeekHeader(tcp);

    int port = tcp.GetDestinationPort();
    int i = port - 5000;
    uint32_t seq = 0;
    if (seq < 1000)
    {
        seq = s[i];
    }
    else
    {
        seq = seq / 1000 + s[i] - 1;
    }

    std::ofstream fPlotSource(filePlotSource[i].c_str(), std::ios::out | std::ios::app);
    fPlotSource << Simulator::Now().GetSeconds() << " " << seq << " 0" << std::endl;
    fPlotSource.close();
}

void DequeueAtRed(Ptr<const QueueItem> item)
{
    //std::cout << "deq" << std::endl;
    UdpHeader tcp;
    Ptr<Packet> pkt = item->GetPacket();
    pkt->PeekHeader(tcp);

    int port = tcp.GetDestinationPort();
    int i = port - 5000;
    uint32_t seq = 0;
    if (seq < 1000)
    {
        seq = s[i];
        s[i]++;
    }
    else
    {
        seq = seq / 1000 + s[i] - 1;
    }

    std::ofstream fPlotSource(filePlotSource[i].c_str(), std::ios::out | std::ios::app);
    fPlotSource << Simulator::Now().GetSeconds() << " " << seq << " 1" << std::endl;
    fPlotSource.close();
}

void DroppedAtRed(Ptr<const QueueItem> item)
{
    //std::cout << "drp" << std::endl;
    UdpHeader tcp;
    Ptr<Packet> pkt = item->GetPacket();
    pkt->PeekHeader(tcp);

    int port = tcp.GetDestinationPort();
    int i = port - 5000;
    uint32_t seq = 0;
    seq = seq / 1000 + s[i] - 1;

    std::ofstream fPlotSource(filePlotSource[i].c_str(), std::ios::out | std::ios::app);
    fPlotSource << Simulator::Now().GetSeconds() << " " << seq << " 2" << std::endl;
    fPlotSource.close();
}


//This code is fine for printing average and actual queue size
void CheckQueueSize(Ptr<QueueDisc> queue)
{
    uint32_t qsize = StaticCast<SacQueueDisc>(queue)->GetQueueSize();
    avgQueueSize += qsize;
    checkTimes++;

    Simulator::Schedule(Seconds(0.01), &CheckQueueSize, queue);

    std::ofstream fPlotQueue(filePlotQueue.str().c_str(), std::ios::out | std::ios::app);
    fPlotQueue << Simulator::Now().GetSeconds() << " " << qsize << std::endl;
    fPlotQueue.close();

    std::ofstream fPlotQueueAvg(filePlotQueueAvg.str().c_str(), std::ios::out | std::ios::app);
    fPlotQueueAvg << Simulator::Now().GetSeconds() << " " << avgQueueSize / checkTimes << std::endl;
    fPlotQueueAvg.close();
}

int main(int argc, char* argv[])
{    
    // parameters
    int runNumber = 0;
    int port = 5000;
    int packetSize = 1000;
    int maxPackets = 1000;
    double minTh = 5;
    double maxTh = 15;
    //double wq = 0.002;
    //double maxp = 1.0 / 50.0;
    string bottleneckLinkBW = "45Mbps";
    string bottleneckLinkDelay = "2ms";
    string leafLinkBW = "100Mbps";
    string leafLinkDelays[] = {"1ms", "4ms", "8ms", "5ms"};
    Ipv4AddressHelper leafAddresses("10.1.1.0", "255.255.255.0");
    Ipv4AddressHelper bottleneckAddresses("10.2.1.0", "255.255.255.0");
    double endTime = 0.25;
    double sinkStart = 0.0;
    double leafStartTimes[] = {0.0, 0.2, 0.4, 0.6};
    string pathOut = ".";

    ns3::PacketMetadata::Enable();

    // command line args
    CommandLine cmd;
    cmd.AddValue("runNumber", "run number for random variable generation", runNumber);
    cmd.Parse(argc, argv);

    // tcp setup
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));

    // red setup
    Config::SetDefault ("ns3::SacQueueDisc::Mode", StringValue("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::SacQueueDisc::MeanPktSize", UintegerValue(packetSize));
    Config::SetDefault ("ns3::SacQueueDisc::MinTh", DoubleValue (minTh));
    Config::SetDefault ("ns3::SacQueueDisc::MaxTh", DoubleValue (maxTh));
    Config::SetDefault ("ns3::SacQueueDisc::LinkBandwidth", StringValue (bottleneckLinkBW));
    Config::SetDefault ("ns3::SacQueueDisc::LinkDelay", StringValue (bottleneckLinkDelay));
    Config::SetDefault ("ns3::SacQueueDisc::QueueLimit", UintegerValue (maxPackets));

    SeedManager::SetSeed(1);
    SeedManager::SetRun(runNumber);

    NodeContainer leafNodes;
    NodeContainer bottleneckNodes;

    NetDeviceContainer leafDevices;
    NetDeviceContainer bottleneckDevices;
    NetDeviceContainer gatewayDevices;

    Ipv4InterfaceContainer leafInterfaces;
    Ipv4InterfaceContainer bottleneckInterfaces;
    Ipv4InterfaceContainer gatewayInterfaces;

    leafNodes.Create(4);
    bottleneckNodes.Create(2);

    // connect sources to gateway
    PointToPointHelper leafHelper;
    leafHelper.SetDeviceAttribute("DataRate", StringValue(leafLinkBW));
    leafHelper.SetQueue("ns3::DropTailQueue", "Mode", StringValue ("QUEUE_MODE_PACKETS"), "MaxPackets", UintegerValue(1));
    for (uint32_t i = 0; i < leafNodes.GetN(); ++i)
    {
        leafHelper.SetChannelAttribute("Delay", StringValue(leafLinkDelays[i]));
        NetDeviceContainer nd = leafHelper.Install(bottleneckNodes.Get(0), leafNodes.Get(i));
        gatewayDevices.Add(nd.Get(0));
        leafDevices.Add(nd.Get(1));
    }

    // connect gateway to sink
    PointToPointHelper bottleneckHelper;
    bottleneckHelper.SetDeviceAttribute("DataRate", StringValue(bottleneckLinkBW));
    bottleneckHelper.SetChannelAttribute("Delay", StringValue(bottleneckLinkDelay));
    bottleneckHelper.SetQueue("ns3::DropTailQueue", "Mode", StringValue ("QUEUE_MODE_PACKETS"), "MaxPackets", UintegerValue(1));
    bottleneckDevices = bottleneckHelper.Install(bottleneckNodes);

    // install internet stack
    InternetStackHelper internet;
    internet.Install(leafNodes);
    internet.Install(bottleneckNodes);

    // setup red queue
    TrafficControlHelper tchBottleneck;
    tchBottleneck.SetRootQueueDisc("ns3::SacQueueDisc");
    Ptr<QueueDisc> redQueue = (tchBottleneck.Install(bottleneckDevices.Get(0))).Get(0);

    //setup traces
    redQueue->TraceConnectWithoutContext("Enqueue", MakeCallback(&EnqueueAtRed));
    redQueue->TraceConnectWithoutContext("Dequeue", MakeCallback(&DequeueAtRed));
    redQueue->TraceConnectWithoutContext("Drop", MakeCallback(&DroppedAtRed));

    // assign ip addresses
    bottleneckInterfaces = bottleneckAddresses.Assign(bottleneckDevices);
    for (uint32_t i = 0; i < leafNodes.GetN(); ++i)
    {
        NetDeviceContainer ndc;
        ndc.Add(leafDevices.Get(i));
        ndc.Add(gatewayDevices.Get(i));
        Ipv4InterfaceContainer ifc = leafAddresses.Assign(ndc);
        leafInterfaces.Add(ifc.Get(0));
        gatewayInterfaces.Add(ifc.Get(1));
        leafAddresses.NewNetwork();
    }

    //Configure Sources
    ApplicationContainer sources;

    //Install Sources
    OnOffHelper sourceHelper("ns3::UdpSocketFactory", Address());
    sourceHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    sourceHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    sourceHelper.SetAttribute("DataRate", DataRateValue(DataRate(leafLinkBW)));
    sourceHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); 

    //NOTE:  How can you make it so you can determine which flow of traffic a packet belongs to by only 
    //       examinining the TCP header which is what is available when the traces fire?  It's something that
    //       you set when you setup your applications

    //

    for(uint32_t i = 0; i < leafNodes.GetN(); ++i)
    {
        AddressValue remoteAddress(InetSocketAddress(bottleneckInterfaces.GetAddress(1), port++));
        sourceHelper.SetAttribute("Remote", remoteAddress);
        ApplicationContainer ac = sourceHelper.Install(leafNodes.Get(i));
        ac.Start(Seconds(leafStartTimes[i]));
        sources.Add(ac);
    }

    sources.Stop(Seconds(endTime));

    //Configure Sinks

    //NOTE:  Keep in mind I could install multiple sinks on a single node.  
    //HINT:  That will probably be useful for the first experiment if you solve the problem the way I did

    ApplicationContainer sinks;

    //Install Sinks
    for(uint32_t i = 0; i < leafNodes.GetN(); ++i)
    {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), --port));
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
        sinks.Add(sinkHelper.Install(bottleneckNodes.Get(1)));
    }

    sinks.Start(Seconds(sinkStart));
    sinks.Stop(Seconds(endTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    filePlotQueue << pathOut << "/" << "redQueue.plot";
    filePlotQueueAvg << pathOut << "/" << "redQueueAvg.plot";
    for (int i = 0; i <= 3; ++i) {
        remove(filePlotSource[i].c_str());
    }
    remove(filePlotQueue.str().c_str());
    remove(filePlotQueueAvg.str().c_str());
    Simulator::ScheduleNow(&CheckQueueSize, redQueue);

    Simulator::Stop(Seconds(endTime));
    Simulator::Run();

    // uint64_t totalRx = 0;

    // for (uint32_t i = 0; i < leafNodes.GetN(); ++i)
    // {
    //     Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinks.Get(i));
    //     uint32_t bytesReceived = sink->GetTotalRx();
    //     totalRx += bytesReceived;
    //     std::cout << "Sink " << i << "\tTotalRx: " << bytesReceived * 1e-6 * 8 << "Mb";
    //     std::cout << "\tThroughput: " << (bytesReceived * 1e-6 * 8) / endTime << "Mbps" << std::endl;
    // }

    // std::cout << std::endl;
    // std::cout << "Totals\tTotalRx: " << totalRx * 1e-6 * 8 << "Mb";
    // std::cout << "\tThroughput: " << (totalRx * 1e-6 * 8) / endTime << "Mbps" << std::endl;

    Simulator::Destroy();
}
