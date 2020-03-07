.PHONY: build clean run

BUILDDIR=./build

all: build

build:
	[ -d $(BUILDDIR) ] || meson $(BUILDDIR)
	ninja -C$(BUILDDIR)

clean:
	rm -r $(BUILDDIR)

run:
	$(BUILDDIR)/sup2srt
