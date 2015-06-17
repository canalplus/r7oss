#include "pes.h"
#include "collator_ctest.h"

using ::testing::NotNull;

void Collator_cTest::insertMarkerFrame(char *data, char markerType) {
    // start code
    data[0] = 0;
    data[1] = 0;
    data[2] = 1;

    // stream_id
    data[3] = 0xFB;

    // PES packet length
    data[4] = 0;
    data[5] = 0x14;

    // Optional PES header
    data[6] = 0x80;

    // Optional PES header flags
    data[7] = 0x1;

    // PES header data length
    data[8] = 0x11;

    // PES extension flags
    data[9] = 0x80;

    // PES private data
    // ASCII marker pattern
    data[10] = 'S';
    data[11] = 'T';
    data[12] = 'M';
    data[13] = 'M';

    // marker type
    data[14] = markerType;

    // marker ID0
    data[15] = 0x01;
    data[16] = 0x02;
    data[17] = 0x03;
    data[18] = 0x04;

    // marker ID1
    data[19] = 0x0A;
    data[20] = 0x0B;
    data[21] = 0x0C;
    data[22] = 0x0D;
}


void Collator_cTest::doTest(const char *filename, int blocksize, int markerFrameOffset, int markerType) {
    PlayerInputDescriptor_t  inputDescriptor;
    inputDescriptor.UnMuxedStream           = &mMockStream;
    inputDescriptor.PlaybackTimeValid       = false;
    inputDescriptor.DecodeTimeValid         = false;
    inputDescriptor.DataSpecificFlags       = 0;

    FILE *fin = fopen(filename, "rb");
    EXPECT_THAT(fin, NotNull()) << "Can not open " << filename;

    struct stat fileInfo;
    int result = stat(filename, &fileInfo);
    EXPECT_EQ(0, result);
    int filesize = fileInfo.st_size;

    char *data = new char [blocksize];

    int readFromFile = 0;

    char markerFrame[PES_CONTROL_SIZE];
    insertMarkerFrame(markerFrame, markerType);

    while (filesize - readFromFile > blocksize) {
        if (markerFrameOffset >= 0 && markerFrameOffset + PES_CONTROL_SIZE <= blocksize) {
            size_t read = fread(data, 1, markerFrameOffset, fin);
            EXPECT_EQ(markerFrameOffset, read);
            readFromFile += read;

            memcpy(data + markerFrameOffset, markerFrame, PES_CONTROL_SIZE);

            read = fread(data + markerFrameOffset + PES_CONTROL_SIZE, 1, blocksize - markerFrameOffset - PES_CONTROL_SIZE, fin);
            EXPECT_EQ(blocksize - markerFrameOffset - PES_CONTROL_SIZE, read);
            readFromFile += read;
        } else if (markerFrameOffset >= 0 && markerFrameOffset < blocksize) {
            size_t read = fread(data, 1, markerFrameOffset, fin);
            EXPECT_EQ(markerFrameOffset, read);
            readFromFile += read;

            memcpy(data + markerFrameOffset, markerFrame, blocksize - markerFrameOffset);
        } else if (markerFrameOffset < 0 && markerFrameOffset + PES_CONTROL_SIZE > 0) {
            memcpy(data, markerFrame - markerFrameOffset, markerFrameOffset + PES_CONTROL_SIZE);

            size_t read = fread(data + markerFrameOffset + PES_CONTROL_SIZE, 1, blocksize - markerFrameOffset - PES_CONTROL_SIZE, fin);
            EXPECT_EQ(blocksize - markerFrameOffset - PES_CONTROL_SIZE, read);
            readFromFile += read;
        } else {
            size_t read = fread(data, 1, blocksize, fin);
            EXPECT_EQ(blocksize, read);
            readFromFile += read;
        }

        PlayerStatus_t status = mCollator.Input(&inputDescriptor, blocksize, data);
        EXPECT_EQ(PlayerNoError, status);

        markerFrameOffset -= blocksize;
        SE_VERBOSE(group_collator_audio, "Read %d bytes from file\n", readFromFile);
    }

    size_t read = fread(data, 1, filesize - readFromFile, fin);
    EXPECT_EQ(filesize - readFromFile, read);

    PlayerStatus_t status = mCollator.Input(&inputDescriptor, filesize - readFromFile, data);
    EXPECT_EQ(PlayerNoError, status);

    readFromFile += read;
    EXPECT_EQ(filesize, readFromFile);

    SE_INFO(group_collator_audio, "Read %d bytes from file\n", readFromFile);

    fclose(fin);
    delete [] data;
}

int controlDataTypeToInternalMarkerType(int controlDataType) {
    switch (controlDataType) {
    case PES_CONTROL_BRK_SPLICING:
        return SplicingMarker;

    default:
        assert(0);
    }
}
