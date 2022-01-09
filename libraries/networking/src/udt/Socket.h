//
//  Socket.h
//  libraries/networking/src/udt
//
//  Created by Stephen Birarda on 2015-07-20.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#ifndef hifi_Socket_h
#define hifi_Socket_h

#include <functional>
#include <unordered_map>
#include <mutex>
#include <list>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include "../SockAddr.h"
#include "TCPVegasCC.h"
#include "Connection.h"
#include "NetworkSocket.h"

//#define UDT_CONNECTION_DEBUG

class UDTTest;

namespace udt {
/// @addtogroup udt
/// @{

class BasePacket;
class Packet;
class PacketList;
class SequenceNumber;

using PacketFilterOperator = std::function<bool(const Packet&)>;
using ConnectionCreationFilterOperator = std::function<bool(const SockAddr&)>;

using BasePacketHandler = std::function<void(std::unique_ptr<BasePacket>)>;
using PacketHandler = std::function<void(std::unique_ptr<Packet>)>;
using MessageHandler = std::function<void(std::unique_ptr<Packet>)>;
using MessageFailureHandler = std::function<void(SockAddr, udt::Packet::MessageNumber)>;

///
/// A class wrapping simple networking functionality. It allows writing `BasePacket`s, `Packet`s, `Datagrams`s to the remote
/// address, setting handler functions for `BasePacket`s, `Packet`s, `Message`s, and `Message` failures,  as well as allowing
/// for advanced  functionality like filtration functions. It is a one-to-many socket that communicates with a domain server
/// and assignment clients of that domain for a client.
/// 
class Socket : public QObject {
    Q_OBJECT

    using Mutex = std::mutex;
    using Lock = std::unique_lock<Mutex>;

public:
    using StatsVector = std::vector<std::pair<SockAddr, ConnectionStats::Stats>>;

    ///
 	/// Constructor of a socket where the given `QOBject*` is the socket's parent
 	/// 
    Socket(QObject* object = 0, bool shouldChangeSocketOptions = true);

    ///
    /// Simply returns the `Socket`s local port
    /// 
    quint16 localPort(SocketType socketType) const { return _networkSocket.localPort(socketType); }
    
    // Simple functions writing to the socket with no processing

    ///
    /// Simple method for writing a `BasePacket` with little processing. This method asserts that the given `BasePacket` is
    /// not a `Packet` or `NLPacket` which must be sent via `Socket::writePacket`
    /// 
    qint64 writeBasePacket(const BasePacket& packet, const SockAddr& sockAddr);

    ///
    /// Simple method for writing a `Packet` or `NLPacket` with little processing. This method first asserts that the `Packet`
    /// is not meant to be sent reliably. This method will create the `Connection` if it does not already exist.
    ///
    /// NOTE: the sequence number is overwritten with  the correct next value in the sequence
    /// 
    qint64 writePacket(const Packet& packet, const SockAddr& sockAddr);

    ///
    /// Simple method for writing a `Packet` or `NLPacket`  with little processing. Compared to
    /// `Socket::writePacket(const Packet& packet, const SockAddr& sockAddr)`, this method will handle reliable packets.
    /// This method will create the `Connection` if it does not already exist.
    ///
    /// NOTE: the sequence number is overwritten with  the correct next value in the sequence
    /// 
    qint64 writePacket(std::unique_ptr<Packet> packet, const SockAddr& sockAddr);

    ///
    /// Simple method for writing a `PacketList`, reliably or unereliably. This method will create the `Connection` if it
    /// does not alerady exist.
    /// 
    /// NOTE: the sequence number is overwritten with  the correct next value in the sequence
    /// 
    qint64 writePacketList(std::unique_ptr<PacketList> packetList, const SockAddr& sockAddr);

    ///
    /// Simple method for writing a bytearray to the socket. Will error if the network socket is unbound.
    /// 
    qint64 writeDatagram(const char* data, qint64 size, const SockAddr& sockAddr);

    ///
    /// Simple method for writing a bytearray to the socket. Will error if the network socket is unbound.
    /// 
    qint64 writeDatagram(const QByteArray& datagram, const SockAddr& sockAddr);

    ///
	/// Simple method for binding to the given address and port with the given `SocketType`
	/// 
    void bind(SocketType socketType, const QHostAddress& address, quint16 port = 0);

    ///
    /// Simple method for rebinding the network socket to the given port with the given `SocketType`
    /// 
    void rebind(SocketType socketType, quint16 port);

    ///
    /// Simple method for rebinding the network socket with a given `SocketType`
    /// 
    void rebind(SocketType socketType);

    ///
    ///  Simple setter for the packet filtration function
    ///   
    void setPacketFilterOperator(PacketFilterOperator filterOperator) { _packetFilterOperator = filterOperator; }
    
