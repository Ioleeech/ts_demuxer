#ifndef __ES_OUTPUT_H__
#define __ES_OUTPUT_H__

typedef void* P_ES_OUTPUT;

#define BAD_ES_OUTPUT ((P_ES_OUTPUT) NULL)

typedef enum _ES_OUTPUT_TYPE {
    ES_OUTPUT_VIDEO = 0,
    ES_OUTPUT_AUDIO,
    ES_OUTPUT_MAX_NUM
} ES_OUTPUT_TYPE;

extern const char* pStrOutputType[];

P_ES_OUTPUT es_output_create    (const char* pFileName, ES_OUTPUT_TYPE eType);
void        es_output_free      (P_ES_OUTPUT pOutput);

int         es_output_parse_pes (P_ES_OUTPUT    pOutput,
                                 unsigned char* pData,
                                 unsigned int   uLength,
                                 unsigned int   uUnitStart,
                                 unsigned int   uContinuity);

#endif // __ES_OUTPUT_H__
