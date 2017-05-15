/*
    Function:DNS Relay ����
    Date:2017/3/11
    Programmer:���н�
    Thoughts about the program:
            1.����53�Ŷ˿�
            2.��������53�ŵ����ݰ�
                2.1 ��ȡ��������Ϣ
            3.�����Ҫ��ѯ�������Ƿ���dns.txt��
                3.1 �����
                    ���ض�Ӧ��IP��ַ
                3.2 ����
                        ������ת�����ⲿ��DNS�������������ؽ��
*/
#include "DNSRelay.h"
using namespace dnsNS;

//��ʼ��windows�е�socket����
bool mySocket::initialize()
{
    WSADATA wsaData;

    int iResult;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup failed:" << iResult << std::endl;
        return false;
    }
    return true;
}

//�����������IP���ձ���ȡsocket��bind��53�Ŷ˿ڵȹ���
dnsRelayer::dnsRelayer(int level, const std::string ip, const std::string file) :debug(level), mySocket()
{
    int flag;
    struct addrinfo *serv, *p, hints;
    std::cout << "DNS Relay, Version 1.0, Build: May 14th 2017" << std::endl;
    std::cout << "Usage: dnsrelay [-d | -dd] [<dns-server>] [<db-file>]\n";
    std::cout << "Name Server : " << ip << ":" << PORT << std::endl;
    std::cout << "Debug Level :" << debug << std::endl;
    std::cout << std::endl;
    std::cout << "Try to load the dns table\n";
    //if (loadTable(file) == false) {
    //    throw std::runtime_error("Failed to load dns relay table.\n");
    //}
    //ԭ�����㵼��ʧ�ܾ��׳��쳣
    //������Ϊ����ʧ�ܴ�ӡ������Ϣ���������������м���������

    if (loadTable(file) == false) {
        std::cout << "Failed to load dns relay table.\n";
    }
    else {
        std::cout << "DNS table loaded.\n";
        if (debug == 1 || debug == 2) {
            std::cout << dnsTable.size() << " pieces of DNS table items in total." << std::endl;
        }
    }

    //��ȡ�ⲿ DNS ��������ַ��Ϣ
    //��ȡ��ַ��Ϣʱ��� hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    flag = getaddrinfo(ip.c_str(), PORT, &hints, &serv);
    if (flag != 0) {
        throw std::runtime_error("Failed to get outside DNS server's address information.");
    }
    servAddr = *(serv->ai_addr);//�����ⲿ��������ַ
    servAddrLen = serv->ai_addrlen;
    freeaddrinfo(serv);

    //��ȡ������ַ��Ϣ
    //��ȡ��ַ��Ϣʱ��� hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_UDP;
    flag = getaddrinfo(NULL, PORT, &hints, &serv);
    if (flag != 0) {
        std::cout << "Failed to get the address information." << std::endl;
        std::cout << gai_strerror(flag) << std::endl;
        throw std::runtime_error("Failed to get address information.");
    }

    //��ȡ socket ����ɰ�
    sock = INVALID_SOCKET;
    for (p = serv; p != NULL; p = p->ai_next) {//�������п��õĵ�ַ��Ϣ
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == SOCKET_ERROR) {
            sock = INVALID_SOCKET;
            continue;
        }

        //ʹ�ñ�ռ�õĶ˿�
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(int));

        //�� socket
        if (bind(sock, p->ai_addr, p->ai_addrlen) == SOCKET_ERROR) {
            closesocket(sock);
            continue;
        }
        break;//�ɹ���ȡһ��socket��������ڶ˿��ϡ�
    }
    if (p == NULL) {//��ʧ��
        //std::cout << "Failed to bind." << std::endl;
        WSACleanup();
        freeaddrinfo(serv);
        throw std::runtime_error("Failed to acquire and bind the socket.Maybe a bad name server ip.\n");
    }

    std::cout << "Acquire a socket.\n";
    std::cout << "Bind it to the specified port 53.\n";
    freeaddrinfo(serv);
}

