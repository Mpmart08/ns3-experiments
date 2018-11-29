#ifndef WORM_H
#define WORM_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/address.h"
#include "ns3/packet-loss-counter.h"
#include "ns3/random-variable-stream.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/double.h"

namespace ns3 {

class Socket;
class Packet;

class Worm : public Application
{
public:

  static TypeId GetTypeId (void);

  Worm ();
  virtual ~Worm ();

  void SetRemote (Address ip, uint16_t port);
  void SetRemote (Address addr);
  uint32_t GetLost (void) const;
  uint64_t GetReceived (void) const;
  uint16_t GetPacketWindowSize () const;
  void SetPacketWindowSize (uint16_t size);

protected:

  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void Send (void);
  void HandleRead (Ptr<Socket> socket);
  uint32_t Uniform();
  uint32_t Local();
  uint32_t Sequential();
  Address Scan();

  bool m_infected;
  uint32_t m_scanRange;
  uint32_t m_scanPattern;
  Ptr<UniformRandomVariable> m_rand;

  uint32_t m_count; //!< Maximum number of packets the application will send
  Time m_interval; //!< Packet inter-send time
  uint32_t m_size; //!< Size of the sent packet (including the SeqTsHeader)

  uint32_t m_sent; //!< Counter for sent packets
  //Ptr<Socket> m_socket; //!< Socket
  Address m_peerAddress; //!< Remote peer address
  uint16_t m_peerPort; //!< Remote peer port
  EventId m_sendEvent; //!< Event to send the next packet

  uint16_t m_port; //!< Port on which we listen for incoming packets.
  Ptr<Socket> m_socket; //!< IPv4 Socket
  Ptr<Socket> m_socket6; //!< IPv6 Socket
  uint64_t m_received; //!< Number of received packets
  PacketLossCounter m_lossCounter; //!< Lost packet counter

};

} // namespace ns3

#endif // WORM_H


