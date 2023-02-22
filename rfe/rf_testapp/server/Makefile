# include tools and build rules
export PROJECT_ROOT:= ../../../
BUILD_ROOT?= $(PROJECT_ROOT)/build

V=enabled
DEBUG=enabled


BUILD_CONFIG?=x86_64.gcc

include $(BUILD_ROOT)/top.mk

################################################################################
# Give SIDEKIQ_SDK a default value
SIDEKIQ_SDK?= $(PROJECT_ROOT)/libraries/sidekiq_sdk/sidekiq_sdk/

# Changing SIDEKIQ_SDK to the absolute path needs `override`
override SIDEKIQ_SDK:= $(abspath $(SIDEKIQ_SDK))

# MAKEOVERRIDES only works if something (anything) is specified on the command line
MAKEOVERRIDES += SIDEKIQ_SDK=$(SIDEKIQ_SDK)

# export only works if SIDEKIQ_SDK is not specified on the command line
export SIDEKIQ_SDK
################################################################################



CFLAGS+= -I$(SIDEKIQ_SDK)/sidekiq_core/inc
CFLAGS+= -I$(SIDEKIQ_SDK)/arg_parser/inc

ifneq ($(RELEASE_SUFFIX),)
LDLIBS+= $(SIDEKIQ_SDK)/lib/libsidekiq__$(RELEASE_SUFFIX).a
LDLIBS+= $(SIDEKIQ_SDK)/arg_parser/lib/arg_parser__$(RELEASE_SUFFIX).a
SUPPORT= $(SIDEKIQ_SDK)/lib/support/$(RELEASE_SUFFIX)
else
LDLIBS+= $(SIDEKIQ_SDK)/lib/libsidekiq__$(SUFFIX).a
LDLIBS+= $(SIDEKIQ_SDK)/arg_parser/lib/arg_parser__$(SUFFIX).a
SUPPORT= $(SIDEKIQ_SDK)/lib/support/$(SUFFIX)
endif

# glib2 / libusb / libiio locations / flags
LDFLAGS+= -L$(SUPPORT)/usr/lib/epiq
LDLIBS+= -lusb-1.0 -lglib-2.0
ifeq ($(BUILD_CONFIG),arm_cortex-a9.gcc4.8_uclibc_openwrt)
LDLIBS+= -lglib-2.0 -lintl -liconv
else ifeq ($(BUILD_CONFIG),arm_cortex-a9.gcc4.9.2_gnueabi)
LDLIBS+= -liio
else ifeq ($(BUILD_CONFIG),arm_cortex-a9.gcc7.2.1_gnueabihf)
LDLIBS+= -liio
else ifeq ($(BUILD_CONFIG),aarch64.gcc6.3)
LDLIBS+= -liio
endif

# the below apps are released to customers as part of the Sidekiq SDK
TESTCSRCS+= src/testapp_server.c

# ancilliary source that links with all test apps
CSRCS+= src/utils_common.c
CSRCS+= src/siggen.c
CSRCS+= src/sigann.c

INSTALL_OTHER= \
    src/utils_common.h \
    tools.mk

include $(BUILD_ROOT)/bottom.mk

# utils_common.o is a dependency of all test apps in this subdirectory
$(TESTAPPS): src/utils_common.o
$(TESTAPPS): src/siggen.o
$(TESTAPPS): src/sigann.o

clean_common:
	$(RM) -f src/utils_common.{o,d,force,sig}

clean: clean_common
clean_all: clean_common

# Additional rule to define how to install artifacts
install_src:
	mkdir -p $(DESTDIR)/utils
	tar -cvf - $(TESTCSRCS) $(CSRCS) $(INSTALL_OTHER) | tar -C $(DESTDIR)/utils -xf -
	cp Makefile.release $(DESTDIR)/utils/Makefile

# Additional rule to define how to install binary / pre-built artifacts
install_bin: $(TESTAPPS)
	mkdir -p $(DESTDIR)/utils
	cp -av $(TESTAPPS) $(DESTDIR)/utils





