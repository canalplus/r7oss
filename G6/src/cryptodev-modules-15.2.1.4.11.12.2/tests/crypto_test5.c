#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/dvb/ca.h>
#include <linux/kernel.h>
#include <linux/dvb/stm_ca.h>
#include <linux/dvb/stm-keyrules/default.h>
#include <pthread.h>
#include <getopt.h>
#include <crypto/cryptodev.h>
#include <errno.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/stm_dmx.h>
#include <sys/poll.h>
#include <string.h>
#define UINT_MAX	(~0U)
int thread = 2;
int interval = 10;
int crypt_size = (192 * 256);
int iterations_per_thread = 1000;

void dvb_ca_test(int device)
{
    int ret, i, j;
    dvb_ca_msg_t ca_msg;
    uint16_t pidList[] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
        0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
        0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
        0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    };
#define NUM_PID ((int)(sizeof(pidList)/sizeof(pidList[0])))
    int ca_fd, input, output, recfd[NUM_PID], sctfd[NUM_PID];
    struct dmx_pes_filter_params rp;
    struct dmx_sct_filter_params sp;
    ca_pid_t pids;
    uint8_t *ts, buf[188 * 192];
    struct pollfd pl;
    char devName[128];

    printf("dvb ca test: using dvb0.%d, dummy data %d x %d\n",
           device, NUM_PID, crypt_size);

    ts = malloc(NUM_PID * 188 * crypt_size);
    if (ts == NULL) {
        return;
    }
    sprintf(devName, "/dev/dvb/adapter0/dvr%d", device);
    input = open(devName, O_WRONLY | O_NONBLOCK);
    if (input < 0) {
        perror("dvr1 open");
        return;
    }
    output = open(devName, O_RDONLY | O_NONBLOCK);
    if (output < 0) {
        perror("dvr2 open");
        return;
    }
    sprintf(devName, "/dev/dvb/adapter0/ca%d", device);
    ca_fd = open(devName, O_RDONLY);
    if (ca_fd < 0) {
        perror("ca open");
        return;
    }

    memset(&ca_msg, 0, sizeof(dvb_ca_msg_t));
    ca_msg.type = CA_STM;
    ca_msg.index = 0;

    /* Set cipher mode Marlin */
    ca_msg.command = DVB_CA_SET_DSC_CONFIG;
    ca_msg.u.config.cipher = DVB_CA_CIPHER_AES;
    ca_msg.u.config.chaining = DVB_CA_CHAINING_CBC;
    ca_msg.u.config.residue = DVB_CA_RESIDUE_DVS042;
    ca_msg.u.config.key_ladder_id = RULE_CLEAR_CW16;
    ca_msg.u.config.iv_ladder_id = RULE_CLEAR_IV16;
    ca_msg.u.config.msc = DVB_CA_MSC_AUTO;
    if (ioctl(ca_fd, CA_SEND_MSG, &ca_msg)) {
        perror("CA_SEND_MSG");
    }

    /* Set IV */
    memset(&ca_msg.u, 0, sizeof(ca_msg.u));
    ca_msg.command = DVB_CA_SET_DSC_IV;
    ca_msg.u.key.size = 16;
    memset(ca_msg.u.key.data, 0, 16); // Zoro IV
    for (i = 0; i < 2; i++) {
        ca_msg.u.key.polarity = i;
        if (ioctl(ca_fd, CA_SEND_MSG, &ca_msg)) {
            perror("CA_SEND_MSG");
        }
    }

    /* Set dummy key */
    ca_msg.command = DVB_CA_SET_DSC_KEY;
    memset(&ca_msg.u, 0, sizeof(ca_msg.u));
    ca_msg.u.key.size = 16;
    memset(ca_msg.u.key.data, 0xff, 16);
    for (i = 0; i < 2; i++) {
        ca_msg.u.key.polarity = i;
        if (ioctl(ca_fd, CA_SEND_MSG, &ca_msg)) {
            perror("CA_SEND_MSG");
        }
    }

    sprintf(devName, "/dev/dvb/adapter0/demux%d", device);
    /* Set record and dummy section filter */
    memset(&rp, 0, sizeof(rp));
    rp.input = DMX_IN_DVR;
    rp.output = DMX_OUT_TS_TAP;
    rp.pes_type = DMX_PES_OTHER;

    memset(&sp, 0, sizeof(sp));
    sp.flags = DMX_CHECK_CRC;

    for (i = 0; i < NUM_PID; i++) {
        recfd[i] = open(devName, O_RDWR);
        if (recfd[i] >= 0) {
            rp.pid = pidList[i];
            if (ioctl(recfd[i], DMX_SET_PES_FILTER, &rp)) {
                printf("DMX_SET_PES_FILTER error %d\n", i);
            }
        } else {
            printf("dmx open error %d\n", i);
        }
        sctfd[i] = open(devName, O_RDWR);
        if (sctfd[i] >= 0) {
            sp.pid = pidList[i];
            if (ioctl(sctfd[i], DMX_SET_FILTER, &sp)) {
                printf("DMX_SET_PES_FILTER error %d\n", i);
            }
        } else {
            printf("dmx open error %d\n", i);
        }

        // set descrambler
        pids.index = 0;
        pids.pid = pidList[i];
        if (ioctl(ca_fd, CA_SET_PID, &pids)) {
            printf("CA_SET_PID error %d\n", i);
        }

        // start
        if (ioctl(sctfd[i], DMX_START, NULL)) {
            printf("sct DMX_START error %d\n", errno);
        }
        if (ioctl(recfd[i], DMX_START, NULL)) {
            printf("rec DMX_START error %d\n", errno);
        }
    }

    // create dummy data
    memset(ts, 0xff, 188 * NUM_PID * crypt_size);
    for (i = 0; i < crypt_size; i++) {
        for (j = 0; j < NUM_PID; j++) {
            ts[(i * 188 * NUM_PID) + (j * 188)] = 0x47;
            ts[(i * 188 * NUM_PID) + (j * 188) + 1] = ((pidList[j] >> 8) & 0x1f);
            ts[(i * 188 * NUM_PID) + (j * 188) + 2] = pidList[j] & 0xff;
            ts[(i * 188 * NUM_PID) + (j * 188) + 3] = ((j % 2) == 0) ? 0x90: 0xd0; // scrambled, only payload
        }
    }

    // write dummy data and read it
    pl.fd = output;
    pl.events = POLLIN | POLLPRI;
    for (i = 0; i < (100 * 1000); i++) {
        if (write(input, ts, NUM_PID * 188) < 0) {
            printf("write error %d:%s\n", errno, strerror(errno));
        }
        while (poll(&pl, 1, 0) > 0) {
            ret = read(output, buf, sizeof(buf));
            if (ret <= 0 ) {
                printf("read error %d:%s\n", errno, strerror(errno));
                break;
            }
#if 0
            printf("%d: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n", ret,
                   buf[0], buf[1], buf[2], buf[3],
                   buf[4], buf[5], buf[6], buf[7],
                   buf[8], buf[9], buf[10], buf[11]);
#endif
        }

        if ((i % (interval)) == 0) {
            // change key
            memset(ca_msg.u.key.data, i, 16);
            for (j = 0; j < 2; j++) {
                ca_msg.u.key.polarity = j;
                if (ioctl(ca_fd, CA_SEND_MSG, &ca_msg)) {
                    perror("CA_SEND_MSG2");
                }
            }
        }
    }

    /* Stop record */
    for (i = 0; i < NUM_PID; i++) {
        close(recfd[i]);
        close(sctfd[i]);
    }

    close(ca_fd);
    close(input);
    close(output);
    free(ts);
}

