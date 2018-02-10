#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int ascii = 0;
static FILE *out;
static char **signame;
static int nsig;
static int *nbit;
static float MHz = 0;
static float baud = 0;
static int totbit = 0;
static unsigned long timestamp;
static double period;

/* Format of over-wire signal:

  Consists of n+1 bit words, where n is the number of signals that are transmitted.

  [topbit] [bit n-1] [bit n-2] ... [bit 1] [bit 0]
  [topbit] [bit n-1] [bit n-2] ... [bit 1] [bit 0]

  The topbit indicates whether the following bits are a repetition counter (1) or a data word (0).
  In case there are no changes for a period of time, a repetition counter is sent instead
  of data words. The repetition counter is equal to the number of clock times that it
  replaces, minus 2.

  A data word being equal to all ones marks a buffer overrun in the fpga.

  The data words are transmitted little endian and padded with zeros to
  fill up a whole number of bytes. 

  byte 1:  [bit 7] [bit 6] [bit 5] [bit 4] [bit 3] [bit 2] [bit 1] [bit 0]
  byte 2:  0       0       0     [topbit] [bit 11] [bit 10] [bit 9] [bit 8]


*/


void printheader_vcd(void) {
  fprintf(out, "$timescale 1ps $end\n");
  fprintf(out, "$scope module logic $end\n");
  for (int i = 0; i < nsig; i++) {
    char name[80];
    strcpy(name, signame[i]);
    if (strchr(name, '/')) {
      *strchr(name, '/') = 0;
    }
    fprintf(out, "$var wire %d %c %s $end\n", nbit[i], 33 + i, name);
  }
  fprintf(out, "$upscope $end\n");
  fprintf(out, "$dumpvars\n");
}

#define TIMECOL 20

int namelen(char *name) {
  int r = strlen(name);
  if (strchr(name, '/'))
    r = strchr(name, '/') - name;
  return r;
}

void printheader_ascii(void) {
  int maxlen = 0;
  for (int i = 0; i < nsig; i++) {
    if (namelen(signame[i]) > maxlen)
      maxlen = namelen(signame[i]);
  }

  for (int i = 0; i < maxlen; i++) {
    for (int j = 0; j < TIMECOL; j++)
      fprintf(out, " ");
    for (int j = 0; j < nsig; j++) {
      int charno = i - maxlen + namelen(signame[j]);
      if (charno >= 0)
	fprintf(out, "%c", signame[j][charno]);
      else
	fputc(' ', out);
      for (int k = 0; k < nbit[j]; k++)
	fprintf(out, " ");
    }
    fputs("\n", out);
  }

  fprintf(out, "\n");
}


void printevent_vcd(unsigned char *x) {
  fprintf(out, "#%f\n", floor((double)(timestamp)* period * 1e12 + 0.5));
  int startbit = totbit;
  for (int i = 0; i < nsig; i++) {
    // placement of bits...
    // 7 6 5 4 3 2 1 0      x x x ts 11 10 9 8
    // we want to list them in reverse order, i.e. 11...0

    fprintf(out, "b");
    for (int j = startbit; --j >= startbit - nbit[i]; )
      fprintf(out, "%d", (x[j / 8] >> (j & 7)) & 1);
    fprintf(out, " %c\n", 33 + i);
    startbit -= nbit[i];
  }
}

void printoverflow_vcd(void) {
  fprintf(out, "#%f\n", floor((double)(timestamp)* period * 1e12 + 0.5));
  int startbit = totbit;
  for (int i = 0; i < nsig; i++) {
    fprintf(out, "b");
    for (int j = startbit; --j >= startbit - nbit[i]; )
      fprintf(out, "x");
    fprintf(out, " %c\n", 33 + i);
    startbit -= nbit[i];
  }
  fprintf(out, "#%f\n", floor(2.*(double)(timestamp)* period * 1e12 + 0.5));
}

void print_interval(unsigned long ti, int col) {
  double t = (double)ti*period;
  const char *prefixes[] = { "", "m", "u", "n", "p", NULL };
  const char **prefix = prefixes;
  while (t < 1. && prefix[1]) {
    t *= 1000.;
    prefix++;
  }
  char buf[1024];
  sprintf(buf, "%.3g %ss", t, *prefix);
  fputs(buf, out);
  for (int j = strlen(buf); j < col; j++)
    fputc(' ', out);
}

void printoverflow_ascii(void) {
  fputs("OVERFLOW\n", out);
}

void printevent_ascii(unsigned char *x) {
  static unsigned long last_timestamp = 0;

  if (last_timestamp != timestamp - 1) {
    fputc('+', out);
    print_interval(timestamp - last_timestamp, TIMECOL - 1);
    fputs("\n", out);
    print_interval(timestamp, TIMECOL);
  }
  else
    for (int j = 0; j < TIMECOL; j++)
      fprintf(out, " ");
  last_timestamp = timestamp;

  int startbit = totbit;
  for (int i = 0; i < nsig; i++) {
    // placement of bits...
    // 7 6 5 4 3 2 1 0      x x x ts 11 10 9 8
    // we want to list them in reverse order, i.e. 11...0

    for (int j = startbit; --j >= startbit - nbit[i]; )
      fprintf(out, "%c", (x[j / 8] >> (j & 7)) & 1 ? '*' : '-');
    fprintf(out, " ");
    startbit -= nbit[i];
  }
  fprintf(out, "\n");
}

