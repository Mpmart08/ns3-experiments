#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

using string = std::string;

NS_LOG_COMPONENT_DEFINE ("P1");

int main (int argc, char *argv[])
{
    SeedManager::SetSeed(1);

    // parameters
    int nSpokes = 8;
    int port = 5000;
    double endTime = 60.0;
    Time sinkStart = Seconds(1.0);
    Time sourceStart = Seconds(2.0);
    Time simulatorStop = Seconds(endTime);
    string protocol = "TcpHybla";
    StringValue spokeDataRate("5Mbps");
    StringValue spokeChannelDelay("10ms");
    StringValue hubDataRate("1Mbps");
    StringValue hubChannelDelay("20ms");
    Ipv4AddressHelper sourceAddresses("10.1.1.0", "255.255.255.0");
    Ipv4AddressHelper sinkAddresses("10.2.1.0", "255.255.255.0");
    Ipv4AddressHelper hubAddresses("10.3.1.0", "255.255.255.0");

    // get command line args
    CommandLine cmd;
    cmd.AddValue("nSpokes", "Number of nodes to place in the star", nSpokes);
    cmd.AddValue ("protocol", "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus ", protocol);
    cmd.Parse(argc, argv);

    // tcp protocol variants
    if (protocol.compare("TcpNewReno") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId ()));
    }
    else if (protocol.compare("TcpHybla") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHybla::GetTypeId ()));
    }
    else if (protocol.compare("TcpHighSpeed") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHighSpeed::GetTypeId ()));
    }
    else if (protocol.compare("TcpVegas") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpVegas::GetTypeId ()));
    }
    else if (protocol.compare("TcpScalable") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpScalable::GetTypeId ()));
    }
    else if (protocol.compare("TcpHtcp") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHtcp::GetTypeId ()));
    }
    else if (protocol.compare("TcpVeno") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpVeno::GetTypeId ()));
    }
    else if (protocol.compare("TcpBic") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBic::GetTypeId ()));
    }
    else if (protocol.compare("TcpYeah") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpYeah::GetTypeId ()));
    }
    else if (protocol.compare("TcpIllinois") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpIllinois::GetTypeId ()));
    }
    else if (protocol.compare("TcpWestwood") == 0)
    { // the default protocol type in ns3::TcpWestwood is WESTWOOD
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId ()));
        Config::SetDefault("ns3::TcpWestwood::FilterType", EnumValue(TcpWestwood::TUSTIN));
    }
    else if (protocol.compare("TcpWestwoodPlus") == 0)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId ()));
        Config::SetDefault("ns3::TcpWestwood::ProtocolType", EnumValue(TcpWestwood::WESTWOODPLUS));
        Config::SetDefault("ns3::TcpWestwood::FilterType", EnumValue(TcpWestwood::TUSTIN));
    }
    else
    {
        NS_LOG_DEBUG ("Invalid TCP version");
        exit (1);
    }

    // set spoke rate and delay
    PointToPointHelper spokeHelper;
    spokeHelper.SetDeviceAttribute("DataRate", spokeDataRate);
    spokeHelper.SetChannelAttribute("Delay", spokeChannelDelay);

    // set hub rate and delay
    PointToPointHelper hubHelper;
    hubHelper.SetDeviceAttribute("DataRate", hubDataRate);
    hubHelper.SetChannelAttribute("Delay", hubChannelDelay);

    // create two stars
    PointToPointStarHelper sources(nSpokes, spokeHelper);
    PointToPointStarHelper sinks(nSpokes, spokeHelper);

    // connect the hubs
    NetDeviceContainer hubs;
    hubs = hubHelper.Install(sources.GetHub(), sinks.GetHub());

    // install internet stack
    InternetStackHelper internet;
    sources.InstallStack(internet);
    sinks.InstallStack(internet);

    // assign ip addresses
    sources.AssignIpv4Addresses(sourceAddresses);
    sinks.AssignIpv4Addresses(sinkAddresses);
    hubAddresses.Assign(hubs);

    // install packet sinks
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;
    for (int i = 0; i < nSpokes; i++)
    {
        sinkApps.Add(sinkHelper.Install(sinks.GetSpokeNode(i)));
    }
    sinkApps.Start(sinkStart);

    // install bulk senders
    BulkSendHelper bulkSender ("ns3::TcpSocketFactory", Address());
    ApplicationContainer sourceApps;
    for (int i = 0; i < nSpokes; i++)
    {
        AddressValue remoteAddress(InetSocketAddress(sinks.GetSpokeIpv4Address(i), port));
        bulkSender.SetAttribute("Remote", remoteAddress);
        sourceApps.Add(bulkSender.Install(sources.GetSpokeNode(i)));
    }
    sourceApps.Start(sourceStart);

    // turn on global static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // run simulation
    Simulator::Stop(simulatorStop);
    Simulator::Run();

    uint64_t totalRx = 0;

    for (int i = 0; i < nSpokes; ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(i));
        uint32_t bytesReceived = sink->GetTotalRx();
        totalRx += bytesReceived;
        std::cout << "Sink " << i << "\tTotalRx: " << bytesReceived * 1e-6 * 8 << "Mb";
        std::cout << "\tThroughput: " << (bytesReceived * 1e-6 * 8) / endTime << "Mbps" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Totals\tTotalRx: " << totalRx * 1e-6 * 8 << "Mb";
    std::cout << "\tThroughput: " << (totalRx * 1e-6 * 8) / endTime << "Mbps" << std::endl;

    Simulator::Destroy();
}
