#include "zhc_lib.h"
#include "zhc_crypto.cpp"
#include "zhc_net.cpp"

#define DGL_IMPLEMENTATION
#include "dgl.h"

#include "dgl_test_helpers.h"

int
main(int argc, char **argv)
{
    DGL_BEGIN_TEST("bits_required returns the required bits for an integer");
    {
        DGL_EXPECT_int32(bits_required(1), ==, 1);
        DGL_EXPECT_int32(bits_required(127), ==, 7);
        DGL_EXPECT_int32(bits_required(128), ==, 8);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("pack_uint32 packs integer bitwise into bitstream buffer");
    {
        Bitstream bs = {};
        uint32 buffer[2] = {};
        bs.data = buffer;
        bs.count = array_count(buffer);

        pack_uint32(&bs, 12, 10, 12); /* 2 bits */
        pack_uint32(&bs, 128, 120, 130); /* 4 bits */
        pack_uint32(&bs, 0xFFFFFFFF, 0, 0xFFFFFFFF); /* 32 bits */
        stream_flush(&bs);

        DGL_EXPECT(buffer[0], ==, 0xFFFFFFE2, uint32, "0x%X");
        DGL_EXPECT(buffer[1], ==, 0x3F, uint32, "0x%X");
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("unpack_uint32 unpacks integer bitwise from bitstream buffer");
    {
        Bitstream bs = {};
        uint32 buffer[2] = {};
        bs.data = buffer;
        bs.count = array_count(buffer);

        pack_uint32(&bs, 12, 10, 12); /* 2 bits */
        pack_uint32(&bs, 128, 120, 130); /* 4 bits */
        pack_uint32(&bs, 0xFFFFFFFF, 0, 0xFFFFFFFF); /* 32 bits */
        stream_flush(&bs);

        bs.index = 0;
        bs.scratch = 0;
        bs.scratch_bits = 0;

        DGL_EXPECT_uint32(unpack_uint32(&bs, 10, 12), ==, 12);
        DGL_EXPECT_uint32(unpack_uint32(&bs, 120, 130), ==, 128);
        DGL_EXPECT(unpack_uint32(&bs, 0, 0xFFFFFFFF), ==, 0xFFFFFFFF, uint32, "0x%X");
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("serializes header");
    {
        Net_Msg_Header header1 = {};
        header1.type = Net_Msg_Header_Hash_Req;
        header1.version = parse_version("1.2.3");
        header1.size = dgl_safe_size_to_uint32(1337);

        uint8 memory[sizeof(header1)] = {};
        Bitstream writer = stream_writer_init(memory, array_count(memory));

        usize count = serialize_header(&writer, &header1);

        DGL_EXPECT(count, ==, 8, usize, "%zu");
        DGL_EXPECT(writer.data[0], ==, 0x00010203, uint32, "0x%X");
        DGL_EXPECT(writer.data[1], ==, 0x29C9, uint32, "0x%X"); /* 0101 0011 1001 (1337) and 001*/
        DGL_EXPECT(writer.data[2], ==, 0x0, uint32, "0x%X");

//         for(int32 index = 0; index < array_count(memory); ++index)
//         {
//             printf("0x%X ", memory[index]);
//         }
//         printf("\n");


        Net_Msg_Header header2 = {};
        Bitstream reader = stream_reader_init(memory, array_count(memory));

        count = serialize_header(&reader, &header2);
        DGL_EXPECT(count, ==, 8, usize, "%zu");
        DGL_EXPECT_uint32(header2.type, ==, Net_Msg_Header_Hash_Req);
        DGL_EXPECT_uint32(header2.version, ==, parse_version("1.2.3"));
        DGL_EXPECT(header2.size, ==, 1337, usize, "%zu");
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("serializes packet");
    {
        Packet packet1 = {};
        packet1.id = 0x9988;
        packet1.type = Packet_Type_Request;
        packet1.salt = 0xFFFF00000000FFFF;
        packet1.index = 23;

        uint8 memory[sizeof(packet1)] = {};
        Bitstream writer = stream_writer_init(memory, array_count(memory));

        usize count = serialize_packet(&writer, &packet1);

        DGL_EXPECT(count, ==, 16, usize, "%zu");
        DGL_EXPECT(writer.data[0], ==, 0x9988, uint32, "0x%X");
        DGL_EXPECT(writer.data[1], ==, 0x07FFF8B9, uint32, "0x%X");
        DGL_EXPECT(writer.data[2], ==, 0xF8000000, uint32, "0x%X");
        DGL_EXPECT(writer.data[3], ==, 0x7FF, uint32, "0x%X");


        Packet packet2 = {};
        Bitstream reader = stream_reader_init(memory, array_count(memory));

        count = serialize_packet(&reader, &packet2);
        DGL_EXPECT(count, ==, 16, usize, "%zu");
        DGL_EXPECT_int32(packet2.id, ==, 0x9988);
        DGL_EXPECT_uint32(packet2.type, ==, Packet_Type_Request);
        DGL_EXPECT_uint64(packet2.salt, ==, 0xFFFF00000000FFFF);
        DGL_EXPECT_int32(packet2.index, ==, 23);
    }
    DGL_END_TEST();

    if(dgl_test_result()) { return(0); }
    else { return(1); }
}
