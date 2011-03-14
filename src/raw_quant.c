#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "fitsio.h"
#include "psrfits.h"
#include "guppi_params.h"
#include "fitshead.h"

/* Parse info from buffer into param struct */
extern void guppi_read_obs_params(char *buf, 
                                     struct guppi_params *g,
                                     struct psrfits *p);


inline int quantize_2bit(struct psrfits *pf, double * mean, double * std);

int compute_stat(struct psrfits *pf, double *mean, double *std);

void print_usage(char *argv[]) {
	fprintf(stderr, "USAGE: %s -i input.raw -o output.quant ('-o stdout' allowed for output, -v or -V for verbose)\n", argv[0]);
}



/* 03/13 - edit to do optimized inplace quantization */

                                     
int main(int argc, char *argv[]) {
	struct guppi_params gf;
    struct psrfits pf;
    char buf[32768];
    char quantfilename[250]; //file name for quantized file
    
	int filepos=0;
	size_t rv=0;
	int by=0;
    
    FILE *fil = NULL;   //input file
    FILE *quantfil = NULL;  //quantized file
    
	int x,y,z;
	int a,b,c;

	
	double *mean = NULL;
	double *std = NULL;
	
    int vflag=0; //verbose



    
	   if(argc < 2) {
		   print_usage(argv);
		   exit(1);
	   }


       opterr = 0;
     
       while ((c = getopt (argc, argv, "Vvi:o:")) != -1)
         switch (c)
           {
           case 'v':
             vflag = 1;
             break;
           case 'V':
             vflag = 2;
             break; 
           case 'i':
			 sprintf(pf.basefilename, optarg);
			 fil = fopen(pf.basefilename, "rb");
             break;
           case 'o':
			 sprintf(quantfilename, optarg);
			 if(strcmp(quantfilename, "stdout")==0) {
				 quantfil = stdout;
			 } else {
				 quantfil = fopen(quantfilename, "wb");			
			 }
             break;
           case '?':
             if (optopt == 'i' || optopt == 'o')
               fprintf (stderr, "Option -%c requires an argument.\n", optopt);
             else if (isprint (optopt))
               fprintf (stderr, "Unknown option `-%c'.\n", optopt);
             else
               fprintf (stderr,
                        "Unknown option character `\\x%x'.\n",
                        optopt);
             return 1;
           default:
             abort ();
           }


   

    pf.filenum=1;
    pf.sub.dat_freqs = (float *)malloc(sizeof(float) * pf.hdr.nchan);
    pf.sub.dat_weights = (float *)malloc(sizeof(float) * pf.hdr.nchan);
    pf.sub.dat_offsets = (float *)malloc(sizeof(float) 
           * pf.hdr.nchan * pf.hdr.npol);
    pf.sub.dat_scales  = (float *)malloc(sizeof(float) 
            * pf.hdr.nchan * pf.hdr.npol);
    pf.sub.data  = (unsigned char *)malloc(pf.sub.bytes_per_subint);


	

	if(!fil || !quantfil) {
		fprintf(stderr, "must specify input/output files\n");
		print_usage(argv);
		exit(1);
	}
	
	filepos=0;
	
	while(fread(buf, sizeof(char), 32768, fil)==32768) {		

		 fseek(fil, -32768, SEEK_CUR);

		 if(vflag>=1) fprintf(stderr, "length: %d\n", gethlength(buf));

		 guppi_read_obs_params(buf, &gf, &pf);
	 
		 if(vflag>=1) fprintf(stderr, "size %d\n",pf.sub.bytes_per_subint + gethlength(buf));
		 by = by + pf.sub.bytes_per_subint + gethlength(buf);
		 if(vflag>=1) fprintf(stderr, "mjd %Lf\n", pf.hdr.MJD_epoch);
		 if(vflag>=1) fprintf(stderr, "zen: %f\n\n", pf.sub.tel_zen);
		 if (pf.sub.data) free(pf.sub.data);
         pf.sub.data  = (unsigned char *)malloc(pf.sub.bytes_per_subint);
		 
		 fseek(fil, gethlength(buf), SEEK_CUR);
		 rv=fread(pf.sub.data, sizeof(char), pf.sub.bytes_per_subint, fil);		 
		 


		
		 if((long int)rv == pf.sub.bytes_per_subint){
			 if(vflag>=1) fprintf(stderr, "%i\n", filepos);
			 if(vflag>=1) fprintf(stderr, "pos: %ld %d\n", ftell(fil),feof(fil));


			 if(filepos == 0) {
				/* beginning of file, compute statistics */
				
			    mean = malloc(pf.hdr.rcvr_polns *  pf.hdr.nchan * sizeof(double));
			    std = malloc(pf.hdr.rcvr_polns *  pf.hdr.nchan * sizeof(double));

			   // memset(mean, 0, pf.hdr.rcvr_polns *  pf.hdr.nchan * sizeof(double));
			   // memset(std, 0, pf.hdr.rcvr_polns *  pf.hdr.nchan * sizeof(double));
	
 				compute_stat(&pf, mean, std);

//			 if(vflag>=1) fprintf(stderr, "chan  %d pol %d mean %f\n", x,y,mean[(x*pf->hdr.rcvr_polns) + y]);
//			 if(vflag>=1) fprintf(stderr, "chan  %d pol %d std %f\n", x,y,std[(x*pf->hdr.rcvr_polns) + y]);


 			 }


			quantize_2bit(&pf, mean, std);


			hputi4 (buf, "BLOCSIZE", pf.sub.bytes_per_subint);
			hputi4 (buf,"NBITS",pf.hdr.nbits);

			fwrite(buf, sizeof(char), gethlength(buf), quantfil);  //write header
			
			/* bytes_per_subint now updated to be the proper length */
			fwrite(pf.sub.data, sizeof(char), pf.sub.bytes_per_subint, quantfil);  //write data

			filepos++;

			 
		} else {
				if(vflag>=1) fprintf(stderr, "only read %ld bytes...\n", (long int) rv);
		}

	}
		if(vflag>=1) fprintf(stderr, "bytes: %d\n",by);
		if(vflag>=1) fprintf(stderr, "pos: %ld %d\n", ftell(fil),feof(fil));
	



    //while ((rv=psrfits_read_subint(&pf))==0) { 
    //    printf("Read subint (file %d, row %d/%d)\n", 
    //            pf.filenum, pf.rownum-1, pf.rows_per_file);
    //}
    //if (rv) { fits_report_error(stderr, rv); }

	fclose(quantfil);
	fclose(fil);
    exit(0);
}





