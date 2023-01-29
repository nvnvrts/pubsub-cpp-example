#include "network.h"
#include "pubsub.h"

class PubSubServer
{
    const int m_port;
    std::shared_ptr<TcpServer> m_tcpServer;
    MQContainer m_mqContainer;

public:
    PubSubServer(int port);
    virtual ~PubSubServer();

    void Run();

    void HandlePublish(TcpConnectionPtr connection, PublishProtocolPtr protocol);
    void HandleSubscribe(TcpConnectionPtr connection, SubscribeProtocolPtr protocol);
};
