.PHONY: build clean run test test-bin

BUILDDIR=./build

all: build

build:
	[ -d $(BUILDDIR) ] || meson $(BUILDDIR)
	ninja -C$(BUILDDIR)

test:
	[ -d $(BUILDDIR) ] || meson $(BUILDDIR)
	cd $(BUILDDIR) && meson test --verbose

test-bin: build
	FRAME_LIMIT=7 FILE=fixtures/bgum1.sup make run
	xattr -l supdata/frame-00001.png

test-sup-rect2: build
	FRAME_LIMIT=7 FILE=fixtures/rect2.sup make run
	xattr -l supdata/frame-00001.png

test-sub: build
	FRAME_LIMIT=7 FILE=fixtures/maiboss1.idx make run
	xattr -l supdata/frame-00001.png

clean:
	rm -r $(BUILDDIR)
	rm -r supdata

run:
	$(BUILDDIR)/sup2srt $(FILE)
