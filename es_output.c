#include <stdio.h>
#include <stdlib.h>

#include "print_out.h"
#include "es_output.h"

#define PES_START_CODE      0x000001

#define STREAM_ID_PRIVATE_1 0xBD
#define STREAM_ID_PADDING   0xBE
#define STREAM_ID_PRIVATE_2 0xBF

#define STREAM_ID_AUDIO_MIN 0xC0
#define STREAM_ID_AUDIO_MAX 0xDF

#define STREAM_ID_VIDEO_MIN 0xE0
#define STREAM_ID_VIDEO_MAX 0xEF

#define PES_NO_PTS_DTS      0x00
#define PES_PTS_ONLY        0x02
#define PES_PTS_DTS         0x03

typedef struct _ES_OUTPUT {
    const char*    pFileName;
    FILE*          pFile;
    ES_OUTPUT_TYPE eType;
    unsigned int   uPacketsNum;
    unsigned int   uContinuity;
} ES_OUTPUT;

static const char pStrEmpty[] = "";
static const char pStrVideo[] = "Video";
static const char pStrAudio[] = "Audio";

static const char* pStrOutputType[ES_OUTPUT_MAX_NUM] = {
    pStrVideo, // ES_OUTPUT_VIDEO
    pStrAudio  // ES_OUTPUT_AUDIO
};

P_ES_OUTPUT es_output_create(const char* pFileName, ES_OUTPUT_TYPE eType)
{
    if ((eType < ES_OUTPUT_VIDEO)
    ||  (eType > ES_OUTPUT_AUDIO))
        return BAD_ES_OUTPUT;

    // Memory allocation for description struct and filling it
    ES_OUTPUT* pEsOutput = (ES_OUTPUT*) malloc(sizeof(ES_OUTPUT));

    if (! pEsOutput)
        return BAD_ES_OUTPUT;

    OUT("%s output file : \"%s\"\n", pStrOutputType[eType], pFileName);

    pEsOutput->pFileName   = pFileName;
    pEsOutput->pFile       = NULL;
    pEsOutput->eType       = eType;
    pEsOutput->uPacketsNum = 0;
    pEsOutput->uContinuity = 0;

    // Return the pointer to description struct
    return (P_ES_OUTPUT) pEsOutput;
}

void es_output_free(P_ES_OUTPUT pOutput)
{
    ES_OUTPUT* pEsOutput = (ES_OUTPUT*) pOutput;

    if (pEsOutput)
    {
        if (pEsOutput->pFile)
            fclose(pEsOutput->pFile);

        free(pEsOutput);
    }
}

