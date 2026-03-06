SHELL := /bin/zsh

CMAKE ?= $(shell if [ -x /opt/homebrew/bin/cmake ]; then echo /opt/homebrew/bin/cmake; else echo cmake; fi)
BUILD_DIR ?= build
CONFIG ?= Debug
JUCE_DIR ?= $(CURDIR)/JUCE

AU_TYPE ?= aumu
AU_SUBTYPE ?= CKBD
AU_MANUFACTURER ?= CBDE
AU_COMPONENT ?= carbide.component
AU_INSTALL_DIR ?= $(HOME)/Library/Audio/Plug-Ins/Components

AU_BUILD_PATH := $(BUILD_DIR)/carbide_artefacts/AU/$(AU_COMPONENT)
DSP_TEST_BIN := $(BUILD_DIR)/carbide_dsp_tests_artefacts/carbide_dsp_tests

.PHONY: help configure build test install-au validate-au reload-au standalone clean

help:
	@echo "Targets:"
	@echo "  make configure   # CMake configure"
	@echo "  make build       # Build plugin + standalone + tests"
	@echo "  make test        # Run DSP self-tests"
	@echo "  make install-au  # Copy AU component to user Components dir"
	@echo "  make validate-au # Run auval for the plugin"
	@echo "  make reload-au   # install-au + validate-au"
	@echo "  make standalone  # Open built standalone app"
	@echo "  make clean       # Remove build directory"
	@echo ""
	@echo "Overridable vars:"
	@echo "  JUCE_DIR=/path/to/JUCE"
	@echo "  BUILD_DIR=build"
	@echo "  CONFIG=Debug|Release"
	@echo "  AU_INSTALL_DIR=$(HOME)/Library/Audio/Plug-Ins/Components"

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DJUCE_DIR=$(JUCE_DIR)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG)

test: build
	$(DSP_TEST_BIN)

install-au: build
	mkdir -p "$(AU_INSTALL_DIR)"
	cp -R "$(AU_BUILD_PATH)" "$(AU_INSTALL_DIR)/"

validate-au:
	auval -v $(AU_TYPE) $(AU_SUBTYPE) $(AU_MANUFACTURER)

reload-au: install-au validate-au

standalone: build
	open "$(BUILD_DIR)/carbide_artefacts/Standalone/carbide.app"

clean:
	rm -rf "$(BUILD_DIR)"
