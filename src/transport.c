/*
 * transport.c — UDP and Serial (UART-over-USB) transport
 *
 * Both transports use non-blocking I/O so the main loop never stalls
 * waiting for the OS to accept bytes.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 04000
#endif
typedef int socklen_t;
#else
#include <unistd.h>
#include <fcntl.h>
/* UDP */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/* Serial */
#include <termios.h>
#endif

#include "transport.h"

#ifdef _WIN32
static int wsa_initialized = 0;
#endif

/* ================================================================== */
/*  UDP                                                               */
/* ================================================================== */

static int udp_init(transport_t *t)
{
#ifdef _WIN32
    if (!wsa_initialized) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
            fprintf(stderr, "[transport] WSAStartup failed.\n");
            return -1;
        }
        wsa_initialized = 1;
    }
#endif

    t->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (t->fd < 0) {
        perror("[transport] socket()");
        return -1;
    }

    /* Non-blocking */
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(t->fd, FIONBIO, &mode) != 0) {
        fprintf(stderr, "[transport] ioctlsocket non-block failed\n");
#else
    int flags = fcntl(t->fd, F_GETFL, 0);
    if (flags < 0 || fcntl(t->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("[transport] fcntl non-block");
#endif
#ifdef _WIN32
        closesocket(t->fd);
#else
        close(t->fd);
#endif
        t->fd = -1;
        return -1;
    }

    /* Bind to local port so we can receive telemetry from RPi */
    if (t->listen_port > 0) {
        struct sockaddr_in bind_addr;
        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family      = AF_INET;
        bind_addr.sin_port        = htons((uint16_t)t->listen_port);
        bind_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(t->fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
            perror("[transport] bind()");
#ifdef _WIN32
            closesocket(t->fd);
#else
            close(t->fd);
#endif
            t->fd = -1;
            return -1;
        }
        printf("[transport] UDP bound to 0.0.0.0:%d  (recv enabled)\n",
               t->listen_port);
    }

    printf("[transport] UDP → %s:%d  (fd %d)\n",
           t->target_ip, t->target_port, t->fd);
    return 0;
}

static int udp_send(transport_t *t, const uint8_t *buf, size_t len)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)t->target_port);

    if (inet_pton(AF_INET, t->target_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "[transport] bad IP: %s\n", t->target_ip);
        return -1;
    }

    ssize_t n = sendto(t->fd, (const char*)buf, len, 0,
                       (struct sockaddr *)&addr, sizeof(addr));
#ifdef _WIN32
    if (n < 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
#else
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
#endif
        perror("[transport] sendto()");
        return -1;
    }
    return (int)n;
}

/* ================================================================== */
/*  Serial  (UART over USB — /dev/ttyUSB0 or /dev/ttyACM0)           */
/* ================================================================== */

#ifndef _WIN32
static speed_t baud_to_speed(int baud)
{
    switch (baud) {
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 921600: return B921600;
    default:
        fprintf(stderr, "[transport] unknown baud %d, using 115200\n", baud);
        return B115200;
    }
}
#endif

static int serial_init(transport_t *t)
{
#ifdef _WIN32
    fprintf(stderr, "[transport] Serial transport not fully implemented on Windows.\n");
    return -1;
#else
    t->fd = open(t->device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (t->fd < 0) {
        perror("[transport] open(serial)");
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(t->fd, &tty) != 0) {
        perror("[transport] tcgetattr");
        close(t->fd);
        t->fd = -1;
        return -1;
    }

    speed_t spd = baud_to_speed(t->baud_rate);
    cfsetospeed(&tty, spd);
    cfsetispeed(&tty, spd);

    /* 8N1 — no parity, one stop bit, no HW flow control */
    tty.c_cflag  = (tty.c_cflag & (unsigned)~CSIZE) | CS8;
    tty.c_cflag &= (unsigned)~(PARENB | CSTOPB | CRTSCTS);
    tty.c_cflag |= CLOCAL | CREAD;

    /* Raw input — no echo, no canonical processing */
    tty.c_iflag &= (unsigned)~(IXON | IXOFF | IXANY |
                                IGNBRK | BRKINT | PARMRK |
                                ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= (unsigned)~OPOST;
    tty.c_lflag &= (unsigned)~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    tty.c_cc[VMIN]  = 0;   /* non-blocking read */
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(t->fd, TCSANOW, &tty) != 0) {
        perror("[transport] tcsetattr");
        close(t->fd);
        t->fd = -1;
        return -1;
    }

    printf("[transport] Serial → %s @ %d baud  (fd %d)\n",
           t->device, t->baud_rate, t->fd);
    return 0;
#endif
}

static int serial_send(transport_t *t, const uint8_t *buf, size_t len)
{
#ifdef _WIN32
    (void)t; (void)buf; (void)len;
    return -1;
#else
    ssize_t n = write(t->fd, buf, len);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("[transport] write(serial)");
        return -1;
    }
    return (int)n;
#endif
}

/* ================================================================== */
/*  Non-blocking receive (UDP or Serial)                              */
/* ================================================================== */

static int udp_recv(transport_t *t, uint8_t *buf, size_t maxlen)
{
    ssize_t n = recvfrom(t->fd, (char*)buf, maxlen, 0, NULL, NULL);
    if (n < 0) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;           /* nothing available */
#endif
        return -1;
    }
    return (int)n;
}

static int serial_recv(transport_t *t, uint8_t *buf, size_t maxlen)
{
#ifdef _WIN32
    (void)t; (void)buf; (void)maxlen;
    return -1;
#else
    ssize_t n = read(t->fd, buf, maxlen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }
    return (int)n;
#endif
}

/* ================================================================== */
/*  Public interface                                                  */
/* ================================================================== */

int transport_init(transport_t *t)
{
    switch (t->type) {
    case TRANSPORT_UDP:    return udp_init(t);
    case TRANSPORT_SERIAL: return serial_init(t);
    }
    return -1;
}

int transport_send(transport_t *t, const uint8_t *buf, size_t len)
{
    switch (t->type) {
    case TRANSPORT_UDP:    return udp_send(t, buf, len);
    case TRANSPORT_SERIAL: return serial_send(t, buf, len);
    }
    return -1;
}

int transport_recv(transport_t *t, uint8_t *buf, size_t maxlen)
{
    switch (t->type) {
    case TRANSPORT_UDP:    return udp_recv(t, buf, maxlen);
    case TRANSPORT_SERIAL: return serial_recv(t, buf, maxlen);
    }
    return -1;
}

void transport_close(transport_t *t)
{
    if (t->fd >= 0) {
#ifdef _WIN32
        closesocket(t->fd);
#else
        close(t->fd);
#endif
        t->fd = -1;
    }
    printf("[transport] Closed.\n");
}
