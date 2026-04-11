#include "pch.h"
#include "ReliableChannel.h"
#include "Packet.h"

namespace Neuron
{
  uint16_t ReliableChannel::QueueReliable(const void* _data, int _size, double _currentTime)
  {
    ASSERT(_data != nullptr);
    ASSERT(_size > 0 && _size <= static_cast<int>(DATALOAD_SIZE));

    uint16_t seq = m_nextSendSeq++;

    PendingPacket pkt{};
    std::memcpy(pkt.Data.data(), _data, _size);
    pkt.Size           = _size;
    pkt.SequenceNumber = seq;
    pkt.SendTime       = _currentTime;
    pkt.ResendCount    = 0;

    m_pending[seq] = pkt;
    return seq;
  }

  void ReliableChannel::Acknowledge(uint16_t _seq)
  {
    m_pending.erase(_seq);
  }

  void ReliableChannel::CollectResends(double _currentTime, std::vector<PendingPacket>& _outPackets)
  {
    // Collect expired packets into a temporary list, then remove dead ones.
    std::vector<uint16_t> dead;

    for (auto& [seq, pkt] : m_pending)
    {
      double elapsed = _currentTime - pkt.SendTime;
      if (elapsed >= DEFAULT_RESEND_INTERVAL)
      {
        if (pkt.ResendCount >= MAX_RESENDS)
        {
          dead.push_back(seq);
          continue;
        }

        pkt.ResendCount++;
        pkt.SendTime = _currentTime;
        _outPackets.push_back(pkt);
      }
    }

    for (uint16_t seq : dead)
      m_pending.erase(seq);
  }

  bool ReliableChannel::IsDuplicate(uint16_t _seq) const
  {
    return m_receivedSeqs.contains(_seq);
  }

  void ReliableChannel::MarkReceived(uint16_t _seq)
  {
    m_receivedSeqs.insert(_seq);

    // Prune old entries to bound memory. Keep the most recent 4096 sequences.
    constexpr size_t MAX_TRACKED = 4096;
    if (m_receivedSeqs.size() > MAX_TRACKED * 2)
    {
      // Find the minimum acceptable sequence (current - MAX_TRACKED).
      // This is a simple heuristic; a sliding window would be more robust.
      uint16_t minKeep = static_cast<uint16_t>(_seq - static_cast<uint16_t>(MAX_TRACKED));
      std::erase_if(m_receivedSeqs, [minKeep](uint16_t s) {
        // Handle wrapping: if minKeep > s, keep s only if it's "close" to current
        int16_t diff = static_cast<int16_t>(s - minKeep);
        return diff < 0;
      });
    }
  }

  int ReliableChannel::BuildAck(uint16_t _seq, void* _outBuffer, int _bufferSize)
  {
    return FramePacket(MessageId::Ack, _seq, 0, nullptr, 0, _outBuffer, _bufferSize);
  }
}
