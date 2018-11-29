#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "sac-queue-disc.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/tcp-header.h"
#include <sstream>
#include <math.h>
#include "ns3/ipv4-queue-disc-item.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SacQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (SacQueueDisc);

TypeId SacQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SacQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName("TrafficControl")
    .AddConstructor<SacQueueDisc> ()
    .AddAttribute ("Mode",
                   "Determines unit for QueueLimit",
                   EnumValue (Queue::QUEUE_MODE_PACKETS),
                   MakeEnumAccessor (&SacQueueDisc::SetMode),
                   MakeEnumChecker (Queue::QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    Queue::QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("MeanPktSize",
                   "Average of packet size",
                   UintegerValue (500),
                   MakeUintegerAccessor (&SacQueueDisc::m_meanPktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("IdlePktSize",
                   "Average packet size used during idle times. Used when m_cautions = 3",
                   UintegerValue (0),
                   MakeUintegerAccessor (&SacQueueDisc::m_idlePktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Wait",
                   "True for waiting between dropped packets",
                   BooleanValue (true),
                   MakeBooleanAccessor (&SacQueueDisc::m_isWait),
                   MakeBooleanChecker ())
    .AddAttribute ("Gentle",
                   "True to increases dropping probability slowly when average queue exceeds maxthresh",
                   BooleanValue (true),
                   MakeBooleanAccessor (&SacQueueDisc::m_isGentle),
                   MakeBooleanChecker ())
    .AddAttribute ("AdaptMaxP",
                   "True to adapt m_curMaxP",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SacQueueDisc::m_isAdaptMaxP),
                   MakeBooleanChecker ())
    .AddAttribute ("MinTh",
                   "Minimum average length threshold in packets/bytes",
                   DoubleValue (5),
                   MakeDoubleAccessor (&SacQueueDisc::m_minTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxTh",
                   "Maximum average length threshold in packets/bytes",
                   DoubleValue (15),
                   MakeDoubleAccessor (&SacQueueDisc::m_maxTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("QueueLimit",
                   "Queue limit in bytes/packets",
                   UintegerValue (25),
                   MakeUintegerAccessor (&SacQueueDisc::SetQueueLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("QW",
                   "Queue weight related to the exponential weighted moving average (EWMA)",
                   DoubleValue (0.002),
                   MakeDoubleAccessor (&SacQueueDisc::m_qW),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("PW",
                   "P weight",
                   DoubleValue (0.002),
                   MakeDoubleAccessor (&SacQueueDisc::m_pW),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("RW",
                   "R weight",
                   DoubleValue (0.002),
                   MakeDoubleAccessor (&SacQueueDisc::m_rW),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("KW",
                   "K weight",
                   DoubleValue (0.0005),
                   MakeDoubleAccessor (&SacQueueDisc::m_kW),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("LInterm",
                   "The maximum probability of dropping a packet",
                   DoubleValue (50),
                   MakeDoubleAccessor (&SacQueueDisc::m_lInterm),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("Interval",
                   "Time interval to update m_curMaxP",
                   TimeValue (Seconds (0.5)),
                   MakeTimeAccessor (&SacQueueDisc::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("LastSet",
                   "Store the last time m_curMaxP was updated",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&SacQueueDisc::m_lastSet),
                   MakeTimeChecker ())
    .AddAttribute ("Ns1Compat",
                   "NS-1 compatibility",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SacQueueDisc::m_isNs1Compat),
                   MakeBooleanChecker ())
    .AddAttribute ("LinkBandwidth", 
                   "The RED link bandwidth",
                   DataRateValue (DataRate ("1.5Mbps")),
                   MakeDataRateAccessor (&SacQueueDisc::m_linkBandwidth),
                   MakeDataRateChecker ())
    .AddAttribute ("LinkDelay", 
                   "The RED link delay",
                   TimeValue (MilliSeconds (20)),
                   MakeTimeAccessor (&SacQueueDisc::m_linkDelay),
                   MakeTimeChecker ())
  ;

  return tid;
}

SacQueueDisc::SacQueueDisc () :
  QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  ns3::PacketMetadata::Enable();
  m_uv = CreateObject<UniformRandomVariable> ();
}

SacQueueDisc::~SacQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

void
SacQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
  QueueDisc::DoDispose ();
}

void
SacQueueDisc::SetMode (Queue::QueueMode mode)
{
  NS_LOG_FUNCTION (this << mode);
  m_mode = mode;
}

Queue::QueueMode
SacQueueDisc::GetMode (void)
{
  NS_LOG_FUNCTION (this);
  return m_mode;
}

void
SacQueueDisc::SetQueueLimit (uint32_t lim)
{
  NS_LOG_FUNCTION (this << lim);
  m_queueLimit = lim;
}

void
SacQueueDisc::SetTh (double minTh, double maxTh)
{
  NS_LOG_FUNCTION (this << minTh << maxTh);
  NS_ASSERT (minTh <= maxTh);
  m_minTh = minTh;
  m_maxTh = maxTh;
}

SacQueueDisc::Stats
SacQueueDisc::GetStats ()
{
  NS_LOG_FUNCTION (this);
  return m_stats;
}

int64_t 
SacQueueDisc::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uv->SetStream (stream);
  return 1;
}

bool
SacQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  uint32_t nQueued = 0;
  nQueued = m_queue.size();

  if (GetMode () == Queue::QUEUE_MODE_BYTES)
    {
      //NS_LOG_DEBUG ("Enqueue in bytes mode");
      //nQueued = GetInternalQueue (0)->GetNBytes ();
    }
  else if (GetMode () == Queue::QUEUE_MODE_PACKETS)
    {
      //NS_LOG_DEBUG ("Enqueue in packets mode");
      //nQueued = GetInternalQueue (0)->GetNPackets ();
    }

  // simulate number of packets arrival during idle period
  uint32_t m = 0;

  if (m_idle == 1)
    {
      NS_LOG_DEBUG ("RED Queue Disc is idle.");
      Time now = Simulator::Now ();

      if (m_cautious == 3)
        {
          double ptc = m_ptc * m_meanPktSize / m_idlePktSize;
          m = uint32_t (ptc * (now - m_idleTime).GetSeconds ());
        }
      else
        {
          m = uint32_t (m_ptc * (now - m_idleTime).GetSeconds ());
        }

      m_idle = 0;
    }

  m_qAvg = Estimator (nQueued, m + 1, m_qAvg, m_qW);

  TcpHeader header;
  Ptr<Packet> pkt = item->GetPacket();
  pkt->PeekHeader(header);
  std::stringstream str;
  pkt->Print(str);
  bool isUDP = (str.str()[5] == 'U');
  int port = header.GetDestinationPort();
  bool found = false;

  if (isUDP) {

    m_rAvg = (1.0 - m_rW) + m_rW;

    //if (m_qAvg < m_maxTh) {

    int k = (int)(m_kAvg + 0.5);
    int j = (int)(m_qAvg / (m_maxTh * k));
      
    //if we're above m_maxTh then set j to max(k) and always drop incoming packet
    if(m_qAvg >= m_maxTh){
      j = k;
      found = true;
    }

    if (j > (int)nQueued) {
      j = nQueued;
    }
    
    for (int i = 1; i <= j; ++i) {

      double rand = m_uv->GetValue(0.0, 0.9999999);
      int n = m_queue.size();
      Ptr<QueueDiscItem> other = *m_queue[(int)(n * rand)];
      pkt = other->GetPacket();
      pkt->PeekHeader(header);
      int other_port = header.GetDestinationPort();

      if (port == other_port) {

        found = true;

        for (int a = 0; a < n; ++a) {

          other = *m_queue[a];
          pkt = other->GetPacket();
          pkt->PeekHeader(header);
          other_port = header.GetDestinationPort();

          if (port == other_port) {
            NS_LOG_DEBUG ("\t Dropping due to Prob Mark " << m_qAvg);
            m_stats.unforcedDrop++;
            m_queue.erase(m_queue.begin() + a);
            //std::cout << "erasing match\n";
            break;
          }
        }

        if (i == 1) {
          m_pAvg = (1 - m_pW) * m_pAvg + m_pW;
        }

      } else if (i == 1) {
        m_pAvg = (1 - m_pW) * m_pAvg;
      }
    }
    //drop incoming packet if it matched and above m_minTh
    if(found && m_qAvg > m_minTh) {
        NS_LOG_DEBUG ("\t Dropping due to Prob Mark " << m_qAvg);
        m_stats.unforcedDrop++;
        Drop (item);
    }
    //}

  } else { //TCP packet just update R and then do RED
    m_rAvg = (1.0 - m_rW) * m_rAvg;
  }
  
  //update k
  m_kAvg = (1-m_kW)*m_kAvg+m_kW*2*sqrt(m_rAvg)/(pow(m_pAvg,.75));

  //NS_LOG_DEBUG ("\t bytesInQueue  " << GetInternalQueue (0)->GetNBytes () << "\tQavg " << m_qAvg);
  //NS_LOG_DEBUG ("\t packetsInQueue  " << GetInternalQueue (0)->GetNPackets () << "\tQavg " << m_qAvg);

  m_count++;
  m_countBytes += item->GetPacketSize ();

  if(!found) {  //don't run RED code if we already dropped the incoming packet
    uint32_t dropType = DTYPE_NONE;
    if (m_qAvg >= m_minTh && nQueued > 1)
      {
        if ((!m_isGentle && m_qAvg >= m_maxTh) ||
            (m_isGentle && m_qAvg >= 2 * m_maxTh))
          {
            NS_LOG_DEBUG ("adding DROP FORCED MARK");
            dropType = DTYPE_FORCED;
          }
        else if (m_old == 0)
          {
            /* 
             * The average queue size has just crossed the
             * threshold from below to above "minthresh", or
             * from above "minthresh" with an empty queue to
             * above "minthresh" with a nonempty queue.
             */
            m_count = 1;
            m_countBytes = item->GetPacketSize ();
            m_old = 1;
          }
        else if (DropEarly (item, nQueued))
          {
            NS_LOG_LOGIC ("DropEarly returns 1");
            dropType = DTYPE_UNFORCED;
          }
      }
    else 
      {
        // No packets are being dropped
        m_vProb = 0.0;
        m_old = 0;
      }

    if ((GetMode () == Queue::QUEUE_MODE_PACKETS && nQueued >= m_queueLimit) ||
        (GetMode () == Queue::QUEUE_MODE_BYTES && nQueued + item->GetPacketSize() > m_queueLimit))
      {
        NS_LOG_DEBUG ("\t Dropping due to Queue Full " << nQueued);
        dropType = DTYPE_FORCED;
        m_stats.qLimDrop++;
      }

    if (dropType == DTYPE_UNFORCED)
      {
        NS_LOG_DEBUG ("\t Dropping due to Prob Mark " << m_qAvg);
        m_stats.unforcedDrop++;
        Drop (item);
        return false;
      }
    else if (dropType == DTYPE_FORCED)
      {
        NS_LOG_DEBUG ("\t Dropping due to Hard Mark " << m_qAvg);
        m_stats.forcedDrop++;
        Drop (item);
        if (m_isNs1Compat)
          {
            m_count = 0;
            m_countBytes = 0;
          }
        return false;
      }
  }

  bool retval = true;//GetInternalQueue (0)->Enqueue (item);
  m_queue.push_back(&item);

  //std::cout << m_queue.size() << ", " << GetInternalQueue (0)->GetNPackets () << "\n";

  // If Queue::Enqueue fails, QueueDisc::Drop is called by the internal queue
  // because QueueDisc::AddInternalQueue sets the drop callback

  //NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
  //NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

  return retval;
}

/*
 * Note: if the link bandwidth changes in the course of the
 * simulation, the bandwidth-dependent RED parameters do not change.
 * This should be fixed, but it would require some extra parameters,
 * and didn't seem worth the trouble...
 */
void
SacQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Initializing RED params.");

  m_cautious = 0;
  m_ptc = m_linkBandwidth.GetBitRate () / (8.0 * m_meanPktSize);

  NS_ASSERT (m_minTh <= m_maxTh);
  m_stats.forcedDrop = 0;
  m_stats.unforcedDrop = 0;
  m_stats.qLimDrop = 0;

  m_pAvg = 1.0;
  m_rAvg = 1.0;
  m_kAvg = 1.0;
  m_qAvg = 0.0;
  m_count = 0;
  m_countBytes = 0;
  m_old = 0;
  m_idle = 1;

  double th_diff = (m_maxTh - m_minTh);
  if (th_diff == 0)
    {
      th_diff = 1.0; 
    }
  m_vA = 1.0 / th_diff;
  m_curMaxP = 1.0 / m_lInterm;
  m_vB = -m_minTh / th_diff;

  if (m_isGentle)
    {
      m_vC = (1.0 - m_curMaxP) / m_maxTh;
      m_vD = 2.0 * m_curMaxP - 1.0;
    }
  m_idleTime = NanoSeconds (0);

/*
 * If m_qW=0, set it to a reasonable value of 1-exp(-1/C)
 * This corresponds to choosing m_qW to be of that value for
 * which the packet time constant -1/ln(1-m)qW) per default RTT 
 * of 100ms is an order of magnitude more than the link capacity, C.
 *
 * If m_qW=-1, then the queue weight is set to be a function of
 * the bandwidth and the link propagation delay.  In particular, 
 * the default RTT is assumed to be three times the link delay and 
 * transmission delay, if this gives a default RTT greater than 100 ms. 
 *
 * If m_qW=-2, set it to a reasonable value of 1-exp(-10/C).
 */
  if (m_qW == 0.0)
    {
      m_qW = 1.0 - std::exp (-1.0 / m_ptc);
    }
  else if (m_qW == -1.0)
    {
      double rtt = 3.0 * (m_linkDelay.GetSeconds () + 1.0 / m_ptc);

      if (rtt < 0.1)
        {
          rtt = 0.1;
        }
      m_qW = 1.0 - std::exp (-1.0 / (10 * rtt * m_ptc));
    }
  else if (m_qW == -2.0)
    {
      m_qW = 1.0 - std::exp (-10.0 / m_ptc);
    }

  NS_LOG_DEBUG ("\tm_delay " << m_linkDelay.GetSeconds () << "; m_isWait " 
                             << m_isWait << "; m_qW " << m_qW << "; m_ptc " << m_ptc
                             << "; m_minTh " << m_minTh << "; m_maxTh " << m_maxTh
                             << "; m_isGentle " << m_isGentle << "; th_diff " << th_diff
                             << "; lInterm " << m_lInterm << "; va " << m_vA <<  "; cur_max_p "
                             << m_curMaxP << "; v_b " << m_vB <<  "; m_vC "
                             << m_vC << "; m_vD " <<  m_vD);
}

// Compute the average queue size
double
SacQueueDisc::Estimator (uint32_t nQueued, uint32_t m, double qAvg, double qW)
{
  NS_LOG_FUNCTION (this << nQueued << m << qAvg << qW);

  double newAve = qAvg * pow(1.0-qW, m);
  newAve += qW * nQueued;

  return newAve;
}

// Check if packet p needs to be dropped due to probability mark
uint32_t
SacQueueDisc::DropEarly (Ptr<QueueDiscItem> item, uint32_t qSize)
{
  NS_LOG_FUNCTION (this << item << qSize);
  m_vProb1 = CalculatePNew (m_qAvg, m_maxTh, m_isGentle, m_vA, m_vB, m_vC, m_vD, m_curMaxP);
  m_vProb = ModifyP (m_vProb1, m_count, m_countBytes, m_meanPktSize, m_isWait, item->GetPacketSize ());

  // Drop probability is computed, pick random number and act
  if (m_cautious == 1)
    {
      /*
       * Don't drop/mark if the instantaneous queue is much below the average.
       * For experimental purposes only.
       * pkts: the number of packets arriving in 50 ms
       */
      double pkts = m_ptc * 0.05;
      double fraction = std::pow ((1 - m_qW), pkts);

      if ((double) qSize < fraction * m_qAvg)
        {
          // Queue could have been empty for 0.05 seconds
          return 0;
        }
    }

  double u = m_uv->GetValue ();

  if (m_cautious == 2)
    {
      /*
       * Decrease the drop probability if the instantaneous
       * queue is much below the average.
       * For experimental purposes only.
       * pkts: the number of packets arriving in 50 ms
       */
      double pkts = m_ptc * 0.05;
      double fraction = std::pow ((1 - m_qW), pkts);
      double ratio = qSize / (fraction * m_qAvg);

      if (ratio < 1.0)
        {
          u *= 1.0 / ratio;
        }
    }

  if (u <= m_vProb)
    {
      NS_LOG_LOGIC ("u <= m_vProb; u " << u << "; m_vProb " << m_vProb);

      // DROP or MARK
      m_count = 0;
      m_countBytes = 0;
      /// \todo Implement set bit to mark

      return 1; // drop
    }

  return 0; // no drop/mark
}

// Returns a probability using these function parameters for the DropEarly funtion
double
SacQueueDisc::CalculatePNew (double qAvg, double maxTh, bool isGentle, double vA,
                         double vB, double vC, double vD, double maxP)
{
  NS_LOG_FUNCTION (this << qAvg << maxTh << isGentle << vA << vB << vC << vD << maxP);
  double p;

  if (isGentle && qAvg >= maxTh)
    {
      // p ranges from maxP to 1 as the average queue
      // Size ranges from maxTh to twice maxTh
      p = vC * qAvg + vD;
    }
  else if (!isGentle && qAvg >= maxTh)
    {
      /* 
       * OLD: p continues to range linearly above max_p as
       * the average queue size ranges above th_max.
       * NEW: p is set to 1.0
       */
      p = 1.0;
    }
  else
    {
      /*
       * p ranges from 0 to max_p as the average queue size ranges from
       * th_min to th_max
       */
      p = vA * qAvg + vB;
      p *= maxP;
    }

  if (p > 1.0)
    {
      p = 1.0;
    }

  return p;
}

// Returns a probability using these function parameters for the DropEarly funtion
double 
SacQueueDisc::ModifyP (double p, uint32_t count, uint32_t countBytes,
                   uint32_t meanPktSize, bool isWait, uint32_t size)
{
  NS_LOG_FUNCTION (this << p << count << countBytes << meanPktSize << isWait << size);
  double count1 = (double) count;

  if (GetMode () == Queue::QUEUE_MODE_BYTES)
    {
      count1 = (double) (countBytes / meanPktSize);
    }

  if (isWait)
    {
      if (count1 * p < 1.0)
        {
          p = 0.0;
        }
      else if (count1 * p < 2.0)
        {
          p /= (2.0 - count1 * p);
        }
      else
        {
          p = 1.0;
        }
    }
  else
    {
      if (count1 * p < 1.0)
        {
          p /= (1.0 - count1 * p);
        }
      else
        {
          p = 1.0;
        }
    }

  if ((GetMode () == Queue::QUEUE_MODE_BYTES) && (p < 1.0))
    {
      p = (p * size) / meanPktSize;
    }

  if (p > 1.0)
    {
      p = 1.0;
    }

  return p;
}

uint32_t
SacQueueDisc::GetQueueSize (void)
{
  NS_LOG_FUNCTION (this);
  if (GetMode () == Queue::QUEUE_MODE_BYTES)
    {
      return GetInternalQueue (0)->GetNBytes ();
    }
  else if (GetMode () == Queue::QUEUE_MODE_PACKETS)
    {
      return GetInternalQueue (0)->GetNPackets ();
    }
  else
    {
      NS_ABORT_MSG ("Unknown RED mode.");
    }
}

Ptr<QueueDiscItem>
SacQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      m_idle = 1;
      m_idleTime = Simulator::Now ();

      return 0;
    }
  else
    {
      m_idle = 0;
      Ptr<QueueDiscItem> item = *m_queue[0]; //StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ());
      GetInternalQueue(0)->Enqueue(item);
      GetInternalQueue(0)->Dequeue();
      m_queue.erase(m_queue.begin());

      NS_LOG_LOGIC ("Popped " << item);

      //NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
      //NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

      return item;
    }
}

Ptr<const QueueDiscItem>
SacQueueDisc::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);
  if (GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<const QueueDiscItem> item = StaticCast<const QueueDiscItem> (GetInternalQueue (0)->Peek ());

  NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
  NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

  return item;
}

bool
SacQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("SacQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("SacQueueDisc cannot have packet filters");
      return false;
    }

  if (GetNInternalQueues () == 0)
    {
      // create a DropTail queue
      Ptr<Queue> queue = CreateObjectWithAttributes<DropTailQueue> ("Mode", EnumValue (m_mode));
      if (m_mode == Queue::QUEUE_MODE_PACKETS)
        {
          queue->SetMaxPackets (m_queueLimit);
        }
      else
        {
          queue->SetMaxBytes (m_queueLimit);
        }
      AddInternalQueue (queue);
    }

  if (GetNInternalQueues () != 1)
    {
      NS_LOG_ERROR ("SacQueueDisc needs 1 internal queue");
      return false;
    }

  if (GetInternalQueue (0)->GetMode () != m_mode)
    {
      NS_LOG_ERROR ("The mode of the provided queue does not match the mode set on the SacQueueDisc");
      return false;
    }

  if ((m_mode ==  Queue::QUEUE_MODE_PACKETS && GetInternalQueue (0)->GetMaxPackets () < m_queueLimit) ||
      (m_mode ==  Queue::QUEUE_MODE_BYTES && GetInternalQueue (0)->GetMaxBytes () < m_queueLimit))
    {
      NS_LOG_ERROR ("The size of the internal queue is less than the queue disc limit");
      return false;
    }

  return true;
}

} // namespace ns3
