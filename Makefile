.PHONY: build clean run test

BUILDDIR=./build

all: build

build:
	[ -d $(BUILDDIR) ] || meson $(BUILDDIR)
	ninja -C$(BUILDDIR)

test: build
	FRAME_LIMIT=7 FILE=test/bgum1.sup make run
	xattr -l supdata/frame-00001.png

clean:
	rm -r $(BUILDDIR)
	rm -r supdata

run:
	$(BUILDDIR)/sup2srt $(FILE)