#ifdef _WIN32
#include <windows.h>

typedef HANDLE serialport_t;
#define INVALID_SERIALPORT INVALID_HANDLE_VALUE

serialport_t open_serialport(char *filename, float baud) {
  WCHAR w_infilename[80];
  MultiByteToWideChar(CP_ACP, 0, filename, strlen(filename), w_infilename, sizeof(w_infilename) / sizeof(w_infilename[0]));
  HANDLE in = CreateFile(w_infilename, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
  if (in == INVALID_HANDLE_VALUE) {
    perror("Opening input file");
    exit(1);
  }

  DCB dcb = { 0 };
  dcb.DCBlength = sizeof(DCB);

  if (!GetCommState(in, &dcb)) {
    fprintf(stderr, "GetCommState : Failed with error %d", GetLastError());
    return INVALID_HANDLE_VALUE;
  }

  dcb.BaudRate = baud;
  dcb.fBinary = TRUE;
  dcb.fParity = TRUE;
  dcb.fOutxCtsFlow = FALSE;
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  dcb.fDsrSensitivity = FALSE;
  dcb.fTXContinueOnXoff = FALSE;
  dcb.fOutX = FALSE;
  dcb.fErrorChar = FALSE;
  dcb.fNull = FALSE;
  dcb.fRtsControl = RTS_CONTROL_DISABLE;
  dcb.fAbortOnError = FALSE;
  dcb.XonLim = 0;
  dcb.XoffLim = 0;
  dcb.ByteSize = 8;
  dcb.Parity = ODDPARITY;// EVENPARITY;
  dcb.StopBits = ONESTOPBIT;

  if (!SetCommState(in, &dcb)) {
    fprintf(stderr, "SetCommState : Failed with error %d", GetLastError());
    return INVALID_HANDLE_VALUE;
  }

  COMMTIMEOUTS to = { 0 };
  //  to.ReadIntervalTimeout = MAXDWORD;
  to.ReadIntervalTimeout = 10;

  if (!SetCommTimeouts(in, &to)) {
    fprintf(stderr, "SetCommTimeouts : Failed with error %d", GetLastError());
    return INVALID_HANDLE_VALUE;
  }

  return in;
}

unsigned char *get_word(serialport_t in, int nbytes) {
  static bool inited = false;
  static unsigned char *buf, *p;
  static int bufsiz;
  static int leftover = 0;

  if (!inited) {
    COMMPROP cprop;
    GetCommProperties(in, &cprop);
    bufsiz = cprop.dwCurrentRxQueue / 2;

    buf = (unsigned char *)calloc(bufsiz + 8, 1);
    inited = true;
  }

  COMSTAT comStat;
  DWORD   dwErrors;

  while (leftover < nbytes) {
    memmove(buf, p, leftover);

    DWORD n;
    if (!ReadFile(in, buf + leftover, bufsiz - leftover, &n, NULL)) {
      fprintf(stderr, "ReadFile failed with error %d\n", GetLastError());
      return NULL;
    }

    ClearCommError(in, &dwErrors, &comStat);
    if (dwErrors) {
      fprintf(stderr, "windows serial port error %d\n", dwErrors);
      if (dwErrors & CE_RXOVER) {
	fprintf(stderr, "windows serial port rx overflow\n");
      }
      if (dwErrors & CE_OVERRUN) {
	fprintf(stderr, "windows serial port overrun error\n");
      }
      if (dwErrors & CE_RXPARITY) {
	fprintf(stderr, "windows serial port parity error\n");
      }
      return NULL;
    }


    leftover += n;
    p = buf;
  }

  leftover -= nbytes;
  p += nbytes;
  return p - nbytes;
}
#else
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

typedef int serialport_t;
#define INVALID_SERIALPORT -1

serialport_t open_serialport(char *filename, float baud) {
  int in = open(filename, O_RDONLY | O_NOCTTY | O_NONBLOCK);
  if (in == -1) {
    perror("Open serialport");
    return INVALID_SERIALPORT;
  }

  if (fcntl(in, F_SETFL, 0) == -1)
    {
      perror("clear nonblock");
      return INVALID_SERIALPORT;
    }

  struct termios termios;

  if (tcgetattr(in, &termios) == -1) {
    perror("Get attrs");
    return INVALID_SERIALPORT;
  }

  cfmakeraw(&termios);

  //  termios.c_iflag = IGNBRK |  /* ignore BREAK condition */
  //    PARMRK |   /* mark parity and framing errors */
  //    INPCK;    /* enable checking of parity errors */
  //  termios.c_cflag = CS8 |        /* 8 bits */
  //    CREAD |      /* enable receiver */
  //    PARENB |     /* parity enable */
  //    PARODD |     /* odd parity, else even */
  //  CLOCAL ;     /* ignore modem status lines */

  //  termios.c_iflag |= INPCK | ISTRIP;
  termios.c_iflag |= INPCK;
  termios.c_cflag |= PARODD | PARENB;
  termios.c_cflag &= ~(CRTS_IFLOW | CCTS_OFLOW);
  termios.c_lflag &= ~(ICANON);
  
  if (tcsetattr(in, TCSANOW, &termios) == -1) {
    perror("Set attrs");
    return INVALID_SERIALPORT;
  }

  speed_t baud_speed = baud;
  if (ioctl(in, IOSSIOSPEED, &baud_speed) == -1) {
    perror("readdump setspeed");
    return INVALID_SERIALPORT;
  }

  return in;
}

unsigned char *get_word(serialport_t in, int nbytes) {
  static bool inited = false;
  static unsigned char *buf, *p;
  static int bufsiz;
  static int leftover = 0;

  if (!inited) {
    bufsiz = 512;

    buf = (unsigned char *)calloc(bufsiz + 8, 1);
    inited = true;
  }

  while (leftover < nbytes) {
    memmove(buf, p, leftover);

    ssize_t n;
    n = read(in, buf + leftover, bufsiz - leftover);

    if (errno == EAGAIN)
      n = 0;
    if (n < 0) {
      perror("readdump: read");
      return NULL;
    }

    leftover += n;
    p = buf;
  }

  leftover -= nbytes;
  p += nbytes;
  return p - nbytes;
}
#endif


int main(int argc, char *argv[])
{
  if (argc == 1) {
    fprintf(stderr, "Usage: readdump <input file> <output file> <MHz> <baud> sig1 sig2 sig3 ...\n");
    fprintf(stderr, "Sigs can have format name (1-bit) or name/n (n-bit)\n");
    fprintf(stderr, "Options: \n");
#ifdef _WIN32
    fprintf(stderr, "   /a  Ascii output (default vcd)\n");
#else
    fprintf(stderr, "   -a  Ascii output (default vcd)\n");
#endif
    exit(0);
  }

  int i;

  signame = (char **)malloc(sizeof(char *) * argc);
  int argno = 0;

  nbit = (int *)malloc(sizeof(int) * argc);
  char *outfilename = NULL;
  char *infilename = NULL;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && strlen(argv[i]) > 1) {
      switch (argv[i][1]) {
      case 'a': ascii = 1;
	break;
      }
    }
    else {
      switch (argno) {
      case 0:
	infilename = argv[i];
	break;
      case 1:
	outfilename = argv[i];
	break;
      case 2:
	MHz = atof(argv[i]);
	break;
      case 3:
	baud = atof(argv[i]);
	break;
      default:
	signame[nsig] = argv[i];
	if (strchr(signame[nsig], '/'))
	  nbit[nsig] = atoi(strchr(signame[nsig], '/') + 1);
	else
	  nbit[nsig] = 1;
	nsig = nsig + 1;
      }
      argno++;
    }
  }

  if (strcmp(outfilename, "-") == 0)
    out = stdout;
  else
    out = fopen(outfilename, "wt");

  if (!out) {
    perror("Opening output file");
    exit(1);
  }

  serialport_t in = open_serialport(infilename, baud);
  if (in == INVALID_SERIALPORT)
    return -1;

  period = 1e-6 / MHz;

  int topbit = 0;

  // logger's internal bit width is nsig + 1 (extra bit to indicate repetition counter)
  // each logged word is sent as (nsig+1)/8 + 1 bytes



  for (int i = 0; i < nsig; i++)
    totbit += nbit[i];

  int nbytes = (totbit + 7) / 8;

  if (ascii)
    printheader_ascii();
  else
    printheader_vcd();

  unsigned long last_timestamp;

  unsigned char *p;
  while ((p = get_word(in, nbytes)) != NULL) {
    int nevent = 0;
    //    printf(".%04x\n", *(unsigned short *)p);

    topbit = p[(totbit + 7) / 8 - 1] & (1 << (totbit & 7));
    if (topbit) {
      unsigned long timeword = *(unsigned long *)p;
      timeword &= ~(-1LL << totbit);

      if (timeword == (unsigned long)(~(-1LL << totbit))) {
	timestamp = ++last_timestamp;
	if (ascii)
	  printoverflow_ascii();
	else
	  printoverflow_vcd();
	printf("fpga buffer overflow\n");
      }
      else
	timestamp += timeword;
    }
    else {
      last_timestamp = timestamp;

      if (ascii)
	printevent_ascii(p);
      else
	printevent_vcd(p);

//      putc('.', stderr);
//      fflush(stderr);

      timestamp++;
    }

    fflush(out);

  }

  return 0;
}
