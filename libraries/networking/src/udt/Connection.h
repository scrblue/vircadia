//
//  Connection.h
//  libraries/networking/src/udt
//
//  Created by Clement on 7/27/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Connection_h
#define hifi_Connection_h

#include <list>
#include <memory>

#include <QtCore/QObject>

#include <PortableHighResolutionClock.h>

#include "ConnectionStats.h"
#include "Constants.h"
#include "LossList.h"
#include "SendQueue.h"
#include "../SockAddr.h"

namespace udt {
    
class CongestionControl;
class ControlPacket;
class Packet;
class PacketList;
class Socket;

class PendingReceivedMessage {
public:
    void enqueuePacket(std::unique_ptr<Packet> packet);
    bool hasAvailablePackets() const;
    std::unique_ptr<Packet> removeNextPacket();
    
    std::list<std::unique_ptr<Packet>> _packets;

private:
    Packet::MessagePartNumber _nextPartNumber = 0;
};

///
/// A `Connection` manages a single network connection for sending and receiving reliable packets
/// 
class Connection : public QObject {
    Q_OBJECT
public:
    ///
    /// By dealing with reliable packets, the `Connection` will have to reckon with `ControlPacket`s which are sent and received
    /// to acknowledge a reliably sent packet 
    /// 
    using ControlPacketPointer = std::unique_ptr<ControlPacket>;

    ///
    /// Basic constructor creating a `Connection` between the `parentSocket` and the `destination`
    /// 
    Connection(Socket* parentSocket, SockAddr destination, std::unique_ptr<CongestionControl> congestionControl);

    ///
    /// Basic deconstructor which stops the thread handling the `sendQueue` and fails any pending, but received messages
    /// 
    virtual ~Connection();

    ///
    /// Simply add the add the `Packet` to the end of the `sendQueue`
    /// 
    void sendReliablePacket(std::unique_ptr<Packet> packet);

    ///
    /// Simply add the append the `PacketList` to the end of the `sendQueue`
    /// 
    void sendReliablePacketList(std::unique_ptr<PacketList> packet);

    ///
    /// This is a rate control method, fired by Socket for all connections on SYN interval. NOP by default.
    /// 
    void sync(); 

    ///
    /// Reads the given `SequenceNumber` and returns a `bool` indicating if this packet should be processed
    /// 
    bool processReceivedSequenceNumber(SequenceNumber sequenceNumber, int packetSize, int payloadSize);

    /// Routes the given `ControlPacketPointer` to internal handlers based on type.
    ///
    /// The handshake process is in the following sequence: Send a `HandshakeRequest` from client to server, then form the
    /// `Connection`. Once the  `Connection` has formed, the server sends a `Handshake` to the client. Finally the client sends
    /// a `HandshakeACK` back. After the handshake process is complete, `ControlPacket`s are used to acknowledge the receipt of
    /// reliably sent packets with the `ACK` variant.
    ///
    /// NOTE: `ControlPacket`s other than `Handshake` or `HandshakeACK` will not be processed if a handshake has not been
    /// completed.
    /// 
    /// For `ACK` packets, this will simply update internal trackers on counts of `ACK` and the last SequenceNumber
    /// acknowledged. It ensures that the entire handshake process has already occured for this to do anything.
    /// 
    /// For `Handshake` packets, this sets an initial SequenceNumber, sends a `HandshakeACK`, and updates an internal tracker
    /// on whether the `Handshake` has taken place. `Handshake`s are from server to client.
    ///
    /// For `HandshakeACK` packets, thiis also sets an initial SequenceNumber and updates the internal tracker  on whether the
    /// handshake process has taken place. `HandshakeACK`s are from client to server.
    ///
    /// For `HandshakeRequest`s, this resets the connection. It ensures that the entire handshake process has already occured
    /// for this to do anything.
    /// 
    void processControl(ControlPacketPointer controlPacket);

