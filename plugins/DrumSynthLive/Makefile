SOURCES = DrumSynthLive.cpp DrumSynthLive.h

INCLUDE_LMMS = -I../../include/
INCLUDE_CONFIG = -I../../build/
INCLUDE_QT = -I/usr/include/x86_64-linux-gnu/qt5/QtCore/ -I/usr/include/x86_64-linux-gnu/qt5/
LDLIBS += $(shell pkg-config --libs Qt5Core)

CXXFLAGS += -fPIC $(INCLUDE_LMMS) $(INCLUDE_CONFIG) $(INCLUDE_QT) -DQT_CORE_LIB -DQT_SHARED

.PHONY: testrun benchmark

DrumSynthLive.o: $(SOURCES)

test: DrumSynthLive.o test.cpp

testrun: test
	./checksums.sh

benchmark: test
	./benchmark.sh

clean:
	rm *.o *.gch test
