FQBN   := esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi
PORT   ?= $(shell ls /dev/cu.usbmodem* 2>/dev/null | head -1)
SKETCH := desk-status-bar

# Load secrets from .env.local if it exists
-include .env.local.mk

# Convert .env.local to make variables (runs once, generates .env.local.mk)
.env.local.mk: .env.local
	@sed 's/=/:=/' $< | sed 's/^/export /' > $@

# Build compiler flags from env vars (only secrets/config, not everything)
BUILD_FLAGS :=
ifdef WIFI_SSID
BUILD_FLAGS += -DWIFI_SSID=\"$(WIFI_SSID)\"
endif
ifdef WIFI_PASS
BUILD_FLAGS += -DWIFI_PASS=\"$(WIFI_PASS)\"
endif
ifdef OWM_API_KEY
BUILD_FLAGS += -DOWM_API_KEY=\"$(OWM_API_KEY)\"
endif
ifdef OWM_CITY
BUILD_FLAGS += -DOWM_CITY=\"$(OWM_CITY)\"
endif
ifdef OWM_UNITS
BUILD_FLAGS += -DOWM_UNITS=\"$(OWM_UNITS)\"
endif
ifdef UTC_OFFSET
BUILD_FLAGS += -DUTC_OFFSET=$(UTC_OFFSET)
endif
ifdef DST_OFFSET
BUILD_FLAGS += -DDST_OFFSET=$(DST_OFFSET)
endif

BUILD_PROPS :=
ifneq ($(strip $(BUILD_FLAGS)),)
BUILD_PROPS := --build-property "compiler.cpp.extra_flags=$(BUILD_FLAGS)"
endif

.PHONY: build upload monitor clean

build: .env.local.mk ## Compile the sketch
	arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPS) $(SKETCH)

upload: build ## Compile and upload to the board
	arduino-cli upload --fqbn $(FQBN) -p $(PORT) $(SKETCH)

monitor: ## Open serial monitor
	arduino-cli monitor -p $(PORT) -c baudrate=115200

clean: ## Remove build artifacts
	arduino-cli cache clean
	rm -f .env.local.mk

help: ## Show this help
	@grep -E '^[a-z]+:.*##' $(MAKEFILE_LIST) | awk -F ':.*## ' '{printf "  %-12s %s\n", $$1, $$2}'
