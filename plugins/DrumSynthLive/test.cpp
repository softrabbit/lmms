#include "DrumSynthLive.h"
#include "DrumSynth.h"

#include <cstdlib>
#include <sys/time.h>

#include <QString>
#include <iostream>
#include <iomanip>

using namespace std;

int main(int argc, char **argv) {
	if(argc==1) {
		cerr << "Usage: " << argv[0] << " dsfile [timing]" << endl;
		return EXIT_FAILURE;
	}

	// Run a drumSynth rendering on the selected file and return:
	// - the length and a checksum (mode 0)
	// - timing averaged over 5 renderings (mode 1)
	QString dsFile = QString(argv[1]);
	int mode = 0;
	if(argc>2) {
		if(QString(argv[2]) == "timing") mode = 1;		
	}

	DrumSynthLive D  = DrumSynthLive();
	DrumSynth     D0 = DrumSynth();
	
	int16_t *buffer;

	if(mode == 1) {
		const int runs = 100; // Enough to give usable times on my system...
		struct timespec start,end;
		long new_ns = 0;
		// Measure render speed for new version
		for(int i=0; i<runs; ++i) {
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
			D.GetDSFileSamples(dsFile, buffer, 1, 48000);
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
	} else {
		int L = D.GetDSFileSamples(dsFile, buffer, 1, 48000);
		unsigned int checksum = 0;
		for(int i = 0; i<L ; ++i) {
			checksum += abs(buffer[i]);
		}
		cout << dsFile.toStdString() << "\t" << L << "\t" << checksum << endl;
	}
	
	return EXIT_SUCCESS;
}
