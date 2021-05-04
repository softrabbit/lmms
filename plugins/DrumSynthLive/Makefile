SOURCES = DrumSynthLive.cpp DrumSynthLive.h
OLDSOURCES = ../../src/core/DrumSynth.cpp ../../include/DrumSynth.h

INCLUDE_LMMS = -I../../include/
INCLUDE_CONFIG = -I../../build/
INCLUDE_QT = -I/usr/include/x86_64-linux-gnu/qt5/QtCore/ -I/usr/include/x86_64-linux-gnu/qt5/
LDLIBS += $(shell pkg-config --libs Qt5Core)

CXXFLAGS += -Wunused -g -fPIC $(INCLUDE_LMMS) $(INCLUDE_CONFIG) $(INCLUDE_QT) -DQT_CORE_LIB -DQT_SHARED

.PHONY: testrun benchmark

DrumSynth.cpp: Makefile
	/bin/cp $(OLDSOURCES) .

DrumSynthLive.o: $(SOURCES)
DrumSynth.o: DrumSynth.cpp DrumSynth.h

test: DrumSynthLive.o DrumSynth.o test.cpp

testrun: test
	./checksums.sh

benchmark: test
	./benchmark.sh

clean:
	rm *.o *.gch test DrumSynth.*
