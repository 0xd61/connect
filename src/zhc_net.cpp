// NOTE(dgl): we are optimizing the network traffic just for fun and education...
internal void
stream_flush(Bitstream *buffer)
{
    assert(buffer->index < buffer->count, "Bitstream buffer overflow");
    uint32 *block = buffer->data + buffer->index++;

    // NOTE(dgl): flush the lower 32 bits to the stream buffer
#if ZHC_BIG_ENDIAN
    // NOTE(dgl): we need to have everything in little endian order to ensure we keep the zeros
    // // TODO(dgl): This is not tested!! We need to verify if this works as expected.
    *block = bswap32(cast(uint32)(buffer->scratch));
#else
    *block = cast(uint32)(buffer->scratch);
#endif

    buffer->scratch >>= (sizeof(*buffer->data) * 8);
    buffer->scratch_bits -= (sizeof(*buffer->data) * 8);
}

internal void
stream_write_bits(Bitstream *buffer, uint32 value, int32 bit_count)
{
    // NOTE(dgl): it is not allowed to shift by 32 on a 32bit integer
    // the value can be 1 if it was 0xFFFFFFFF. Otherwise it will be 0
    assert((value >> (bit_count - 1)) <= 1, "Value cannot be bigger than bit_count");

    uint64 aligned_bits = cast(uint64)value << buffer->scratch_bits;

    buffer->scratch |= aligned_bits;
    buffer->scratch_bits += bit_count;

    if(buffer->scratch_bits > (sizeof(*buffer->data) * 8))
    {
        stream_flush(buffer);
    }
}

internal uint32
stream_read_bits(Bitstream *buffer, int32 bit_count)
{
    uint32 result = 0;

    if(bit_count > buffer->scratch_bits)
    {
        assert(buffer->index < buffer->count, "Bitstream buffer overflow");
#if ZHC_BIG_ENDIAN
        // NOTE(dgl): we need to have everything in little endian order to ensure we keep the zeros
        // TODO(dgl): This is not tested!! We need to verify if this works as expected.
        uint64 aligned_bits = (cast(uint64)bswap32(buffer->data[buffer->index++])) << buffer->scratch_bits;
#else
        uint64 aligned_bits = (cast(uint64)(buffer->data[buffer->index++])) << buffer->scratch_bits;
#endif

        buffer->scratch |= aligned_bits;
        buffer->scratch_bits += (sizeof(*buffer->data) * 8);
    }

    result = cast(uint32)(buffer->scratch & ((1LLU << bit_count) - 1));
    buffer->scratch >>= bit_count;
    buffer->scratch_bits -= bit_count;

    return(result);
}

internal Bitstream
stream_reader_init(uint8 *buffer, usize buffer_size)
{
    Bitstream result = {};
    result.is_reading = true;
    result.is_writing = false;
    result.data = cast(uint32 *)buffer;
    result.count = buffer_size / sizeof(*result.data);

    return(result);
}

internal Bitstream
stream_writer_init(uint8 *buffer, usize buffer_size)
{
    Bitstream result = {};
    result.is_writing = true;
    result.is_reading = false;
    result.data = cast(uint32 *)buffer;
    result.count = buffer_size / sizeof(*result.data);

    return(result);
}

internal void
pack_uint32(Bitstream *buffer, uint32 value, uint32 min, uint32 max)
{
    assert((value <= max && value >= min), "Value is not between min and max");
    int32 bit_count = bits_required(max - min);
    stream_write_bits(buffer, value - min, bit_count);
}

internal uint32
unpack_uint32(Bitstream *buffer, uint32 min, uint32 max)
{
    uint32 result = 0;
    int32 bit_count = bits_required(max - min);
    uint32 value = stream_read_bits(buffer, bit_count);
    result = dgl_clamp(value + min, min, max);

    return(result);
}

