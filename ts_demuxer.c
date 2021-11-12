#include <stdio.h>
#include <stdlib.h>

#include "print_out.h"
#include "ts_demuxer.h"

#define TS_PACKET_SIZE_188  188
#define TS_PACKET_SIZE_192  192
#define TS_PACKET_SIZE_204  204

#define TS_PACKET_SIZE_MIN  TS_PACKET_SIZE_188
#define TS_PACKET_SIZE_MAX  TS_PACKET_SIZE_204

#define TS_SYNC_CODE        0x47

#define TS_PAYLOAD_ONLY     0x01
#define TS_ADAPT_FIELD_ONLY 0x02
#define TS_BOTH_FIELDS      0x03

#define TS_PID_PAT          0x0000
#define TS_PID_MIN          0x0020
#define TS_PID_MAX          0x1FFA

#define ES_STREAM_H264      0x1B
#define ES_STREAM_ADTS_AAC  0x0F

typedef struct _TS_DEMUXER {
    const char*  pFileName;
    FILE*        pFile;
    unsigned int uFileOffset;
    unsigned int uPacketSize;
    unsigned int uPacketsNum;
    unsigned int uPMT_PID;
    unsigned int uVideoPID;
    unsigned int uAudioPID;
    P_ES_OUTPUT  pVideoOutput;
    P_ES_OUTPUT  pAudioOutput;
} TS_DEMUXER;

static int _ts_demuxer_get_file_info(FILE* pFile, unsigned int* pFileOffset, unsigned int* pPacketSize)
{
    unsigned int uFileOffset = 0;
    unsigned int uPacketSize = 0;
    unsigned int i, j;

    // Memory allocation for temporary buffer and filling the buffer by some first bytes from the file
    unsigned int   uBufSize = TS_PACKET_SIZE_MAX * 6;
    unsigned char* pBuffer  = (unsigned char*) malloc(uBufSize);

    if (! pBuffer)
        return EXIT_FAILURE;

    if ((fseek(pFile, 0, SEEK_SET) < 0)
    ||  (fread(pBuffer, 1, uBufSize, pFile) != uBufSize))
    {
        free(pBuffer);
        return EXIT_FAILURE;
    }

    // Finding of first TS packet and detection of packet size
    for (i = 0; i < TS_PACKET_SIZE_MAX; i ++)
    {
        if (pBuffer[i] != TS_SYNC_CODE)
            continue;

        for (j = i; j < (uBufSize - TS_PACKET_SIZE_MIN); j += uPacketSize)
        {
                 if ((pBuffer[j + TS_PACKET_SIZE_188] == TS_SYNC_CODE) && ((! uPacketSize) || (uPacketSize == TS_PACKET_SIZE_188)))
                uPacketSize = TS_PACKET_SIZE_188;
            else if ((pBuffer[j + TS_PACKET_SIZE_192] == TS_SYNC_CODE) && ((! uPacketSize) || (uPacketSize == TS_PACKET_SIZE_192)))
                uPacketSize = TS_PACKET_SIZE_192;
            else if ((pBuffer[j + TS_PACKET_SIZE_204] == TS_SYNC_CODE) && ((! uPacketSize) || (uPacketSize == TS_PACKET_SIZE_204)))
                uPacketSize = TS_PACKET_SIZE_204;
            else
                uPacketSize = 0;

            if (! uPacketSize)
                break;
        }

        uFileOffset = i;
        break;
    }

    if (! uPacketSize)
    {
        free(pBuffer);
        return EXIT_FAILURE;
    }

    // Release temporary buffer
    free(pBuffer);
    pBuffer = NULL;

    // Moving the file position indicator to the begining of first TS packet
    if (fseek(pFile, uFileOffset, SEEK_SET) < 0)
        return EXIT_FAILURE;

    // Return the results
    if (pFileOffset) *pFileOffset = uFileOffset;
    if (pPacketSize) *pPacketSize = uPacketSize;

    return EXIT_SUCCESS;
}