int es_output_parse_pes(P_ES_OUTPUT pOutput, unsigned char* pData, unsigned int uLength, unsigned int uPID, unsigned int uUnitStart, unsigned int uContinuity)
{
    ES_OUTPUT* pEsOutput = (ES_OUTPUT*) pOutput;

    if (! pEsOutput)
        return EXIT_FAILURE;

    // Continuity counter checking
    if ((pEsOutput->uPacketsNum > 0) && (uContinuity != ((pEsOutput->uContinuity + 1) & 0x0F)))
    {
        ERR("PID %u : Incorrect continuity value (%u)\n", uPID, uContinuity);
        return EXIT_FAILURE;
    }

    // Parse PES header
    if ((uUnitStart) && (uLength > 9))
    {
        unsigned int  uStartCode  =  pData[0]         << 16;
                      uStartCode |=  pData[1]         << 8;
                      uStartCode |=  pData[2];
        unsigned char uStreamID   =  pData[3];
//      unsigned int  uPacketLen  =  pData[4]         << 8;
//                    uPacketLen |=  pData[5];
        unsigned char uMarker     = (pData[6] & 0xC0) >> 6; // Must be equal to 0x02
//      unsigned char uScrambling = (pData[6] & 0x30) >> 4;
//      unsigned char uPriority   = (pData[6] & 0x08) >> 3;
//      unsigned char uAlignment  = (pData[6] & 0x04) >> 2;
//      unsigned char uCopyright  = (pData[6] & 0x02) >> 1;
//      unsigned char uOriginal   = (pData[6] & 0x01);
        unsigned char uPTS_DTS    = (pData[7] & 0xC0) >> 6;
//      unsigned char uESCR       = (pData[7] & 0x20) >> 5;
//      unsigned char uEsRate     = (pData[7] & 0x10) >> 4;
//      unsigned char uDsmTrick   = (pData[7] & 0x08) >> 3;
//      unsigned char uCopyInfo   = (pData[7] & 0x04) >> 2;
//      unsigned char uCRC        = (pData[7] & 0x02) >> 1;
//      unsigned char uExtension  = (pData[7] & 0x01);
        unsigned char uHeaderLen  =  pData[8];

        pData   += 9;
        uLength -= 9;

        if (uStartCode != PES_START_CODE)
        {
            ERR("PID %u : Incorrect start code (%06X)\n", uPID, uStartCode);
            return EXIT_FAILURE;
        }

        if ((pEsOutput->eType == ES_OUTPUT_VIDEO)
        && ((uStreamID < STREAM_ID_VIDEO_MIN)
        ||  (uStreamID > STREAM_ID_VIDEO_MAX)))
        {
            ERR("PID %u : Incorrect stream ID (%02X)\n", uPID, uStreamID);
            return EXIT_FAILURE;
        }

        if ((pEsOutput->eType == ES_OUTPUT_AUDIO)
        &&  (uStreamID != STREAM_ID_PRIVATE_1)
        && ((uStreamID  < STREAM_ID_AUDIO_MIN)
        ||  (uStreamID  > STREAM_ID_AUDIO_MAX)))
        {
            ERR("PID %u : Incorrect stream ID (%02X)\n", uPID, uStreamID);
            return EXIT_FAILURE;
        }

        if ((uMarker != 0x02)
        ||  (uHeaderLen > uLength))
        {
            ERR("PID %u : Incorrect PES header\n", uPID);
            return EXIT_FAILURE;
        }

        // Parse PES header extensions
        if (((uPTS_DTS == PES_PTS_ONLY) || (uPTS_DTS == PES_PTS_DTS)) && (uHeaderLen > 4))
        {
            unsigned long long lluPTS_90kHz = 0LLU;
            unsigned long long lluDTS_90kHz = 0LLU;

            // 33-bit PTS
            lluPTS_90kHz  = (pData[0] >> 1) & 0x07; lluPTS_90kHz <<= 8;
            lluPTS_90kHz |=  pData[1];              lluPTS_90kHz <<= 7;
            lluPTS_90kHz |= (pData[2] >> 1);        lluPTS_90kHz <<= 8;
            lluPTS_90kHz |=  pData[3];              lluPTS_90kHz <<= 7;
            lluPTS_90kHz |= (pData[4] >> 1);

            // 33-bit DTS
            lluDTS_90kHz = lluPTS_90kHz;

            if ((uPTS_DTS == PES_PTS_DTS) && (uHeaderLen > 9))
            {
                lluDTS_90kHz  = (pData[5] >> 1) & 0x07; lluDTS_90kHz <<= 8;
                lluDTS_90kHz |=  pData[6];              lluDTS_90kHz <<= 7;
                lluDTS_90kHz |= (pData[7] >> 1);        lluDTS_90kHz <<= 8;
                lluDTS_90kHz |=  pData[8];              lluDTS_90kHz <<= 7;
                lluDTS_90kHz |= (pData[9] >> 1);
            }

            OUT("PID %u: %s frame, PTS %llu, DTS %llu\n", uPID, pStrOutputType[pEsOutput->eType], lluPTS_90kHz, lluDTS_90kHz);
        }

        pData   += uHeaderLen;
        uLength -= uHeaderLen;
    }

    // Write data
    if (uLength > 0)
    {
        if (! pEsOutput->pFile)
            pEsOutput->pFile = fopen(pEsOutput->pFileName, "wb");

        if (! pEsOutput->pFile)
            return EXIT_FAILURE;

        fwrite(pData, 1, uLength, pEsOutput->pFile);
    }

    pEsOutput->uPacketsNum += 1;
    pEsOutput->uContinuity  = uContinuity;

    return EXIT_SUCCESS;
}

const char* es_output_type_str(ES_OUTPUT_TYPE eType)
{
    return ((eType < ES_OUTPUT_VIDEO) || (eType > ES_OUTPUT_AUDIO)) ? pStrEmpty : pStrOutputType[eType];
}
