.PHONY: build clean run test

BUILDDIR=./build

all: build

build:
	[ -d $(BUILDDIR) ] || meson $(BUILDDIR)
	ninja -C$(BUILDDIR)

test-sup: build
	FRAME_LIMIT=7 FILE=fixtures/bgum1.sup make run
	xattr -l supdata/frame-00001.png

test-sub: build
	FRAME_LIMIT=7 FILE=fixtures/maiboss1.idx make run
	xattr -l supdata/frame-00001.png

clean:
	rm -r $(BUILDDIR)
	rm -r supdata

run:
	$(BUILDDIR)/sup2srt $(FILE)
