#include <stdio.h>
#include <stdlib.h>

#include "print_out.h"
#include "es_output.h"

#define PES_BOTH_STAMPS 0xC0
#define PES_PTS_ONLY    0x80
#define PES_DTS_ONLY    0x00

typedef struct _ES_OUTPUT {
    const char*    pFileName;
    FILE*          pFile;
    ES_OUTPUT_TYPE eType;
    unsigned int   uPacketsNum;
    unsigned int   uContinuity;
} ES_OUTPUT;

const char pStrVideo[] = "video";
const char pStrAudio[] = "audio";

const char* pStrOutputType[ES_OUTPUT_MAX_NUM] = {
    pStrVideo, // ES_OUTPUT_VIDEO
    pStrAudio  // ES_OUTPUT_AUDIO
};

P_ES_OUTPUT es_output_create(const char* pFileName, ES_OUTPUT_TYPE eType)
{
    if ((eType < ES_OUTPUT_VIDEO)
    ||  (eType > ES_OUTPUT_AUDIO))
        return BAD_ES_OUTPUT;

    // Opening of output file
    FILE* pFile = fopen(pFileName, "wb");

    if (! pFile)
        return BAD_ES_OUTPUT;

    // Memory allocation for description struct and filling it
    ES_OUTPUT* pEsOutput = (ES_OUTPUT*) malloc(sizeof(ES_OUTPUT));

    if (! pEsOutput)
    {
        fclose(pFile);
        return BAD_ES_OUTPUT;
    }

    OUT("Output %s file : \"%s\"\n", pStrOutputType[eType], pFileName);

    pEsOutput->pFileName   = pFileName;
    pEsOutput->pFile       = pFile;
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

int es_output_parse_pes(P_ES_OUTPUT pOutput, unsigned char* pData, unsigned int uLength, unsigned int uUnitStart, unsigned int uContinuity)
{
    ES_OUTPUT* pEsOutput = (ES_OUTPUT*) pOutput;

    if (! pEsOutput)
        return EXIT_FAILURE;

    // Continuity counter checking
    if ((pEsOutput->uPacketsNum > 0) && (uContinuity != ((pEsOutput->uContinuity + 1) & 0x0F)))
    {
        ERR("Incorrect continuity value (%u)\n", uContinuity);
        return EXIT_FAILURE;
    }

    if ((uUnitStart) && (uLength > 12))
    {
        // Skip start code
        pData   += 4;
        uLength -= 4;

//      unsigned int pes_len  = pData[0] << 8;
//                   pes_len |= pData[1];
        unsigned int marker  =  pData[2] & 0xC0;
        unsigned int pts_dts =  pData[3] & 0xC0;
        unsigned int hdr_len =  pData[4];

        if ((marker == 0x80) && ((pts_dts == PES_BOTH_STAMPS) || (pts_dts == PES_PTS_ONLY)) && (hdr_len >= 5))
        {
            // 33-bit PTS
            unsigned long long pts_90kHz  = (pData[5] >> 1) & 0x07; pts_90kHz <<= 8;
                               pts_90kHz |=  pData[6];              pts_90kHz <<= 7;
                               pts_90kHz |= (pData[7] >> 1);        pts_90kHz <<= 8;
                               pts_90kHz |=  pData[8];              pts_90kHz <<= 7;
                               pts_90kHz |= (pData[9] >> 1);

            OUT("PTS (%s) %llu\n", pStrOutputType[pEsOutput->eType], pts_90kHz);
        }
    }

    if (uLength > 0)
        fwrite(pData, 1, uLength, pEsOutput->pFile);

    pEsOutput->uPacketsNum += 1;
    pEsOutput->uContinuity  = uContinuity;

    return EXIT_SUCCESS;
}
