#include <unordered_map>

#include "network.h"

class PubSubGateway
{
    const int m_port;
    const std::vector<int> m_pubsubEndpoints;
    std::shared_ptr<TcpServer> m_tcpServer;
    std::vector<TcpConnectionPtr> m_pubsubServers;
    std::unordered_map<std::string, TcpConnectionPtr> m_users;

public:
    PubSubGateway(int port, const std::vector<int>& pubsubEndpoints);
    virtual ~PubSubGateway();

    void Run();

    void RouteRequest(TcpConnectionPtr connection, ProtocolPtr protocol);
    void RouteResponse(TcpConnectionPtr connection, PushProtocolPtr protocol);
};
