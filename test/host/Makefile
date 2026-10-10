# Host unit tests for hardware-independent firmware code.
#   make -C test/host            (IDF_PATH must point at ESP-IDF, for cJSON)

CJSON_DIR ?= $(IDF_PATH)/components/json/cJSON
MAIN_DIR  := ../../main
CFLAGS    += -std=gnu17 -Wall -Wextra -Werror -O1 -g -I$(MAIN_DIR) -I$(CJSON_DIR)

ifeq ($(IDF_PATH),)
$(error IDF_PATH is not set -- source ESP-IDF's export.sh or pass CJSON_DIR=...)
endif

BUILD := build
BIN   := $(BUILD)/test_main

.PHONY: test clean
test: $(BIN)
	./$(BIN)

$(BIN): test_main.c $(MAIN_DIR)/ui_format.c $(MAIN_DIR)/ui_format.h $(MAIN_DIR)/json_util.h $(CJSON_DIR)/cJSON.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_main.c $(MAIN_DIR)/ui_format.c $(CJSON_DIR)/cJSON.c -lm

clean:
	rm -rf $(BUILD)