    ///
    /// Places the received message packet in the right spot of the queue depending on the `Packet`'s position in the message.
    /// Queued messages are simply passed to the `Socket::MessageHandler` function of the `Connection`'s `Socket` once complete.
    /// 
    void queueReceivedMessagePacket(std::unique_ptr<Packet> packet);

    ///
    /// Reads a sample of the `ConnectionStats` collected from this one `Connection`
    /// 
    ConnectionStats::Stats sampleStats() { return _stats.sample(); }

    ///
    /// Simply returns the `SockAddr` of the destination of this `Connection`
    /// 
    SockAddr getDestination() const { return _destination; }

    /// Simply sets the maximmum bandwidth use of the `Connection`'s `CongestionControl`
    void setMaxBandwidth(int maxBandwidth);

    /// Starts the handshake process from the side of the client by creating a `HandshakeRequest` `ControlPacket` and writing that
    /// to the `Connection`'s `Socket`
    void sendHandshakeRequest();

    ///
    /// Whether the handshake process has been completed
    /// 
    bool hasReceivedHandshake() const { return _hasReceivedHandshake; }

    ///
    /// Record the sending of an unreliable packet to the internal `ConnectionStats` member.
    /// 
    /// NOTE: This doesn't actually send any packets.
    /// 
    void recordSentUnreliablePackets(int wireSize, int payloadSize);

    ///
    /// Record the receipt of an unreliable packet to the internal `ConnectionStats` member.
    /// 
    /// NOTE: This doesn't handle any packets.
    /// 
    void recordReceivedUnreliablePackets(int wireSize, int payloadSize);

    ///
    /// Simply set the destination `SockAddr`
    /// 
    void setDestinationAddress(const SockAddr& destination);

signals:
    void packetSent();
    void receiverHandshakeRequestComplete(const SockAddr& sockAddr);
    void destinationAddressChange(SockAddr currentAddress);

private slots:
    void recordSentPackets(int wireSize, int payloadSize, SequenceNumber seqNum, p_high_resolution_clock::time_point timePoint);
    void recordRetransmission(int wireSize, int payloadSize, SequenceNumber sequenceNumber, p_high_resolution_clock::time_point timePoint);

    void queueInactive();
    void queueTimeout();
    
private:
    void sendACK();
    
    void processACK(ControlPacketPointer controlPacket);
    void processHandshake(ControlPacketPointer controlPacket);
    void processHandshakeACK(ControlPacketPointer controlPacket);
    
    void resetReceiveState();
    
    SendQueue& getSendQueue();
    SequenceNumber nextACK() const;
    
    void updateCongestionControlAndSendQueue(std::function<void()> congestionCallback);
    
    void stopSendQueue();
    
    bool _hasReceivedHandshake { false }; // flag for receipt of handshake from server
    bool _hasReceivedHandshakeACK { false }; // flag for receipt of handshake ACK from client
    bool _didRequestHandshake { false }; // flag for request of handshake from server
   
    SequenceNumber _initialSequenceNumber; // Randomized on Connection creation, identifies connection during re-connect requests
    SequenceNumber _initialReceiveSequenceNumber; // Randomized by peer Connection on creation, identifies connection during re-connect requests

    MessageNumber _lastMessageNumber { 0 };

    LossList _lossList; // List of all missing packets
    SequenceNumber _lastReceivedSequenceNumber; // The largest sequence number received from the peer
    SequenceNumber _lastReceivedACK; // The last ACK received
    
    Socket* _parentSocket { nullptr };
    SockAddr _destination;
   
    std::unique_ptr<CongestionControl> _congestionControl;
   
    std::unique_ptr<SendQueue> _sendQueue;
    
    std::map<MessageNumber, PendingReceivedMessage> _pendingReceivedMessages;

    // Re-used control packets
    ControlPacketPointer _ackPacket;
    ControlPacketPointer _handshakeACK;

    ConnectionStats _stats;
};
    
}

#endif // hifi_Connection_h