/* optimized 2-bit quantization */

/* applies 2 bit quantization to the data pointed to by pf->sub.data        			*/
/* mean and std should be formatted as returned by 'compute_stat'         				*/
/* quantization is performed 'in-place,' overwriting existing contents    				*/
/* pf->hdr.nbits and pf->sub.bytes_per_subint are updated to reflect changes    		*/
/* quantization scheme described at http://seti.berkeley.edu/kepler_seti_quantization  	*/

inline int quantize_2bit(struct psrfits *pf, double * mean, double * std) {

register unsigned int x,y;
unsigned int bytesperchan;


/* temporary variables for quantization routine */
float nthr[2];
float n_thr[2];
float chan_mean[2];
float sample;

register unsigned int offset;
register unsigned int address;

unsigned int pol0lookup[256];   /* Lookup tables for pols 0 and 1 */
unsigned int pol1lookup[256];


bytesperchan = pf->sub.bytes_per_subint/pf->hdr.nchan;

for(x=0;x < pf->hdr.nchan; x = x + 1)   {

		
		nthr[0] = (float) 0.98159883 * std[(x*pf->hdr.rcvr_polns) + 0];
		n_thr[0] = (float) -0.98159883 * std[(x*pf->hdr.rcvr_polns) + 0];
		chan_mean[0] = (float) mean[(x*pf->hdr.rcvr_polns) + 0];
		
		if(pf->hdr.rcvr_polns == 2) {
		   nthr[1] = (float) 0.98159883 * std[(x*pf->hdr.rcvr_polns) + 1];
		   n_thr[1] = (float) -0.98159883 * std[(x*pf->hdr.rcvr_polns) + 1];
		   chan_mean[1] = (float) mean[(x*pf->hdr.rcvr_polns) + 1];
		} else {
			nthr[1] = nthr[0];
			n_thr[1] = n_thr[0];
			chan_mean[1] = chan_mean[0];						
		}
								
		
		
		/* build the lookup table */
		for(y=0;y<128;y++) {   
			sample = ((float) y) - chan_mean[0]; 
			if (sample > nthr[0]) {
				pol0lookup[y] = 0;  						
			} else if (sample > 0) {
				pol0lookup[y] = 1; 												
			} else if (sample > n_thr[0]) {
				pol0lookup[y] = 2;																		 
			} else {
				pol0lookup[y] = 3;																		
			}	
		
			sample = ((float) y) - chan_mean[1]; 
			if (sample > nthr[1]) {
				pol1lookup[y] = 0;  						
			} else if (sample > 0) {
				pol1lookup[y] = 1; 												
			} else if (sample > n_thr[1]) {
				pol1lookup[y] = 2;																		 
			} else {
				pol1lookup[y] = 3;																		
			}			
		}
		
		for(y=128;y<256;y++) {   
			sample = ((float) y) - chan_mean[0] - 256; 
			if (sample > nthr[0]) {
				pol0lookup[y] = 0;  						
			} else if (sample > 0) {
				pol0lookup[y] = 1; 												
			} else if (sample > n_thr[0]) {
				pol0lookup[y] = 2;																		 
			} else {
				pol0lookup[y] = 3;																		
			}	
		
			sample = ((float) y) - chan_mean[1] - 256; 
			if (sample > nthr[1]) {
				pol1lookup[y] = 0;  						
			} else if (sample > 0) {
				pol1lookup[y] = 1; 												
			} else if (sample > n_thr[1]) {
				pol1lookup[y] = 2;																		 
			} else {
				pol1lookup[y] = 3;																		
			}			
		}


		/* memory position offset for this channel */
		offset = x * bytesperchan; 
		
		/* starting point for writing quantized data */
		address = offset/4;

		/* in this quantization code we'll sort-of assume that we always have two pols, but we'll set the pol0 thresholds to the pol1 values above if  */
		/* if only one pol is present. */
							
		for(y=0;y < bytesperchan; y = y + 4){
		
			/* form one 4-sample quantized byte */
			pf->sub.data[address] = pol0lookup[pf->sub.data[((offset) + y)]] + (pol0lookup[pf->sub.data[((offset) + y) + 1]] * 4) + (pol1lookup[pf->sub.data[((offset) + y) + 2]] * 16) + (pol1lookup[pf->sub.data[((offset) + y) + 3]] * 64);

			address++;																
		
		}					

}


/* update pf struct */
pf->sub.bytes_per_subint = pf->sub.bytes_per_subint / 4;			
pf->hdr.nbits = 2;			


return 1;
}