static int _ts_demuxer_parse_adapt_field(TS_DEMUXER* pTsDemuxer, unsigned char* pAdaptField, unsigned int uAdaptLen)
{
//  OUT("%08X : Adaptation field (%u bytes)\n", pTsDemuxer->uFileOffset, uAdaptLen);

    if (uAdaptLen < 1)
        return EXIT_SUCCESS;

    if ((uAdaptLen > 0) && ((uAdaptLen + 5) > pTsDemuxer->uPacketSize))
    {
        ERR("%08X : Incorrect adaptation field length (%u bytes)\n", pTsDemuxer->uFileOffset, uAdaptLen);
        return EXIT_FAILURE;
    }

//  unsigned int uDiscontinuity = pAdaptField[0] & 0x80;
//  unsigned int uRandomAccess  = pAdaptField[0] & 0x40;
//  unsigned int uEsPriority    = pAdaptField[0] & 0x20;
    unsigned int uPCR           = pAdaptField[0] & 0x10;
//  unsigned int uOPCR          = pAdaptField[0] & 0x08;
//  unsigned int uSplicingPoint = pAdaptField[0] & 0x04;
//  unsigned int uPrivateData   = pAdaptField[0] & 0x02;
//  unsigned int uExtension     = pAdaptField[0] & 0x01;

    if ((uPCR) && (uAdaptLen > 6))
    {
        // 48-bit program clock reference (PCR):
        // 33 bits: base
        //  6 bits: reserved
        //  9 bits: extension
        //
        // PCR (90 kHz clock) = base
        // PCR (27 MHz clock) = base * 300 + extension
        //
        // 90 kHz PCR
        unsigned long long lluPCR_90kHz  =  pAdaptField[1]; lluPCR_90kHz <<= 8;
                           lluPCR_90kHz |=  pAdaptField[2]; lluPCR_90kHz <<= 8;
                           lluPCR_90kHz |=  pAdaptField[3]; lluPCR_90kHz <<= 8;
                           lluPCR_90kHz |=  pAdaptField[4]; lluPCR_90kHz <<= 1;
                           lluPCR_90kHz |= (pAdaptField[5] >> 7);

        OUT("PCR %llu\n", lluPCR_90kHz);
    }

    return EXIT_SUCCESS;
}

static int _ts_demuxer_parse_pat(TS_DEMUXER* pTsDemuxer, unsigned char* pPayload, unsigned int uPayloadLen)
{
//  unsigned int  uPayloadPointer =  pPayload[0];
    unsigned char uTableID        =  pPayload[1];              // Must be 0 for PAT
    unsigned char uSyntaxSection  = (pPayload[2] & 0x80) >> 7;
    unsigned char uPrivateBit     = (pPayload[2] & 0x40) >> 6; // Must be 0 for PAT
    unsigned char uReserved1      = (pPayload[2] & 0x3C) >> 2; // Must be equal to 0x0C
    unsigned int  uSectionLength  = (pPayload[2] & 0x03) << 8;
                  uSectionLength |=  pPayload[3];

    if ((uPrivateBit)
    ||  (uTableID            != 0x00)
    ||  (uReserved1          != 0x0C)
    || ((uSectionLength + 4) != uPayloadLen)
    ||  (uSectionLength < 4))
    {
        ERR("%08X : Incorrect table header for PAT (%02X %02X %02X)\n", pTsDemuxer->uFileOffset, pPayload[0], pPayload[1], pPayload[2]);
        return EXIT_FAILURE;
    }

    // Exclude CRC32
    uSectionLength -= 4;

    if ((uSyntaxSection) && (uSectionLength > 5))
    {
        unsigned char* pSection = pPayload + 4;

//      unsigned int  uStreamID    =  pSection[0]         << 8;
//                    uStreamID   |=  pSection[1];
//      unsigned char uReserved2   = (pSection[2] & 0xC0) >> 6; // Must be equal to 0x03
//      unsigned char uVersion     = (pSection[2] & 0x3E) >> 1;
//      unsigned char uCurrent     = (pSection[2] & 0x01);
        unsigned char uSectionNum  =  pSection[3];
        unsigned char uSectionLast =  pSection[4];

        pSection       += 5;
        uSectionLength -= 5;

        for ( ; (uSectionLength > 3) ; )
        {
//          unsigned int  uProgramNum  =  pSection[0]         << 8; // Must be equal to Stream ID
//                        uProgramNum |=  pSection[1];
//          unsigned char uReserved3   = (pSection[2] & 0xE0) >> 5; // Must be equal to 0x07
            unsigned int  uPMT_PID     = (pSection[2] & 0x1F) << 8;
                          uPMT_PID    |=  pSection[3];

            if (! pTsDemuxer->uPMT_PID)
                pTsDemuxer->uPMT_PID = uPMT_PID;

            OUT("PMT PID %u\n", uPMT_PID);

            if (uSectionNum >= uSectionLast)
                break;

            pSection       += 4;
            uSectionLength -= 4;
            uSectionNum    += 1;
        }
    }

    return EXIT_SUCCESS;
}

