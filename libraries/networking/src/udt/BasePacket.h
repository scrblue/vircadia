//
//  BasePacket.h
//  libraries/networking/src/udt
//
//  Created by Stephen Birarda on 2015-07-23.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#ifndef hifi_BasePacket_h
#define hifi_BasePacket_h

#include <memory>

#include <PortableHighResolutionClock.h>

#include "../SockAddr.h"
#include "Constants.h"
#include "../ExtendedIODevice.h"

namespace udt {
/// @defgroup udt
/// @ingroup Networking
/// @{

///
/// The `BasePacket` class is a simple packet type for which Vircadia packet classes extend. A `BasePacket` by itself is
/// useless extended by a `Packet` or `ControlPacket`.
/// 
class BasePacket : public ExtendedIODevice {
    Q_OBJECT
public:
    ///
    /// The error code returned on a write error
    /// 
    static const qint64 PACKET_WRITE_ERROR;

    ///
    /// Creates a unique pointer to a new BasePacket with the given size
    /// 
    static std::unique_ptr<BasePacket> create(qint64 size = -1);

	///
    /// Creates a unique pointer to a new BasePacket whose data is taken from a bytearray that is a received packet
    /// 
    static std::unique_ptr<BasePacket> fromReceivedPacket(std::unique_ptr<char[]> data, qint64 size,
                                                          const SockAddr& senderSockAddr);
    ///
    /// Current level's header size
    /// 
    static int localHeaderSize();

    ///
    /// Cumulated size of all the headers
    /// 
    static int totalHeaderSize();

	///
    /// The maximum payload size this packet can use to fit in MTU
    /// 
    static int maxPayloadSize();
    
    ///
    /// Direct access to the payload, use responsibly!
    /// 
    char* getPayload() { return _payloadStart; }

    ///
    /// `const` variation of BasePacket::getPayload
    /// 
    const char* getPayload() const { return _payloadStart; }

    ///
    /// Return direct access to the entire packet, use responsibly!
    /// 
    char* getData() { return _packet.get(); }

    ///
    /// `const` variation of BasePacket::getData
    /// 
    const char* getData() const { return _packet.get(); }

   	///
    /// Returns the size of the packet, including the header
    /// 
    qint64 getDataSize() const { return (_payloadStart - _packet.get()) + _payloadSize; }

   	///
    /// Returns the size of the packet, including the header AND the UDP/IP header
    /// 
    qint64 getWireSize() const { return getDataSize() + UDP_IPV4_HEADER_SIZE; }
    
   	/// 
    /// Returns the size of the payload only
    /// 
    qint64 getPayloadSize() const { return _payloadSize; }

   	///
    /// Allows a writer to change the size of the payload used when writing directly
    /// 
    void setPayloadSize(qint64 payloadSize);

    ///
    /// Returns the number of bytes allocated for the payload
    /// 
    qint64 getPayloadCapacity() const  { return _payloadCapacity; }

    ///
    /// The number of bytes left to read in the packet as implemented on top of QIODevice
    ///
    qint64 bytesLeftToRead() const { return _payloadSize - pos(); }

    ///
    /// The number of bytes you can write to the packet based on it's payloadCapacity as implemented on top of QIODevice
    /// 
    qint64 bytesAvailableForWrite() const { return _payloadCapacity - pos(); }

    ///
    /// Returns the socket address of the sender of the packet
    ///
    SockAddr& getSenderSockAddr() { return _senderSockAddr; }

    ///
    /// `const` variation of BasePacket::getSenderSockAddr
    /// 
    const SockAddr& getSenderSockAddr() const { return _senderSockAddr; }
    
    // QIODevice virtual functions
    // WARNING: Those methods all refer to the payload ONLY and NOT the entire packet

    ///
    /// Returns that the payload of this packet is nonsequential
    /// See: <a href="https://doc.qt.io/qt-5/qiodevice.html#isSequential">the Qt documentation</a>
    /// 
    virtual bool isSequential() const override { return false; }

    ///
    /// Sets the read/write cursor to the start of the payload and returns whether it was a success
    /// See: <a href="https://doc.qt.io/qt-5/qiodevice.html#reset">the Qt documentation</a>
    ///
    virtual bool reset() override;

    ///
    /// Returns the payload capacity
    /// See: <a href="https://doc.qt.io/qt-5/qiodevice.html#size">the Qt documentation</a>
    /// 
    virtual qint64 size() const override { return _payloadCapacity; }

    using QIODevice::read; // Bring QIODevice::read methods to scope, otherwise they are hidden by following method

    ///
    /// Read up to the maxSize in bytes from the payload and return it as a bytearray
    /// See: <a href="https://doc.qt.io/qt-5/qiodevice.html#read-1">the Qt documentation</a>
    ///
    QByteArray read(qint64 maxSize);

    /// Read up to the maxSize in bytes from the payload and return it as a bytearray
    /// NOTE: This returns a value by reference so it can only be used if the packet will stay in scope
    QByteArray readWithoutCopy(qint64 maxSize);

    ///
	/// Writes a UTF-8 string to the BasePacket's payload
	/// 
    qint64 writeString(const QString& string);

    ///
    /// Reads the entire contents of a BasePacket's payload as a UTF-8 string and returns a QString
    /// 
    QString readString();

    ///
	/// Sets the packet's receiveTime to the give value
	/// 
    void setReceiveTime(p_high_resolution_clock::time_point receiveTime) { _receiveTime = receiveTime; }

    ///
    /// Returns the packet's receiveTime
    /// 
    p_high_resolution_clock::time_point getReceiveTime() const { return _receiveTime; }
    
protected:
    BasePacket(qint64 size);
    BasePacket(std::unique_ptr<char[]> data, qint64 size, const SockAddr& senderSockAddr);
    BasePacket(const BasePacket& other) : ExtendedIODevice() { *this = other; }
    BasePacket& operator=(const BasePacket& other);
    BasePacket(BasePacket&& other);
    BasePacket& operator=(BasePacket&& other);
    
    // QIODevice virtual functions
    virtual qint64 writeData(const char* data, qint64 maxSize) override;
    virtual qint64 readData(char* data, qint64 maxSize) override;
    
    void adjustPayloadStartAndCapacity(qint64 headerSize, bool shouldDecreasePayloadSize = false);
    
    qint64 _packetSize = 0;                           // Total size of the allocated memory
    std::unique_ptr<char[]> _packet;                  // Allocated memory
    
    char* _payloadStart = nullptr;                    // Start of the payload
    qint64 _payloadCapacity = 0;                      // Total capacity of the payload
    
    qint64 _payloadSize = 0;                          // How much of the payload is actually used
    
    SockAddr _senderSockAddr;                         // sender address for packet (only used on receiving end)

    p_high_resolution_clock::time_point _receiveTime; // captures the time the packet received (only used on receiving end)
};

/// @}
} // namespace udt


#endif // hifi_BasePacket_h
