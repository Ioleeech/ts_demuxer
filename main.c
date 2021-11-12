#include <stdlib.h>

#include "print_out.h"
#include "ts_demuxer.h"

// Main routine
//
// Command-line arguments:
// 1 (argv[0]) = Application name
// 2 (argv[1]) = Input TS file location
// 3 (argv[2]) = Output video file location
// 4 (argv[3]) = Output audio file location
int main(const int argc, const char* argv[])
{
    if (argc == 4)
    {
        const char* pTsFileName    = argv[1];
        const char* pVideoFileName = argv[2];
        const char* pAudioFileName = argv[3];

        P_TS_DEMUXER pDemuxer = ts_demuxer_create(pTsFileName);

        if (pDemuxer != BAD_TS_DEMUXER)
        {
            int nResult = EXIT_SUCCESS;

            if (nResult == EXIT_SUCCESS)
                nResult = ts_demuxer_add_output(pDemuxer, ES_OUTPUT_VIDEO, pVideoFileName);

            if (nResult == EXIT_SUCCESS)
                nResult = ts_demuxer_add_output(pDemuxer, ES_OUTPUT_AUDIO, pAudioFileName);

            if (nResult == EXIT_SUCCESS)
                nResult = ts_demuxer_start(pDemuxer);

            ts_demuxer_free(pDemuxer);
            return nResult;
        }
    }
    else
    {
        OUT("\n");
        OUT("  Usage:\n");
        OUT("  ts_demuxer <input.ts> <video.out> <audio.out>\n");
        OUT("\n");
    }

    return EXIT_FAILURE;
}