bool dnsRelayer::loadTable(const std::string file)
{
    std::ifstream tableFile;
    std::string name, ip;
    tableFile.open(file);
    int count = 0;
    if (tableFile.is_open() == false) {//δ�ܴ�DNSת���ļ�
        return false;
    }
    while (tableFile >> ip) {//��˳�����ip������
        tableFile >> name;
        dnsTable.insert(std::make_pair(name, ip));
        count += 1;
        if (debug == 2)
            std::cout << std::setw(8) << count << ": " << std::setw(16) << ip << std::setw(28) << std::setiosflags(std::ios::right) << name << std::endl;
    }
    tableFile.close();
    return true;
}

dnsPacket dnsNS::dnsRelayer::parsePacket(const char * buf, const int bytes)
{
    int len = 0, index = 0;
    dnsPacket::_query tmpQuery;
    dnsPacket result;

    result.header.ID = (buf[0] << 8 | (0x00ff & buf[1]));
    result.header.QR = buf[2] >> 7 & 0x01;
    result.header.OPCODE = (buf[2] >> 3 & 0x0f);
    result.header.AA = buf[2] & 0x04;
    result.header.TC = buf[2] & 0x02;
    result.header.RD = buf[2] & 0x01;
    result.header.RA = buf[3] & 0x80;
    result.header.RCODE = buf[3] & 0x0f;
    result.header.QDCOUNT = (buf[4] << 8) | buf[5];
    result.header.ANCOUNT = (buf[6] << 8) | buf[7];
    result.header.NSCOUNT = (buf[8] << 8) | buf[9];
    result.header.ARCOUNT = (buf[10] << 8) | buf[11];
    result.length = bytes;

    if (result.header.QR == 0) {//�����������飬���һ��������ѯ����������Ϣ
        count += 1; //��ѯ���������Ŀ��һ

        //������ѯ���ֵ�����
        index = 12;
        len = buf[index];
        std::string name = "";
        while (len != 0) {
            for (int i = 0;i < len;++i) {
                ++index;
                name += buf[index];
            }
            ++index;
            name += '.';
            len = buf[index];
        }
        name.pop_back();//�������һ�� '.'
        tmpQuery.QNAME = name;

        ++index;//index = 13
        tmpQuery.QTYPE = buf[index] << 8 | buf[index + 1];
        index += 2;//index = 15
        tmpQuery.QCLASS = buf[index] << 8 | buf[index + 1];
        result.query.push_back(tmpQuery);
    }
    return std::move(result);
}

//��dns packet ��������ʾ����
void dnsNS::dnsRelayer::displayPacket(const dnsPacket & packet)
{
    if (debug == 0)
        return;
    else if (debug == 1) {
        if (packet.header.QR == 0) {
            for (auto i = packet.query.begin();i != packet.query.end();++i) {
                std::cout << std::setw(8) << (clock() / CLOCKS_PER_SEC)<< "�� " << "NAME: ";
                std::cout << std::setw(25) << std::setiosflags(std::ios::right) << i->QNAME;
                std::cout << "  TYPE: " << std::setw(2) << std::setiosflags(std::ios::right) << i->QTYPE;
                std::cout << " CLASS: " << std::setw(2) << std::setiosflags(std::ios::right) << i->QCLASS << std::endl;
            }
        }
    }
    else if (debug == 2) {
        std::cout << "ID:" << packet.header.ID << " QR:" << packet.header.QR << " OPCODE:" << packet.header.OPCODE;
        std::cout << " AA:" << packet.header.AA << " TC:" << packet.header.TC << " RD:" << packet.header.RD;
        std::cout << " RA:" << packet.header.RA << " QDCOUNT:" << packet.header.QDCOUNT;
        std::cout << " ANCOUNT:" << packet.header.ANCOUNT << " NSCOUNT:" << packet.header.NSCOUNT;
        std::cout << "ARCOUNT:" << packet.header.ARCOUNT << std::endl;
        std::cout << std::endl;
    }
}

