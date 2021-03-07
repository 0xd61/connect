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

struct Bitstream
{
    bool32 is_reading;
    bool32 is_writing;

    // NOTE(dgl): must be 64 bits for overflow
    uint64 scratch;
    int32 scratch_bits;

    // NOTE(dgl): IMPORTANT! This is a uint32 memory buffer.
    // we used index and count to not be confused with byte buffers.
    usize index;
    usize count;
    uint32 *data;
};

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
        pack_uint32(buffer, header->size, 0, ZHC_MAX_FILESIZE);
        stream_flush(buffer);
    }
    else if(buffer->is_reading)
    {
        uint32 raw_version = stream_read_bits(buffer, 32);
        header->version = dgl_clamp(raw_version, 1, 0xFFFFFFFF);
        header->type = cast(Net_Msg_Header_Type)unpack_uint32(buffer, Net_Msg_Header_Noop, Net_Msg_Header_Max - 1);
        header->size = unpack_uint32(buffer, 0, ZHC_MAX_FILESIZE);
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
    Bitstream writer = stream_writer_init(memory, array_count(memory));

    usize count = serialize_header(&writer, &header);

    // TODO(dgl): calculate bit count for serializing
    platform.send_data(socket, destination, memory, count);
}

// NOTE(dgl): returns true is more data is available. This is not a great solution but I
// did not want to create a separate check on the platform layer. If this is too annoying @@cleanup
// Providing the header as parameter is also similar to the recv_data function.
internal bool32
net_recv_header(Zhc_Net_Socket *socket, Zhc_Net_IP *source, Net_Msg_Header *header)
{
    bool32 result = false;

    Net_Msg_Header tmp_header = {};
    uint8 memory[sizeof(header)] = {};
    Bitstream reader = stream_reader_init(memory, array_count(memory));

    // NOTE(dgl): we try to serialize the header without any data to get the size of the data @@performance
    usize count = serialize_header(&reader, &tmp_header);

    assert(count <= array_count(memory), "Databuffer cannot be smaller than the header structure");
    result = platform.receive_data(socket, source, memory, count);

    // NOTE(dgl): reset the reader;
    reader = stream_reader_init(memory, array_count(memory));
    usize res = serialize_header(&reader, header);

    assert(count == res, "Failed to serialize");

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

    uint8 memory[sizeof(hash)] = {};
    Bitstream writer = stream_writer_init(memory, array_count(memory));

    usize count = serialize_hash(&writer, &hash);

    assert(count == 4, "Failed to serialize hash");
    platform.send_data(socket, destination, memory, count);
}

internal bool32
net_recv_hash(Zhc_Net_Socket *socket, Zhc_Net_IP *source, uint32 *hash)
{
    bool32 result = false;

    uint8 memory[sizeof(*hash)] = {};

    result = platform.receive_data(socket, source, memory, sizeof(*hash));

    Bitstream reader = stream_reader_init(memory, array_count(memory));
    usize res = serialize_hash(&reader, hash);

    assert(sizeof(*hash) == res, "Failed to serialize");

    return(result);
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
