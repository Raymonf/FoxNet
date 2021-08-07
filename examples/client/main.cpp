#include "FoxClient.hpp"
#include "../shared/UserPackets.hpp"

#include <iostream>

uint32_t a, b;

void HANDLE_S2C_NUM_RESPONSE(FoxNet::FoxPeer *peer, void *udata) {
    uint32_t resp;
    ByteStream *stream = peer->getStream();

    stream->readUInt(resp);
    stream->flush();

    std::cout << "got result of " << resp << std::endl;
}

void onPeerReady(FoxNet::FoxPeer *peer) {
    ByteStream *stream = peer->getStream();

    // write our addition request
    stream->writeByte(C2S_REQ_ADD);
    stream->writeUInt(a);
    stream->writeUInt(b);
}

int main() {
    std::cout << "X + Y = ?\nEnter X: ";
    std::cin >> a;

    std::cout << "Enter Y: ";
    std::cin >> b;

    FoxNet::FoxClient client("127.0.0.1", "1337");

    FoxNet::registerUserPacket(S2C_NUM_RESPONSE, HANDLE_S2C_NUM_RESPONSE, PEER_CLIENT, sizeof(uint32_t));
    client.setEvent(PEEREVENT_ONREADY, onPeerReady);
    while (client.isAlive()) {
        client.pollPeer(-1);
    }

    return 0;
}