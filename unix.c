/** 
 @file  unix.c
 @brief ENet Unix system specific functions
*/
#ifndef WIN32

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define ENET_BUILDING_LIB 1
#include "enet2/enet.h"

#ifdef HAS_FCNTL
#include <fcntl.h>
#endif

#ifdef __APPLE__
#undef HAS_POLL
#endif

#ifdef HAS_POLL
#include <sys/poll.h>
#endif

#ifndef HAS_SOCKLEN_T
typedef int socklen_t;
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static enet_uint32 timeBase = 0;

int
enet_initialize (void)
{
    return 0;
}

void
enet_deinitialize (void)
{
}

enet_uint32
enet_time_get (void)
{
    struct timeval timeVal;

    gettimeofday (& timeVal, NULL);

    return timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000 - timeBase;
}

void
enet_time_set (enet_uint32 newTimeBase)
{
    struct timeval timeVal;

    gettimeofday (& timeVal, NULL);
    
    timeBase = timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000 - newTimeBase;
}

static enet_uint16
enet_af (ENetAddressFamily family)
{
    if (family == ENET_IPV4)
        return AF_INET;
    if (family == ENET_IPV6)
        return AF_INET6;
    return 0;
}

static socklen_t
enet_sa_size (ENetAddressFamily family)
{
    if (family == ENET_IPV4)
        return sizeof (struct sockaddr_in);
    if (family == ENET_IPV6)
        return sizeof (struct sockaddr_in6);
    return 0;
}

static ENetAddressFamily
enet_address_set_address (ENetAddress * address, const struct sockaddr * sin)
{
    memset (address, 0, sizeof (ENetAddress));
    if (sin -> sa_family == AF_INET)
    {
        address -> host = enet_address_map4 ((((struct sockaddr_in *) sin) -> sin_addr.s_addr));
        /* address -> scopeID = 0; */
        address -> port = ENET_NET_TO_HOST_16 (((struct sockaddr_in *) sin) -> sin_port);
        return ENET_IPV4;
    }
    if (sin -> sa_family == AF_INET6)
    {
        address -> host = * (ENetHostAddress *) & ((struct sockaddr_in6 *) sin) -> sin6_addr;
        address -> scopeID = ((struct sockaddr_in6 *) sin) -> sin6_scope_id;
        address -> port = ENET_NET_TO_HOST_16 (((struct sockaddr_in6 *) sin) -> sin6_port);
        return ENET_IPV6;
    }
    return ENET_NO_ADDRESS_FAMILY;
}

static int
enet_address_set_sin (struct sockaddr * sin, const ENetAddress * address, ENetAddressFamily family)
{
    memset (sin, 0, enet_sa_size(family));
    if (family == ENET_IPV4 &&
      (enet_get_address_family (address) == ENET_IPV4 ||
      !memcmp (& address -> host, & ENET_HOST_ANY, sizeof(ENetHostAddress))))
    {
        ((struct sockaddr_in *) sin) -> sin_family = AF_INET;
        ((struct sockaddr_in *) sin) -> sin_addr = * (struct in_addr *) & address -> host.addr[12];
        ((struct sockaddr_in *) sin) -> sin_port = ENET_HOST_TO_NET_16 (address -> port);
        return 0;
    }
    else if (family == ENET_IPV6)
    {
        ((struct sockaddr_in6 *) sin) -> sin6_family = AF_INET6;
        ((struct sockaddr_in6 *) sin) -> sin6_addr = * (struct in6_addr *) & address -> host;
        ((struct sockaddr_in6 *) sin) -> sin6_scope_id = address -> scopeID;
        ((struct sockaddr_in6 *) sin) -> sin6_port = ENET_HOST_TO_NET_16 (address -> port);
        return 0;
    }
    return -1;
}

int
enet_address_set_host (ENetAddress * address, const char * name)
{
    enet_uint16 port = address -> port;
    struct addrinfo hints;
    struct addrinfo * result;
    struct addrinfo * res;

    memset(& hints, 0, sizeof (hints));
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;

    if ( getaddrinfo(name, NULL, &hints, &result) )
        return -1;

    for (res = result; res != NULL; res = res -> ai_next)
    {
        if ( enet_address_set_address(address, res -> ai_addr) != ENET_NO_ADDRESS_FAMILY )
            break;
    }

    address -> port = port;
    freeaddrinfo(result);
    if (res == NULL) return -1;

    return 0;
}

static int
enet_address_get_host_x (const ENetAddress * address, char * name, size_t nameLength, int flags)
{
    struct sockaddr_storage sin;
    enet_address_set_sin((struct sockaddr *) & sin, address, ENET_IPV6);

    if ( getnameinfo((struct sockaddr *) & sin, enet_sa_size (ENET_IPV6), name, nameLength, NULL, 0, flags))
        return -1;

    return 0;
}

