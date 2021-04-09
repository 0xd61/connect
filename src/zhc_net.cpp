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
serialize_header(Bitstream *buffer, Net_Msg_Header *header)
{
    // TODO(dgl): handle different versions
    assert(bits_required((Net_Msg_Header_Max - 1) - Net_Msg_Header_Noop) == 3, "Bitcount for type changed. Please update the version.");
    assert(bits_required(ZHC_MAX_FILESIZE) == 21, "Bitcount for size changed. Please update the version.");

    usize result = 0;
    if(buffer->is_writing)
    {
        stream_write_bits(buffer, header->version, 32);
        pack_uint32(buffer, cast(uint32)header->type, Net_Msg_Header_Noop, Net_Msg_Header_Max - 1);

        if(header->type == Net_Msg_Header_Chunk)
        {
            stream_write_bits(buffer, cast(uint32)header->_chunk.hash, sizeof(header->_chunk.hash)*8);
            pack_uint32(buffer, header->_chunk.last_slice_size, 0, NET_MTU_SIZE);
            pack_uint32(buffer, header->_chunk.slice_count, 0, NET_MTU_SIZE*8);
        }
        else if(header->type == Net_Msg_Header_Slice)
        {
            stream_write_bits(buffer, cast(uint32)header->_slice.hash, sizeof(header->_slice.hash)*8);
            pack_uint32(buffer, header->_slice.index, 0, NET_MTU_SIZE*8);
        }
        else if(header->type == Net_Msg_Header_Ack)
        {
            stream_write_bits(buffer, cast(uint32)header->_ack.id, sizeof(header->_ack.id)*8);
        }
        else
        {
            pack_uint32(buffer, dgl_safe_size_to_uint32(header->size), 0, ZHC_MAX_FILESIZE);
        }
        stream_flush(buffer);
    }
    else if(buffer->is_reading)
    {
        uint32 raw_version = stream_read_bits(buffer, 32);
        header->version = dgl_clamp(raw_version, 1, 0xFFFFFFFF);
        header->type = cast(Net_Msg_Header_Type)unpack_uint32(buffer, Net_Msg_Header_Noop, Net_Msg_Header_Max - 1);

        if(header->type == Net_Msg_Header_Chunk)
        {
            header->_chunk.hash = cast(uint16)stream_read_bits(buffer, sizeof(header->_chunk.hash)*8);
            header->_chunk.last_slice_size = unpack_uint32(buffer, 0, NET_MTU_SIZE);
            header->_chunk.slice_count = unpack_uint32(buffer, 0, NET_MTU_SIZE*8);
        }
        else if(header->type == Net_Msg_Header_Slice)
        {
            header->_slice.hash = cast(uint16)stream_read_bits(buffer, sizeof(header->_slice.hash)*8);
            header->_slice.index = unpack_uint32(buffer, 0, NET_MTU_SIZE*8);
        }
        else if(header->type == Net_Msg_Header_Ack)
        {
            header->_ack.id = cast(uint16)stream_read_bits(buffer, sizeof(header->_ack.id)*8);
        }
        else
        {
            header->size = cast(usize)unpack_uint32(buffer, 0, ZHC_MAX_FILESIZE);
        }
    }
    else
    {
        assert(false, "Bitstream buffer must be reading or writing");
    }

    result = buffer->index * sizeof(*buffer->data);
    return(result);
}

internal usize
serialize_hash(Bitstream *buffer, uint32 *hash)
{
    usize result = 0;
    if(buffer->is_writing)
    {
        stream_write_bits(buffer, *hash, 32);
        stream_flush(buffer);
    }
    else if(buffer->is_reading)
    {
        *hash = stream_read_bits(buffer, 32);
    }
    else
    {
        assert(false, "Bitstream buffer must be reading or writing");
    }
    result = buffer->index * sizeof(*buffer->data);
    return(result);
}

