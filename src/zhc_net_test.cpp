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

    DGL_BEGIN_TEST("serializes packet");
    {
        Packet packet1 = {};
        packet1.id = 0x9988;
        packet1.version = parse_version("1.2.3");
        packet1.type = Packet_Type_Request;
        packet1.salt = 0xFFFF00000000FFFF;

        uint8 memory[sizeof(packet1)] = {};
        Bitstream writer = stream_writer_init(memory, array_count(memory));

        usize count = serialize_packet_(&writer, &packet1);

        DGL_EXPECT(count, ==, 20, usize, "%zu");
        DGL_EXPECT(writer.data[0], ==, 0x09988, uint32, "0x%X");
        DGL_EXPECT(writer.data[1], ==, 0x10203, uint32, "0x%X");
        DGL_EXPECT(writer.data[2], ==, 0x0000FFFF, uint32, "0x%X");
        DGL_EXPECT(writer.data[3], ==, 0xFFFF0000, uint32, "0x%X");
        DGL_EXPECT(writer.data[4], ==, 0x2, uint32, "0x%X");

        Packet packet2 = {};
        Bitstream reader = stream_reader_init(memory, array_count(memory));

        count = serialize_packet_(&reader, &packet2);
        DGL_EXPECT(count, ==, 20, usize, "%zu");
        DGL_EXPECT_int32(packet2.id, ==, packet1.id);
        DGL_EXPECT_uint32(packet2.version, ==, packet1.version);
        DGL_EXPECT_uint32(packet2.type, ==, packet1.type);
        DGL_EXPECT_uint64(packet2.salt, ==, packet1.salt);
    }
    DGL_END_TEST();

    if(dgl_test_result()) { return(0); }
    else { return(1); }
}
