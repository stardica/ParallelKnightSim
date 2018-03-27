KNIGHTSIM_DIR = ./KnightSim/
LIB_DIR = ./lib/
INCLUDE_DIR = ./include/

CC_FLAGS_64 = -O3 -Wall #-g3
CC_FLAGS_32 = -O3 -Wall -m32 #-g3
CC = gcc
LINKER_FLAGS_64 = -lKnightSim64 -lpthread
LINKER_FLAGS_32 = -lKnightSim32 -lpthread

all: 
	$(MAKE) -C $(KNIGHTSIM_DIR) all
	$(CC) $(CC_FLAGS_64) Event.c -o Event64 -I$(KNIGHTSIM_DIR) -I$(INCLUDE_DIR) -L$(LIB_DIR) $(LINKER_FLAGS_64)
	@echo "Built $@ successfully"
	$(CC) $(CC_FLAGS_32) Event.c -o Event32 -I$(KNIGHTSIM_DIR) -I$(INCLUDE_DIR) -L$(LIB_DIR) $(LINKER_FLAGS_32)
	@echo "Built $@ successfully"
	$(CC) $(CC_FLAGS_64) ProducerConsumer.c -o ProducerConsumer64 -I$(KNIGHTSIM_DIR) -I$(INCLUDE_DIR) -L$(LIB_DIR) $(LINKER_FLAGS_64)
	@echo "Built $@ successfully"


clean:
	$(MAKE) -C $(KNIGHTSIM_DIR) clean
	rm -f \
	ProducerConsumer64 \
	Event32 \
	Event64