internal usize
serialize_packet(Bitstream *buffer, Packet *packet)
{
    usize result = 0;

    // TODO(dgl): handle different versions

    // NOTE(dgl): default serialization
    if(buffer->is_writing)
    {
        stream_write_bits(buffer, cast(uint32)packet->id, 32);
        stream_write_bits(buffer, packet->version, 32);
        stream_write_bits(buffer, cast(uint32)(packet->salt & 0xFFFFFFFF), 32);
        stream_write_bits(buffer, cast(uint32)(packet->salt >> 32), 32);
        pack_uint32(buffer, cast(uint32)packet->type, 0, Packet_Type_Max - 1);
        pack_uint32(buffer, cast(uint32)packet->msg_type, 0, Net_Message_Max - 1);
    }
    else if(buffer->is_reading)
    {
        packet->id = cast(int32)stream_read_bits(buffer, 32);
        uint32 tmp_version = stream_read_bits(buffer, 32);
        packet->version = dgl_clamp(tmp_version, 1, 0xFFFFFFFF);
        uint64 tmp_salt = 0;
        tmp_salt = cast(uint64)stream_read_bits(buffer, 32);
        tmp_salt |= (cast(uint64)stream_read_bits(buffer, 32)) << 32;
        packet->salt = tmp_salt;
        packet->type = cast(Packet_Type)unpack_uint32(buffer, 0, Packet_Type_Max - 1);
        packet->msg_type = cast(Net_Message_Type)unpack_uint32(buffer, 0, Net_Message_Max - 1);
    }
     else
    {
        assert(false, "Bitstream buffer must be reading or writing");
    }

    // NOTE(dgl): type specific serialization
    switch(packet->type)
    {
        case Packet_Type_Chunk:
        {
            if(buffer->is_writing)
            {
                stream_write_bits(buffer, cast(uint32)packet->chunk.hash, sizeof(packet->chunk.hash)*8);
                pack_uint32(buffer, packet->chunk.last_slice_size, 0, NET_MTU_SIZE);
                pack_uint32(buffer, packet->chunk.slice_count, 0, NET_MTU_SIZE*8);
            }
            else
            {
                packet->chunk.hash = cast(uint16)stream_read_bits(buffer, sizeof(packet->chunk.hash)*8);
                packet->chunk.last_slice_size = unpack_uint32(buffer, 0, NET_MTU_SIZE);
                packet->chunk.slice_count = unpack_uint32(buffer, 0, NET_MTU_SIZE*8);
            }
        } break;
        case Packet_Type_Slice:
        {
            if(buffer->is_writing)
            {
                stream_write_bits(buffer, cast(uint32)packet->slice.hash, sizeof(packet->slice.hash)*8);
                pack_uint32(buffer, packet->slice.index, 0, NET_MTU_SIZE*8);
            }
            else
            {
                packet->slice.hash = cast(uint16)stream_read_bits(buffer, sizeof(packet->slice.hash)*8);
                packet->slice.index = unpack_uint32(buffer, 0, NET_MTU_SIZE*8);
            }
        } break;
        case Packet_Type_Ack:
        {
            if(buffer->is_writing)
            {
                stream_write_bits(buffer, cast(uint32)packet->ack.hash, sizeof(packet->ack.hash)*8);
            }
            else
            {
                 packet->ack.hash = cast(uint16)stream_read_bits(buffer, sizeof(packet->ack.hash)*8);
            }
        } break;
        default:
        {
            //LOG_DEBUG("Packet type %d not serialized", packet->type);
        }
    }

    if(buffer->is_writing)
    {
        stream_flush(buffer);
    }

    result = buffer->index * sizeof(*buffer->data);
    return(result);
}

internal Zhc_Net_Address
parse_address(char *ip, uint16 port)
{
    Zhc_Net_Address result = {};

    int32 index = 0;
    int32 number = 0;
    while(*ip)
    {
        if(*ip == '.')
        {
            assert(number <= 0xFF && number >= 0, "Invalid IP segment (cannot be bigger than 255)");
            result.ip[index++] = cast(uint8)number;
            number = 0;
            ++ip;
        }

        number *= 10;
        number += *ip - '0';

        ++ip;
    }

    assert(number <= 0xFF && number >= 0, "Invalid IP segment (cannot be bigger than 255)");
    assert(index == 3, "Invalid ip address");
    result.ip[index] = cast(uint8)number;
    result.port = port;

    return(result);
}

internal bool32
address_compare(Zhc_Net_Address a, Zhc_Net_Address b)
{
    bool32 result = false;
    if((a.host == b.host) &&
       (a.port == b.port))
    {
        result = true;
    }

    return(result);
}

internal usize
get_serialized_packet_size(Packet_Type type)
{
    // TODO(dgl): need a better way get the packet size. We should be able to get this on compile time
    // for each type.
    usize result = 0;

    Packet tmp = {};
    Bitstream writer = stream_writer_init(cast(uint8 *)&tmp, sizeof(tmp));
    tmp.type = type;

    result = serialize_packet(&writer, &tmp);
    return(result);
}

//
//
//

// NOTE(dgl): ignores disconnected connections
internal Net_Conn_ID
get_connection(Connection_List *conns, Zhc_Net_Address address)
{
    Net_Conn_ID result = -1;

    for(int32 index = 0; index < conns->max_count; ++index)
    {
        if(conns->state[index] == Net_Conn_State_Disconnected)
        {
            continue;
        }

        if(address_compare(conns->address[index], address))
        {
            result = index;
            break;
        }
    }

    return(result);
}

