#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/seq-ts-header.h"
#include "ns3/packet-loss-counter.h"
#include "ns3/ipv4.h"
#include <cstdlib>
#include <cstdio>
#include "worm.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Worm");

NS_OBJECT_ENSURE_REGISTERED (Worm);

TypeId
Worm::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Worm")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<Worm> ()
    .AddAttribute ("MaxPackets",
                   "The maximum number of packets the application will send",
                   UintegerValue (0xffffffff),
                   MakeUintegerAccessor (&Worm::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval",
                   "The time to wait between packets", TimeValue (Seconds (0.0002)),
                   MakeTimeAccessor (&Worm::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress",
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&Worm::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", "The destination port of the outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&Worm::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize",
                   "Size of packets generated. The minimum packet size is 12 bytes which is the size of the header carrying the sequence number and the time stamp.",
                   UintegerValue (12),
                   MakeUintegerAccessor (&Worm::m_size),
                   MakeUintegerChecker<uint32_t> (12,1500))
    .AddAttribute ("Port",
                   "Port on which we listen for incoming packets.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&Worm::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketWindowSize",
                   "The size of the window used to compute the packet loss. This value should be a multiple of 8.",
                   UintegerValue (32),
                   MakeUintegerAccessor (&Worm::GetPacketWindowSize,
                                         &Worm::SetPacketWindowSize),
                   MakeUintegerChecker<uint16_t> (8,256))
    .AddAttribute("ScanRange",
                  "The number of IP Addresses to be scanned.",
                  UintegerValue(200),
                  MakeUintegerAccessor(&Worm::m_scanRange),
                  MakeUintegerChecker<uint32_t> (1,256))
    .AddAttribute("ScanPattern",
                  "The pattern used to scan the range of IP addresses Uniform (0), Local(1), Sequential(2) .",
                  UintegerValue(0),
                  MakeUintegerAccessor(&Worm::m_scanPattern),
                  MakeUintegerChecker<uint32_t> (0,2))
    .AddAttribute("Infected",
                  "The state of the node on which this application is installed.",
                  BooleanValue(false),
                  MakeBooleanAccessor(&Worm::m_infected),
                  MakeBooleanChecker());
  ;
  return tid;
}

Worm::Worm ()
  : m_lossCounter(0)
{
  NS_LOG_FUNCTION (this);
  m_received = 0;
  m_sent = 0;
  m_socket = 0;
  m_sendEvent = EventId ();
  m_rand = CreateObject<UniformRandomVariable>();
  m_rand->SetAttribute ("Min", DoubleValue(0.0));
  m_rand->SetAttribute ("Max", DoubleValue(0.9999999999));
}

Worm::~Worm ()
{
  NS_LOG_FUNCTION (this);
}

void
Worm::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void
Worm::SetRemote (Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_peerAddress = addr;
}

uint16_t
Worm::GetPacketWindowSize () const
{
  NS_LOG_FUNCTION (this);
  return m_lossCounter.GetBitMapSize ();
}

void
Worm::SetPacketWindowSize (uint16_t size)
{
  NS_LOG_FUNCTION (this << size);
  m_lossCounter.SetBitMapSize (size);
}

uint32_t
Worm::GetLost (void) const
{
  NS_LOG_FUNCTION (this);
  return m_lossCounter.GetLost ();
}

uint64_t
Worm::GetReceived (void) const
{
  NS_LOG_FUNCTION (this);
  return m_received;
}

void
Worm::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
Worm::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
                                                   m_port);
      m_socket->Bind (local);
    }

  m_socket->SetRecvCallback (MakeCallback (&Worm::HandleRead, this));

  if (m_socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (),
                                                   m_port);
      m_socket6->Bind (local);
    }

  m_socket6->SetRecvCallback (MakeCallback (&Worm::HandleRead, this));
  m_socket->SetAllowBroadcast (true);

  if (m_infected)
  {
  m_sendEvent = Simulator::Schedule (Seconds(0.0), &Worm::Send, this);
  }
}

void
Worm::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  Simulator::Cancel (m_sendEvent);
  if (m_socket != 0)
  {
  m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  }
}

