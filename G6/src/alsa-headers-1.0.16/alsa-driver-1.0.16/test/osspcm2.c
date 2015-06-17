#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <time.h>

int main(void)
{
  int fd = open("/dev/dsp", O_RDONLY);
  if (fd == -1) { perror("open"); exit(127); }
  int res = AFMT_S16_LE;
  if (ioctl(fd, SNDCTL_DSP_SETFMT, &res) == -1) {
    perror("ioctl"); exit(127); }
  res = 0;
  if (ioctl(fd, SNDCTL_DSP_STEREO, &res) == -1) {
    perror("ioctl"); exit(127); }
  res = 22050;
  if (ioctl(fd, SOUND_PCM_READ_RATE, 0xbfffdcfc) == -1) {
    perror("ioctl"); exit(127); }
  res = 0x2000a;
  if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &res) == -1) {
    perror("ioctl"); exit(127); }
  audio_buf_info info;
  if (ioctl(fd, SNDCTL_DSP_GETISPACE, &info) == -1) {
    perror("read"); exit(127); }
  printf("    Size of a fragment in bytes: %d\n", info.fragsize);
  printf("    Allocated fragments for buffering: %d\n", info.fragstotal);
  char buf[1024];
  if (read(fd, buf, sizeof(buf)) < 0) { perror("read"); exit(127); }
  static struct timespec naptime = { 0, 100000000 };
  int count = 0;
  do {
    if (ioctl(fd, SNDCTL_DSP_GETISPACE, &info) == -1) {
      perror("read"); exit(127); }
    nanosleep(&naptime, 0);
    if (++count == 20) { printf("Failed to cause an xrun.\n"); exit(127); }
    printf("info.bytes = %u, info.fragsize = %u, info.fragstotal = %u\n", info.bytes, info.fragsize, info.fragstotal);
  } while(info.bytes < info.fragsize * info.fragstotal);
  printf("    Successfully caused an xrun.\n");
  printf("    non-blocking fragments: %d\n", info.fragments);
  printf("    non-blocking bytes: %d\n", info.bytes);
  ssize_t bufsize = info.bytes;
  ssize_t trlen = 0;
  int nf = 0;
  for (;;)
  {
    if (ioctl(fd, SNDCTL_DSP_GETISPACE, &info) == -1) {
      perror("ioctl"); exit(127); }
    if (info.fragments > 0) {
      ssize_t rlen;
      if ((rlen = read(fd, buf, sizeof(buf))) < 0)
      { perror("read"); exit(127); }
      trlen += rlen;
      if (trlen > bufsize) {
        printf("    Read %d bytes: stream successfully restarted.\n", trlen);
	exit(0);
      }
      nf = 0;
    }
    else if (++nf > 10) {
      printf("    Stream is not restarted after xrun.\n");
      exit(1);
    }
  }
  close(fd);
  return 0;
}