internal Net_Conn_ID
get_free_conn(Connection_List *conns)
{
    Net_Conn_ID result = -1;

    int32 index = conns->index;
    while(++index != conns->index)
    {
        // NOTE(dgl): we increase the counter first and then check the bound.
        // this here is fine, because we will always have at least 1 element.
        if(index >= conns->max_count) { index = 0; }

        assert(index < conns->max_count, "Index is out of bounds");
        // NOTE(dgl): We consider connecting connections as "free" to prevent
        // opening connections fill up the conn slots and never do the full handshake.
        if(conns->state[index] != Net_Conn_State_Connected)
        {
            result = index;
            // NOTE(dgl): we set the starting index to the result id, to prevent
            // that it is overwritten on the next connection request because we always
            // return the first not established connection in the list.
            conns->index = result;
            break;
        }
    }

    return(result);
}

internal Net_Conn_ID
push_connection(Connection_List *conns, Zhc_Net_Address address, uint64 salt)
{
    // NOTE(dgl): get_connection returns the first found connection with the address.
    // This ensures that all addresses are unique. When pushing an
    // existing address it simply gets overwritten. This should be no performance issue because
    // we only check on new incoming connections.
    Net_Conn_ID result = -1;
    Net_Conn_ID existing = get_connection(conns, address);
    if(existing >= 0)
    {
        result = existing;
    }
    else
    {
        result = get_free_conn(conns);
    }

    if(result >= 0)
    {
        // NOTE(dgl): reset the fields and set the state to connecting, as we expect packets to be send/received.
        conns->state[result] = Net_Conn_State_Connecting;
        conns->address[result] = address;
        conns->salt[result] = salt;
        conns->no_timeout[result] = true;
    }
    else
    {
        LOG_DEBUG("No free connection available");
    }

    return(result);
}

internal Packet
default_packet(Packet_Type type)
{
    Packet result = {};
    result.id = 0x1234;
    result.version = parse_version(ZHC_VERSION);
    result.type = type;

    // NOTE(dgl): salt is added when we serialize and put the packet into the packet buffer
    // because then we have the connection index available.

    return(result);
}

internal void
packet_buffer_append(Packet_Buffer *buffer, uint8 *payload, usize payload_size)
{
    assert(buffer->offset > 0, "Packet buffer must be initialized before appending");
    // TODO(dgl): are we one off here?
    assert(buffer->offset + payload_size <= array_count(buffer->data), "Payload too large. Packet buffer overflow");

    dgl_memcpy(buffer->data + buffer->offset, payload, payload_size);

    LOG_DEBUG("Appending payload with %llu bytes to the buffer at %p (%llu bytes).", payload_size, buffer->data, buffer->offset);
    buffer->offset += payload_size;
}

internal Packet_Buffer*
packet_buffer_init(Connection_List *conns, Net_Conn_ID index, Packet packet)
{
    assert(index >= 0, "Invalid index");
    Packet_Buffer *result = 0;

    packet.salt = conns->salt[index];

    assert(conns->packet_buffer, "Packet buffer array not initialized");
    Packet_Buffer *packet_buffer = conns->packet_buffer + index;
    usize buffer_size = array_count(packet_buffer->data);

    Bitstream writer = stream_writer_init(packet_buffer->data, buffer_size);
    packet_buffer->offset = serialize_packet(&writer, &packet);

    // NOTE(dgl): zeroing remaining buffer
    dgl_memset(packet_buffer->data + packet_buffer->offset, 0, buffer_size - packet_buffer->offset);
    LOG_DEBUG("Initializing packet of type %d with %llu bytes of data into the buffer at %p (Salt: %llx)", packet.type, packet_buffer->offset, packet_buffer->data, packet.salt);

    result = packet_buffer;
    return(result);
}

// NOTE(dgl): closes current socket, if it exists.
internal void
net_open_socket(Net_Context *ctx)
{
    Zhc_Net_Socket *socket = &ctx->socket;
    // NOTE(dgl): we cannot notify the peers if the socket has an error. They will have to try
    // and get a denied packet when trying to send data. Then they have to reauthenticate.
    dgl_memset(ctx->conns->state, Net_Conn_State_Disconnected, cast(usize)ctx->conns->max_count);
    platform.close_socket(socket);
    // TODO(dgl): arena not needed. Will be removed later.
    platform.open_socket(0, socket);
    assert(socket->handle.no_error, "Failed to open socket");
    LOG_DEBUG("Listening for connection: %d.%d.%d.%d:%d", socket->address.ip[0], socket->address.ip[1], socket->address.ip[2], socket->address.ip[3], socket->address.port);
}