/* calculates the mean and sample std dev of the data pointed to by pf->sub.data 	*/

/* mean[0] = mean(chan 0, pol 0)             	*/
/* mean[1] = mean(chan 0, pol 1)             	*/
/* mean[n-1] = mean(chan n/2, pol 0)         	*/
/* mean[n] = mean(chan n/2, pol 1)             	*/
/* std same as above                			*/


int compute_stat(struct psrfits *pf, double *mean, double *std){


double running_sum;
double running_sum_sq;
int x,y,z;
int sample;
	
 /* calulcate mean and rms for each channel-polarization */
 /* we'll treat the real and imaginary parts identically - considering them as 2 samples/period) */

/* This code is much slower than it needs to be, but it doesn't run very often */

 for(x=0;x < pf->hdr.nchan; x = x + 1)   {
		for(y=0;y<pf->hdr.rcvr_polns;y=y+1) {
			 running_sum = 0;
			 running_sum_sq = 0;
			 
			 for(z=0;z < pf->sub.bytes_per_subint/pf->hdr.nchan; z = z + (pf->hdr.rcvr_polns * 2)){
				 //pol 0, real imag

				 //real
				 sample = (int) ((signed char) pf->sub.data[(x * pf->sub.bytes_per_subint/pf->hdr.nchan) + z + (y * 2)]);
				 running_sum = running_sum + (double) sample;

				 //imag
				 sample = (int) ((signed char) pf->sub.data[(x * pf->sub.bytes_per_subint/pf->hdr.nchan) + z + (y * 2) + 1]);
				 running_sum = running_sum + (double) sample;

			 }

			 mean[(x*pf->hdr.rcvr_polns) + y] =  running_sum / (double) (pf->sub.bytes_per_subint/(pf->hdr.nchan * pf->hdr.rcvr_polns) );

			 for(z=0;z < pf->sub.bytes_per_subint/pf->hdr.nchan; z = z + (pf->hdr.rcvr_polns * 2)){
					 //sample = (int) ((signed char) pf.sub.data[(x * pf.sub.bytes_per_subint/pf.hdr.nchan) + z]);

					 //real
					 sample = (int) ((signed char) pf->sub.data[(x * pf->sub.bytes_per_subint/pf->hdr.nchan) + z + (y * 2)]);
					 running_sum_sq = running_sum_sq + pow( ((double) sample - mean[(x*pf->hdr.rcvr_polns) + y]) , 2);

					 //imag
					 sample = (int) ((signed char) pf->sub.data[(x * pf->sub.bytes_per_subint/pf->hdr.nchan) + z + (y * 2) + 1]);
					 running_sum_sq = running_sum_sq + pow( ((double) sample - mean[(x*pf->hdr.rcvr_polns) + y]) , 2);

			 }

			 std[(x*pf->hdr.rcvr_polns) + y] = pow(running_sum_sq / ((double) (pf->sub.bytes_per_subint/(pf->hdr.nchan*pf->hdr.rcvr_polns)) - 1), 0.5);
									 
			 
		}			
 }
 
return 1;

}


void bin_print_verbose(unsigned char x)
/* function to print decimal numbers in verbose binary format */
/* x is integer to print, n_bits is number of bits to print */
{

   int j;
   printf("no. 0x%08x in binary \n",(int) x);

   for(j=8-1; j>=0;j--){
	   printf("bit: %i = %i\n",j, (x>>j) & 01);
   }

}