int crypto_test(int encrypt)
{
    int fd, i, j;
    struct session_op sess;
    struct crypt_op cryp;
    uint8_t *data;
    uint8_t iv[16] = {0};
    uint8_t key[16] = {0};
    uint8_t tmp[16];

    data = malloc(crypt_size);
    if (data == NULL) {
        fprintf(stderr, "malloc error\n");
        return 1;
    }

    fd = open("/dev/crypto", O_RDWR);
    if (fd < 0) {
        perror("crypto open");
        free(data);
        return 1;
    }

    memset(&sess, 0, sizeof(sess));
    sess.cipher = CRYPTO_AES_CBC;
    sess.keylen = 16;
    sess.key = key;

    memset(&cryp, 0, sizeof(cryp));
    cryp.len = crypt_size;
    cryp.src = data;
    cryp.dst = data;
    cryp.iv = iv;
    if (encrypt) {
        cryp.op = COP_ENCRYPT;
    } else {
        cryp.op = COP_DECRYPT;
    }

    for (i = 0; i < iterations_per_thread; i++) {

	    printf("Loop %d\n",i);
        if ((i % interval) == 0) {
            // change to new dummy iv and key
            for (j = 0; j < 16; j++) {
                iv[j] += i * j;
                key[j] += i * j;
            }
            if (ioctl(fd, CIOCGSESSION, &sess) < 0) {
                fprintf(stderr, "CIOCGSESSION %d:%s", errno, strerror(errno));
                close(fd);
                free(data);
                return 1;
            }
            cryp.ses = sess.ses;
        }

        memcpy(tmp, &data[crypt_size - 16], 16); // save iv

        if (ioctl(fd, CIOCCRYPT, &cryp) < 0) {
            fprintf(stderr, "CIOCCRYPT %d:%s\n", errno, strerror(errno));
            close(fd);
            free(data);
            return 1;
        }

        memcpy(iv, tmp, 16); // set next iv

        if ((i % interval) == (interval - 1)) {
            if (ioctl(fd, CIOCFSESSION, &sess.ses) < 0) {
                fprintf(stderr, "CIOCFSESSION %d:%s\n", errno, strerror(errno));
                close(fd);
                free(data);
                return 1;
            }
        }
    }

    close(fd);
    free(data);
    return 0;
}

