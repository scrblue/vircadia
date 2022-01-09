//
//  ControlPacket.h
//  libraries/networking/src/udt
//
//  Created by Stephen Birarda on 2015-07-24.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#ifndef hifi_ControlPacket_h
#define hifi_ControlPacket_h

#include <vector>

#include "BasePacket.h"
#include "Packet.h"

namespace udt {
/// @addtogroup udt
/// @{

///
/// The `ControlPacket` extends the `BasePacket` and provides a schema for confirming connections and acknowledging the receipt
/// ```
///                               ControlPacket Format:
///     0                   1                   2                   3
///     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
///    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///    |C|           Type              |          (unused)             |
///    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///    |                          Control Data                         |
///    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
/// ```
/// C: Control bit -- this is always 1 in `ControlPacket`s

class ControlPacket : public BasePacket {
    Q_OBJECT
public:
    ///
    /// The ControlBit and Type take up the first 16 bits of the `ControlPacket` as per the diagram for the `ControlPacket`
    /// 
    using ControlBitAndType = uint32_t;

    ///
    /// The type of `ControlPacket` represented as an enum with members `ACK`, `Handshake`, `HandshakeACK`, and
    /// `HandshakeRequest`
    /// 
    enum Type : uint16_t {
        ACK,
        Handshake,
        HandshakeACK,
        HandshakeRequest
    };

    ///
    /// Create a new `ControlPacket` from a given type and control data size.
    /// 
    static std::unique_ptr<ControlPacket> create(Type type, qint64 size = -1);

    ///
    /// Create a new `ControlPacket`, moving the contents from the given bytearray
    /// 
    static std::unique_ptr<ControlPacket> fromReceivedPacket(std::unique_ptr<char[]> data, qint64 size,
                                                             const SockAddr& senderSockAddr);

    ///
    /// Get the current level's header size
    /// 
    static int localHeaderSize();

    ///
    /// Get the cumulated size of all the headers
    /// 
    static int totalHeaderSize();

    ///
    /// Get the maximum payload size this packet can use to fit in MTU
    /// 
    static int maxPayloadSize();

    ///
    /// Get the `Type` of this `ControlPacket`
    /// 
    Type getType() const { return _type; }

    ///
    /// Set the `Type` of this `ControlPacket`
    /// 
    void setType(Type type);
    
private:
    Q_DISABLE_COPY(ControlPacket)
    ControlPacket(Type type, qint64 size = -1);
    ControlPacket(std::unique_ptr<char[]> data, qint64 size, const SockAddr& senderSockAddr);
    ControlPacket(ControlPacket&& other);
    
    ControlPacket& operator=(ControlPacket&& other);
    
    // Header read/write
    void readType();
    void writeType();
    
    Type _type;
};

/// @}
} // namespace udt


#endif // hifi_ControlPacket_h