internal usize
serialize_packet(Bitstream *buffer, Packet *packet)
{
    usize result = 0;
    if(buffer->is_writing)
    {
        stream_write_bits(buffer, cast(uint32)packet->id, 32);
        pack_uint32(buffer, cast(uint32)packet->type, 0, Packet_Type_Max);
        pack_uint32(buffer, cast(uint32)packet->index, 0, NET_MAX_CLIENTS);
        stream_write_bits(buffer, cast(uint32)(packet->salt & 0xFFFFFFFF), 32);
        stream_write_bits(buffer, cast(uint32)(packet->salt >> 32), 32);
        stream_flush(buffer);
    }
    else if(buffer->is_reading)
    {
        packet->id = cast(int32)stream_read_bits(buffer, 32);
        packet->type = cast(Packet_Type)unpack_uint32(buffer, 0, Packet_Type_Max);
        packet->index = cast(Net_Conn_ID)unpack_uint32(buffer, 0, NET_MAX_CLIENTS);

        uint64 tmp_salt = 0;
        tmp_salt = cast(uint64)stream_read_bits(buffer, 32);
        tmp_salt |= (cast(uint64)stream_read_bits(buffer, 32)) << 32;
        packet->salt = tmp_salt;
    }
    else
    {
        assert(false, "Bitstream buffer must be reading or writing");
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
get_serialized_packet_size()
{
    usize result = 0;
    Packet packet = {};
    Bitstream writer = stream_writer_init(cast(uint8 *)&packet, sizeof(packet));
    result = serialize_packet(&writer, &packet);
    return(result);
}

internal usize
get_serialized_header_size(Net_Msg_Header_Type type)
{
    // TODO(dgl): need a better way get the header size. We should be able to get this on compile time
    // for each type.
    usize result = 0;

    Net_Msg_Header tmp = {};
    Bitstream writer = stream_writer_init(cast(uint8 *)&tmp, sizeof(tmp));
    tmp.type = type;

    result = serialize_header(&writer, &tmp);
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
    }
    else
    {
        LOG_DEBUG("No free connection available");
    }

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
packet_buffer_init(Connection_List *conns, Net_Conn_ID index, Packet_Type type)
{
    assert(index >= 0, "Invalid index");
    Packet_Buffer *result = 0;

    Packet packet = {};
    packet.id = 0x1234;
    packet.type = type;
    packet.salt = conns->salt[index];
    assert(conns->packet_buffer, "Packet buffer not initialized");
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

// NOTE(dgl): maybe we could also return the conn index instead of the address and drop invalid
// packages right here. For now we keep this separate.
internal int32
recv_packet(Net_Context *ctx, Zhc_Net_Address *address, Packet *packet, uint8 *payload, usize payload_size)
{
    int32 result = 0;
    uint8 buffer[NET_MTU_SIZE] = {};
    result = platform.receive_data(&ctx->socket, address, buffer, array_count(buffer));

    if(result > 0)
    {
        Bitstream reader = stream_reader_init(buffer, array_count(buffer));
        usize count = serialize_packet(&reader, packet);

        if(packet->type == Packet_Type_Payload && payload_size > 0)
        {
            assert(payload, "Invalid payload buffer");
            assert(payload_size > array_count(buffer) - count, "Payload buffer overflow");
            dgl_memcpy(payload, buffer + count, payload_size);
        }

        LOG_DEBUG("Received packet of type %d with %d bytes of data (Salt: %llx)", packet->type, count, packet->salt);
    }

    return(result);
}

internal void
send_denied_packet(Net_Context *ctx, Zhc_Net_Address address)
{
    Packet packet = {};
    packet.id = 0x1234;
    packet.type = Packet_Type_Denied;

    uint8 buffer[NET_MTU_SIZE] = {};
    Bitstream writer = stream_writer_init(buffer, array_count(buffer));
    usize count = serialize_packet(&writer, &packet);

    platform.send_data(&ctx->socket, &address, buffer, count);
    LOG_DEBUG("Sending denied packet");
}

internal bool32
packet_is_valid(Connection_List *conns, Net_Conn_ID index, Packet packet)
{
    bool32 result = false;

    assert(index >= 0, "Invalid connection index");

    if((conns->state[index] != Net_Conn_State_Disconnected) &&
       (conns->salt[index] == packet.salt))
    {
        result = true;
    }

    return(result);
}

internal Net_Msg_Header
build_default_header(Net_Msg_Header_Type type)
{
    Net_Msg_Header result = {};
    result.version = parse_version(ZHC_VERSION);
    result.type = type;

    return(result);
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
        conns->address = dgl_mem_arena_push_array(arena, Zhc_Net_Address, casted_count);
        conns->salt = dgl_mem_arena_push_array(arena, uint64, casted_count);
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
        conns->max_count = 1;
        usize casted_count = cast(usize)conns->max_count;
        conns->address = dgl_mem_arena_push_array(arena, Zhc_Net_Address, casted_count);
        conns->salt = dgl_mem_arena_push_array(arena, uint64, casted_count);
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

// NOTE(dgl): This is only temporary. We will have a broadcast message from the server, with
// the servers ip and port. When we receive this message an do not have a connection
// we initiate a new attempt.
internal void
net_request_server_connection(Net_Context *ctx)
{
    assert(!ctx->is_server, "Only clients can connect to a server");

    Zhc_Net_Address address = parse_address(ZHC_SERVER_IP, ZHC_SERVER_PORT);
    Net_Conn_ID server = get_connection(ctx->conns, address);
    if(server < 0 || ctx->conns->state[server] == Net_Conn_State_Disconnected)
    {
        LOG_DEBUG("Pushing new server connection");
        uint64 salt = 0;
        get_random_bytes(cast(uint8 *)&salt, sizeof(salt));
        Net_Conn_ID server = push_connection(ctx->conns, address, salt);
        packet_buffer_init(ctx->conns, server, Packet_Type_Request);
    }
}

internal Net_Conn_ID
server_process_packet(Net_Context *ctx, Zhc_Net_Address address, Packet packet)
{
    Net_Conn_ID index = -1;
    Net_Conn_ID result = index;
    Connection_List *conns = ctx->conns;

    // NOTE(dgl): prepare new incoming connection.
    if(packet.type == Packet_Type_Request)
    {
        uint64 salt = 0;
        get_random_bytes(cast(uint8 *)&salt, sizeof(salt));
        index = push_connection(conns, address, salt);
        if(index < 0)
        {
            LOG("No free connection available");
            send_denied_packet(ctx, address);
        }
        else
        {
            packet_buffer_init(conns, index, Packet_Type_Challenge);
            conns->salt[index] ^= packet.salt;
        }
    }
    else if((index = get_connection(conns, address)) >= 0)
    {
        if(packet.type == Packet_Type_Denied)
        {
            conns->state[index] = Net_Conn_State_Disconnected;
        }
        else if(packet_is_valid(conns, index, packet))
        {
            if((packet.type == Packet_Type_Challenge_Resp) &&
               (conns->state[index] == Net_Conn_State_Connecting))
            {
                // NOTE(dgl): we do not set the connection to connected yet,
                // to spam the packet to the client when we send the pending buffers.
                // The connection is set to connected on the first payload packet
                // from the client.
                // TODO(dgl): should we send a simple message header here?
                packet_buffer_init(conns, index, Packet_Type_Payload);
            }
            else if((packet.type == Packet_Type_Payload) &&
                    (conns->state[index] != Net_Conn_State_Disconnected))
            {
                conns->state[index] = Net_Conn_State_Connected;
                result = index;
            }
            else if((packet.type == Packet_Type_Disconnect) &&
                    (conns->state[index] == Net_Conn_State_Connected))
            {
                conns->state[index] = Net_Conn_State_Disconnected;
            }
            else
            {
                LOG_DEBUG("Ignoring unexpected packet of type %d", packet.type);
            }
        }
        else
        {
            LOG_DEBUG("Invalid packet of type %d received", packet.type);
            send_denied_packet(ctx, address);
            conns->state[index] = Net_Conn_State_Disconnected;
        }
    }
    else
    {
        LOG_DEBUG("No connection found. Initiate a new connection by sending a Request packet.");
        send_denied_packet(ctx, address);
    }

    return(result);
}

internal Net_Conn_ID
client_process_packet(Net_Context *ctx, Zhc_Net_Address address, Packet packet)
{
    Net_Conn_ID index = -1;
    Net_Conn_ID result = index;

    Connection_List *conns = ctx->conns;

    if((index = get_connection(conns, address)) >= 0)
    {
        if(packet.type == Packet_Type_Denied)
        {
            conns->state[index] = Net_Conn_State_Disconnected;
        }
        else if((packet.type == Packet_Type_Challenge) &&
               (conns->state[index] == Net_Conn_State_Connecting))
        {
            LOG_DEBUG("Received server salt %llx, Client salt %llx", packet.salt, conns->salt[index]);
            conns->salt[index] ^= packet.salt;
            packet_buffer_init(conns, index, Packet_Type_Challenge_Resp);
        }
        else if(packet_is_valid(conns, index, packet))
        {
            if((packet.type == Packet_Type_Payload) &&
                conns->state[index] != Net_Conn_State_Disconnected)
            {
                conns->state[index] = Net_Conn_State_Connected;
                result = index;
            }
            else if((packet.type == Packet_Type_Disconnect) &&
                    (conns->state[index] == Net_Conn_State_Connected))
            {
                conns->state[index] = Net_Conn_State_Disconnected;
            }
            else
            {
                LOG_DEBUG("Ignoring unexpected packet");
            }
        }
        else
        {
            LOG_DEBUG("Invalid packet of type %d received", packet.type);
            send_denied_packet(ctx, address);
            conns->state[index] = Net_Conn_State_Disconnected;
        }
    }
    else
    {
        LOG_DEBUG("No connection found");
        send_denied_packet(ctx, address);
    }

    return(result);
}

// NOTE(dgl): returns the connection index, when a payload was received from an established connection.
internal Net_Conn_ID
net_recv_payload(Net_Context *ctx, uint8 *payload, usize payload_size)
{
    Net_Conn_ID result = -1;

    Zhc_Net_Socket *socket = &ctx->socket;
    if(socket->handle.no_error)
    {
        Packet packet = {};
        Zhc_Net_Address address = {};
        // NOTE(dgl): handle all incoming packets until no data is available or we received a
        // package with a payload.
        while(recv_packet(ctx, &address, &packet, payload, payload_size))
        {
            if(ctx->is_server) { result = server_process_packet(ctx, address, packet); }
            else { result = client_process_packet(ctx, address, packet); }

            if(result >= 0) { break; }
        }
    }
    else
    {
        // NOTE(dgl): we cannot notify the peers if the socket has an error. They will have to try
        // and get a denied packet when trying to send data. Then they have to reauthenticate.
        dgl_memset(ctx->conns->state, Net_Conn_State_Disconnected, cast(usize)ctx->conns->max_count);
        platform.close_socket(socket);
        // TODO(dgl): arena not needed. Will be removed later.
        platform.open_socket(0, socket);
        LOG_DEBUG("Listening for connection: %d.%d.%d.%d:%d", socket->address.ip[0], socket->address.ip[1], socket->address.ip[2], socket->address.ip[3], socket->address.port);
    }

    return(result);
}

internal Net_Conn_ID
net_recv_header(DGL_Mem_Arena *arena, Net_Context *ctx, Net_Msg_Header *header)
{
    Net_Conn_ID result = -1;
    uint8 *memory = dgl_mem_arena_push_array(arena, uint8, NET_MTU_SIZE);

    // TODO(dgl): reveive the payload. If header type is chunk or slice do not return
    // and check for more available messages. If there is no message pending, send an ack
    // to the peer. If everything has been received return a Data Res message header. Put the
    // chunk_buffer as payload.

    Net_Conn_ID index = -1;
    bool32 chunk_buffer_updated = false;

    // TODO(dgl): @cleanup we provide a buffer here, but recv_packet allocates another
    // buffer and does a memcopy.
    while((index = net_recv_payload(ctx, memory, NET_MTU_SIZE)) >= 0)
    {
        Bitstream reader = stream_reader_init(memory, NET_MTU_SIZE);
        usize count = serialize_header(&reader, header);

        // NOTE(dgl): assert is kinda useless. But we ensure, the header cannot be bigger than the MTU
        // This prevents an invalid payload pointer.

        assert(count < NET_MTU_SIZE, "Header cannot be bigger than MTU");
        if(header->type == Net_Msg_Header_Chunk)
        {
            if(ctx->chunk_hash != header->_chunk.hash)
            {
                chunk_buffer_updated = true;
                usize packet_size = get_serialized_packet_size();
                usize slice_size = NET_MTU_SIZE - packet_size - get_serialized_header_size(Net_Msg_Header_Slice);
                usize chunk_size = (slice_size*header->_chunk.slice_count) - (slice_size - header->_chunk.last_slice_size);
                assert(chunk_size < ctx->chunk_buffer_size, "Chunk buffer overflow");

                ctx->chunk_slice_count = header->_chunk.slice_count;
                ctx->chunk_hash = header->_chunk.hash;
                ctx->chunk_buffer_offset = chunk_size;
                LOG_DEBUG("Prepare receiving new chunk %u of size %llu", header->_chunk.hash, chunk_size);
            }
        }
        else if(header->type == Net_Msg_Header_Slice)
        {
            if(ctx->chunk_hash == header->_slice.hash)
            {
                chunk_buffer_updated = true;
                usize packet_size = get_serialized_packet_size();
                usize slice_size = NET_MTU_SIZE - packet_size - get_serialized_header_size(Net_Msg_Header_Slice);
                usize offset = cast(usize)header->_slice.index * slice_size;
                usize size = 0;
                if(header->_slice.index == ctx->chunk_slice_count - 1)
                {
                    size = ctx->chunk_buffer_offset - offset;
                    LOG_DEBUG("Received last slice of size %llu", size);
                }
                else
                {
                    size = slice_size;
                    LOG_DEBUG("Received slice of size %llu", size);
                }

                dgl_memcpy(ctx->chunk_buffer + offset, memory + count, size);

                // TODO(dgl): do ack the package

                // TODO(dgl): if everything is acked we return the header and payload
                if(header->_slice.index == ctx->chunk_slice_count - 1)
                {
                    header->type = Net_Msg_Header_Data_Res;
                    header->size = ctx->chunk_buffer_offset;
                    header->_payload = ctx->chunk_buffer;
                    result = index;
                }
            }
        }
        else
        {
            header->_payload = memory + count;
            result = index;
        }
    }
    return(result);
}

internal Packet_Buffer *
packet_push_header(Net_Context *ctx, Net_Conn_ID index, Net_Msg_Header header)
{
    Packet_Buffer *result = 0;
    assert(index >= 0, "Invalid connection index");

    result = packet_buffer_init(ctx->conns, index, Packet_Type_Payload);

    Bitstream writer = stream_writer_init(result->data + result->offset, array_count(result->data) - result->offset);
    result->offset += serialize_header(&writer, &header);

    return(result);
}

// NOTE(dgl): we could merge the send_hash and send_data into send_message and decide on the type
// how we want to send the message. @cleanup
internal void
net_send_message(Net_Context *ctx, Net_Conn_ID index, Net_Msg_Header_Type type)
{
    Net_Msg_Header header = build_default_header(type);
    packet_push_header(ctx, index, header);
    net_send_packet_buffer(ctx, index);
}

internal void
net_send_hash(Net_Context *ctx, Net_Conn_ID index, uint32 hash)
{
    if(index >= 0 && ctx->conns->state[index] == Net_Conn_State_Connected)
    {
        Net_Msg_Header header = build_default_header(Net_Msg_Header_Hash_Res);
        header.size = sizeof(hash);
        Packet_Buffer *buffer = packet_push_header(ctx, index, header);
        packet_buffer_append(buffer, cast(uint8 *)&hash, sizeof(hash));
        net_send_packet_buffer(ctx, index);
    }
    else
    {
        LOG("Connection does not exist.");
    }
}

internal void
net_extract_hash(Net_Msg_Header *header, uint32 *hash)
{
    assert(header->type == Net_Msg_Header_Hash_Res, "Header type not compatible");
    assert(header->_payload, "Header payload not initialized");
    Bitstream reader = stream_reader_init(header->_payload, header->size);
    usize res = serialize_hash(&reader, hash);

    assert(sizeof(*hash) == res, "Failed to serialize");
}

// NOTE(dgl): the ack_mask_size is not really needed. This is only as a safety measure
internal void
send_chunk_buffer(Net_Context *ctx, Net_Conn_ID index, uint8 *ack_mask, usize ack_mask_size)
{
    usize packet_size = get_serialized_packet_size();
    usize slice_space = NET_MTU_SIZE - packet_size - get_serialized_header_size(Net_Msg_Header_Slice);
    usize ack_space = NET_MTU_SIZE - packet_size - get_serialized_header_size(Net_Msg_Header_Ack);

    uint32 slice_count = cast(uint32)((cast(real32)(ctx->chunk_buffer_offset) / cast(real32)(slice_space)) + 1.0f);
    ctx->chunk_slice_count = slice_count;
    uint32 last_slice_size = dgl_safe_size_to_uint32(ctx->chunk_buffer_offset % slice_space);
    assert(slice_count <= ack_space*8, "Payload too large. Not enough ack bits available");
    uint32 chunk_hash = HASH_OFFSET_BASIS;
    hash(&chunk_hash, ctx->chunk_buffer, ctx->chunk_buffer_offset);

    // NOTE(dgl): we always send the chunk info to the client. The client has to decide
    // if this is a completly new chunk or if it is just a retry (based on the hash).
    {
        Net_Msg_Header header = build_default_header(Net_Msg_Header_Chunk);
        header._chunk.hash = chunk_hash;
        header._chunk.last_slice_size = last_slice_size;
        header._chunk.slice_count = slice_count;
        packet_push_header(ctx, index, header);
        net_send_packet_buffer(ctx, index);
        LOG_DEBUG("Sending chunk %u (%u slices, %llu bytes)", header._chunk.hash, header._chunk.slice_count, ctx->chunk_buffer_offset);
    }

    Net_Msg_Header header = build_default_header(Net_Msg_Header_Slice);
    header._slice.hash = chunk_hash;
    uint8 *root = ctx->chunk_buffer;
    for(uint32 slice_index = 0;
        slice_index < slice_count;
        ++slice_index)
    {
        if(ack_mask)
        {
            // NOTE(dgl): if the bit in the mask is set to 1 the slice has already been received.
            // We can skip it in here.
            int32 mask_byte = slice_index / 8;
            uint32 mask_bit = 1 << (slice_index % 8);
            assert(mask_byte < ack_mask_size, "Invalid ack mask byte");

            if((ack_mask[mask_byte] & mask_bit) == mask_bit)
            {
                LOG_DEBUG("Slice %u already received by the client. Skipping...", slice_index);
                continue;
            }
        }

        usize size = 0;
        if(slice_index == slice_count - 1)
        {
            size = last_slice_size;
        }
        else
        {
            size = slice_space;
        }

        header._slice.index = slice_index;
        Packet_Buffer *buffer = packet_push_header(ctx, index, header);
        packet_buffer_append(buffer, root, size);
        root += size;
        net_send_packet_buffer(ctx, index);
        LOG_DEBUG("Sending slice %u (%llu bytes)", header._slice.index, size);
    }
}

// TODO(dgl): @cleanup we should prepare the payload independent of sending a packet, like serializeing a hash etc. and
// after that simply call net_send_data and send the payload.
internal void
net_send_data(Net_Context *ctx, Net_Conn_ID index, uint8 *payload, usize payload_size)
{
    if(index >= 0 && ctx->conns->state[index] == Net_Conn_State_Connected)
    {
//         // NOTE(dgl): we only send a chunk, if the data does not fit into the packet.
//         // This way we ensure
//         usize available_payload_size = get_serialized_header_size(Net_Msg_Header_Data_Res);
//         if(payload_size > available_payload_size)
//         {
            uint32 payload_hash = HASH_OFFSET_BASIS;
            hash(&payload_hash, payload, payload_size);

            assert(payload_size <= ctx->chunk_buffer_size, "Payload larger than max file");
            if(payload_hash != ctx->chunk_hash)
            {
                LOG_DEBUG("Copying new payload into chunk buffer");
                ctx->chunk_buffer_offset = payload_size;
                dgl_memcpy(ctx->chunk_buffer, payload, ctx->chunk_buffer_offset);
                hash(&ctx->chunk_hash, ctx->chunk_buffer, ctx->chunk_buffer_offset);
            }

            send_chunk_buffer(ctx, index, 0, 0);
//         }
//         else
//         {
//             Net_Msg_Header header = build_default_header(Net_Msg_Header_Data_Res);
//             header..size = sizeof(payload_size);
//             Packet_Buffer *buffer = packet_push_header(ctx, index, header);
//             packet_buffer_append(buffer, cast(uint8 *)&hash, sizeof(hash));
//             net_send_packet_buffer(ctx, index);
//         }
    }
    else
    {
        LOG("Connection does not exist.");
    }
}

internal void
net_extract_data(Net_Msg_Header *header, uint8 *buffer, usize buffer_size)
{
    // NOTE(dgl): We have no check for the size in production because
    // for performance reasons we disable the asserts in prod. This is a vulnerability!!
    // This will not be an issue if we sign our message and an attacker
    // is unable to change the contents. Until then, this application
    // is vulnerable. @security
    assert(header->_payload, "Header payload not initialized");
    assert(header->size <= buffer_size, "Buffer too small");

    dgl_memcpy(buffer, header->_payload, header->size);
}