void *testThread(void *arg)
{
    int i, id = (int)arg;

    printf("thread %d start\n", id);

    if (id < 0 && id > -8) {
        dvb_ca_test(id * -1);
        return NULL;
    }

    for (i = 0; i < 2; i++) {
        if (i == ((id + 0) % 2))
            if (crypto_test(0)) break;
        if (i == ((id + 1) % 2))
            if (crypto_test(1)) break;
    }
    printf("thread %d done\n", id);
    return NULL;
}

void print_usage(void)
{
    printf("crypto_test4 [opt]\n"
           "  -l <num>: encrypt/decrypt iterations\n"
           "  -t <num>: thread num\n"
           "  -i <num>: interval to change keys\n"
           "  -s <num>: crypt data size\n"
           "  -c <num>: dvb ca test\n");
}

int main(int argc, char **argv)
{
    pthread_t *t;
    int i, option;
    struct option op[] = {
        {"loops", 0, 0, 'l'},
        {"interval", 0, 0, 'i'},
        {"thread", 0, 0, 't'},
        {"crypt_size", 0, 0, 's'},
        {"ca_test", 0, 0, 'c'},
    };

    while ((option = getopt_long(argc, argv, "i:t:s:c:l:", op, NULL)) != -1) {
        switch (option) {
        case 'i':
            interval = atoi(optarg);
            break;
        case 't':
            thread = atoi(optarg);
            break;
        case 'l':
            iterations_per_thread = atoi(optarg);
            break;
        case 's':
            if (atoi(optarg) >= 16) {
                crypt_size = atoi(optarg);
            }
            break;
        case 'c':
            dvb_ca_test(atoi(optarg));
            return 1;
        default:
            print_usage();
            return 1;
        }
    }
    printf("thread = %d, interval = %d, crypt_size = %d, iter_per_thread = %d\n",
           thread, interval, crypt_size, iterations_per_thread);
    t = malloc(sizeof(pthread_t) * thread);
    if (t == NULL) {
        return 1;
    }

    for (i = 0; i < thread; i++) {
        pthread_create(&t[i], NULL, testThread, (void*)i);
    }

    for (i = 0; i < thread; i++) {
        pthread_join(t[i], NULL);
    }

    free(t);
    return 0;
}

