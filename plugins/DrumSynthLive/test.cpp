#include "DrumSynthLive.h"
#include "DrumSynth.h"

#include <cstdlib>
#include <sys/time.h>

#include <QString>
#include <iostream>
#include <iomanip>

#ifdef __SSE__
#include <immintrin.h>
#ifdef __GNUC__
#include <x86intrin.h>
#endif

using namespace std;

// Intel® 64 and IA-32 Architectures Software Developer’s Manual,
// Volume 1: Basic Architecture,
// 11.6.3 Checking for the DAZ Flag in the MXCSR Register
int inline can_we_daz() {
  alignas(16) unsigned char buffer[512] = {0};
#if defined(LMMS_HOST_X86)
  _fxsave(buffer);
#elif defined(LMMS_HOST_X86_64)
  _fxsave64(buffer);
#endif
  // Bit 6 of the MXCSR_MASK, i.e. in the lowest byte,
  // tells if we can use the DAZ flag.
  return ((buffer[28] & (1 << 6)) != 0);
}
#endif


int main(int argc, char **argv) {
	if(argc==1) {
		cerr << "Usage: " << argv[0] << " dsfile [timing|stereo]" << endl;
		return EXIT_FAILURE;
	}
	
#ifdef __SSE__
	// Had some denormal trouble with the filter for some files...
	
        /* Setting DAZ might freeze systems not supporting it */
	if (can_we_daz()) {
	   _MM_SET_DENORMALS_ZERO_MODE( _MM_DENORMALS_ZERO_ON );
	}
	/* FTZ flag */
	_MM_SET_FLUSH_ZERO_MODE( _MM_FLUSH_ZERO_ON );
#endif	
	
	// Run a drumSynth rendering on the selected file and return:
	// - the length and a checksum (mode 0)
	// - timing averaged over 50 renderings (mode 1)
	QString dsFile = QString(argv[1]);
	int mode = 0;
	if(argc>2) {
		if(QString(argv[2]) == "timing") mode = 1;
		if(QString(argv[2]) == "stereo") mode = 2;
		
	}

	DrumSynthLive D  = DrumSynthLive();
	DrumSynth     D0 = DrumSynth();
	
	int16_t *buffer, *buffer0;

	if(mode == 1) {
		// Benchmark mode
		const int runs = 50; // Enough to give usable times on my system...
		struct timespec start,end;
		long new_ns = 0;
		// Measure render speed for new version
		D.LoadFile(dsFile);
		for(int i=0; i<runs; ++i) {
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
			D.GetSamples(buffer, 1, 48000);
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);
			free(buffer);
			new_ns += (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
		}
		// ...and then for old
		long old_ns = 0;
		for(int i=0; i<runs; ++i) {
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
			D0.GetDSFileSamples(dsFile, buffer, 1, 48000);
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
			free(buffer);
			old_ns += (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
		}


		cout << setprecision(9) << fixed << dsFile.toStdString() << "\t" << new_ns << "\t"
		     << old_ns << "\t" << setprecision(2) << (double)new_ns/old_ns << endl;
		return EXIT_SUCCESS;
	} else if(mode == 2) {
		// Stereo output... original in channel 0, modified in channel 1
		// Importable in Audacity as "Signed 16-bit PCM, little-endian, 2 channels"
		D.LoadFile(dsFile);
		int L = D.GetSamples(buffer, 1, 44100);
		srand(1);
		int L0 = D0.GetDSFileSamples(dsFile, buffer0, 1, 44100);
		int len = max(L,L0);
		if(!freopen(NULL, "wb", stdout)) {
			return EXIT_FAILURE;
		}
		int i;
		for(i = 0; i<min(L,L0); ++i) {
			fwrite(buffer0+i, sizeof(int16_t),1,stdout);
			fwrite(buffer+i, sizeof(int16_t),1,stdout);
		}
		if(L0>L) {
			for( ; i<len; ++i) {
				fwrite(buffer0+i,sizeof(int16_t),1,stdout);
				fwrite(buffer+L-1,sizeof(int16_t),1,stdout);
			}
		}
		if(L>L0) {
			for( ; i<len; ++i) {
				fwrite(buffer0+L0-1,sizeof(int16_t),1,stdout);
				fwrite(buffer0+i,sizeof(int16_t),1,stdout);
			}
		}
		return EXIT_SUCCESS;
	} else {
		
		
		srand(1); // Make the old code use the same fixed random sequence as the new
		int L0 = D0.GetDSFileSamples(dsFile, buffer0, 1, 48000);
		unsigned int checksum0 = 0;
		int i;
		for(i = 0; i<L0 ; ++i) {
			checksum0 += abs(buffer0[i]);
		}

		D.LoadFile(dsFile);
		int L = D.GetSamples(buffer, 1, 48000);
		unsigned int checksum = 0;
		for(i = 0; i<L ; ++i) {
			checksum += abs(buffer[i]);
		}
		for(i=0; i<min(L,L0) && buffer0[i]==buffer[i]; ++i);
		
		if(checksum == checksum0 && i == min(L, L0)) {
			cout << dsFile.toStdString() << "\t" << L << "\t" << checksum << "\tOK" <<endl;
			return EXIT_SUCCESS;
		} else {
			cout << dsFile.toStdString() << "\t" << L << "\t" << checksum <<
				"\tFAIL, expected: " << L0 << "\t" << checksum0 <<
				"\tfirst difference:" << i << endl;
			return EXIT_FAILURE;
		}			
	}
	
	return EXIT_SUCCESS;
}