// NOTE(dgl): must be sent without a packet buffer available. Therefore
// this call is separate.
internal void
send_denied_packet(Net_Context *ctx, Zhc_Net_Address address)
{
    Packet packet = default_packet(Packet_Type_Denied);

    uint8 buffer[NET_MTU_SIZE] = {};
    Bitstream writer = stream_writer_init(buffer, array_count(buffer));
    usize count = serialize_packet(&writer, &packet);

    platform.send_data(&ctx->socket, &address, buffer, count);
    LOG_DEBUG("Sending denied packet");
}

internal void
send_discovery_packet(Net_Context *ctx)
{
    Packet packet = default_packet(Packet_Type_Server_Discovery);

    uint8 buffer[NET_MTU_SIZE] = {};
    Bitstream writer = stream_writer_init(buffer, array_count(buffer));
    usize count = serialize_packet(&writer, &packet);

    // TODO(dgl): should we use a subnet broadcast?
    Zhc_Net_Address address = { .host=0xFFFFFFFF, .port=ZHC_SERVER_PORT };

    platform.send_data(&ctx->socket, &address, buffer, count);
    LOG_DEBUG("Sending discovery packet");
}

internal Net_Context *
net_init_server(DGL_Mem_Arena *arena)
{
    Net_Context *result = dgl_mem_arena_push_struct(arena, Net_Context);
    result->chunk_buffer_size = ZHC_MAX_FILESIZE;
    result->chunk_buffer = dgl_mem_arena_push_array(arena, uint8, result->chunk_buffer_size);
    result->conns = dgl_mem_arena_push_struct(arena, Connection_List);
    {
        Connection_List *conns = result->conns;
        conns->max_count = NET_MAX_CLIENTS;
        usize casted_count = cast(usize)conns->max_count;
        conns->no_timeout = dgl_mem_arena_push_array(arena, bool32, casted_count);
        dgl_memset(conns->no_timeout, false, sizeof(*conns->no_timeout)*casted_count);
        conns->address = dgl_mem_arena_push_array(arena, Zhc_Net_Address, casted_count);
        conns->salt = dgl_mem_arena_push_array(arena, uint64, casted_count);
        conns->last_packet_hash = dgl_mem_arena_push_array(arena, uint32, casted_count);
        conns->packet_buffer = dgl_mem_arena_push_array(arena, Packet_Buffer, casted_count);
        conns->state = dgl_mem_arena_push_array(arena, Net_Conn_State, casted_count);
    }

    result->socket.address.port = ZHC_SERVER_PORT;

    result->is_server = true;

    return(result);
}

internal Net_Context *
net_init_client(DGL_Mem_Arena *arena)
{
    Net_Context *result = dgl_mem_arena_push_struct(arena, Net_Context);
    result->chunk_buffer_size = ZHC_MAX_FILESIZE;
    result->chunk_buffer = dgl_mem_arena_push_array(arena, uint8, result->chunk_buffer_size);
    result->conns = dgl_mem_arena_push_struct(arena, Connection_List);
    {
        Connection_List *conns = result->conns;
        // NOTE(dgl): if we allow more than one connections here, we need a way to only
        // use the fastest connection or ignore already received packets. This is currently
        // not needed. We maybe develop something like this just for educational reasons in the
        // future.
        conns->max_count = 1;
        usize casted_count = cast(usize)conns->max_count;
        conns->no_timeout = dgl_mem_arena_push_array(arena, bool32, casted_count);
        dgl_memset(conns->no_timeout, false, sizeof(*conns->no_timeout)*casted_count);
        conns->address = dgl_mem_arena_push_array(arena, Zhc_Net_Address, casted_count);
        conns->salt = dgl_mem_arena_push_array(arena, uint64, casted_count);
        conns->last_packet_hash = dgl_mem_arena_push_array(arena, uint32, casted_count);
        conns->packet_buffer = dgl_mem_arena_push_array(arena, Packet_Buffer, casted_count);
        conns->state = dgl_mem_arena_push_array(arena, Net_Conn_State, casted_count);
    }

    result->is_server = false;

    return(result);
}

internal void
net_send_packet_buffer(Net_Context *ctx, Net_Conn_ID index)
{
    Connection_List *conns = ctx->conns;
    assert(conns->packet_buffer, "Packet buffer not initialized");
    assert(index >= 0, "Invalid connection index");

    Packet_Buffer *buffer = conns->packet_buffer + index;
    Zhc_Net_Address *address = conns->address + index;
    LOG_DEBUG("Sending packet buffer to conn index %d (%llu bytes)", index, buffer->offset);
    platform.send_data(&ctx->socket, address, buffer->data, buffer->offset);
}

