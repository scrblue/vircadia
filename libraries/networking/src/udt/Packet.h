//
//  Packet.h
//  libraries/networking/src/udt
//
//  Created by Clement on 7/2/15.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Packet_h
#define hifi_Packet_h

#include <memory>

#include <QtCore/QIODevice>

#include "BasePacket.h"
#include "PacketHeaders.h"
#include "SequenceNumber.h"

namespace udt {

///
/// The `Packet` is the basic Vircadia protocol packet. It extends the `BasePacket` with an explicit header format. It is
/// extended by the `NLPacket` class which applies a structured payload and is used as the "default" packet for Vircadia.
///
/// ```
///                              Packet Header Format:
///
///     0                   1                   2                   3
///     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
///    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///    |C|R|M| O |               Sequence Number                       |
///    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///    | P |                     Message Number                        |  Optional (only if M = 1)
///    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///    |                         Message Part Number                   |  Optional (only if M = 1)
///    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///    |                                                               |
///    |                         Payload (variable size)               |
///    |                                                               |
///    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///    
/// ```
///  C: Control bit -- this is always 0 in `Packet`s such as to distinguish it from a `ControlPacket` which also extends
///  `BasePacket`.
///  
///  R: Reliable bit
///  
///  M: Message bit
///  
///  O: Obfuscation level -- the obfuscation level of the payload -- this should not be used for security purposes
///    * 00 -- No obfuscation
///    * 01 -- Level 1
///    * 02 -- Level 2
///    * 03 -- Level 3
///    
///  P: Position bits -- where in the complete message a given packet should be placed.
///    * 00 -- ONLY
///    * 10 -- FIRST
///    * 11 -- MIDDLE
///    * 01 -- LAST 
///    
class Packet : public BasePacket {
    Q_OBJECT
public:
    ///
    /// The SequenceNumber is the unique, incrementing packet identifier. It is a 27-bit usigned integer that wraps around at
    /// the maximum value to continue counting at 0. Magnitude comparisons are conducted in the "forward" direction around the
    /// loop.
    /// 
    /// NOTE: While the SequenceNumber is only actually 29 bits, it is a 32-bit unsigned integer to leave room for a bit field
    /// 
    using SequenceNumberAndBitField = uint32_t;

    ///
    /// Given the packet is a or is a part of a message, a 30-bit unsigned integer identifies the message in question.
    /// NOTE: While th MessageNumber is only actually 30 bits, it is a 32-bit integer to leave room for the position bits
    /// 
    using MessageNumber = uint32_t;

    ///
    /// This is the 30-bit MessageNumber preceded by a 2-bit Position enum
    /// 
    using MessageNumberAndBitField = uint32_t;

    /// Given the packet is a or is a part of a message, a 32-bit unsigned integer identifies the position of the packet within
    /// the message identified by the message number
    using MessagePartNumber = uint32_t;

    // Use same size as MessageNumberAndBitField so we can use the enum with bitwise operations
    ///
    /// This enum represents the position bits in the packet and identifies where in the message the given packet should be
    /// placed 
    /// 
    enum PacketPosition : MessageNumberAndBitField {
        ONLY   = 0x0, // 00
        FIRST  = 0x2, // 10
        MIDDLE = 0x3, // 11
        LAST   = 0x1  // 01
    };

    // Use same size as SequenceNumberAndBitField so we can use the enum with bitwise operations
    ///
    /// This enum represents the level of obfuscation of the packet -- this is not used for security purposes.
    /// 
    enum ObfuscationLevel : SequenceNumberAndBitField {
        NoObfuscation = 0x0, // 00
        ObfuscationL1 = 0x1, // 01
        ObfuscationL2 = 0x2, // 10
        ObfuscationL3 = 0x3, // 11
    };

    ///
    /// Create a `Packet` and return a unique pointer to it 
    /// 
    static std::unique_ptr<Packet> create(qint64 size = -1, bool isReliable = false, bool isPartOfMessage = false);

    ///
    /// Create a `Packet` and return a unique pointer to it with data moved from the given bytearray
    /// 
    static std::unique_ptr<Packet> fromReceivedPacket(std::unique_ptr<char[]> data, qint64 size, const SockAddr& senderSockAddr);

    ///
    /// Simply deep-copy the given `Packet`. Provided for convenience, try to limit use.
    /// 
    static std::unique_ptr<Packet> createCopy(const Packet& other);

    ///
    /// Current level's header size
    /// 
    static int localHeaderSize(bool isPartOfMessage = false);

    ///
    /// Cumulated size of all the headers
    /// 
    static int totalHeaderSize(bool isPartOfMessage = false);

    ///
    /// The maximum payload size this packet can use to fit in MTU
    /// 
    static int maxPayloadSize(bool isPartOfMessage = false);

    ///
    /// Simply return whether the message bit is set.
    /// 
    bool isPartOfMessage() const { return _isPartOfMessage; }

    ///
    /// Simply return whether the reliable bit is set.
    /// 
    bool isReliable() const { return _isReliable; }

    ///
    /// Set the reliable bit to the given boolean value;
    /// 
    void setReliable(bool reliable) { _isReliable = reliable; }

    ///
    /// Returns the current ObfuscationLevel.
    /// 
    ObfuscationLevel getObfuscationLevel() const { return _obfuscationLevel; }

    ///
    /// Returns the current SequenceNumber.
    /// 
    SequenceNumber getSequenceNumber() const { return _sequenceNumber; }


    ///
    /// Returns the current MessageNumber.
    /// 
    MessageNumber getMessageNumber() const { return _messageNumber; }

    ///
    /// Returns the current PacketPosition enum variant
    /// 
    PacketPosition getPacketPosition() const { return _packetPosition; }

    ///
    /// Returns the MessagePartNumber
    /// 
    MessagePartNumber getMessagePartNumber() const { return _messagePartNumber; }

    ///
    /// Set everything related to the message information  including MessageNumber, PacketPosition, and MessagePartNumber of
    /// the `Packet`
    /// 
    void writeMessageNumber(MessageNumber messageNumber, PacketPosition position, MessagePartNumber messagePartNumber);

    ///
    /// Set the SequenceNumber of the `Packet`
    /// 
    void writeSequenceNumber(SequenceNumber sequenceNumber) const;

    /// Set the ObfuscationLevel of  the `Packet`
    void obfuscate(ObfuscationLevel level);

protected:
    Packet(qint64 size, bool isReliable = false, bool isPartOfMessage = false);
    Packet(std::unique_ptr<char[]> data, qint64 size, const SockAddr& senderSockAddr);
    
    Packet(const Packet& other);
    Packet(Packet&& other);
    
    Packet& operator=(const Packet& other);
    Packet& operator=(Packet&& other);

private:
    void copyMembers(const Packet& other);

    // Header readers - these read data to member variables after pulling packet off wire
    void readHeader() const;
    void writeHeader() const;

    // Simple holders to prevent multiple reading and bitwise ops
    mutable bool _isReliable { false };
    mutable bool _isPartOfMessage { false };
    mutable ObfuscationLevel _obfuscationLevel { NoObfuscation };
    mutable SequenceNumber _sequenceNumber { 0 };
    mutable MessageNumber _messageNumber { 0 };
    mutable PacketPosition _packetPosition { PacketPosition::ONLY };
    mutable MessagePartNumber _messagePartNumber { 0 };
};

} // namespace udt

Q_DECLARE_METATYPE(udt::Packet*);

#endif // hifi_Packet_h
