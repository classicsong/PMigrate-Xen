XEN_ROOT=$(CURDIR)/../..
include $(XEN_ROOT)/tools/Rules.mk

CFLAGS   += -Werror -lssl

INCLUDES += -I $(XEN_XC)
INCLUDES += -I $(XEN_LIBXC)
INCLUDES += -I $(XEN_INCLUDE)
CFLAGS   += $(INCLUDES)

HDRS     = $(wildcard *.h)

TARGETS-y := xenperf xenpm xen-tmem-list-parse gtraceview gtracestat xenlockprof xenwatchdogd
TARGETS-$(CONFIG_X86) += xen-detect xen-hvmctx xen-hvmcrash
TARGETS-$(CONFIG_MIGRATE) += xen-hptool
TARGETS := $(TARGETS-y)

SUBDIRS-$(CONFIG_LOMOUNT) += lomount
SUBDIRS-$(CONFIG_MINITERM) += miniterm
SUBDIRS := $(SUBDIRS-y)

INSTALL_BIN-y := xencons
INSTALL_BIN-$(CONFIG_X86) += xen-detect
INSTALL_BIN := $(INSTALL_BIN-y)

INSTALL_SBIN-y := xm xen-bugtool xen-python-path xend xenperf xsview xenpm xen-tmem-list-parse gtraceview gtracestat xenlockprof xenwatchdogd
INSTALL_SBIN-$(CONFIG_X86) += xen-hvmctx xen-hvmcrash
INSTALL_SBIN-$(CONFIG_MIGRATE) += xen-hptool
INSTALL_SBIN := $(INSTALL_SBIN-y)

.PHONY: all
all: build

.PHONY: build
build: $(TARGETS)
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d; done

.PHONY: install
install: build
	$(INSTALL_DIR) $(DESTDIR)$(BINDIR)
	$(INSTALL_DIR) $(DESTDIR)$(SBINDIR)
	$(INSTALL_PYTHON_PROG) $(INSTALL_BIN) $(DESTDIR)$(BINDIR)
	$(INSTALL_PYTHON_PROG) $(INSTALL_SBIN) $(DESTDIR)$(SBINDIR)
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d install-recurse; done

.PHONY: clean
clean:
	$(RM) *.o $(TARGETS) *~ $(DEPS)
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

%.o: %.c $(HDRS) Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

xen-hvmctx xen-hvmcrash xenperf xenpm gtracestat xenlockprof xen-hptool xenwatchdogd: %: %.o Makefile
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS_libxenctrl) $(LDLIBS_libxenguest) $(LDLIBS_libxenstore)

gtraceview: %: %.o Makefile
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(CURSES_LIBS)

-include $(DEPS)