static int _ts_demuxer_parse_pmt(TS_DEMUXER* pTsDemuxer, unsigned char* pPayload, unsigned int uPayloadLen)
{
//  unsigned int  uPayloadPointer =  pPayload[0];
    unsigned char uTableID        =  pPayload[1];              // Must be 2 for PMT
    unsigned char uSyntaxSection  = (pPayload[2] & 0x80) >> 7;
    unsigned char uPrivateBit     = (pPayload[2] & 0x40) >> 6; // Must be 0 for PMT
    unsigned char uReserved1      = (pPayload[2] & 0x3C) >> 2; // Must be equal to 0x0C
    unsigned int  uSectionLength  = (pPayload[2] & 0x03) << 8;
                  uSectionLength |=  pPayload[3];

    if ((uPrivateBit)
    ||  (uTableID            != 0x02)
    ||  (uReserved1          != 0x0C)
    || ((uSectionLength + 4) != uPayloadLen)
    ||  (uSectionLength < 4))
    {
        ERR("%08X : Incorrect table header for PMT (%02X %02X %02X)\n", pTsDemuxer->uFileOffset, pPayload[0], pPayload[1], pPayload[2]);
        return EXIT_FAILURE;
    }

    // Exclude CRC32
    uSectionLength -= 4;

    if ((uSyntaxSection) && (uSectionLength > 5))
    {
        unsigned char* pSection = pPayload + 4;

//      unsigned int  uProgramNum  =  pSection[0]         << 8;
//                    uProgramNum |=  pSection[1];
//      unsigned char uReserved2   = (pSection[2] & 0xC0) >> 6; // Must be equal to 0x03
//      unsigned char uVersion     = (pSection[2] & 0x3E) >> 1;
//      unsigned char uCurrent     =  pSection[2] & 0x01;
        unsigned char uSectionNum  =  pSection[3];
        unsigned char uSectionLast =  pSection[4];

        pSection       += 5;
        uSectionLength -= 5;

        for ( ; (uSectionLength > 3) ; )
        {
//          unsigned char uReserved3 = (pSection[0] & 0xE0) >> 5; // Must be equal to 0x07
            unsigned int  uPCR_PID   = (pSection[0] & 0x1F) << 8;
                          uPCR_PID  |=  pSection[1];
//          unsigned char uReserved4 = (pSection[2] & 0xFC) >> 2; // Must be equal to 0x3C
            unsigned int  uInfoLen   = (pSection[2] & 0x03) << 8;
                          uInfoLen  |=  pSection[3];

            OUT("PCR PID %u\n", uPCR_PID);

            if (uSectionLength < (4 + uInfoLen))
                break;

            pSection       += (4 + uInfoLen);
            uSectionLength -= (4 + uInfoLen);

            for ( ; (uSectionLength > 4) ; )
            {
                unsigned char uStreamType =  pSection[0];
//              unsigned char uReserved5  = (pSection[1] & 0xE0) >> 5; // Must be equal to 0x07
                unsigned int  uStreamPID  = (pSection[1] & 0x1F) << 8;
                              uStreamPID |=  pSection[2];
//              unsigned char uReserved6  = (pSection[3] & 0xFC) >> 2; // Must be equal to 0x3C
                unsigned int  uStrInfLen  = (pSection[3] & 0x03) << 8;
                              uStrInfLen |=  pSection[4];

                if ((! pTsDemuxer->uVideoPID) && (uStreamType == ES_STREAM_H264))
                    pTsDemuxer->uVideoPID = uStreamPID;

                if ((! pTsDemuxer->uAudioPID) && (uStreamType == ES_STREAM_ADTS_AAC))
                    pTsDemuxer->uAudioPID = uStreamPID;

                OUT("Stream type 0x%02X PID %u\n", uStreamType, uStreamPID);

                if (uSectionLength < (5 + uStrInfLen))
                    break;

                pSection       += (5 + uStrInfLen);
                uSectionLength -= (5 + uStrInfLen);
            }

            if (uSectionNum >= uSectionLast)
                break;

            uSectionNum ++;
        }
    }

    return EXIT_SUCCESS;
}