internal void
net_send_pending_packet_buffers(Net_Context *ctx)
{
    Connection_List *conns = ctx->conns;
    assert(conns->packet_buffer, "Packet buffer not initialized");

    for(int32 index = 0; index < conns->max_count; ++index)
    {
        if(conns->state[index] == Net_Conn_State_Connecting)
        {
            net_send_packet_buffer(ctx, index);
        }
    }
}

internal void
net_request_server_connection(Net_Context *ctx)
{
    assert(!ctx->is_server, "Only clients can connect to a server");
    // NOTE(dgl): a client only has one connection slot available.
    // if this is not connected, we send a discovery packet to request
    // the connection to a server. We connect to the server replying the fastest.
    // However, currently we do not support having multiple servers.
    if(ctx->conns->state[0] == Net_Conn_State_Disconnected)
    {
        send_discovery_packet(ctx);
    }
}

internal bool32
chunk_complete(uint8 *buffer, usize buffer_size, uint32 bit_count)
{
    bool32 result = true;

    int32 required_bytes = dgl_safe_uint32_to_int32(bit_count / 8) + 1;
    assert(required_bytes <= buffer_size, "Bit count is larger than bytes available in buffer");

    uint8 last_bits = cast(uint8)(bit_count % 8);
    uint8 last_mask_bits = cast(uint8)~(0xFFFFFFFF << (last_bits));

    int32 index = required_bytes - 1;
    LOG_DEBUG("Chunk buffer index 0x%X, mask 0x%X", buffer[index], last_mask_bits);
    if(buffer[index--] != last_mask_bits)
    {
        result = false;
    }

    while(index >= 0)
    {
        // NOTE(dgl): @performance if this is too slow switch to 64bit integers
        LOG_DEBUG("Chunk buffer index 0x%X, mask 0x%X", buffer[index], 0xFF);
        if(buffer[index--] == 0xFF)
        {
            result = false;
            break;
        }
    }


    return(result);
}

// NOTE(dgl): Make sure to send the chunk packet before sending the chunk buffer!
// Otherwise the packets will be ignored by the client.
internal void
send_chunk_buffer(Net_Context *ctx, Net_Conn_ID index, uint8 *ack_mask, usize ack_mask_size)
{
    usize slice_space = NET_MTU_SIZE - get_serialized_packet_size(Packet_Type_Slice);
    Packet packet = default_packet(Packet_Type_Slice);
    packet.slice.hash = ctx->chunk_info.hash;
    uint8 *root = ctx->chunk_buffer;
    for(uint32 slice_index = 0;
        slice_index < ctx->chunk_info.slice_count;
        ++slice_index)
    {
        if(ack_mask)
        {
            // NOTE(dgl): if the bit in the mask is set to 1 the slice has already been received.
            // We can skip it in here.
            int32 mask_byte = slice_index / 8;
            uint32 mask_bit = 1 << (slice_index % 8);
            assert(mask_byte < ack_mask_size, "Invalid ack mask byte");

            // TODO(dgl): this check does not work properly yet.
            if((ack_mask[mask_byte] & mask_bit) == mask_bit)
            {
                LOG_DEBUG("Slice %u already received by the client. Skipping...", slice_index);
                continue;
            }
        }

        usize size = 0;
        if(slice_index == ctx->chunk_info.slice_count - 1)
        {
            size = ctx->chunk_info.last_slice_size;
        }
        else
        {
            size = slice_space;
        }

        packet.slice.index = slice_index;
        Packet_Buffer *buffer = packet_buffer_init(ctx->conns, index, packet);
        packet_buffer_append(buffer, root, size);
        root += size;

        net_send_packet_buffer(ctx, index);
        LOG_DEBUG("Sending slice %u (%llu bytes)", packet.slice.index, size);
    }
}