int
enet_address_get_host_ip (const ENetAddress * address, char * name, size_t nameLength)
{
    return enet_address_get_host_x(address, name, nameLength, NI_NUMERICHOST);
}

int
enet_address_get_host (const ENetAddress * address, char * name, size_t nameLength)
{
    return enet_address_get_host_x(address, name, nameLength, 0);
}

int
enet_socket_bind (ENetSocket socket, const ENetAddress * address, ENetAddressFamily family)
{
    struct sockaddr_storage sin;

    if (address != NULL)
    {
        enet_address_set_sin((struct sockaddr *) & sin, address, family);
    }
    else
    {
        ENetAddress address_ = { ENET_HOST_ANY_INIT, 0, 0 };
        enet_address_set_sin((struct sockaddr *) & sin, & address_, family);
    }

    return bind (socket, (struct sockaddr *) & sin, enet_sa_size(family));
}

int
enet_socket_listen (ENetSocket socket, int backlog)
{
    return listen (socket, backlog < 0 ? SOMAXCONN : backlog);
}

ENetSocket
enet_socket_create (ENetSocketType type, ENetAddressFamily family)
{
    ENetSocket sock = socket (enet_af (family), type == ENET_SOCKET_TYPE_DATAGRAM ? SOCK_DGRAM : SOCK_STREAM, 0);

#ifdef IPV6_V6ONLY
    if (family == ENET_IPV6)
    {
        int value = 1;
        setsockopt (sock, IPPROTO_IPV6, IPV6_V6ONLY, & value, sizeof (int));
    }
#endif /* IPV6_V6ONLY */

    return sock;
}

