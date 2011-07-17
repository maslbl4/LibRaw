/* -*- C++ -*-
 * File: postprocessing_benchmark.cpp
 * Copyright 2008-2010 LibRaw LLC (info@libraw.org)
 * Created: Jul 13, 2011
 *
 * LibRaw simple C++ API:  creates 8 different renderings from 1 source file. The 1st and 4th one should be identical

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of three licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

3. LibRaw Software License 27032010
   (See file LICENSE.LibRaw.pdf provided in LibRaw distribution archive for details).



 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

#include "libraw/libraw.h"

void timerstart(void);
float timerend(void);


int main(int argc, char *argv[])
{
    int  i, ret,rep=1;
    LibRaw RawProcessor;
#define OUT RawProcessor.imgdata.params
#define S RawProcessor.imgdata.sizes

    if(argc<2) 
        {
            printf(
                "postprocessing benchmark: LibRaw %s sample, %d cameras supported\n"
                "Measures postprocessing speed with different options\n"
                "Usage: %s [-a] [-H N] [-q N] [-h] [-m N] [-n N] [-s N] [-B x y w h] [-R N]\n"
                "-a             average image for white balance\n"
                "-H <num>       Highlight mode (0=clip, 1=unclip, 2=blend, 3+=rebuild)\n"
                "-q <num>       Set the interpolation quality\n"
                "-h             Half-size color image\n"
                "-m <num>       Apply a num-passes 3x3 median filter to R-G and B-G\n"
                "-n <num>       Set threshold for wavelet denoising\n"
                "-s <num>       Select one raw image from input file\n"
                "-B <x y w h>   Crop output image\n"
                "-R <num>       Number of repetitions\n"
                ,LibRaw::version(), LibRaw::cameraCount(),
                argv[0]);
            return 0;
        }
    char opm,opt,*cp,*sp;
    int arg,c;

    argv[argc] = (char*)"";
    for (arg=1; (((opm = argv[arg][0]) - 2) | 2) == '+'; ) 
        {
            char *optstr = argv[arg];
            opt = argv[arg++][1];
            if ((cp = strchr (sp=(char*)"HqmnsBR", opt))!=0)
                for (i=0; i < "1111141"[cp-sp]-'0'; i++)
                    if (!isdigit(argv[arg+i][0]) && !optstr[2]) 
                        {
                            fprintf (stderr,"Non-numeric argument to \"-%c\"\n", opt);
                            return 1;
                        }
            switch (opt) 
                {
                case 'a': 
                    OUT.use_auto_wb = 1;  
                    break;
                case 'H':
                    OUT.highlight   = atoi(argv[arg++]);  
                    break;
                case 'q':
                    OUT.user_qual   = atoi(argv[arg++]);  
                    break;
                case 'h':  
                    OUT.half_size = 1;		
                    OUT.four_color_rgb    = 1;  
                    break;
                case 'm':
                    OUT.med_passes  = atoi(argv[arg++]);  
                    break;
                case 'n':  
                    OUT.threshold   = (float)atof(argv[arg++]);  
                    break;
                case 's':  
                    OUT.shot_select = abs(atoi(argv[arg++])); 
                    break;
                case 'B':  
                    for(c=0; c<4;c++) OUT.cropbox[c]  = atoi(argv[arg++]); 
                    break;
                case 'R':  
                    rep = abs(atoi(argv[arg++])); 
                    if(rep<1) rep = 1;
                    break;
                default:
                    fprintf (stderr,"Unknown option \"-%c\".\n", opt);
                    return 1;
                }
        }
    for ( ; arg < argc; arg++)
        {
            printf("Processing file %s\n",argv[arg]);
            if( (ret = RawProcessor.open_file(argv[arg])) != LIBRAW_SUCCESS)
                {
                    fprintf(stderr,"Cannot open_file %s: %s\n",argv[arg],libraw_strerror(ret));
                    continue; // no recycle b/c open file will recycle itself
                }
            
            if( (ret = RawProcessor.unpack() ) != LIBRAW_SUCCESS)
                {
                    fprintf(stderr,"Cannot unpack %s: %s\n",argv[arg],libraw_strerror(ret));
                    continue;
                }
            float mpix,rmpix;
            rmpix = (S.raw_width*S.raw_height)/1000000.0f;
            timerstart();
            for(c=0; c < rep; c++)
                {
                    if( (ret = RawProcessor.dcraw_process() ) != LIBRAW_SUCCESS)
                        {
                            fprintf(stderr,"Cannot postprocess %s: %s\n",argv[arg],libraw_strerror(ret));
                            break;
                        }
                    libraw_processed_image_t *p = RawProcessor.dcraw_make_mem_image();
                    if(c==0)
                        mpix = float(S.width*S.height)/1000000.0f;
                    if(p)
                        RawProcessor.dcraw_clear_mem(p);
                    RawProcessor.free_image();
                }
            float msec = timerend()/(float)rep;
            float mpixsec = mpix*1000.0f/msec;
            if(c==rep)
                {
                    unsigned int crop[4];
                    crop[0] = OUT.cropbox[0];
                    crop[1] = OUT.cropbox[1];
                    crop[2] = OUT.cropbox[2];
                    crop[3] = OUT.cropbox[3];
                    if(crop[0]+crop[2]>S.width) crop[2] = S.width-crop[0];
                    if(crop[1]+crop[3]>S.height) crop[3] = S.height-crop[1];
                    printf(
                        "Performance: %.2f Mpix/sec\n"
                        "File: %s, Frame: %d %.1f total Mpix, %.1f msec\n"
                        "Params:      WB=%s Highlight=%d Qual=%d HalfSize=%s Median=%d Wavelet=%.0f\n"
                        "Crop:        %u-%u:%ux%u, active Mpix: %.1f, %.1f frames/sec\n",
                        mpixsec
                        ,argv[arg],OUT.shot_select,rmpix,msec,
                        OUT.use_auto_wb?"auto":"default",OUT.highlight,OUT.user_qual,OUT.half_size?"YES":"No",
                        OUT.med_passes,OUT.threshold,
                        crop[0],crop[1],crop[2],crop[3],mpix,1000.0f/msec);
                }
        }
    
    return 0;
}


#ifndef WIN32
static struct timeval start,end;
void timerstart(void)
{
    gettimeofday(&start,NULL);
}
float timerend(void)
{
    gettimeofday(&end,NULL);
    float msec = (end.tv_sec - start.tv_sec)*1000.0f + (end.tv_usec - start.tv_usec)/1000.0f;
    return msec;
}
#else
LARGE_INTEGER start;
void timerstart(void)
{
	QueryPerformanceCounter(&start);
}
float timerend()
{
	LARGE_INTEGER unit,end;
	QueryPerformanceCounter(&end);
	QueryPerformanceFrequency(&unit);
	float msec = (float)(end.QuadPart - start.QuadPart);
	msec /= (float)unit.QuadPart/1000.0f;
        return msec;
}

#endif
