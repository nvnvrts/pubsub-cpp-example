#include "gateway.h"

PubSubGateway::PubSubGateway(int port, const std::vector<int>& pubsubEndpoints)
        :m_port(port),
         m_pubsubEndpoints(pubsubEndpoints)
{
}

PubSubGateway::~PubSubGateway()
{
}

void PubSubGateway::Run()
{
    auto ioService = std::make_shared<IOService>();

    for (auto port : m_pubsubEndpoints)
    {
        auto pubsubClient = std::make_shared<TcpClient>(ioService,
                [this](TcpConnectionPtr connection)
                {
                    std::cout << "Connected to pubsub server!" << std::endl;
                    m_pubsubServers.push_back(connection);
                },
                [this](TcpConnectionPtr connection, ProtocolPtr protocol)
                {
                    if (protocol->GetType() == Protocol::Push)
                    {
                        RouteResponse(connection, std::static_pointer_cast<PushProtocol>(protocol));
                    }
                });

        std::cout << "Connecting to 127.0.0.1:" << port << "..." << std::endl;

        pubsubClient->ConnectSync("127.0.0.1", port);
        //pubsubClient->ConnectAsync("127.0.0.1", port);
    }

    auto tcpServer = std::make_shared<TcpServer>(ioService,
            m_port,
            [this](TcpConnectionPtr connection)
            {
            },
            [this](TcpConnectionPtr connection, ProtocolPtr protocol)
            {
                RouteRequest(connection, protocol);
            });

    tcpServer->Start();

    ioService->Run();

    tcpServer->Stop();
}

void PubSubGateway::RouteRequest(TcpConnectionPtr connection, ProtocolPtr protocol)
{
    if (m_pubsubServers.empty())
    {
        return;
    }

    if (protocol->Command == "sub")
    {
        const auto &user = protocol->User;
        m_users[user] = connection;
    }

    static std::hash<std::string> hasher;
    const auto hash = hasher(protocol->Topic);
    const auto idx = hash % m_pubsubServers.size();

    auto pubsubServer = m_pubsubServers[idx];
    if (pubsubServer)
    {
        if (pubsubServer->IsClosed())
        {
            //
        }
        else
        {
            auto dump = protocol->ToString();
            pubsubServer->Send(dump.data(), dump.size());
        }
    }
}

void PubSubGateway::RouteResponse(TcpConnectionPtr connection, PushProtocolPtr protocol)
{
    std::cout << protocol->ToString() << std::endl;

    const auto& user = protocol->User;

    auto it = m_users.find(user);
    if (it != m_users.end())
    {
        auto connection = it->second;
        if (connection->IsClosed())
        {
            m_users.erase(it);
        }
        else
        {
            std::stringstream ss;
            ss << protocol->Message << "\n";

            auto message = ss.str();
            connection->Send(message.data(), message.size());
        }
    }
}

int main(int argc, char** argv)
{
    std::vector<int> pubsubEndpoints;

    int port = 7777;
    if (argc > 1)
    {
        port = std::atoi(argv[1]);
    }

    if (argc > 2)
    {
        for (int i = 2; i < argc ; i++)
        {
            auto pubsubPort = std::atoi(argv[i]);
            pubsubEndpoints.push_back(pubsubPort);
        }
    }

    auto pubsubGateway = std::make_shared<PubSubGateway>(port, pubsubEndpoints);

    pubsubGateway->Run();

    return 0;
}
