#include <winsock2.h>
#include <iostream>

#include "ping.hpp"

#define DEFAULT_PACKET_SIZE 32
#define DEFAULT_TTL 30
#define MAX_PING_DATA_SIZE 1024ull
#define MAX_PING_PACKET_SIZE (MAX_PING_DATA_SIZE + sizeof(IPHeader))

using namespace std;

int allocate_buffers(ICMPHeader*& send_buf, IPHeader*& recv_buf,
        int packet_size);


int main(int argc, char* argv[])
{
    int seq_no = 0;
    ICMPHeader* send_buf = 0;
    IPHeader* recv_buf = 0;

    if (argc < 2) {
        cerr << "usage: " << argv[0] << " <host> [data_size] [ttl]" <<
                endl;
        cerr << "\tdata_size can be up to " << MAX_PING_DATA_SIZE <<
                " bytes.  Default is " << DEFAULT_PACKET_SIZE << "." << 
                endl; 
        cerr << "\tttl should be 255 or lower.  Default is " <<
                DEFAULT_TTL << "." << endl;
        return 1;
    }

    int packet_size = DEFAULT_PACKET_SIZE;
    int ttl = DEFAULT_TTL;
    if (argc > 2) {
        int temp = atoi(argv[2]);
        if (temp != 0) {
            packet_size = temp;
        }
        if (argc > 3) {
            temp = atoi(argv[3]);
            if ((temp >= 0) && (temp <= 255)) {
                ttl = temp;
            }
        }
    }
    packet_size = max(sizeof(ICMPHeader), 
            min(MAX_PING_DATA_SIZE, (unsigned long long)packet_size));

    // Start Winsock up
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) {
        cerr << "Failed to find Winsock 2.1 or better." << endl;
        return 1;
    }

    SOCKET sd;
    sockaddr_in dest, source;
    printf("Argv 1 is %s\n", argv[1]);
    if (setup_for_ping(argv[1], ttl, sd, dest) < 0) {
        goto cleanup;
    }
    if (allocate_buffers(send_buf, recv_buf, packet_size) < 0) {
        goto cleanup;
    }
    init_ping_packet(send_buf, packet_size, seq_no);

    // Send the ping and receive the reply
    if (send_ping(sd, dest, send_buf, packet_size) >= 0) {
        while (1) {
            if (recv_ping(sd, source, recv_buf, MAX_PING_PACKET_SIZE) <
                    0) {
                unsigned short header_len = recv_buf->h_len * 4;
                ICMPHeader* icmphdr = (ICMPHeader*)
                        ((char*)recv_buf + header_len);
                if (icmphdr->seq != seq_no) {
                    cerr << "bad sequence number!" << endl;
                    continue;
                }
                else {
                    break;
                }
            }
            if (decode_reply(recv_buf, packet_size, &source) != -2) {
                break;
            }
        }
    }

cleanup:
    delete[]send_buf;
    delete[]recv_buf;
    WSACleanup();
    return 0;
}


int allocate_buffers(ICMPHeader*& send_buf, IPHeader*& recv_buf,
        int packet_size)
{
    // First the send buffer
    send_buf = (ICMPHeader*)new char[packet_size];  
    if (send_buf == 0) {
        cerr << "Failed to allocate output buffer." << endl;
        return -1;
    }

    // And then the receive buffer
    recv_buf = (IPHeader*)new char[MAX_PING_PACKET_SIZE];
    if (recv_buf == 0) {
        cerr << "Failed to allocate output buffer." << endl;
        return -1;
    }
    
    return 0;
}