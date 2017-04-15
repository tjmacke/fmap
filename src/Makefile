UNAME_S := $(shell uname -s)

CC=	cc
ifeq ($(UNAME_S),Darwin)
  CFLAGS=	-g -I/usr/local/ssl/include
else
  CFLAGS=	-g
endif
AR=	ar

LIBS=	-L/usr/local/ssl/lib -lcrypto

LIBFMAP_OBJS=	\
	args.o	\
	fmap.o	\
	log.o	\
	split.o

PROGRAMS=	\
	getentry	\
	indexlines	\
	mk_key2rnum	\
	mk_md5_lines	\
	upd_fmap

all:	$(PROGRAMS) libfmap.a

libfmap.a:	$(LIBFMAP_OBJS)
	$(AR) rv libfmap.a $(LIBFMAP_OBJS)
	-ranlib libfmap.a

getentry:	getentry.o libfmap.a
	$(CC) $(CFLAGS) -o getentry getentry.o libfmap.a $(LIBS)

indexlines:	indexlines.o libfmap.a
	$(CC) $(CFLAGS) -o indexlines indexlines.o libfmap.a $(LIBS)

mk_key2rnum:	mk_key2rnum.o libfmap.a
	$(CC) $(CFLAGS) -o mk_key2rnum mk_key2rnum.o libfmap.a $(LIBS)

mk_md5_lines:	mk_md5_lines.o libfmap.a
	$(CC) $(CFLAGS) -o mk_md5_lines mk_md5_lines.o libfmap.a $(LIBS)

upd_fmap:	upd_fmap.o libfmap.a
	$(CC) $(CFLAGS) -o upd_fmap upd_fmap.o libfmap.a $(LIBS)

getentry.o:	log.h args.h index.h fmap.h

indexlines.o:	log.h args.h index.h

mk_key2rnum.o:	log.h args.h index.h fmap.h

mk_md5_lines.o:	log.h args.h

upd_fmap.o:	log.h fmap.h

args.o:	log.h args.h

fmap.o:	log.h fmap.h

log.o:	log.h

install:	all
	mkdir -p ../bin
	mv $(PROGRAMS) ../bin

clean:
	rm -f $(PROGRAMS) *.o *.a core.* junk.*
