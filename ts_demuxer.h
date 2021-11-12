#ifndef __TS_DEMUXER_H__
#define __TS_DEMUXER_H__

#include "es_output.h"

typedef void* P_TS_DEMUXER;

#define BAD_TS_DEMUXER ((P_TS_DEMUXER) NULL)

P_TS_DEMUXER ts_demuxer_create     (const char* pFileName);
void         ts_demuxer_free       (P_TS_DEMUXER pDemuxer);

int          ts_demuxer_add_output (P_TS_DEMUXER pDemuxer, ES_OUTPUT_TYPE eOutType, const char* pFileName);
int          ts_demuxer_start      (P_TS_DEMUXER pDemuxer);

#endif // __TS_DEMUXER_H__