// NOTE(dgl): messy. Needs a refactor @cleanup
internal Net_Conn_ID
net_recv_message(DGL_Mem_Arena *arena, Net_Context *ctx, real32 frametime_in_ms, Net_Message *message)
{
    Connection_List *conns = ctx->conns;
    // NOTE(dgl): index is used for the internal connection index. If we want to return
    // the packet/message we put the index into result. This indicates the client that
    // we received a message. Otherwise the messages are handled internal. We loop and
    // handle all packets until we have a packet which is returned as message to the client.
    Net_Conn_ID result = -1;
    Net_Conn_ID index = -1;

    // NOTE(dgl): disconnect connections which were not updated in the last x seconds.
    ctx->message_timeout += frametime_in_ms;
    if(ctx->message_timeout >= NET_CONN_TIMEOUT)
    {
        LOG_DEBUG("Checking connection timeouts");
        ctx->message_timeout = 0.0f;
        for(int32 index = 0; index < conns->max_count; ++index)
        {
            if(conns->no_timeout[index] == false)
            {
                // NOTE(dgl): We do not have to send a message here. If we hit a timeout, there is something
                // wrong with this connection and the message will most likely not receive the peer.
                LOG_DEBUG("Disconnecting index %d", index);
                conns->state[index] = Net_Conn_State_Disconnected;
            }
        }
        dgl_memset(conns->no_timeout, false, sizeof(*conns->no_timeout)*cast(usize)conns->max_count);
    }

    bool32 chunk_buffer_updated = false;
    Zhc_Net_Address address = {};
    usize memory_max_size = NET_MTU_SIZE;
    usize memory_size = 0;
    usize memory_offset = 0;
    uint8 *memory = dgl_mem_arena_push_array(arena, uint8, memory_size);
    while((memory_size = platform.receive_data(&ctx->socket, &address, memory, memory_max_size)) > 0)
    {
        memory_offset = 0;

        // NOTE(dgl): parse packet from memory buffer
        Packet packet = {};
        Bitstream reader = stream_reader_init(memory, memory_size);
        usize packet_header_size = serialize_packet(&reader, &packet);
        memory_offset = packet_header_size;

        assert(memory_size >= memory_offset, "Packet memory offset cannot be bigger than the memory size");
        uint8 *payload = memory + memory_offset;
        usize payload_size = memory_size - memory_offset;

        LOG_DEBUG("Received packet (%d bytes) - Salt: %llx, Type: type %d, Header: %d bytes", memory_size, packet.salt, packet.type, packet_header_size);

        // NOTE(dgl): handle packet

        if(packet.id != 0x1234) { continue; }

        // NOTE(dgl): we mix client and server packets in here because they are handled very similarly.
        // If this causes too much complexity it is easy to separate them. However in my opinion
        // this is a cleaner code, at least for the current state. I hope I'll find a better solution
        // because I don't really like mixing those.
        // TODO(dgl): the disconnect/denied state is not really defined. Must we send a disconnect packet
        // on each denied packet, to ensure a connection is reset if it has a connection state?
        index = get_connection(conns, address);
        if(index >= 0) { conns->no_timeout[index] = true; }

        if(packet.type > _Packet_Type_Connected)
        {
            if(index >= 0 &&
               conns->state[index] > Net_Conn_State_Disconnected &&
               conns->salt[index] == packet.salt)
            {
                conns->state[index] = Net_Conn_State_Connected;

                switch(packet.type) {
                case Packet_Type_Disconnect:
                    {
                        conns->state[index] = Net_Conn_State_Disconnected;
                    } break;
                case Packet_Type_Payload:
                    {
                        message->type = packet.msg_type;
                        message->payload = payload;
                        message->payload_size = payload_size;
                        result = index;
                    } break;
                case Packet_Type_Chunk:
                    {
                        if(ctx->chunk_info.hash != packet.chunk.hash)
                        {
                            chunk_buffer_updated = true;
                            usize slice_size = NET_MTU_SIZE - get_serialized_packet_size(Packet_Type_Slice);
                            usize chunk_size = (slice_size*packet.chunk.slice_count) - (slice_size - packet.chunk.last_slice_size);
                            assert(chunk_size < ctx->chunk_buffer_size, "Chunk buffer overflow");

                            ctx->chunk_info.slice_count = packet.chunk.slice_count;
                            ctx->chunk_info.hash = packet.chunk.hash;
                            ctx->chunk_info.last_slice_size= packet.chunk.last_slice_size;
                            ctx->chunk_type = packet.msg_type;

                            dgl_memset(ctx->ack_buffer.data, 0, array_count(ctx->ack_buffer.data));
                            LOG_DEBUG("Prepare receiving new chunk %u of size %llu", packet.chunk.hash, chunk_size);
                        }
                    } break;
                case Packet_Type_Slice:
                    {
                        if(ctx->chunk_info.hash == packet.slice.hash)
                        {
                            assert(packet.slice.index < ctx->chunk_info.slice_count, "Invalid slice index");
                            chunk_buffer_updated = true;
                            usize slice_size = NET_MTU_SIZE - get_serialized_packet_size(Packet_Type_Slice);
                            usize offset = cast(usize)packet.slice.index * slice_size;

                            // TODO(dgl): @cleanup should be returned by recv_packet
                            // It would be great to have recv_packet and recv_payload separately.
                            dgl_memcpy(ctx->chunk_buffer + offset, payload, payload_size);

                            usize ack_size = get_serialized_packet_size(Packet_Type_Ack);
                            assert((ctx->chunk_info.slice_count / 8) + 1 <= array_count(ctx->ack_buffer.data) - ack_size, "Cannot have more slices than bits in the ack buffer");
                            int32 mask_byte = packet.slice.index / 8;
                            uint32 mask_bit = 1 << (packet.slice.index % 8);
                            ctx->ack_buffer.data[mask_byte] |= mask_bit;
                        }
                    } break;
                case Packet_Type_Ack:
                    {
                        if(packet.ack.hash == ctx->chunk_info.hash)
                        {
                            uint8 *ack_buffer = payload;
                            usize ack_buffer_size = payload_size;
                            assert(ack_buffer, "Invalid ack buffer");
                            if(!chunk_complete(ack_buffer, ack_buffer_size, ctx->chunk_info.slice_count))
                            {
                                send_chunk_buffer(ctx, index, ack_buffer, ack_buffer_size);
                            }
                        }
                    } break;
                default:
                    {
                        message->type = packet.msg_type;
                        message->payload = 0;
                        message->payload_size = 0;
                        result = index;
                    }
                }
            }
            else
            {
                LOG_DEBUG("Invalid packet.");
                // TODO(dgl): I think the easiest way would be for now to rely on
                // connection timeouts. But we need a proper solution for this.
//                 if(index >= 0)
//                 {
//                     Net_Message message = {};
//                     message.type = Net_Message_Disconnect;
//                     net_send_message(ctx, index, message);
//                     LOG_DEBUG("Send disconnect message");
//                     conns->state[index] = Net_Conn_State_Disconnected;
//                 }
            }
        }
        else
        {
            // NOTE(dgl): Handle connection handshake packets
            if(index < 0)
            {
                // NOTE(dgl): handling connection requests
                if((ctx->is_server && packet.type == Packet_Type_Server_Discovery) ||
                   (!ctx->is_server && packet.type == Packet_Type_Request))
                {
                    uint64 salt = 0;
                    get_random_bytes(cast(uint8 *)&salt, sizeof(salt));
                    index = push_connection(conns, address, salt);
                    LOG_DEBUG("Connection request from %u.%u.%u.%u:%u", address.ip[0], address.ip[1], address.ip[2], address.ip[3], address.port);
                    if(index < 0)
                    {
                        LOG("No free connection available");
                        send_denied_packet(ctx, address);
                    }
                    else
                    {
                        if(ctx->is_server)
                        {
                            Packet resp = default_packet(Packet_Type_Request);
                            packet_buffer_init(conns, index, resp);
                        }
                        else
                        {
                            Packet resp = default_packet(Packet_Type_Challenge);
                            packet_buffer_init(conns, index, resp);
                            conns->salt[index] ^= packet.salt;
                        }
                    }
                }
                else
                {
                    LOG_DEBUG("Invalid connection request of type %d from from %u.%u.%u.%u:%u", packet.type, address.ip[0], address.ip[1], address.ip[2], address.ip[3], address.port);
                    send_denied_packet(ctx, address);
                }
            }
            else
            {
                // NOTE(dgl): connection which are disconnected are ignored.
                if(conns->state[index] > Net_Conn_State_Disconnected)
                {
                    // NOTE(dgl): if the connection is already connected, we simply ignore the packet.
                    // this also makes sure only not established connections can receive a denied packet.
                    if(conns->state[index] == Net_Conn_State_Connected) { continue; }

                    switch(packet.type) {
                    case Packet_Type_Denied:
                        {
                            conns->state[index] = Net_Conn_State_Disconnected;
                        } break;
                    case Packet_Type_Challenge:
                        {
                            if(ctx->is_server)
                            {
                                LOG_DEBUG("Challenge salt %llx, conn salt %llx", packet.salt, conns->salt[index]);
                                conns->salt[index] ^= packet.salt;
                                Packet resp = default_packet(Packet_Type_Challenge_Resp);
                                packet_buffer_init(conns, index, resp);
                            }
                        } break;
                    case Packet_Type_Challenge_Resp:
                        {
                            if(!ctx->is_server)
                            {
                                if(conns->salt[index] == packet.salt)
                                {
                                    conns->state[index] = Net_Conn_State_Connected;
                                    Net_Message message = {};
                                    message.type = Net_Message_Hash_Req;
                                    net_send_message(ctx, index, message);
                                }
                                else
                                {
                                    LOG_DEBUG("Challenge response salt invalid (packet salt: %llx, conn salt: %llx). Sending denied packet", conns->salt[index], packet.salt);
                                    send_denied_packet(ctx, address);
                                }
                            }
                        } break;
                    default:
                        {
                            LOG_DEBUG("Invalid packet of type %d. Ignoring...", packet.type);
                        }
                    }
                }
                else
                {
                    // NOTE(dgl): we send a denied packet to let the peer reset its connection state
                    LOG_DEBUG("Connection %d is disconnected. Sending denied packet", index);
                    send_denied_packet(ctx, address);
                }
            }
        }
    }

    if(chunk_buffer_updated)
    {
        if(chunk_complete(ctx->ack_buffer.data, array_count(ctx->ack_buffer.data), ctx->chunk_info.slice_count))
        {
            LOG_DEBUG("All chunk slices received");
            usize slice_size = get_serialized_packet_size(Packet_Type_Slice);
            usize payload_size = NET_MTU_SIZE - slice_size;

            message->type = ctx->chunk_type;
            message->payload_size = (cast(usize)(ctx->chunk_info.slice_count - 1) * payload_size) + ctx->chunk_info.last_slice_size;
            message->payload = ctx->chunk_buffer;
            result = index;
        }
        else
        {
            LOG_DEBUG("Sending chunk ack buffer");
            Packet ack_packet = default_packet(Packet_Type_Ack);
            ack_packet.ack.hash = ctx->chunk_info.hash;
            Packet_Buffer *buffer = packet_buffer_init(conns, index, ack_packet);
            // TODO(dgl): send only the necessary bits...
            packet_buffer_append(buffer, ctx->ack_buffer.data, (ctx->chunk_info.slice_count / 8) + 1);
            net_send_packet_buffer(ctx, index);
        }
    }

    return(result);
}

