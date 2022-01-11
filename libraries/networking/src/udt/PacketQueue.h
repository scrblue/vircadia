//
//  PacketQueue.h
//  libraries/networking/src/udt
//
//  Created by Clement on 9/16/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PacketQueue_h
#define hifi_PacketQueue_h

#include <list>
#include <vector>
#include <memory>
#include <mutex>

#include "Packet.h"

namespace udt {
    
class PacketList;
    
using MessageNumber = uint32_t;

/// @addtogroup udt
/// @{

///
/// A queue of `Packet`s and extensions thereof (ie `NLPacket`s) to be sent reliably. Individual `Packet`s are added to a main
/// channel while `PacketList`s create a new `Channel` at the end of  the list.
/// 
/// `Packet`s are taken from each channel round-robin up until the 16th channel.
/// 
/// Used in the `SendQueue` class which implements the sending of packets on top of a `PacketQueue` instance.
/// 
class PacketQueue {
    using Mutex = std::recursive_mutex;
    using LockGuard = std::lock_guard<Mutex>;
    using PacketPointer = std::unique_ptr<Packet>;
    using PacketListPointer = std::unique_ptr<PacketList>;
    using RawChannel = std::list<PacketPointer>;
    using Channel = std::unique_ptr<RawChannel>;
    using Channels = std::list<Channel>;
    
public:
    ///
    /// Create a new `PacketQueue` with the given initial message number
    /// 
    PacketQueue(MessageNumber messageNumber = 0);

    ///
    /// Queue a `Packet` into the first channel
    /// 
    void queuePacket(PacketPointer packet);

    ///
	/// Queue a `PacketList` into a new channel in the list of channels. If there are more than sixteen channels already, these
	/// packets will not be taken from the queue until the channel is among the first sixteen.
	/// 
	/// If the `PacketList::isOrdered`, then the message number of every packet will be replaced by the next number tracked by the
	/// `PacketQueue`.
	/// 
    void queuePacketList(PacketListPointer packetList);

    ///
    /// Returns true if there is only the main channel in the internal channel list and if that channel is empty   
    /// 
    bool isEmpty() const;

    ///
    /// Takes `Packet`s one packet from one of the first sixteen channels in a round-robin fashion
    /// 
    PacketPointer takePacket();

    ///
    /// Lock a `recursive_mutex` guarding the channels
    /// 
    Mutex& getLock() { return _packetsLock; }

    ///
	/// Returns the message number last used when queuing an ordered `PacketList`
	/// 
    MessageNumber getCurrentMessageNumber() const { return _currentMessageNumber; }
    
private:
    MessageNumber getNextMessageNumber();

    MessageNumber _currentMessageNumber { 0 };
    
    mutable Mutex _packetsLock; // Protects the packets to be sent.
    Channels _channels; // One channel per packet list + Main channel

    Channels::iterator _currentChannel;
    unsigned int _channelsVisitedCount { 0 };
};

/// @}

}


#endif // hifi_PacketQueue_h
