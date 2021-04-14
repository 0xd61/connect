#ifndef ZHC_NET_H
#define ZHC_NET_H

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

//
// NOTE(dgl): Connections
//

#define NET_MTU_SIZE 1200
#define NET_MAX_CLIENTS 128
typedef int32 Net_Conn_ID;

enum Net_Conn_State
{
    Net_Conn_State_Disconnected,
    Net_Conn_State_Connecting,
    Net_Conn_State_Connected
};

struct Packet_Buffer
{
    usize offset;
    uint8 data[NET_MTU_SIZE];
};

struct Connection_List
{
    int32 max_count;
    int32 index;

    Zhc_Net_Address *address;
    uint64 *salt;
    Net_Conn_State *state;
    Packet_Buffer *packet_buffer; /* to be able to resend packages. */
};

enum Net_Message_Type
{
    Net_Message_Noop,
    Net_Message_Hash_Req,
    Net_Message_Hash_Res,
    Net_Message_Data_Req,
    Net_Message_Data_Res,
    Net_Message_Max
};

enum Packet_Type
{
    // NOTE(dgl): pending types
    Packet_Type_Denied,
    Packet_Type_Request,
    Packet_Type_Challenge,
    Packet_Type_Challenge_Resp,

    // NOTE(dgl): virtual type to make the check for connected types easier.
    // only these types can have a payload.
    _Packet_Type_Connected,
    Packet_Type_Disconnect,
    Packet_Type_Empty,
    Packet_Type_Payload,
    Packet_Type_Chunk,
    Packet_Type_Slice,
    Packet_Type_Ack,
    Packet_Type_Max
};

struct Packet_Ack
{
    uint32 hash;
    // NOTE(dgl): we use a bitfield of NET_MTU_SIZE to indicate which
    // message ids have been received. This bitfield is stored in the message
    // payload pointer. Received slices are 1, not received slices are 0.
};

struct Packet_Chunk
{
    uint32 hash;
    // NOTE(dgl): all slices use the available space up until
    // NET_MTU_SIZE. Only the last slize is smaller. This size
    // is sent in the first chunk packet.
    uint32 last_slice_size;
    uint32 slice_count;
};

struct Packet_Slice
{
    uint32 hash;
    uint32 index;
};

struct Packet
{
    int32 id;
    uint32 version;
    uint64 salt;
    // TODO(dgl): put CRC32 in here?

    Packet_Type type;
    Net_Message_Type msg_type;

    union
    {
        // NOTE(dgl): should not be used by
        Packet_Chunk chunk;
        Packet_Slice slice;
        Packet_Ack ack;
    };

    // NOTE(dgl): not serialized
    usize payload_size;
    uint8 *payload;
};

struct Net_Message
{
    uint32 version; /* 16 bit major, 8 bit minor, 8 bit patch */
    Net_Message_Type type;

    usize payload_size;
    uint8 *payload;
};

struct Net_Context
{
    // TODO(dgl): dont really like the packet buffer per connection
    // I think it would be better to make a packet stack and then send
    // all packages in the stack. Afterwards reset the stack. Or use a ring
    // buffer. This is probably better, because then we can have longer living
    // packets, like the ack packet.

    // NOTE(dgl): We currently support to send one chunk at a time
    // This creates the issue that if we change the active file while
    // sending a chunk, and another client requests the data, it gets the
    // old chunk. This is temporary for now. Later we will support a double
    // buffer. One for the old and one for the new data, to prevent this issue
    // This double buffer is only necessary on the server. Clients ony receive
    // one chunk at a time.
    // Another option would be to have a chunk reset package and resend the whole
    // data if a new chunk will be sent. This would be more memory efficient.
    // However, if we do this, we lose our spamming packets incase they are lost
    // ability during the connecting state. IMO this should not be a big issue.
    // But we have to test this.
    Packet_Chunk chunk_info;
    Net_Message_Type chunk_type;
    usize chunk_buffer_size;
    uint8 *chunk_buffer;

    // NOTE(dgl): dont like this in here. Should have this in an arena. @cleanup
    Packet_Buffer ack_buffer;

    // NOTE(dgl): is there a better way?
    // we use this to send the correct packet types
    // based on connection states
    bool32 is_server;

    real32 message_timeout;
    Zhc_Net_Socket socket;

    Connection_List *conns;
};

#endif // ZHC_NET_H