// TODO(dgl): @cleanup find better name, can we get rid of the net_message here?
internal Packet
build_packet(Net_Context *ctx, Net_Message message)
{
    Packet result = {};
    switch(message.type)
    {
        case Net_Message_Hash_Req:
        case Net_Message_Data_Req:
        {
            result = default_packet(Packet_Type_Empty);
        } break;
        case Net_Message_Hash_Res:
        case Net_Message_Data_Res:
        {
            assert(message.payload, "Message with this type must have a payload");
            usize max_payload_size = NET_MTU_SIZE - get_serialized_packet_size(Packet_Type_Payload);
            if(message.payload_size > max_payload_size)
            {
                uint32 payload_hash = HASH_OFFSET_BASIS;
                hash(&payload_hash, message.payload, message.payload_size);

                assert(message.payload_size <= ctx->chunk_buffer_size, "Payload larger than max file");
                if(payload_hash != ctx->chunk_info.hash)
                {
                    LOG_DEBUG("Copying new payload into chunk buffer");
                    dgl_memcpy(ctx->chunk_buffer, message.payload, message.payload_size);
                    ctx->chunk_info.hash = payload_hash;
                }

                usize slice_space = NET_MTU_SIZE - get_serialized_packet_size(Packet_Type_Slice);
                uint32 slice_count = cast(uint32)((cast(real32)(message.payload_size) / cast(real32)(slice_space)) + 1.0f);
                ctx->chunk_info.slice_count = slice_count;
                ctx->chunk_info.last_slice_size = dgl_safe_size_to_uint32(message.payload_size % slice_space);

                usize ack_bit_count = (NET_MTU_SIZE - get_serialized_packet_size(Packet_Type_Ack))*8;
                assert(slice_count <= ack_bit_count*8, "Payload too large. Not enough ack bits available");

                result = default_packet(Packet_Type_Chunk);
                result.chunk.hash = ctx->chunk_info.hash;
                result.chunk.last_slice_size = ctx->chunk_info.last_slice_size;
                result.chunk.slice_count = ctx->chunk_info.slice_count;
            }
            else
            {
                result = default_packet(Packet_Type_Payload);
            }
        } break;
        default:
        {
            result = default_packet(Packet_Type_Empty);
        } break;
    }

    result.msg_type = message.type;

    return(result);
}

// TODO(dgl): refactor this into send message and send message with payload
internal void
net_send_message(Net_Context *ctx, Net_Conn_ID index, Net_Message message)
{
    if(index >= 0 && ctx->conns->state[index] == Net_Conn_State_Connected)
    {
        Packet packet = build_packet(ctx, message);
        Packet_Buffer *buffer = packet_buffer_init(ctx->conns, index, packet);

        if(packet.type == Packet_Type_Payload)
        {
            packet_buffer_append(buffer, message.payload, message.payload_size);
        }

        net_send_packet_buffer(ctx, index);

        if(packet.type == Packet_Type_Chunk)
        {
            send_chunk_buffer(ctx, index, 0, 0);
        }
    }
    else
    {
        LOG("Client on index %d is not connected. Cannot send message", index);
    }
}

internal void
net_multicast_message(Net_Context *ctx, Net_Message message)
{
    Connection_List *conns = ctx->conns;
    for(int32 index = 0; index < conns->max_count; ++index)
    {
        if(conns->state[index] == Net_Conn_State_Connected)
        {
            net_send_message(ctx, index, message);
        }
    }
}

