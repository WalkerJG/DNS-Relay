#pragma once
// link with Ws2_32.lib
#pragma comment(lib,"Ws2_32.lib")

#include<iostream>
#include<string>
#include<list>
#include<unordered_map>
#include<fstream>
#include<ctime>
#include<limits.h>
#include<winsock2.h>
#include<ws2tcpip.h>
#include<iomanip>
#define _WINSOCK_DEPRECATED_NO_WARNINGS

const char defaultServer[] = "10.3.9.5";
const char defaultFile[] = "dnsrelay.txt";
const char PORT[] = "53";
const int MAXBUFLEN = 2048;
const int maxResponseTime = 4;//���Ӧʱ��

namespace dnsNS {

    struct dnsPacket {
        struct _header {
            unsigned short ID;
            unsigned short QR;
            unsigned short OPCODE;
            unsigned short AA;
            unsigned short TC;
            unsigned short RD;
            unsigned short RA;
            unsigned short Z;
            unsigned short RCODE;
            unsigned short QDCOUNT;
            unsigned short ANCOUNT;
            unsigned short NSCOUNT;
            unsigned short ARCOUNT;
        };
        struct _query {
            std::string QNAME;
            unsigned short QTYPE;
            unsigned short QCLASS;
        };
        struct _RR {
            std::string NAME;
            unsigned short TYPE;
            unsigned short CLASS;
            unsigned int TTL;
            unsigned short RELENGTH;
            std::string RDATA;
        };
        _header header;
        std::list<_query> query;
        std::list<_RR> answer;
        std::list<_RR> authority;
        std::list<_RR> additional;
        int length;
    };
    struct innerID {
        unsigned short inID;//DNSRelay�����յ��Ĳ�ѯID
        //unsigned short outID;//DNSRelay���򷢳���DNS����ID
        sockaddr_storage addr;
        clock_t timeStamp;//ʱ���
    };
    
    //��Ϊʹ�� Socket ͨ�ŵĻ��࣬�����ʼ������������
    class mySocket {
    public:
        mySocket()
        {
            bool flag = initialize();
            if (flag == false) {//�����ʼ��ʧ�ܣ��׳��쳣
                throw std::runtime_error("Failed to initialize the environment!\nPlease rerun the program...\n");
            }
        }
        bool initialize();
    };

    //�ڲ����ⲿ DNS ����� ID ת����
    class convertTable {
    private:
        unsigned short nextOutID;
        std::unordered_map<unsigned short, struct innerID> idTable;//(�ⲿ ID ���ڲ� ID ��socket ��ַ��ʱ���)
    public:
        convertTable() { nextOutID = 1; }
        //ά��ת����ɾ����ʱ��Ŀ
        void maintainTable();

        //��ת�����м�����Ϣ
        void  addTable(unsigned short & outID, struct innerID & inner);

        //�����ⲿ ID ��ȡ�ڲ�ID����洢��ַ����Ϣ
        bool fetchInnerID(const unsigned short & outID, struct innerID & inner);
    };

    //DNS �м���
    class dnsRelayer :mySocket {
    private:
        std::unordered_map<std::string, std::string> dnsTable;//Ϊ����߲�ѯЧ�ʣ�ʹ�ù��������洢����-IPӳ���,
                                                                                          //��ͬ���������ظ�ӳ��
        convertTable idTable;//������Ϣ ID ת����
        SOCKET sock;//ͨ�� socket
        int debug = 0;//Debug �ȼ�
        sockaddr servAddr;//�ⲿ DNS ��������ַ
        size_t servAddrLen;//�ⲿ DNS ��������ַ����
        int count = 0;//�Ѵ���� DNS ��ѯ������Ŀ

        bool loadTable(const std::string file);//���ⲿ�ļ��������� - IPӳ���
        dnsPacket parsePacket(const char * buf, const int bytes);//���� DNS ����
        void displayPacket(const dnsPacket & packet);//��ʾ DNS ����
        void response(char * recvBuf, const dnsPacket & packet, sockaddr_storage & theirAddr);//��Ӧ���� DNS ����
        std::string addrToIP(const sockaddr_storage & addr);
    public:
        dnsRelayer(int level = 0, const std::string ip = defaultServer, const std::string file = defaultFile);
        void relay();//�м̺���
    };

    //���ݲ�������һ�� DNS �м���
    dnsRelayer * getRelayer(int argc, char * argv[]);

}