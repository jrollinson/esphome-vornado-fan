CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -Istubs -I../../components
GTEST    = $(shell pkg-config --cflags --libs gtest_main gmock)

.PHONY: test clean

test: test_timing
	./test_timing

test_timing: test_timing.cpp stubs/esphome/core/component.h \
             stubs/esphome/core/automation.h \
             stubs/esphome/components/remote_transmitter/remote_transmitter.h \
             stubs/esphome/components/remote_base/symphony_protocol.h \
             ../../components/vornado_controller/vornado_controller.h
	$(CXX) $(CXXFLAGS) test_timing.cpp $(GTEST) -o test_timing

clean:
	rm -f test_timing
