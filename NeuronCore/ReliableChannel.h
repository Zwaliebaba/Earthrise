#pragma once

namespace Neuron
{
  // A pending reliable packet awaiting ACK from the remote peer.
  struct PendingPacket
  {
    std::array<uint8_t, DATALOAD_SIZE> Data{};
    int      Size           = 0;
    uint16_t SequenceNumber = 0;
    double   SendTime       = 0.0;
    int      ResendCount    = 0;
  };

  // Reliability layer built on top of unreliable UDP datagrams.
  // Tracks outgoing reliable packets (resends on timeout) and
  // deduplicates incoming reliable packets by sequence number.
  class ReliableChannel
  {
  public:
    // Default resend interval in seconds.
    static constexpr double DEFAULT_RESEND_INTERVAL = 0.25;
    // Maximum number of resends before dropping a packet.
    static constexpr int    MAX_RESENDS             = 10;

    ReliableChannel() = default;

    // Queue a reliable packet for delivery. Assigns the next sequence number
    // and stores a copy for potential resend. Returns the assigned sequence.
    uint16_t QueueReliable(const void* _data, int _size, double _currentTime);

    // Process a received ACK for the given sequence number.
    // Removes the corresponding packet from the pending queue.
    void Acknowledge(uint16_t _seq);

    // Collect packets that need resending (timeout exceeded).
    // Increments each packet's resend count and updates send time.
    void CollectResends(double _currentTime, std::vector<PendingPacket>& _outPackets);

    // Check whether an incoming sequence number has already been received (duplicate).
    [[nodiscard]] bool IsDuplicate(uint16_t _seq) const;

    // Mark an incoming reliable sequence as received (for deduplication).
    void MarkReceived(uint16_t _seq);

    // Build an ACK datagram for a given sequence number.
    // Returns total bytes written to _outBuffer.
    static int BuildAck(uint16_t _seq, void* _outBuffer, int _bufferSize);

    [[nodiscard]] uint16_t NextSendSequence() const noexcept { return m_nextSendSeq; }
    [[nodiscard]] size_t PendingCount() const noexcept { return m_pending.size(); }

  private:
    uint16_t m_nextSendSeq = 1;

    // Pending reliable outgoing packets indexed by sequence number.
    std::unordered_map<uint16_t, PendingPacket> m_pending;

    // Set of received incoming reliable sequence numbers (for dedup).
    // In a production system this would be a sliding window; for now a bounded set.
    std::unordered_set<uint16_t> m_receivedSeqs;
  };
}