    ///
    ///  Simple setter for the `Packet` handling function
    ///   
    void setPacketHandler(PacketHandler handler) { _packetHandler = handler; }

    ///
    ///  Simple setter for the `Message` handling function
    ///   
    void setMessageHandler(MessageHandler handler) { _messageHandler = handler; }

    ///
    ///  Simple setter for the `Message` failure handling function
    ///   
    void setMessageFailureHandler(MessageFailureHandler handler) { _messageFailureHandler = handler; }


    ///
	/// Simple setter for a filter which restricts the creation of `Connection` classes in the `Socket::writeBasePacket`,
	/// `socket::writePacket`, and `sockent::writePacketList` functions. 
	/// 
    void setConnectionCreationFilterOperator(ConnectionCreationFilterOperator filterOperator)
        { _connectionCreationFilterOperator = filterOperator; }

    ///
    /// Simple addition to a map of `SockAddr`s to `BasePacketHandler`s which will be callled when reading from that connection
    /// before being routed to `Connection::processControl`, `Connection::queueReceivedMessagePacket`, or the set PacketHandler
    /// 
    void addUnfilteredHandler(const SockAddr& senderSockAddr, BasePacketHandler handler)
        { _unfilteredHandlers[senderSockAddr] = handler; }

    ///
	/// Set the `CongestionControlVirtualFactory` that is used to create a `CongestionControl` class for every `Connection`
	/// instance creaaed
	/// 
    void setCongestionControlFactory(std::unique_ptr<CongestionControlVirtualFactory> ccFactory);

    ///
    /// Forwards the given maxBandwidth to every connection managed by this `Socket`
    /// 
    void setConnectionMaxBandwidth(int maxBandwidth);

    ///
    /// Simply moves the packet into the argument of the set MessageHandler
    /// 
    void messageReceived(std::unique_ptr<Packet> packet);

    ///
    ///Simply passes the destination of the `Connection` and messageNumber to the set MessageFailureHandler
    ///
    void messageFailed(Connection* connection, Packet::MessageNumber messageNumber);

    ///
    /// Returns a statistics sample from every connection
    /// 
    StatsVector sampleStatsForAllConnections();

#if defined(WEBRTC_DATA_CHANNELS)
    const WebRTCSocket* getWebRTCSocket();
#endif

#if (PR_BUILD || DEV_BUILD)
    void sendFakedHandshakeRequest(const SockAddr& sockAddr);
#endif

signals:
    void clientHandshakeRequestComplete(const SockAddr& sockAddr);

public slots:
    void cleanupConnection(SockAddr sockAddr);
    void clearConnections();
    void handleRemoteAddressChange(SockAddr previousAddress, SockAddr currentAddress);

private slots:
    void readPendingDatagrams();
    void checkForReadyReadBackup();

    void handleSocketError(SocketType socketType, QAbstractSocket::SocketError socketError);
    void handleStateChanged(SocketType socketType, QAbstractSocket::SocketState socketState);

private:
    void setSystemBufferSizes(SocketType socketType);
    Connection* findOrCreateConnection(const SockAddr& sockAddr, bool filterCreation = false);
   
    // privatized methods used by UDTTest - they are private since they must be called on the Socket thread
    ConnectionStats::Stats sampleStatsForConnection(const SockAddr& destination);
    
    std::vector<SockAddr> getConnectionSockAddrs();
    void connectToSendSignal(const SockAddr& destinationAddr, QObject* receiver, const char* slot);
    
    Q_INVOKABLE void writeReliablePacket(Packet* packet, const SockAddr& sockAddr);
    Q_INVOKABLE void writeReliablePacketList(PacketList* packetList, const SockAddr& sockAddr);
    
    NetworkSocket _networkSocket;
    PacketFilterOperator _packetFilterOperator;
    PacketHandler _packetHandler;
    MessageHandler _messageHandler;
    MessageFailureHandler _messageFailureHandler;
    ConnectionCreationFilterOperator _connectionCreationFilterOperator;

    Mutex _unreliableSequenceNumbersMutex;
    Mutex _connectionsHashMutex;

    std::unordered_map<SockAddr, BasePacketHandler> _unfilteredHandlers;
    std::unordered_map<SockAddr, SequenceNumber> _unreliableSequenceNumbers;
    std::unordered_map<SockAddr, std::unique_ptr<Connection>> _connectionsHash;

    QTimer* _readyReadBackupTimer { nullptr };

    int _maxBandwidth { -1 };

    std::unique_ptr<CongestionControlVirtualFactory> _ccFactory { new CongestionControlFactory<TCPVegasCC>() };

    bool _shouldChangeSocketOptions { true };

    int _lastPacketSizeRead { 0 };
    SequenceNumber _lastReceivedSequenceNumber;
    SockAddr _lastPacketSockAddr;
    
    friend UDTTest;
};

/// @}

} // namespace udt

#endif // hifi_Socket_h
