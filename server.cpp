#include "server.h"

PubSubServer::PubSubServer(int port)
        :m_port(port)
{
}

PubSubServer::~PubSubServer()
{
}

void PubSubServer::Run()
{
    auto ioService = std::make_shared<IOService>();

    auto tcpServer = std::make_shared<TcpServer>(ioService,
            m_port,
            [this](TcpConnectionPtr connection)
            {
            },
            [this](TcpConnectionPtr connection, ProtocolPtr protocol)
            {
                if (protocol->GetType() == Protocol::Publish)
                {
                    HandlePublish(connection, std::static_pointer_cast<PublishProtocol>(protocol));
                }
                else if (protocol->GetType() == Protocol::Subscribe)
                {
                    HandleSubscribe(connection, std::static_pointer_cast<SubscribeProtocol>(protocol));
                }
                else if (protocol->GetType() == Protocol::Push)
                {
                }
            });

    tcpServer->Start();

    ioService->Run();

    tcpServer->Stop();
}

void PubSubServer::HandlePublish(TcpConnectionPtr connection, PublishProtocolPtr protocol)
{
    const auto topic = protocol->Topic;

    auto mq = m_mqContainer.GetOrCreate(topic);
    mq->publish(protocol->User, protocol->Message);
}

void PubSubServer::HandleSubscribe(TcpConnectionPtr connection, SubscribeProtocolPtr protocol)
{
    const auto user = protocol->User;

    auto subscriber = std::make_shared<Subscriber>(user, connection);

    auto mq = m_mqContainer.GetOrCreate(protocol->Topic);
    mq->subscribe(subscriber);
}

int main(int argc, char** argv)
{
    int port = 9991;

    if (argc > 1)
    {
        port = std::atoi(argv[1]);
    }

    auto pubsubServer = std::make_shared<PubSubServer>(port);

    pubsubServer->Run();

    return 0;
}