//��ʼ�м̴���DNS����
void dnsRelayer::relay()
{
    int numbytes = 0, theirlen = 0;
    char * recvBuf = new char[MAXBUFLEN];
    struct sockaddr_storage theirAddr;
    theirlen = sizeof(theirAddr);
    while (true) {
        //time_t a = time(NULL);

        numbytes = recvfrom(sock, recvBuf, MAXBUFLEN - 1, 0,
            (struct sockaddr *)&theirAddr, &theirlen);
        if (numbytes == -1) {
            int t = WSAGetLastError();
            if(debug != 0)
                std::cout << "Error in recvfrom : # " << t << std::endl;
            continue;
            //throw std::runtime_error("Error when receving...\n");
        }
        if (debug == 2) {
            std::cout << "Receive from " << addrToIP(theirAddr) << " (" << numbytes << " bytes)\n";
            std::cout << "PACKET:";
            for (int i = 0;i < numbytes;++i)
                std::cout << std::hex << std::setfill('0') << std::setw(2) << (unsigned int)(unsigned char)recvBuf[i] << " ";
            std::cout << std::endl;
        }
        recvBuf[numbytes] = '\0';
        dnsPacket recvPacket = parsePacket(recvBuf, numbytes);
        displayPacket(recvPacket);
        response(recvBuf, recvPacket, theirAddr);
        //time_t b = time(NULL);
        //std::cout << "wanle" << (b - a) << std::endl;
    }
    delete recvBuf;
}

//�޸Ļ������е������Ϣ����DNS���������Ӧ
void dnsRelayer::response(char * recvBuf, const dnsPacket & packet, sockaddr_storage & theirAddr)
{
    unsigned short outID;
    innerID tmpInner;
    if (packet.header.QR == 0) {//�յ���DNS����Ϊ����
                                //˵����DNS������Ӧ�ò�ѯ��������-IP���ձ�(ʹ��Сд)
                                //������ڣ�������Ӧ��//�޸�Buffer
                                //���򣬱��淢�ͷ��ĵ�ַ��Ϣ���ỰID
        for (auto i = packet.query.begin(); i != packet.query.end(); ++i) {
            std::string tmp = i->QNAME;
            auto pos = dnsTable.find(tmp);
            if (pos == dnsTable.end() || i->QTYPE == 0x001c) {//DNS�в����ڸ����� ���� IPV6
                tmpInner.addr = theirAddr;
                tmpInner.inID = packet.header.ID;
                tmpInner.timeStamp = clock() / CLOCKS_PER_SEC;

                idTable.addTable(outID, tmpInner);//�������� ID ӳ��

                recvBuf[0] = outID >> 8;
                recvBuf[1] = outID;
                int t = sendto(sock, recvBuf, packet.length, 0, (sockaddr *)&servAddr, servAddrLen);
                if (debug == 2) {
                    std::cout << "Send to " << addrToIP(theirAddr) << " (" << packet.length << " bytes)\n";
                    std::cout << "[ID:" << tmpInner.inID << " -> " << outID << "]" << std::endl;
                    std::cout << std::endl;
                }
                if (t != packet.length && debug != 0) {
                    t = WSAGetLastError();
                    std::cout << "sento() Error # " << t << std::endl;
                }
            }
            else {
                recvBuf[2] |= 0x80;
                recvBuf[7] = 1;

                int index = packet.length;
                recvBuf[index++] = 0xc0;//����ѹ����ָ�� query �е� NAME
                recvBuf[index++] = 0x0c;

                recvBuf[index++] = 0x00;//��ʾ TYPE Ϊ 1 ��������ַ
                recvBuf[index++] = 0x01;

                recvBuf[index++] = 0x00;//CLASS Ϊ 1
                recvBuf[index++] = 0x01;

                recvBuf[index++] = 0x00;//TTL Ϊ 395
                recvBuf[index++] = 0x00;
                recvBuf[index++] = 0x01;
                recvBuf[index++] = 0x8b;

                recvBuf[index++] = 0x00;//IP��ַ����
                recvBuf[index++] = 0x04;

                //IP��ַ
                std::string ip = pos->second;
                if (ip == "0.0.0.0") {//��������
                    recvBuf[3] |= 0x03;
                }
                else {
                    int ipSec = 0;
                    for (size_t i = 0;i < ip.length();++i) {
                        if (ip[i] != '.' && i != ip.length() - 1) {
                            ipSec = ipSec * 10 + (ip[i] - '0');
                        }
                        else {
                            recvBuf[index++] = ipSec;
                            ipSec = 0;
                        }
                    }
                }
                recvBuf[index++] = 0x00;//��β��־
               int t = sendto(sock, recvBuf, index, 0, (sockaddr *)&theirAddr, sizeof(theirAddr));
                if (debug == 2) {
                    std::cout << "Send to " << addrToIP(theirAddr) << " (" << packet.length << " bytes)" << std::endl;
                    std::cout << std::endl;
                }
                if (t != packet.length&& debug == 2) {
                    t = WSAGetLastError();
                    std::cout << "sento() Error # " << t << std::endl;
                }
            }
        }
    }
    else {//�յ���DNS����Ϊ��Ӧ
          //˵���Ǵ��ⲿ�������յ���
        innerID tmp;
        if (idTable.fetchInnerID(packet.header.ID, tmp) == true) {
            idTable.maintainTable();
            recvBuf[0] = (tmp.inID >> 8);
            recvBuf[1] = (tmp.inID);
            int t1= sendto(sock, recvBuf, packet.length, 0, (sockaddr *)&servAddr, sizeof(servAddr));//DELETE!!!
            int t = sendto(sock, recvBuf, packet.length, 0, (sockaddr *)&tmp.addr, sizeof(tmp.addr));
            if (debug == 2) {
                std::cout << "Send to " << addrToIP(tmp.addr) << " (" << packet.length << " bytes)\n";
                std::cout << "[ID:" << packet.header.ID << " -> " << tmp.inID << "]" << std::endl;
                std::cout << std::endl;
            }
            if (t != packet.length && debug != 0) {
                t = WSAGetLastError();
                std::cout << "sento() Error # " << t << std::endl;
            }
        }
        //���������ڲ���ϢID ת�����У�˵����ʱ��ֱ�Ӷ�����
        //else {
        //}
    }
}

