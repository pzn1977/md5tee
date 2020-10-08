CFLAGS = -Wall -Werror -O2

all: md5tee

md5tee: md5tee.c
	gcc -Wall -Werror -O2 -o $@ $^ -lcrypto

clean:
	rm -v -f *~

distclean: clean
	rm -v -f md5tee

install: all
	install -m 0755 -o root -g root md5tee /usr/local/bin/

test: md5tee
	test "$(shell echo -n | CHUNKSIZE=0 ./md5tee 2>&1 >/dev/null)" = "0 d41d8cd98f00b204e9800998ecf8427e"
	test "$(shell echo -n Pedro | CHUNKSIZE=0 ./md5tee 2>&1 >/dev/null)" = "5 38e2b2e31c0fce9537f735dda9fdf10a"
	test "$(shell cat /dev/urandom 2>/dev/null | tr -d -c a | head -c 1234567 | CHUNKSIZE=131072 ./md5tee 2>&1 >/dev/null)" = "1234567 95bff98a791ba06b828d53f68ca487e1 55d084377b1bfde61e989465e3c2b3b3-10(131072)"
	@echo "ALL TESTS OK"
