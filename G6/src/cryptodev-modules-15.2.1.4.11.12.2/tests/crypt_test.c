#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <crypto/cryptodev.h>
#include <sys/time.h>

static int si = 1; /* SI by default */

static double udifftimeval(struct timeval start, struct timeval end)
{
	return (double)(end.tv_usec - start.tv_usec) +
	       (double)(end.tv_sec - start.tv_sec) * 1000 * 1000;
}
static char *units[] = { "", "Ki", "Mi", "Gi", "Ti", 0};
static char *si_units[] = { "", "K", "M", "G", "T", 0};

static void value2human(int si, double bytes, double time, double* data, double* speed,char* metric)
{
	int unit = 0;

	*data = bytes;
	
	if (si) {
		while (*data > 1000 && si_units[unit + 1]) {
			*data /= 1000;
			unit++;
		}
		*speed = *data / time;
		sprintf(metric, "%sB", si_units[unit]);
	} else {
		while (*data > 1024 && units[unit + 1]) {
			*data /= 1024;
			unit++;
		}
		*speed = *data / time;
		sprintf(metric, "%sB", units[unit]);
	}
}

#define MAX(x,y) ((x)>(y)?(x):(y))


int run_crypt(int encrypt, uint8_t *key, uint8_t *iv, uint8_t *buf, size_t size)
{
    int fd, i;
    struct session_op sess;
    struct crypt_op cryp;

    fd = open("/dev/crypto", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    for (i = 0; i < 500; i++) {
        memset(&sess, 0, sizeof(sess));
        sess.cipher = CRYPTO_AES_CBC;
        sess.keylen = 16;
        sess.key = key;
        if (ioctl(fd, CIOCGSESSION, &sess) < 0) {
            perror("CIOCGSESSION");
            close(fd);
            return 1;
        }

        memset(&cryp, 0, sizeof(cryp));
        cryp.ses = sess.ses;
        cryp.len = size;
        cryp.src = buf;
        cryp.dst = buf;

        cryp.iv = iv;
        if (encrypt) {
            cryp.op = COP_ENCRYPT;
        } else {
            cryp.op = COP_DECRYPT;
        }
        if (ioctl(fd, CIOCCRYPT, &cryp) < 0) {
            perror("CIOCCRYPT");
            close(fd);
            return 1;
        }

        if (ioctl(fd, CIOCFSESSION, &sess.ses) < 0) {
            perror("CIOCFSESSION");
            close(fd);
            return 1;
        }
    }

    close(fd);
    return 0;
}

#define BUF_SIZE (192 * 1024)
//#define 0x180000
int main(int argc, char **argv)
{
  int i;
  int size =  BUF_SIZE, loops=10;
    uint8_t key[16] = {0};
    uint8_t iv[16] = {0};
    uint8_t *buf = NULL;
	char metric[16];

	struct timeval start, end;
	double total = 0;
	double secs, ddata, dspeed;

	if (argc > 2) {
	  if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
	    printf("Usage: crypt_test [--size (bytes)]\n");
	    exit(0);
	  }
	  if (strcmp(argv[1], "--size") == 0) {
	    size = atoi(argv[2]);
	  }
	  if (strcmp(argv[3], "--loops") == 0) {
	    loops = atoi(argv[4]);
	  }

	}

	printf("Buf Size = %d bytes\n",size);
	printf("Number of loops = %d\n",loops);

    buf = malloc(size);
    if (buf == NULL) {
        return 1;
    }

    for (i = 0; i < loops; i++) {
      total = 0;
        printf("round %d\n", i);
	gettimeofday(&start, NULL);

        memset(key, i, 16);
        memset(iv, 0xFF - i, 16);
        if (run_crypt(i % 2, key, iv, buf, size)) {
            printf("run_crypt error at %d\n", i);
            break;
        }
	
	total += (size *500);
	gettimeofday(&end, NULL);
	secs = udifftimeval(start, end)/ 1000000.0;

	value2human(si, total, secs, &ddata, &dspeed, metric);
	printf ("done. %.2f %s in %.2f secs: ", ddata, metric, secs);
	printf ("%.2f %s/sec\n", dspeed, metric);
    }
    free(buf);

    return 0;
}