static int _ts_demuxer_parse_payload(TS_DEMUXER*    pTsDemuxer,
                                     unsigned char* pPayload,
                                     unsigned int   uPayloadLen,
                                     unsigned int   uPID,
                                     unsigned int   uUnitStart,
                                     unsigned int   uContinuity)
{
    P_ES_OUTPUT pOutput = BAD_ES_OUTPUT;

//  OUT("%08X : Payload (%u bytes, %02X %02X %02X %02X), PID %u, Unit start %u, Continuity %u\n",
//      pTsDemuxer->uFileOffset,
//      uPayloadLen,
//      pPayload[0], pPayload[1], pPayload[2], pPayload[3],
//      uPID,
//      uUnitStart,
//      uContinuity);

    if (uPayloadLen < 1)
    {
        ERR("%08X : Incorrect payload length (%u bytes)\n", pTsDemuxer->uFileOffset, uPayloadLen);
        return EXIT_FAILURE;
    }

    if (uUnitStart)
    {
        if (uPID == TS_PID_PAT)
        {
            // Program association table (PAT)
            return _ts_demuxer_parse_pat(pTsDemuxer, pPayload, uPayloadLen);
        }
        else if ((pTsDemuxer->uPMT_PID > 0) && (uPID == pTsDemuxer->uPMT_PID))
        {
            // Program map table (PMT)
            return _ts_demuxer_parse_pmt(pTsDemuxer, pPayload, uPayloadLen);
        }
    }

    if ((pTsDemuxer->uVideoPID > 0) && (uPID == pTsDemuxer->uVideoPID))
        pOutput = pTsDemuxer->pVideoOutput;

    if ((pTsDemuxer->uAudioPID > 0) && (uPID == pTsDemuxer->uAudioPID))
        pOutput = pTsDemuxer->pAudioOutput;

    return (pOutput != BAD_ES_OUTPUT)
           ? es_output_parse_pes(pOutput, pPayload, uPayloadLen, uUnitStart, uContinuity)
           : EXIT_SUCCESS;
}

static int _ts_demuxer_parse_packet(TS_DEMUXER* pTsDemuxer, unsigned char* pPacket, unsigned int uRest)
{
    for ( ; ; )
    {
        if (uRest < pTsDemuxer->uPacketSize)
            break;

        if (pPacket[0] != TS_SYNC_CODE)
        {
            ERR("%08X : Sync byte was not found (0x%02X)\n", pTsDemuxer->uFileOffset, pPacket[0]);
            return EXIT_FAILURE;
        }
        else
        {
            unsigned char* pPayload    = NULL;
            unsigned int   uPayloadLen = 0;

            unsigned char* pAdaptField = NULL;
            unsigned int   uAdaptLen   = 0;

//          unsigned int uError     = (pPacket[1] & 0x80) ? 1 : 0;
            unsigned int uUnitStart = (pPacket[1] & 0x40) ? 1 : 0;
//          unsigned int uPriority  = (pPacket[1] & 0x20) ? 1 : 0;

            unsigned int uPID  = (pPacket[1] & 0x1F) << 8;
                         uPID |=  pPacket[2];

//          unsigned int uScrambling = (pPacket[3] & 0xC0) >> 6;
            unsigned int uFieldCtrl  = (pPacket[3] & 0x30) >> 4;
            unsigned int uContinuity = (pPacket[3] & 0x0F);

            switch(uFieldCtrl)
            {
                case TS_PAYLOAD_ONLY:
                    pPayload    = pPacket                 + 4;
                    uPayloadLen = pTsDemuxer->uPacketSize - 4;
                    break;

                case TS_ADAPT_FIELD_ONLY:
                    pAdaptField = pPacket + 5;
                    uAdaptLen   = pPacket[4];
                    break;

                case TS_BOTH_FIELDS:
                    pAdaptField = pPacket + 5;
                    uAdaptLen   = pPacket[4];
                    pPayload    = pPacket                 + (uAdaptLen + 5);
                    uPayloadLen = pTsDemuxer->uPacketSize - (uAdaptLen + 5);
                    break;

                default:
                    ERR("%08X : Incorrect adaptation field control value (0x%02X)\n", pTsDemuxer->uFileOffset, uFieldCtrl);
                    return EXIT_FAILURE;
            }

            if ((pAdaptField) && (_ts_demuxer_parse_adapt_field(pTsDemuxer, pAdaptField, uAdaptLen) != EXIT_SUCCESS))
                return EXIT_FAILURE;

            if ((pPayload) && (_ts_demuxer_parse_payload(pTsDemuxer, pPayload, uPayloadLen, uPID, uUnitStart, uContinuity) != EXIT_SUCCESS))
                return EXIT_FAILURE;
        }

        pTsDemuxer->uPacketsNum += 1;
        pTsDemuxer->uFileOffset += pTsDemuxer->uPacketSize;

        uRest   -= pTsDemuxer->uPacketSize;
        pPacket += pTsDemuxer->uPacketSize;
    }

    return EXIT_SUCCESS;
}

