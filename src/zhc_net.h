#ifndef ZHC_NET_H
#define ZHC_NET_H

// TODO(dgl): merge packet and message and reduce abstractions.

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

enum Packet_Type
{
    // NOTE(dgl): pending types
    Packet_Type_Denied,
    Packet_Type_Request,
    Packet_Type_Challenge,
    Packet_Type_Challenge_Resp,

    // NOTE(dgl): established types
    Packet_Type_Payload,
    Packet_Type_Disconnect,
    Packet_Type_Max
};

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

struct Packet
{
    int32 id;
    // TODO(dgl): put CRC32 in here?
    Packet_Type type;
    uint64 salt;
    Net_Conn_ID index;
};

struct Net_Context
{
    // NOTE(dgl): We currently support to send one chunk at a time
    // This creates the issue that if we change the active file while
    // sending a chunk, and another client requests the data, it gets the
    // old chunk. This is temporary for now. Later we will support a double
    // buffer. One for the old and one for the new data, to prevent this issue
    // This double buffer is only necessary on the server. Clients ony receive
    // one chunk at a time.
    // Another option would be to have a chunk reset package and resend the whole
    // data if a new chunk will be sent. This would be more memory efficient.
    uint32 chunk_hash;
    usize chunk_buffer_size;
    usize chunk_buffer_offset;
    uint32 chunk_slice_count;
    uint8 *chunk_buffer;

    // NOTE(dgl): is there a better way?
    // we use this to send the correct packet types
    // based on connection states
    bool32 is_server;

    real32 message_timeout;
    Zhc_Net_Socket socket;

    Connection_List *conns;
};

//
//
//

enum Net_Msg_Header_Type
{
    Net_Msg_Header_Noop,
    Net_Msg_Header_Hash_Req,
    Net_Msg_Header_Hash_Res,
    Net_Msg_Header_Data_Req,
    Net_Msg_Header_Data_Res,
    // NOTE(dgl): chunk is the first slice of the dataset
    // it contains additional information about the slices
    // these are for internal use and should not be used by
    // the caller.
    Net_Msg_Header_Chunk,
    Net_Msg_Header_Slice,
    Net_Msg_Header_Ack,
    Net_Msg_Header_Max
};

struct Msg
{
    uint32 size;
};

struct Msg_Ack
{
    uint16 id;
    // NOTE(dgl): we use a bitfield of NET_MTU_SIZE to indicate which
    // message ids have been received. This bitfield is stored in the message
    // _payload pointer. Received slices are 1, not received slices are 0.
};

struct Msg_Chunk
{
    uint32 hash;
    // NOTE(dgl): all slices use the available space up until
    // NET_MTU_SIZE. Only the last slize is smaller. This size
    // is sent in the first chunk packet.
    uint32 last_slice_size;
    uint32 slice_count;
};

struct Msg_Slice
{
    uint32 hash;
    uint32 index;
};

struct Net_Msg_Header
{
    uint32 version; /* 16 bit major, 8 bit minor, 8 bit patch */
    Net_Msg_Header_Type type;

    union
    {
        struct {
            usize size;
        };
        Msg_Chunk _chunk;
        Msg_Slice _slice;
        Msg_Ack _ack;
    };

    // NOTE(dgl): this is only set if the payload size is > 0.
    // This pointer is not sent over the network and is ignored on
    // serialization. It is used for pointing to the payload location
    // when receiving a header with payload. This is not a optimal solutino
    // but the easiest one for now. @cleanup
    uint8 *_payload;
};


#endif // ZHC_NET_H
