#include "FoxClient.hpp"

#include <cstring>

using namespace FoxNet;

FoxClient::FoxClient(std::string ip, std::string port) {
    struct addrinfo res, *result;

    // setup packet ids
    INIT_FOXNET_PACKET(S2C_HANDSHAKE, (sizeof(Byte) + FOXMAGICLEN))

    // zero out our address info and setup the type
    memset(&res, 0, sizeof(addrinfo));
    res.ai_family = AF_UNSPEC;
    res.ai_socktype = SOCK_STREAM;

    // grab the address info
    if (getaddrinfo(ip.c_str(), port.c_str(), &res, &result) != 0) {
        FOXFATAL("getaddrinfo() failed!");
    }

    // getaddrinfo returns a list of possible addresses, step through them and try them until we find a valid address
    struct addrinfo *curr;
    for (curr = result; curr != NULL; curr = curr->ai_next) {
        sock = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);

        // if it failed, try the next sock
        if (SOCKETINVALID(sock))
            continue;
        
        // if it's not an invalid socket, break and exit the loop, we found a working addr!
        if (!SOCKETINVALID(connect(sock, curr->ai_addr, curr->ai_addrlen)))
            break;

        killSocket(sock);
    }
    freeaddrinfo(result);

    // if we reached the end of the linked list, we failed looking up the addr
    if (curr == NULL) {
        FOXFATAL("couldn't connect a valid address handle to socket!");
    }

    if (!setSockNonblocking(sock)) {
        FOXFATAL("failed to unblock socket!");
    }

    fd = {sock, POLLIN};

    // connection successful! send handshake
    writeData((PktID)C2S_HANDSHAKE);
    writeBytes((Byte*)FOXMAGIC, FOXMAGICLEN);
    writeByte(FOXNET_MAJOR);
    writeByte(FOXNET_MINOR);
    writeByte(isBigEndian());
    flushSend();
}

FoxClient::~FoxClient() {
    kill();
}

DECLARE_FOXNET_PACKET(S2C_HANDSHAKE, FoxClient) {
    char magic[FOXMAGICLEN];
    Byte response;

    peer->readBytes((Byte*)magic, FOXMAGICLEN);
    peer->readByte(response);

    if (response)
        peer->onReady();
    else
        peer->kill();
}

void FoxClient::pollPeer(int timeout) {
    if (!isAlive()) {
        FOXWARN("pollPeer() called on a dead connection!");
        return;
    }

    if (!sendStep()) {
        kill();
        return;
    }

    // poll() blocks until there's an event on our socket
    int events = poll(&fd, 1, timeout); // poll returns -1 for error, or the number of events
    if (SOCKETERROR(events)) {
        FOXFATAL("poll() failed!");
    }

    // if it wasn't a POLLIN, it was an error or socket hangup
    if (fd.revents & ~POLLIN) {
        FOXFATAL("error on client socket!");
    }

    // try steping, if there was an error handling a packet, kill the connection!
    if (fd.revents & POLLIN && !recvStep())
        kill();

    fd.revents = 0;
}
