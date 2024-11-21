.PHONY: all clean

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -pthread -O3
OUTPUT_OPTION = -MMD -MP -o $@

STUDENT_ID = 110550108
TARGET = sched_demo_$(STUDENT_ID)
SOURCE = sched_demo_$(STUDENT_ID).cpp
OBJS = $(SOURCE:.cpp=.o)
DEPS = $(SOURCE:.cpp=.d)

all: $(TARGET)

-include $(DEPS)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< $(OUTPUT_OPTION)

clean:
	@rm -f $(TARGET) $(OBJS) $(DEPS)
