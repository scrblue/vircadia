//
//  SendQueue.h
//  libraries/networking/src/udt
//
//  Created by Clement on 7/21/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SendQueue_h
#define hifi_SendQueue_h

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <QtCore/QObject>
#include <QtCore/QReadWriteLock>

#include <PortableHighResolutionClock.h>

#include "../SockAddr.h"

#include "Constants.h"
#include "PacketQueue.h"
#include "SequenceNumber.h"
#include "LossList.h"

namespace udt {
    
class BasePacket;
class ControlPacket;
class Packet;
class PacketList;
class Socket;

/// @addtogroup udt
/// @{

///
/// The `SendQueue` is built on top of the `PacketQueue` and `Socket` and is used to queue and send `Packet`s and `PacketList`s reliably.
/// See `PacketQueue` for information on how `Packet`s are dequeued.
/// 
class SendQueue : public QObject {
    Q_OBJECT
    
public:
    ///
    /// The `SendQueue` has a simple selection of states: `NotStarted`, `Running`, and `Stopped`. This is checked internally on
    /// most methods.
    /// 
    enum class State {
        NotStarted,
        Running,
        Stopped
    };

    ///
    /// Creates a `SendQueue` and sets up a thread for it to run on
    /// 
    static std::unique_ptr<SendQueue> create(Socket* socket, SockAddr destination,
                                             SequenceNumber currentSequenceNumber, MessageNumber currentMessageNumber,
                                             bool hasReceivedHandshakeACK);
    ///
    /// Does nothing
    /// 
    virtual ~SendQueue();

    ///
    /// Moves the `Packet` into the queue, while also starting the thread if it is not already running
    /// 
    void queuePacket(std::unique_ptr<Packet> packet);

    ///
    /// Moves the `PacketList` into a new channel in the queue, while also starting the thread if it is not already running
    /// 
    void queuePacketList(std::unique_ptr<PacketList> packetList);

    ///
    /// Returns the `SequenceNumber` of the last sent packet. Sequence numbers are automatically assigned, so this number will
    /// increase (wrapping around to 0) as long as `Packet`s are being sent.
    /// 
    SequenceNumber getCurrentSequenceNumber() const { return SequenceNumber(_atomicCurrentSequenceNumber); }

    ///
    /// Returns the `MessageNumber` of the last sent packet. Message numbers are automatically assigned, so this number will
    /// increase (wrapping around to 0) as long as messages are being sent.
    /// 
    /// NOTE: Not all `Packet`s are messages.
    /// 
    MessageNumber getCurrentMessageNumber() const { return _packets.getCurrentMessageNumber(); }

    ///
    /// Sets the "flow window size" which determines the number of unacknowledged `Packet`s that may be sent.
    /// 
    void setFlowWindowSize(int flowWindowSize) { _flowWindowSize = flowWindowSize; }

    ///
    /// Gets the period at which `Packet`s are dequeued and sent in microseconds.
    ///
    int getPacketSendPeriod() const { return _packetSendPeriod; }

    ///
    /// Sets the period at which `Packet`s are dequeued and sent in microseconds.
    ///
    void setPacketSendPeriod(int newPeriod) { _packetSendPeriod = newPeriod; }

    ///
    /// Sets the timeout period in which to wait for a response in microseconds. If this timeout period is exceeded, the thread
    /// will stop.
    /// 
    void setEstimatedTimeout(int estimatedTimeout) { _estimatedTimeout = estimatedTimeout; }
    
public slots:
    void stop();
    
    void ack(SequenceNumber ack);
    void fastRetransmit(SequenceNumber ack);
    void handshakeACK();
    void updateDestinationAddress(SockAddr newAddress);

signals:
    void packetSent(int wireSize, int payloadSize, SequenceNumber seqNum, p_high_resolution_clock::time_point timePoint);
    void packetRetransmitted(int wireSize, int payloadSize, SequenceNumber seqNum, p_high_resolution_clock::time_point timePoint);
    
    void queueInactive();

    void timeout();
    
private slots:
    void run();
    
private:
    Q_DISABLE_COPY_MOVE(SendQueue)
    SendQueue(Socket* socket, SockAddr dest, SequenceNumber currentSequenceNumber,
              MessageNumber currentMessageNumber, bool hasReceivedHandshakeACK);
    
    void sendHandshake();
    
    int sendPacket(const Packet& packet);
    bool sendNewPacketAndAddToSentList(std::unique_ptr<Packet> newPacket, SequenceNumber sequenceNumber);
    
    int maybeSendNewPacket(); // Figures out what packet to send next
    bool maybeResendPacket(); // Determines whether to resend a packet and which one
    
    bool isInactive(bool attemptedToSendPacket);
    void deactivate(); // makes the queue inactive and cleans it up

    bool isFlowWindowFull() const;
    
    // Increments current sequence number and return it
    SequenceNumber getNextSequenceNumber();
    
    PacketQueue _packets;
    
    Socket* _socket { nullptr }; // Socket to send packet on
    SockAddr _destination; // Destination addr
    
    std::atomic<uint32_t> _lastACKSequenceNumber { 0 }; // Last ACKed sequence number
    
    SequenceNumber _currentSequenceNumber { 0 }; // Last sequence number sent out
    std::atomic<uint32_t> _atomicCurrentSequenceNumber { 0 }; // Atomic for last sequence number sent out
    
    std::atomic<int> _packetSendPeriod { 0 }; // Interval between two packet send event in microseconds, set from CC
    std::atomic<State> _state { State::NotStarted };
    
    std::atomic<int> _estimatedTimeout { 0 }; // Estimated timeout, set from CC
    
    std::atomic<int> _flowWindowSize { 0 }; // Flow control window size (number of packets that can be on wire) - set from CC
    
    mutable std::mutex _naksLock; // Protects the naks list.
    LossList _naks; // Sequence numbers of packets to resend
    
    mutable QReadWriteLock _sentLock; // Protects the sent packet list
    using PacketResendPair = std::pair<uint8_t, std::unique_ptr<Packet>>; // Number of resend + packet ptr
    std::unordered_map<SequenceNumber, PacketResendPair> _sentPackets; // Packets waiting for ACK.
    
    std::mutex _handshakeMutex; // Protects the handshake ACK condition_variable
    std::atomic<bool> _hasReceivedHandshakeACK { false }; // flag for receipt of handshake ACK from client
    std::condition_variable _handshakeACKCondition;
    
    std::condition_variable_any _emptyCondition;

    std::chrono::high_resolution_clock::time_point _lastPacketSentAt;

    static const std::chrono::microseconds MAXIMUM_ESTIMATED_TIMEOUT;
    static const std::chrono::microseconds MINIMUM_ESTIMATED_TIMEOUT;
};

/// @}

}
    
#endif // hifi_SendQueue_h
