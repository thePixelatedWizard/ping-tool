#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#include "ping.hpp"
#include "ip_checksum.hpp"

using namespace std;


int setup_for_ping(char* host, int ttl, SOCKET& sd, sockaddr_in& dest)
{
    // Create the socket
    sd = WSASocket(AF_INET, SOCK_RAW, IPPROTO_ICMP, 0, 0, 0);
    if (sd == INVALID_SOCKET) {
        cerr << "Failed to create raw socket: " << WSAGetLastError() <<
                endl;
        return -1;
    }

    if (setsockopt(sd, IPPROTO_IP, IP_TTL, (const char*)&ttl, 
            sizeof(ttl)) == SOCKET_ERROR) {
        cerr << "TTL setsockopt failed: " << WSAGetLastError() << endl;
        return -1;
    }

    memset(&dest, 0, sizeof(dest));

    unsigned int addr = inet_addr(host);
    if (addr != INADDR_NONE) {
        dest.sin_addr.s_addr = addr;
        dest.sin_family = AF_INET;
    }
    else {
        hostent* hp = gethostbyname(host);
        if (hp != 0) {
            memcpy(&(dest.sin_addr), hp->h_addr, hp->h_length);
            dest.sin_family = hp->h_addrtype;
        }
        else {
            // Not a recognized hostname either!
            cerr << "Failed to resolve " << host << endl;
            return -1;
        }
    }

    return 0;
}


void init_ping_packet(ICMPHeader* icmp_hdr, int packet_size, int seq_no)
{
    icmp_hdr->type = ICMP_ECHO_REQUEST;
    icmp_hdr->code = 0;
    icmp_hdr->checksum = 0;
    icmp_hdr->id = (USHORT)GetCurrentProcessId();
    icmp_hdr->seq = seq_no;
    icmp_hdr->timestamp = GetTickCount();

    const unsigned long int deadmeat = 0xDEADBEEF;
    char* datapart = (char*)icmp_hdr + sizeof(ICMPHeader);
    int bytes_left = packet_size - sizeof(ICMPHeader);
    while (bytes_left > 0) {
        memcpy(datapart, &deadmeat, min(int(sizeof(deadmeat)), 
                bytes_left));
        bytes_left -= sizeof(deadmeat);
        datapart += sizeof(deadmeat);
    }

    icmp_hdr->checksum = ip_checksum((USHORT*)icmp_hdr, packet_size);
}


int send_ping(SOCKET sd, const sockaddr_in& dest, ICMPHeader* send_buf,
        int packet_size)
{
    cout << "Sending " << packet_size << " bytes to " << 
            inet_ntoa(dest.sin_addr) << "..." << flush;
    int bwrote = sendto(sd, (char*)send_buf, packet_size, 0, 
            (sockaddr*)&dest, sizeof(dest));
    if (bwrote == SOCKET_ERROR) {
        cerr << "send failed: " << WSAGetLastError() << endl;
        return -1;
    }
    else if (bwrote < packet_size) {
        cout << "sent " << bwrote << " bytes..." << flush;
    }

    return 0;
}


int recv_ping(SOCKET sd, sockaddr_in& source, IPHeader* recv_buf, 
        int packet_size)
{
    int fromlen = sizeof(source);
    int bread = recvfrom(sd, (char*)recv_buf, 
            packet_size + sizeof(IPHeader), 0,
            (sockaddr*)&source, &fromlen);
    if (bread == SOCKET_ERROR) {
        cerr << "read failed: ";
        if (WSAGetLastError() == WSAEMSGSIZE) {
            cerr << "buffer too small" << endl;
        }
        else {
            cerr << "error #" << WSAGetLastError() << endl;
        }
        return -1;
    }

    return 0;
}


int decode_reply(IPHeader* reply, int bytes, sockaddr_in* from) 
{
    unsigned short header_len = reply->h_len * 4;
    ICMPHeader* icmphdr = (ICMPHeader*)((char*)reply + header_len);

    if (bytes < header_len + ICMP_MIN) {
        cerr << "too few bytes from " << inet_ntoa(from->sin_addr) << 
                endl;
        return -1;
    }
    else if (icmphdr->type != ICMP_ECHO_REPLY) {
        if (icmphdr->type != ICMP_TTL_EXPIRE) {
            if (icmphdr->type == ICMP_DEST_UNREACH) {
                cerr << "Destination unreachable" << endl;
            }
            else {
                cerr << "Unknown ICMP packet type " << int(icmphdr->type) <<
                        " received" << endl;
            }
            return -1;
        }
    }
    else if (icmphdr->id != (USHORT)GetCurrentProcessId()) {
        return -2;
    }

    int nHops = int(256 - reply->ttl);
    if (nHops == 192) { 
        nHops = 1;
    }
    else if (nHops == 128) {
        nHops = 0;
    }

    cout << endl << bytes << " bytes from " << 
            inet_ntoa(from->sin_addr) << ", icmp_seq " << 
            icmphdr->seq << ", ";
    if (icmphdr->type == ICMP_TTL_EXPIRE) {
        cout << "TTL expired." << endl;
    }
    else {
        cout << nHops << " hop" << (nHops == 1 ? "" : "s");
        cout << ", time: " << (GetTickCount() - icmphdr->timestamp) <<
                " ms." << endl;
    }

    return 0;
}