void
Worm::Send (void)
{
  NS_LOG_FUNCTION (this);

  //std::cout << "send\n";

  m_peerAddress = Scan();

  if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
  {
    m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
  }
  else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
  {
    m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
  }
  else if (InetSocketAddress::IsMatchingType (m_peerAddress) == true)
  {
    m_socket->Connect (m_peerAddress);
  }
  else if (Inet6SocketAddress::IsMatchingType (m_peerAddress) == true)
  {
    m_socket->Connect (m_peerAddress);
  }
  else
  {
    NS_ASSERT_MSG (false, "Incompatible address type: " << m_peerAddress);
  }

  NS_ASSERT (m_sendEvent.IsExpired ());
  SeqTsHeader seqTs;
  seqTs.SetSeq (m_sent);
  Ptr<Packet> p = Create<Packet> (m_size-(8+4)); // 8+4 : the size of the seqTs header
  p->AddHeader (seqTs);

  std::stringstream peerAddressStringStream;
  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      peerAddressStringStream << Ipv4Address::ConvertFrom (m_peerAddress);
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      peerAddressStringStream << Ipv6Address::ConvertFrom (m_peerAddress);
    }

  if ((m_socket->Send (p)) >= 0)
    {
      ++m_sent;
      NS_LOG_INFO ("TraceDelay TX " << m_size << " bytes to "
                                    << peerAddressStringStream.str () << " Uid: "
                                    << p->GetUid () << " Time: "
                                    << (Simulator::Now ()).GetSeconds ());

    }
  else
    {
      NS_LOG_INFO ("Error while sending " << m_size << " bytes to "
                                          << peerAddressStringStream.str ());
    }

  if (m_sent < m_count)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &Worm::Send, this);
    }
}

void
Worm::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () > 0)
        {
          SeqTsHeader seqTs;
          packet->RemoveHeader (seqTs);
          uint32_t currentSequenceNumber = seqTs.GetSeq ();
          if (InetSocketAddress::IsMatchingType (from))
            {
              NS_LOG_INFO ("TraceDelay: RX " << packet->GetSize () <<
                           " bytes from "<< InetSocketAddress::ConvertFrom (from).GetIpv4 () <<
                           " Sequence Number: " << currentSequenceNumber <<
                           " Uid: " << packet->GetUid () <<
                           " TXtime: " << seqTs.GetTs () <<
                           " RXtime: " << Simulator::Now () <<
                           " Delay: " << Simulator::Now () - seqTs.GetTs ());
            }
          else if (Inet6SocketAddress::IsMatchingType (from))
            {
              NS_LOG_INFO ("TraceDelay: RX " << packet->GetSize () <<
                           " bytes from "<< Inet6SocketAddress::ConvertFrom (from).GetIpv6 () <<
                           " Sequence Number: " << currentSequenceNumber <<
                           " Uid: " << packet->GetUid () <<
                           " TXtime: " << seqTs.GetTs () <<
                           " RXtime: " << Simulator::Now () <<
                           " Delay: " << Simulator::Now () - seqTs.GetTs ());
            }

          m_lossCounter.NotifyReceived (currentSequenceNumber);
          m_received++;
        }
    }

    if (!m_infected)
    {
      std::cout << Simulator::Now () << std::endl;
      m_infected = true;
      m_sendEvent = Simulator::Schedule (m_interval, &Worm::Send, this);
      Simulator::Stop(Seconds(0.1));
    }
}

uint32_t Worm::Uniform()
{
  double randVal = m_rand->GetValue();
  return (uint32_t)(randVal * m_scanRange);
}

uint32_t Worm::Local()
{
  double randVal = m_rand->GetValue();
  if (randVal < 0.75)
  {
    Ptr<Node> node = GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
    Ipv4Address ipv4Address = iaddr.GetLocal();
    uint32_t ip = ipv4Address.Get();
    uint32_t subnet = ip && 0xff00;
    uint32_t hub = ip && 0xff000000;

    uint32_t index = (hub - 10) * 48
                   + (subnet - 1) * 6
                   + (uint32_t)(randVal / 0.75 * 6);

    return index;
  }
  else
  {
    return Uniform();
  }
}

uint32_t Worm::Sequential()
{
  static uint32_t value = 0;
  return value++ % m_scanRange;
}

Address Worm::Scan()
{
  uint32_t index;
  if (m_scanPattern == 1)
  {
    index = Local();
  }
  else if (m_scanPattern == 2)
  {
    index = Sequential();
  }
  else
  {
    index = Uniform();
  }
  
  uint32_t hub;
  uint32_t subnet;
  uint32_t host;
  if (index < 192)
  {
    hub = index / 48 + 10;  // hub = [10, 13]
    index = index % 48;     // index = [0, 47]
    subnet = index / 6 + 1; // subnet = [1, 8]
    host = index % 6 + 1;   // host = [1, 6]
  }
  else
  {
    hub = 14;
    subnet = 1;
    host = index - 191;     // host = [1, 8]
  }
  
  uint32_t address = (hub << 24) | (0x01 << 16) | (subnet << 8) | (host);
  //std::cout << hub << ".1." << subnet << "." << host << std::endl;
  return Ipv4Address(address);
}

} // Namespace ns3


