//
//  PacketList.h
//
//
//  Created by Clement on 7/13/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PacketList_h
#define hifi_PacketList_h

#include <memory>

#include "../ExtendedIODevice.h"
#include "Packet.h"
#include "PacketHeaders.h"

class LimitedNodeList;

namespace udt {
    
class Packet;

/// @addtogroup udt
/// @{

/// A simple class for representing a list of `Packet`s, be a part of a single message or not, reliable or not.
class PacketList : public ExtendedIODevice {
    Q_OBJECT
public:
    using MessageNumber = uint32_t;
    using PacketPointer = std::unique_ptr<Packet>;

    ///
    /// Create a new `PacketList` with the given parameters. If the `isOrdered` flag is set, then the included `Packets` are
    /// assumed to be a part of a single message. The isReliable flag applies to all `Packet`s in the list by default.
    /// 
    static std::unique_ptr<PacketList> create(PacketType packetType, QByteArray extendedHeader = QByteArray(),
                                              bool isReliable = false, bool isOrdered = false);

    ///
    /// Create a new `PacketList` from an existing list of `Packet`s received over the network.
    /// 
    static std::unique_ptr<PacketList> fromReceivedPackets(std::list<std::unique_ptr<Packet>>&& packets);

    ///
    /// Returns the internal `PacketType` tracker whose value is set upon creation of the `PacketList`. This does not provide
    /// any internal utilty, but getting this value may be useful when the `PacketList` is meant to be one message for example
    /// 
    PacketType getType() const { return _packetType; }

    ///
    /// Return whether `Packet`s in the `PacketList` are reliable by default.
    /// 
    bool isReliable() const { return _isReliable; }

    ///
    /// Return whether `Packet`s in the `PacketList` are a part of one message by default.
    /// 
    bool isOrdered() const { return _isOrdered; }

    ///
    /// Returns the number of `Packet`s in the `PacketList`
    /// 
    size_t getNumPackets() const { return _packets.size() + (_currentPacket ? 1 : 0); }

    ///
    /// Returns the sum of every `Packet`'s `Packet::getDataSize` result.
    /// 
    size_t getDataSize() const;


    ///
    /// Returns the sum of every `Packet`'s `Packet::getPayloadSize` result.
    /// 
    size_t getMessageSize() const;
    
    ///
    /// Returns a bytearray with every `Packet` in the `PacketList`.
    ///
    QByteArray getMessage() const;

    ///
    /// Returns the internal extended header bytearray. Little logic is implemented around this member internally and it is
    /// provided primarily for extrenal use.
    /// 
    QByteArray getExtendedHeader() const { return _extendedHeader; }

    ///
    /// Sets the internal `_segmentStartIndex` to the current seek position in the current packet or the first byte of the
    /// current packet if there is not current packet.
    /// 
    void startSegment();

    ///
    ///  Sets the internal `_segmentstartindex` to -1;
    ///  
    void endSegment();

    ///
    /// Returns the maximum payload size for `Packet`s in this `PacketList`.
    /// 
    virtual qint64 getMaxSegmentSize() const { return Packet::maxPayloadSize(_isOrdered); }

    ///
    /// Returns the `Packet::getSenderSockAddr` of the first `Packet` in the `PacketList`.
    /// 
    SockAddr getSenderSockAddr() const;

    ///
    /// Stops writing the current `Packet` in being written and appends it to the `PacketList`.
    /// 
    void closeCurrentPacket(bool shouldSendEmpty = false);

    /// QIODevice virtual functions
    virtual bool isSequential() const override { return false; }
    virtual qint64 size() const override { return getDataSize(); }

    ///
    /// Writes the given string as a UTF-8 encoded bytearray and writes that to the current `Packet` being written setting one
    /// up if there is not one  already.
    /// 
    qint64 writeString(const QString& string);

    ///
    /// Gets the  time of receipt for the first `Packet` in the `PacketList` using `Packet::getReceiveTime`.
    /// 
    p_high_resolution_clock::time_point getFirstPacketReceiveTime() const;
    
    
protected:
    PacketList(PacketType packetType, QByteArray extendedHeader = QByteArray(), bool isReliable = false, bool isOrdered = false);
    PacketList(PacketList&& other);
    
    void preparePackets(MessageNumber messageNumber);

    virtual qint64 writeData(const char* data, qint64 maxSize) override;
    // Not implemented, added an assert so that it doesn't get used by accident
    virtual qint64 readData(char* data, qint64 maxSize) override { Q_ASSERT(false); return 0; }
    
    PacketType _packetType;
    std::list<std::unique_ptr<Packet>> _packets;

    bool _isOrdered = false;
    
private:
    friend class ::LimitedNodeList;
    friend class PacketQueue;
    friend class SendQueue;
    friend class Socket;

    Q_DISABLE_COPY(PacketList)
    
    // Takes the first packet of the list and returns it.
    template<typename T> std::unique_ptr<T> takeFront();
    
    // Creates a new packet, can be overriden to change return underlying type
    virtual std::unique_ptr<Packet> createPacket();
    std::unique_ptr<Packet> createPacketWithExtendedHeader();
    
    Packet::MessageNumber _messageNumber;
    bool _isReliable = false;
    
    std::unique_ptr<Packet> _currentPacket;
    
    int _segmentStartIndex = -1;
    
    QByteArray _extendedHeader;
};

/// @}

template<typename T> std::unique_ptr<T> PacketList::takeFront() {
    static_assert(std::is_base_of<Packet, T>::value, "T must derive from Packet.");
    
    auto packet = std::move(_packets.front());
    _packets.pop_front();
    return std::unique_ptr<T>(dynamic_cast<T*>(packet.release()));
}
    
}

Q_DECLARE_METATYPE(udt::PacketList*);

}


#endif // hifi_PacketList_h