int
enet_socket_set_option (ENetSocket socket, ENetSocketOption option, int value)
{
    int result = -1;
    switch (option)
    {
        case ENET_SOCKOPT_NONBLOCK:
#ifdef HAS_FCNTL
            result = fcntl (socket, F_SETFL, O_NONBLOCK | fcntl (socket, F_GETFL));
#else
            result = ioctl (socket, FIONBIO, & value);
#endif
            break;

        case ENET_SOCKOPT_BROADCAST:
            result = setsockopt (socket, SOL_SOCKET, SO_BROADCAST, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_REUSEADDR:
            result = setsockopt (socket, SOL_SOCKET, SO_REUSEADDR, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_SNDBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDBUF, (char *) & value, sizeof (int));
            break;

        default:
            break;
    }
    return result == -1 ? -1 : 0;
}

int
enet_socket_connect (ENetSocket socket, const ENetAddress * address, ENetAddressFamily family)
{
    struct sockaddr_storage sin;
    enet_address_set_sin((struct sockaddr *) & sin, address, family);

    return connect (socket, (struct sockaddr *) & sin, enet_sa_size (family));
}

ENetSocket
enet_socket_accept (ENetSocket socket, ENetAddress * address, ENetAddressFamily family)
{
    int result;
    struct sockaddr_storage sin;
    socklen_t sinLength = enet_sa_size (family);

    result = accept (socket, 
                     address != NULL ? (struct sockaddr *) & sin : NULL,
                     address != NULL ? & sinLength : NULL);

    if (result == -1)
      return ENET_SOCKET_NULL;

    if (address != NULL)
    {
        enet_address_set_address(address, (struct sockaddr *) & sin);
    }

    return result;
}

void
enet_socket_destroy (ENetSocket socket)
{
    close (socket);
}

int
enet_socket_send (ENetSocket socket,
                  const ENetAddress * address,
                  const ENetBuffer * buffers,
                  size_t bufferCount,
                  ENetAddressFamily family)
{
    struct msghdr msgHdr;
    struct sockaddr_storage sin;
    int sentLength;

    memset (& msgHdr, 0, sizeof (struct msghdr));

    if (address != NULL)
    {
        enet_address_set_sin((struct sockaddr *) & sin, address, family);
        msgHdr.msg_name = & sin;
        msgHdr.msg_namelen = enet_sa_size (family);
    }

    msgHdr.msg_iov = (struct iovec *) buffers;
    msgHdr.msg_iovlen = bufferCount;

    sentLength = sendmsg (socket, & msgHdr, MSG_NOSIGNAL);
    
    if (sentLength == -1)
    {
       if (errno == EWOULDBLOCK)
         return 0;

       return -1;
    }

    return sentLength;
}

int
enet_socket_receive (ENetSocket socket,
                     ENetAddress * address,
                     ENetBuffer * buffers,
                     size_t bufferCount,
                     ENetAddressFamily family)
{
    struct msghdr msgHdr;
    struct sockaddr_storage sin;
    int recvLength;

    memset (& msgHdr, 0, sizeof (struct msghdr));

    if (address != NULL)
    {
        msgHdr.msg_name = & sin;
        msgHdr.msg_namelen = enet_sa_size (family);
    }

    msgHdr.msg_iov = (struct iovec *) buffers;
    msgHdr.msg_iovlen = bufferCount;

    recvLength = recvmsg (socket, & msgHdr, MSG_NOSIGNAL);

    if (recvLength == -1)
    {
       if (errno == EWOULDBLOCK)
         return 0;

       return -1;
    }

#ifdef HAS_MSGHDR_FLAGS
    if (msgHdr.msg_flags & MSG_TRUNC)
      return -1;
#endif

    if (address != NULL)
    {
        enet_address_set_address(address, (struct sockaddr *) & sin);
    }

    return recvLength;
}

int
enet_socketset_select (ENetSocket maxSocket, ENetSocketSet * readSet, ENetSocketSet * writeSet, enet_uint32 timeout)
{
    struct timeval timeVal;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    return select (maxSocket + 1, readSet, writeSet, NULL, & timeVal);
}

int
enet_socket_wait (ENetSocket socket4, ENetSocket socket6, enet_uint32 * condition, enet_uint32 timeout)
{
#ifdef HAS_POLL
    struct pollfd pollSocket[2];
    int pollCount;

    pollSocket[0].fd = socket4;
    pollSocket[1].fd = socket6;
    pollSocket[0].events = 0;
    pollSocket[1].events = 0;
    /* pollSocket[0].revents = 0; */
    pollSocket[1].revents = 0;

    if (pollSocket[0].fd == ENET_SOCKET_NULL)
    {
        pollSocket[0].fd = pollSocket[1].fd;
        pollSocket[1].fd = ENET_SOCKET_NULL;
    }

    if (* condition & ENET_SOCKET_WAIT_SEND)
    {
        pollSocket[0].events |= POLLOUT;
        pollSocket[1].events |= POLLOUT;
    }

    if (* condition & ENET_SOCKET_WAIT_RECEIVE)
    {
        pollSocket[0].events |= POLLIN;
        pollSocket[1].events |= POLLIN;
    }

    pollCount = poll (pollSocket, pollSocket[1].fd != ENET_SOCKET_NULL ? 2 : 1, timeout);

    if (pollCount < 0)
      return -1;

    * condition = ENET_SOCKET_WAIT_NONE;

    if (pollCount == 0)
      return 0;

    if ((pollSocket[0].revents | pollSocket[1].revents) & POLLOUT)
      * condition |= ENET_SOCKET_WAIT_SEND;
    
    if ((pollSocket[0].revents | pollSocket[1].revents) & POLLIN)
      * condition |= ENET_SOCKET_WAIT_RECEIVE;

    return 0;
#else
    fd_set readSet, writeSet;
    struct timeval timeVal;
    int selectCount;
    ENetSocket maxSocket;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO (& readSet);
    FD_ZERO (& writeSet);

    if (* condition & ENET_SOCKET_WAIT_SEND)
    {
        if (socket4 != ENET_SOCKET_NULL)
            FD_SET (socket4, & writeSet);
        if (socket6 != ENET_SOCKET_NULL)
            FD_SET (socket6, & writeSet);
    }

    if (* condition & ENET_SOCKET_WAIT_RECEIVE)
    {
        if (socket4 != ENET_SOCKET_NULL)
            FD_SET (socket4, & readSet);
        if (socket6 != ENET_SOCKET_NULL)
            FD_SET (socket6, & readSet);
    }

    maxSocket = 0;
    if (socket4 != ENET_SOCKET_NULL)
        maxSocket = socket4;
    if (socket6 != ENET_SOCKET_NULL && socket6 > maxSocket)
        maxSocket = socket6;

    selectCount = select (maxSocket + 1, & readSet, & writeSet, NULL, & timeVal);

    if (selectCount < 0)
      return -1;

    * condition = ENET_SOCKET_WAIT_NONE;

    if (selectCount == 0)
      return 0;

    if ( (socket4 != ENET_SOCKET_NULL && FD_ISSET (socket4, & writeSet)) ||
        (socket6 != ENET_SOCKET_NULL && FD_ISSET (socket6, & writeSet)) )
        * condition |= ENET_SOCKET_WAIT_SEND;

    if ( (socket4 != ENET_SOCKET_NULL && FD_ISSET (socket4, & readSet)) ||
        (socket6 != ENET_SOCKET_NULL && FD_ISSET (socket6, & readSet)) )
        * condition |= ENET_SOCKET_WAIT_RECEIVE;

    return 0;
#endif
}

#endif