P_TS_DEMUXER ts_demuxer_create(const char* pFileName)
{
    // Opening of input file
    FILE* pFile = fopen(pFileName, "rb");

    if (! pFile)
        return BAD_TS_DEMUXER;

    // Get file parameters
    unsigned int uFileOffset = 0;
    unsigned int uPacketSize = 0;

    if (_ts_demuxer_get_file_info(pFile, &uFileOffset, &uPacketSize) != EXIT_SUCCESS)
    {
        fclose(pFile);
        return BAD_TS_DEMUXER;
    }

    // Memory allocation for TS description struct and filling it
    TS_DEMUXER* pTsDemuxer = (TS_DEMUXER*) malloc(sizeof(TS_DEMUXER));

    if (! pTsDemuxer)
    {
        fclose(pFile);
        return BAD_TS_DEMUXER;
    }

    OUT("Input TS file     : \"%s\"\n",   pFileName);
    OUT("Initial offset    : %u bytes\n", uFileOffset);
    OUT("Packet size       : %u bytes\n", uPacketSize);

    pTsDemuxer->pFileName    = pFileName;
    pTsDemuxer->pFile        = pFile;
    pTsDemuxer->uFileOffset  = uFileOffset;
    pTsDemuxer->uPacketSize  = uPacketSize;
    pTsDemuxer->uPacketsNum  = 0;
    pTsDemuxer->uPMT_PID     = 0;
    pTsDemuxer->uVideoPID    = 0;
    pTsDemuxer->uAudioPID    = 0;
    pTsDemuxer->pVideoOutput = BAD_ES_OUTPUT;
    pTsDemuxer->pAudioOutput = BAD_ES_OUTPUT;

    // Return the pointer to TS description struct
    return (P_TS_DEMUXER) pTsDemuxer;
}

void ts_demuxer_free(P_TS_DEMUXER pDemuxer)
{
    TS_DEMUXER* pTsDemuxer = (TS_DEMUXER*) pDemuxer;

    if (pTsDemuxer)
    {
        if (pTsDemuxer->pVideoOutput != BAD_ES_OUTPUT)
            es_output_free(pTsDemuxer->pVideoOutput);

        if (pTsDemuxer->pAudioOutput != BAD_ES_OUTPUT)
            es_output_free(pTsDemuxer->pAudioOutput);

        if (pTsDemuxer->pFile)
            fclose(pTsDemuxer->pFile);

        free(pTsDemuxer);
    }
}

int ts_demuxer_add_output(P_TS_DEMUXER pDemuxer, ES_OUTPUT_TYPE eOutType, const char* pFileName)
{
    P_ES_OUTPUT* ppOutput   = NULL;
    TS_DEMUXER*  pTsDemuxer = (TS_DEMUXER*) pDemuxer;

    if (! pTsDemuxer)
        return EXIT_FAILURE;

    switch (eOutType)
    {
        case ES_OUTPUT_VIDEO: ppOutput = &pTsDemuxer->pVideoOutput; break;
        case ES_OUTPUT_AUDIO: ppOutput = &pTsDemuxer->pAudioOutput; break;
        default:                                                    break;
    }

    if (! ppOutput)
        return EXIT_FAILURE;

    if (*ppOutput != BAD_ES_OUTPUT)
    {
        ERR("Output already exists (%s)\n", pStrOutputType[eOutType]);
        return EXIT_FAILURE;
    }

    *ppOutput = es_output_create(pFileName, eOutType);

    return (*ppOutput != BAD_ES_OUTPUT) ? EXIT_SUCCESS : EXIT_FAILURE;
}

int ts_demuxer_start(P_TS_DEMUXER pDemuxer)
{
    TS_DEMUXER* pTsDemuxer = (TS_DEMUXER*) pDemuxer;

    if (! pTsDemuxer)
        return EXIT_FAILURE;

    // Memory allocation for data buffer
    unsigned int   uBufSize = pTsDemuxer->uPacketSize * 1024;
    unsigned char* pBuffer  = (unsigned char*) malloc(uBufSize);

    if (! pBuffer)
        return EXIT_FAILURE;

    // Read data to buffer and process it
    for ( ; ; )
    {
        unsigned int uRead = fread(pBuffer, 1, uBufSize, pTsDemuxer->pFile);

        if (_ts_demuxer_parse_packet(pTsDemuxer, pBuffer, uRead) != EXIT_SUCCESS)
            break;

        if (uRead != uBufSize)
            break;
    }

    // Release data buffer
    free(pBuffer);

    OUT("%u packets were processed\n", pTsDemuxer->uPacketsNum);
    return EXIT_SUCCESS;
}
