struct Buffer
{
    usize offset;
    usize size;
    uint8 *memory;
};

enum Net_Msg_Header_Type
{
    Net_Msg_Header_Noop,
    Net_Msg_Header_Hash_Req,
    Net_Msg_Header_Hash_Res,
    Net_Msg_Header_Data_Req,
    Net_Msg_Header_Data_Res,
    Net_Msg_Header_Max
};

struct Net_Msg_Header
{
    uint32 version; /* 16 bit major, 8 bit minor, 8 bit patch */
    Net_Msg_Header_Type type;
    uint32 size;
};

internal void
write_uint32(Buffer *buffer, uint32 value)
{
    assert((buffer->offset + sizeof(value)) < buffer->size, "Buffer overflow. Please increase the buffer size");
#if ZHC_BIG_ENDIAN
    *((uint32 *)(buffer->memory + buffer->offset)) = value;
#else
    *((uint32 *)(buffer->memory + buffer->offset)) = bswap32(value);
#endif
    buffer->offset += sizeof(value);
}

internal uint32
read_uint32(Buffer *buffer, uint32 min, uint32 max)
{
    uint32 result = 0;
    assert((buffer->offset + sizeof(result)) < buffer->size, "Buffer overflow. Please increase the buffer size");
#if ZHC_BIG_ENDIAN
    result = *((uint32 *)(buffer->memory + buffer->offset));
#else
    result = bswap32(*((uint32 *)(buffer->memory + buffer->offset)));
#endif
    buffer->offset += sizeof(result);

    result = dgl_clamp(result, min, max);
    return(result);
}

internal void
write_header(Buffer *buffer, Net_Msg_Header header)
{
    write_uint32(buffer, header.version);
    write_uint32(buffer, cast(uint32)header.type);
    write_uint32(buffer, header.size);
}

internal Net_Msg_Header
read_header(Buffer *buffer)
{
    Net_Msg_Header result = {};
    result.version = read_uint32(buffer, 1, parse_version(ZHC_VERSION));
    result.type = cast(Net_Msg_Header_Type)read_uint32(buffer, Net_Msg_Header_Noop, Net_Msg_Header_Max - 1);
    result.size = read_uint32(buffer, 0, ZHC_MAX_FILESIZE);

    return(result);
}

internal Zhc_Net_Socket
net_init_socket(DGL_Mem_Arena *arena, char *ip, uint16 port)
{
    Zhc_Net_Socket result = {};

    int32 index = 0;
    int32 number = 0;
    while(*ip)
    {
        if(*ip == '.')
        {
            assert(number <= 0xFF && number >= 0, "Invalid IP segment (cannot be bigger than 255)");
            result.address.ip[index++] = cast(uint8)number;
            number = 0;
            ++ip;
        }

        number *= 10;
        number += *ip - '0';

        ++ip;
    }

    assert(number <= 0xFF && number >= 0, "Invalid IP segment (cannot be bigger than 255)");
    assert(index == 3, "Invalid ip address");
    result.address.ip[index] = cast(uint8)number;
    result.address.port = port;

    platform.setup_socket(arena, &result);
    return(result);
}

internal void
net_send_header(Zhc_Net_Socket *socket, Zhc_Net_IP *destination, Net_Msg_Header_Type type, usize size = 0)
{
    Net_Msg_Header header = {};
    header.type = type;
    header.version = parse_version(ZHC_VERSION);
    header.size = dgl_safe_size_to_uint32(size);

    uint8 memory[sizeof(header)] = {};
    Buffer buffer = {};
    buffer.size = array_count(memory);
    buffer.offset = 0;
    buffer.memory = memory;

    write_header(&buffer, header);

    platform.send_data(socket, destination, buffer.memory, array_count(memory));
}

// NOTE(dgl): returns true is more data is available. This is not a great solution but I
// did not want to create a separate check on the platform layer. If this is too annoying @@cleanup
// Providing the header as parameter is also similar to the recv_data function.
internal bool32
net_recv_header(Zhc_Net_Socket *socket, Zhc_Net_IP *source, Net_Msg_Header *header)
{
    bool32 result = false;
    uint8 memory[sizeof(result)] = {};
    Buffer buffer = {};
    buffer.size = array_count(memory);
    buffer.offset = 0;
    buffer.memory = memory;

    result = platform.receive_data(socket, source, buffer.memory, array_count(memory));
    *header = read_header(&buffer);

    return(result);
}

internal bool32
net_recv_data(Zhc_Net_Socket *socket, Zhc_Net_IP *source, void *buffer, usize buffer_size)
{
    bool32 result = platform.receive_data(socket, source, buffer, buffer_size);
    return(result);
}

internal void
net_send_hash_request(Zhc_Net_Socket *socket, Zhc_Net_IP *destination)
{
    net_send_header(socket, destination, Net_Msg_Header_Hash_Req);
}

internal void
net_send_hash_response(Zhc_Net_Socket *socket, Zhc_Net_IP *destination, uint32 hash)
{
    net_send_header(socket, destination, Net_Msg_Header_Hash_Res, sizeof(hash));
    platform.send_data(socket, destination, &hash, sizeof(hash));
}

internal void
net_send_data_request(Zhc_Net_Socket *socket, Zhc_Net_IP *destination)
{
    net_send_header(socket, destination, Net_Msg_Header_Data_Req);
}

internal void
net_send_data_response(Zhc_Net_Socket *socket, Zhc_Net_IP *destination, uint8 *buffer, usize buffer_size)
{
    net_send_header(socket, destination, Net_Msg_Header_Data_Res, buffer_size);
    platform.send_data(socket, destination, buffer, buffer_size);
}
