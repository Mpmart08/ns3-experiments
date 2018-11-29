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

using namespace ns3;

using string = std::string;
using stringstream = std::stringstream;

stringstream fileLeftQueue;
stringstream fileLeftQueueAvg;
stringstream fileRightQueue;
stringstream fileRightQueueAvg;
stringstream fileQueueEvents;

uint32_t checkTimes;
double avgLeftQueueSize;
double avgRightQueueSize;

NS_LOG_COMPONENT_DEFINE ("P2C");

void EnqueueAtRed(Ptr<const QueueItem> item)
{
    TcpHeader tcp;
    Ptr<Packet> pkt = item->GetPacket();
    pkt->PeekHeader(tcp);

    uint32_t seq = tcp.GetSequenceNumber().GetValue();
    seq = seq / 1000;

    std::ofstream fPlotSource(fileQueueEvents.str().c_str(), std::ios::out | std::ios::app);
    fPlotSource << Simulator::Now().GetSeconds() << " " << seq << " 0" << std::endl;
    fPlotSource.close();
}

void DequeueAtRed(Ptr<const QueueItem> item)
{
    TcpHeader tcp;
    Ptr<Packet> pkt = item->GetPacket();
    pkt->PeekHeader(tcp);

    uint32_t seq = tcp.GetSequenceNumber().GetValue();
    seq = seq / 1000;

    std::ofstream fPlotSource(fileQueueEvents.str().c_str(), std::ios::out | std::ios::app);
    fPlotSource << Simulator::Now().GetSeconds() << " " << seq << " 1" << std::endl;
    fPlotSource.close();
}

void DroppedAtRed(Ptr<const QueueItem> item)
{
    TcpHeader tcp;
    Ptr<Packet> pkt = item->GetPacket();
    pkt->PeekHeader(tcp);

    uint32_t seq = tcp.GetSequenceNumber().GetValue();
    seq = seq / 1000;

    std::ofstream fPlotSource(fileQueueEvents.str().c_str(), std::ios::out | std::ios::app);
    fPlotSource << Simulator::Now().GetSeconds() << " " << seq << " 2" << std::endl;
    fPlotSource.close();
}