//�� sockaddr_storage ����ȡ IP �� �˿�
std::string dnsNS::dnsRelayer::addrToIP(const sockaddr_storage & addr)
{
    std::string res;
    char * tmp = nullptr;
    unsigned short port = ((sockaddr_in *)&addr)->sin_port;
    int p = 100000;
    tmp = inet_ntoa(((sockaddr_in *)&addr)->sin_addr);
    res = res + std::string(tmp) + ":";
    while (port / p == 0)
        p /= 10;
    while (port != 0) {
        res += (port / p + '0');
        port = port % p;
        p /= 10;
    }
    return res;
}

void dnsNS::convertTable::maintainTable()
{
    for (auto i = idTable.begin(); i != idTable.end();++i) {
        if ((i->second.timeStamp - (clock() / CLOCKS_PER_SEC)) >= maxResponseTime)
            idTable.erase(i);
    }
}

void  dnsNS::convertTable::addTable(unsigned short & outID, innerID & inner)
{
    //������ֵ
    while (idTable.find(nextOutID) != idTable.end()) {
        nextOutID = (nextOutID + 1) % USHRT_MAX;
    }
    idTable.insert(std::make_pair(nextOutID, inner));
    outID = nextOutID;
    nextOutID = (nextOutID + 1) % USHRT_MAX;
}

bool dnsNS::convertTable::fetchInnerID(const unsigned short & outID, innerID & inner)
{
    auto i = idTable.find(outID);
        if (i != idTable.end()) {
            inner = i->second;
            idTable.erase(i);
            return true;
        }
    return false;
}

dnsNS::dnsRelayer * dnsNS::getRelayer(int argc, char * argv[]) {
    dnsRelayer * res;
    if (argc == 1) {
        res = new dnsRelayer();
    }
    else if (argc == 2) {
        if (strcmp(argv[1], "-d") == 0)
            res = new dnsRelayer(1);
        else if (strcmp(argv[1], "-dd") == 0)
            res = new dnsRelayer(2);
        else
            res = new dnsRelayer(0, argv[1]);
    }
    else if (argc == 3) {
        if (strcmp(argv[1], "-d") == 0)
            res = new dnsRelayer(1, argv[2]);
        else if (strcmp(argv[1], "-dd") == 0)
            res = new dnsRelayer(2, argv[2]);
        else
            res = new dnsRelayer(0, argv[1],argv[2]);
    }
    else if (argc == 4) {
        if (strcmp(argv[1], "-d") == 0)
            res = new dnsRelayer(1, argv[2],argv[3]);
        else if (strcmp(argv[1], "-dd") == 0)
            res = new dnsRelayer(2, argv[2],argv[3]);
        else
            res = new dnsRelayer(0, argv[2],argv[3]);
    }
    return res;
}