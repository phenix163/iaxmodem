/*
 * SpanDSP - a series of DSP components for telephony
 *
 * lpc10_tests.c - Test the LPC10 low bit rate speech codec.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: lpc10_tests.c,v 1.18 2008/05/13 13:17:26 steveu Exp $
 */

/*! \file */

/*! \page lpc10_tests_page LPC10 codec tests
\section lpc10_tests_page_sec_1 What does it do?

\section lpc10_tests_page_sec_2 How is it used?
To perform a general audio quality test, lpc10 should be run. The file ../test-data/local/short_nb_voice.wav
will be compressed to LPC10 data, decompressed, and the resulting audio stored in post_lpc10.wav.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <audiofile.h>

#include "spandsp.h"

#define BLOCK_LEN       180

#define BLOCKS_PER_READ 5

#define IN_FILE_NAME            "../test-data/local/dam9.wav"
#define REF_FILE_NAME           "../test-data/local/dam9_lpc55.wav"
#define COMPRESS_FILE_NAME      "lpc10_out.lpc10"
#define DECOMPRESS_FILE_NAME    "lpc10_in.lpc10"
#define OUT_FILE_NAME           "post_lpc10.wav"

int main(int argc, char *argv[])
{
    AFfilehandle inhandle;
    AFfilehandle refhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int frames;
    int outframes;
    float x;
    double pre_energy;
    double post_energy;
    double ref_energy;
    double diff_energy;
    int16_t pre_amp[BLOCKS_PER_READ*BLOCK_LEN];
    int16_t post_amp[BLOCKS_PER_READ*BLOCK_LEN];
    int16_t ref_amp[BLOCKS_PER_READ*BLOCK_LEN];
    int16_t log_amp[BLOCKS_PER_READ*BLOCK_LEN*3];
    uint8_t lpc10_data[BLOCKS_PER_READ*7];
    double xx;
    lpc10_encode_state_t *lpc10_enc_state;
    lpc10_decode_state_t *lpc10_dec_state;
    int i;
    int block_no;
    int log_error;
    int compress;
    int decompress;
    const char *in_file_name;
    int compress_file;
    int decompress_file;
    int len;
    int enc_len;
    int dec_len;

    compress = FALSE;
    decompress = FALSE;
    log_error = TRUE;
    in_file_name = IN_FILE_NAME;
    for (i = 1;  i < argc;  i++)
    {
        if (strcmp(argv[i], "-c") == 0)
        {
            compress = TRUE;
            continue;
        }
        if (strcmp(argv[i], "-d") == 0)
        {
            decompress = TRUE;
            continue;
        }
        if (strcmp(argv[i], "-i") == 0)
        {
            in_file_name = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-l") == 0)
        {
            log_error = FALSE;
            continue;
        }
    }

    compress_file = -1;
    decompress_file = -1;
    inhandle = AF_NULL_FILEHANDLE;
    refhandle = AF_NULL_FILEHANDLE;
    outhandle = AF_NULL_FILEHANDLE;
    if (!decompress)
    {
        if ((inhandle = afOpenFile(in_file_name, "r", 0)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open wave file '%s'\n", in_file_name);
            exit(2);
        }
        if ((x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
        {
            fprintf(stderr, "    Unexpected frame size in wave file '%s'\n", in_file_name);
            exit(2);
        }
        if ((x = afGetRate(inhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
        {
            fprintf(stderr, "    Unexpected sample rate in wave file '%s'\n", in_file_name);
            exit(2);
        }
        if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
        {
            fprintf(stderr, "    Unexpected number of channels in wave file '%s'\n", in_file_name);
            exit(2);
        }
        if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
        {
            fprintf(stderr, "    Failed to create file setup\n");
            exit(2);
        }

        if ((refhandle = afOpenFile(REF_FILE_NAME, "r", 0)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open wave file '%s'\n", REF_FILE_NAME);
            exit(2);
        }
        if ((x = afGetFrameSize(refhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
        {
            fprintf(stderr, "    Unexpected frame size in wave file '%s'\n", REF_FILE_NAME);
            exit(2);
        }
        if ((x = afGetRate(refhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
        {
            fprintf(stderr, "    Unexpected sample rate in wave file '%s'\n", REF_FILE_NAME);
            exit(2);
        }
        if ((x = afGetChannels(refhandle, AF_DEFAULT_TRACK)) != 1.0)
        {
            fprintf(stderr, "    Unexpected number of channels in wave file '%s'\n", REF_FILE_NAME);
            exit(2);
        }
    }
    else
    {
        if ((decompress_file = open(DECOMPRESS_FILE_NAME, O_RDONLY)) < 0)
        {
            fprintf(stderr, "    Cannot open decompressed data file '%s'\n", DECOMPRESS_FILE_NAME);
            exit(2);
        }
    }

    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    if ((outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    
    if ((lpc10_enc_state = lpc10_encode_init(NULL, TRUE)) == NULL)
    {
        fprintf(stderr, "    Cannot create encoder\n");
        exit(2);
    }
            
    if ((lpc10_dec_state = lpc10_decode_init(NULL, TRUE)) == NULL)
    {
        fprintf(stderr, "    Cannot create decoder\n");
        exit(2);
    }

    if (compress)
    {
        if ((compress_file = open(COMPRESS_FILE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        {
            fprintf(stderr, "    Cannot create compressed data file '%s'\n", COMPRESS_FILE_NAME);
            exit(2);
        }
    }

    pre_energy = 0.0;
    post_energy = 0.0;
    ref_energy = 0.0;
    diff_energy = 0.0;

    if (decompress)
    {
        while ((len = read(decompress_file, lpc10_data, BLOCKS_PER_READ*7)) > 0)
        {
            lpc10_decode(lpc10_dec_state, post_amp, lpc10_data, len/7);
            outframes = afWriteFrames(outhandle, AF_DEFAULT_TRACK, post_amp, BLOCK_LEN*len/7);
        }
    }
    else
    {
        block_no = 0;
        while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, pre_amp, BLOCKS_PER_READ*BLOCK_LEN)) == BLOCKS_PER_READ*BLOCK_LEN
                &&
                (frames = afReadFrames(refhandle, AF_DEFAULT_TRACK, ref_amp, BLOCKS_PER_READ*BLOCK_LEN)) == BLOCKS_PER_READ*BLOCK_LEN)
        {
            enc_len = lpc10_encode(lpc10_enc_state, lpc10_data, pre_amp, BLOCKS_PER_READ*BLOCK_LEN);
            if (compress)
                write(compress_file, lpc10_data, enc_len);
            dec_len = lpc10_decode(lpc10_dec_state, post_amp, lpc10_data, enc_len);
            for (i = 0;  i < dec_len;  i++)
            {
                pre_energy += (double) pre_amp[i]*(double) pre_amp[i];
                post_energy += (double) post_amp[i]*(double) post_amp[i];
                ref_energy += (double) ref_amp[i]*(double) ref_amp[i];
                /* The reference file has some odd clipping, so eliminate this from the
                   energy measurement. */
                if (ref_amp[i] == 32767  ||  ref_amp[i] == -32768)
                    xx = 0.0;
                else
                    xx = post_amp[i] - ref_amp[i];
                diff_energy += (double) xx*(double) xx;
                log_amp[i] = xx;
            }
            block_no++;
            if (log_error)
                outframes = afWriteFrames(outhandle, AF_DEFAULT_TRACK, log_amp, dec_len);
            else
                outframes = afWriteFrames(outhandle, AF_DEFAULT_TRACK, post_amp, dec_len);
        }
        if (afCloseFile(inhandle) != 0)
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", in_file_name);
            exit(2);
        }
        if (afCloseFile(refhandle) != 0)
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", REF_FILE_NAME);
            exit(2);
        }
    }
    
    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    if (compress)
        close(compress_file);
    if (decompress)
        close(decompress_file);
    lpc10_encode_release(lpc10_enc_state);
    lpc10_decode_release(lpc10_dec_state);

    if (!decompress)
    {
        printf("Output energy is %f%% of input energy.\n", 100.0*post_energy/pre_energy);
        printf("Difference energy is %f%% of the total.\n", 100.0*diff_energy/ref_energy);
        if (fabs(1.0 - post_energy/pre_energy) > 0.05
            ||
            fabs(diff_energy/post_energy) > 0.03)
        {
            printf("Tests failed.\n");
            exit(2);
        }
        printf("Tests passed.\n");
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