//This code is fine for printing average and actual queue size
void CheckQueueSize(Ptr<QueueDisc> leftQueue, Ptr<QueueDisc> rightQueue)
{
    Simulator::Schedule(Seconds(0.01), &CheckQueueSize, leftQueue, rightQueue);
    checkTimes++;

    // left queue

    uint32_t qsize = StaticCast<RedQueueDisc>(leftQueue)->GetQueueSize();
    avgLeftQueueSize += qsize;

    std::ofstream fLeftQueue(fileLeftQueue.str().c_str(), std::ios::out | std::ios::app);
    fLeftQueue << Simulator::Now().GetSeconds() << " " << qsize << std::endl;
    fLeftQueue.close();

    std::ofstream fLeftQueueAvg(fileLeftQueueAvg.str().c_str(), std::ios::out | std::ios::app);
    fLeftQueueAvg << Simulator::Now().GetSeconds() << " " << avgLeftQueueSize / checkTimes << std::endl;
    fLeftQueueAvg.close();

    // right queue

    qsize = StaticCast<RedQueueDisc>(rightQueue)->GetQueueSize();
    avgRightQueueSize += qsize;

    std::ofstream fRightQueue(fileRightQueue.str().c_str(), std::ios::out | std::ios::app);
    fRightQueue << Simulator::Now().GetSeconds() << " " << qsize << std::endl;
    fRightQueue.close();

    std::ofstream fRightQueueAvg(fileRightQueueAvg.str().c_str(), std::ios::out | std::ios::app);
    fRightQueueAvg << Simulator::Now().GetSeconds() << " " << avgRightQueueSize / checkTimes << std::endl;
    fRightQueueAvg.close();
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
    string leftLeafDelays[] = {"0.5ms", "1ms", "3ms", "5ms"};
    string rightLeafDelays[] = {"0.5ms", "1ms", "5ms", "2ms"};
    Ipv4AddressHelper leftLeafAddresses("10.1.1.0", "255.255.255.0");
    Ipv4AddressHelper rightLeafAddresses("10.2.1.0", "255.255.255.0");
    Ipv4AddressHelper gatewayAddresses("10.3.1.0", "255.255.255.0");
    double endTime = 1.0;
    double sinkStart = 0.0;
    string pathOut = ".";

    // command line args
    CommandLine cmd;
    cmd.AddValue("runNumber", "run number for random variable generation", runNumber);
    cmd.Parse(argc, argv);

    // tcp setup
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));

    // red setup
    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue(packetSize));
    Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (minTh));
    Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (maxTh));
    Config::SetDefault ("ns3::RedQueueDisc::LinkBandwidth", StringValue (bottleneckLinkBW));
    Config::SetDefault ("ns3::RedQueueDisc::LinkDelay", StringValue (bottleneckLinkDelay));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (maxPackets));

    SeedManager::SetSeed(1);
    SeedManager::SetRun(runNumber);

    NodeContainer leftLeafNodes;
    NodeContainer rightLeafNodes;
    NodeContainer gatewayNodes;

    NetDeviceContainer leftLeafDevices;
    NetDeviceContainer rightLeafDevices;
    NetDeviceContainer gatewayDevices;
    NetDeviceContainer leftGatewayDevices;
    NetDeviceContainer rightGatewayDevices;

    Ipv4InterfaceContainer leftLeafInterfaces;
    Ipv4InterfaceContainer leftGatewayInterfaces;
    Ipv4InterfaceContainer rightLeafInterfaces;
    Ipv4InterfaceContainer rightGatewayInterfaces;
    Ipv4InterfaceContainer gatewayInterfaces;

    leftLeafNodes.Create(4);
    rightLeafNodes.Create(4);
    gatewayNodes.Create(2);

    // connect gateways
    PointToPointHelper bottleneckHelper;
    bottleneckHelper.SetDeviceAttribute("DataRate", StringValue(bottleneckLinkBW));
    bottleneckHelper.SetChannelAttribute("Delay", StringValue(bottleneckLinkDelay));
    bottleneckHelper.SetQueue("ns3::DropTailQueue", "Mode", StringValue ("QUEUE_MODE_PACKETS"), "MaxPackets", UintegerValue(1));
    gatewayDevices = bottleneckHelper.Install(gatewayNodes);

    // connect left leaves
    PointToPointHelper leafHelper;
    leafHelper.SetDeviceAttribute("DataRate", StringValue(leafLinkBW));
    leafHelper.SetQueue("ns3::DropTailQueue", "Mode", StringValue ("QUEUE_MODE_PACKETS"), "MaxPackets", UintegerValue(1));
    for (uint32_t i = 0; i < leftLeafNodes.GetN(); ++i)
    {
        leafHelper.SetChannelAttribute("Delay", StringValue(leftLeafDelays[i]));
        NetDeviceContainer c = leafHelper.Install(gatewayNodes.Get(0), leftLeafNodes.Get(i));
        leftGatewayDevices.Add(c.Get(0));
        leftLeafDevices.Add(c.Get(1));
    }

    // connect right leaves
    for (uint32_t i = 0; i < rightLeafNodes.GetN(); ++i)
    {
        leafHelper.SetChannelAttribute("Delay", StringValue(rightLeafDelays[i]));
        NetDeviceContainer c = leafHelper.Install(gatewayNodes.Get(1), rightLeafNodes.Get(i));
        rightGatewayDevices.Add(c.Get(0));
        rightLeafDevices.Add(c.Get(1));
    }

    // install internet stack
    InternetStackHelper internet;
    internet.Install(gatewayNodes);
    internet.Install(leftLeafNodes);
    internet.Install(rightLeafNodes);

    // setup red queues
    TrafficControlHelper tchBottleneck;
    tchBottleneck.SetRootQueueDisc("ns3::RedQueueDisc");
    Ptr<QueueDisc> leftRedQueue = (tchBottleneck.Install(gatewayDevices.Get(0))).Get(0);
    Ptr<QueueDisc> rightRedQueue = (tchBottleneck.Install(gatewayDevices.Get(1))).Get(0);

    //setup traces
    leftRedQueue->TraceConnectWithoutContext("Enqueue", MakeCallback(&EnqueueAtRed));
    leftRedQueue->TraceConnectWithoutContext("Dequeue", MakeCallback(&DequeueAtRed));
    leftRedQueue->TraceConnectWithoutContext("Drop", MakeCallback(&DroppedAtRed));

    // assign ip addresses
    gatewayInterfaces = gatewayAddresses.Assign(gatewayDevices);
    for (uint32_t i = 0; i < leftLeafNodes.GetN(); ++i)
    {
        NetDeviceContainer ndc;
        ndc.Add(leftLeafDevices.Get(i));
        ndc.Add(leftGatewayDevices.Get(i));
        Ipv4InterfaceContainer ifc = leftLeafAddresses.Assign(ndc);
        leftLeafInterfaces.Add(ifc.Get(0));
        leftGatewayInterfaces.Add(ifc.Get(1));
        leftLeafAddresses.NewNetwork();
    }
    for (uint32_t i = 0; i < rightLeafNodes.GetN(); ++i)
    {
        NetDeviceContainer ndc;
        ndc.Add(rightLeafDevices.Get(i));
        ndc.Add(rightGatewayDevices.Get(i));
        Ipv4InterfaceContainer ifc = rightLeafAddresses.Assign(ndc);
        rightLeafInterfaces.Add(ifc.Get(0));
        rightGatewayInterfaces.Add(ifc.Get(1));
        rightLeafAddresses.NewNetwork();
    }

    //Configure Sources
    ApplicationContainer sources;

    //Install Sources
    OnOffHelper sourceHelper("ns3::TcpSocketFactory", Address());
    sourceHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    sourceHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    sourceHelper.SetAttribute("DataRate", DataRateValue(DataRate(leafLinkBW)));
    sourceHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    rand->SetAttribute ("Min", DoubleValue(0.0));
    rand->SetAttribute ("Max", DoubleValue(1.0));

    // 21 sources on the left
    for(uint32_t i = 0; i < 21; ++i)
    {
        // random number of packets between 20 and 400
        double randVal = rand->GetValue();
        int packets = (int)(randVal * 380 + 20);
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(packets * packetSize));

        // destination is random right leaf
        randVal = rand->GetValue();
        int index = (randVal >= 0.25) + (randVal >= 0.5) + (randVal >= 0.75);
        AddressValue remoteAddress(InetSocketAddress(rightLeafInterfaces.GetAddress(index), port));
        sourceHelper.SetAttribute("Remote", remoteAddress);

        // source is random left leaf
        randVal = rand->GetValue();
        index = (randVal >= 0.25) + (randVal >= 0.5) + (randVal >= 0.75);
        ApplicationContainer ac = sourceHelper.Install(leftLeafNodes.Get(index));

        // source has random start time
        ac.Start(Seconds(rand->GetValue()));
        sources.Add(ac);
    }

    // 20 sources on the right
    for(uint32_t i = 0; i < 20; ++i)
    {
        // random number of packets between 20 and 400
        double randVal = rand->GetValue();
        int packets = (int)(randVal * 380 + 20);
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(packets * packetSize));

        // destination is random left leaf
        randVal = rand->GetValue();
        int index = (randVal >= 0.25) + (randVal >= 0.5) + (randVal >= 0.75);
        AddressValue remoteAddress(InetSocketAddress(leftLeafInterfaces.GetAddress(index), port));
        sourceHelper.SetAttribute("Remote", remoteAddress);

        // source is random right leaf
        randVal = rand->GetValue();
        index = (randVal >= 0.25) + (randVal >= 0.5) + (randVal >= 0.75);
        ApplicationContainer ac = sourceHelper.Install(rightLeafNodes.Get(index));

        // random start time
        ac.Start(Seconds(rand->GetValue()));
        sources.Add(ac);
    }

    sources.Stop(Seconds(endTime));

    //Configure Sinks
    ApplicationContainer sinks;

    //Install Sinks
    for(uint32_t i = 0; i < leftLeafNodes.GetN(); ++i)
    {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        sinks.Add(sinkHelper.Install(leftLeafNodes.Get(i)));
    }

    for(uint32_t i = 0; i < rightLeafNodes.GetN(); ++i)
    {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        sinks.Add(sinkHelper.Install(rightLeafNodes.Get(i)));
    }

    sinks.Start(Seconds(sinkStart));
    sinks.Stop(Seconds(endTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    fileLeftQueue << pathOut << "/" << "leftRedQueue.plot";
    fileLeftQueueAvg << pathOut << "/" << "leftRedQueueAvg.plot";
    fileRightQueue << pathOut << "/" << "rightRedQueue.plot";
    fileRightQueueAvg << pathOut << "/" << "rightRedQueueAvg.plot";
    fileQueueEvents << pathOut << "/" << "queueEvents.plot";
    remove(fileLeftQueue.str().c_str());
    remove(fileLeftQueueAvg.str().c_str());
    remove(fileRightQueue.str().c_str());
    remove(fileRightQueueAvg.str().c_str());
    remove(fileQueueEvents.str().c_str());

    Simulator::ScheduleNow(&CheckQueueSize, leftRedQueue, rightRedQueue);
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
