
#define HTON16(x) ((x >> 8 & 0xff) | (x << 8 & 0xff00))
#define HTON24(x) ((x >> 16 & 0xff) | (x << 16 & 0xff0000) | (x & 0xff00))
#define HTON32(x) ((x >> 24 & 0xff) | (x >> 8 & 0xff00) | \
				   (x << 8 & 0xff0000) | (x << 24 & 0xff000000))
#define HTONTIME(x) ((x >> 16 & 0xff) | (x << 16 & 0xff0000) | (x & 0xff00) | (x & 0xff000000))