#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>

#include "network.h"

Protocol::Type PublishProtocol::GetType() const
{
    return Protocol::Publish;
}

std::string PublishProtocol::ToString() const
{
    std::stringstream ss;
    ss << User << " " << Command << " " << Topic << " " << Message << "\n";
    return ss.str();
}

Protocol::Type SubscribeProtocol::GetType() const
{
    return Protocol::Subscribe;
}

std::string SubscribeProtocol::ToString() const
{
    std::stringstream ss;
    ss << User << " " << Command << " " << Topic << "\n";
    return ss.str();
}

Protocol::Type PushProtocol::GetType() const
{
    return Protocol::Push;
}

std::string PushProtocol::ToString() const
{
    std::stringstream ss;
    ss << User << " " << Command << " " << Topic << " " << Sender << " " << Message << "\n";
    return ss.str();
}

ProtocolPtr Codec::Decode(const char* data, size_t size, size_t& result)
{
    static auto parser = [](std::vector<std::string> tokens) -> ProtocolPtr
    {
        if (tokens.size() < 3)
        {
            return nullptr;
        }

        auto user = tokens[0];
        auto command = tokens[1];
        auto topic = tokens[2];

        if (command.compare("pub") == 0)
        {
            if (tokens.size() < 4)
            {
                return nullptr;
            }

            auto message = tokens[3];

            return std::make_shared<PublishProtocol>(user, command, topic, message);
        }
        else if (command.compare("sub") == 0)
        {
            return std::make_shared<SubscribeProtocol>(user, command, topic);
        }
        else if (command.compare("push") == 0)
        {
            if (tokens.size() < 5)
            {
                return nullptr;
            }

            auto sender = tokens[3];
            auto message = tokens[4];

            return std::make_shared<PushProtocol>(user, command, topic, sender, message);
        }
        else
        {
            // invalid command
            return nullptr;
        }
    };

    for (size_t i = 0; i < size; i ++)
    {
        if (data[i] == '\n')
        {
            auto line = std::string(data, i + 1);

            std::vector<std::string> tokens;
            boost::split(tokens, line, boost::is_any_of(" \r\n"), boost::token_compress_on);

            result = i + 1;

            //std::cout << "Parse... (" << boost::join(tokens, ",") << ")" << std::endl;

            return parser(tokens);
        }
    }

    return nullptr;
}

TcpConnection::TcpConnection(boost::asio::ip::tcp::socket socket)
        :m_socket(std::move(socket))
{
}

TcpConnection::~TcpConnection()
{
}

void TcpConnection::Open(ConnectionHandler connectionHandler, ProtocolHandler protocolHandler)
{
    m_connectionHandler = connectionHandler;
    m_protocolHandler = protocolHandler;

    m_connectionHandler(shared_from_this());

    Recv();
}

void TcpConnection::Recv()
{
    m_recvBuf.Reserve(512);

    m_socket.async_read_some(boost::asio::buffer(m_recvBuf.WritePtr(), m_recvBuf.Capacity()),
            boost::bind(&TcpConnection::OnRecv, shared_from_this(), _1, _2));
}

void TcpConnection::OnRecv(boost::system::error_code ec, size_t transferred)
{
    if (ec)
    {
        OnClose();
        return;
    }

    if (transferred == 0)
    {
        OnClose();
        return;
    }

    m_recvBuf.WriteSeek(transferred);

    //std::string message(m_recvBuf.ReadPtr(), transferred);
    //std::cout << "TcpConnection: Recv (" << message << ")" << std::endl;

    do
    {
        boost::system::error_code ec;

        size_t result = 0;

        auto protocol = m_codec.Decode(m_recvBuf.ReadPtr(), m_recvBuf.Size(), result);
        if (protocol)
        {
            m_recvBuf.ReadSeek(result);

            //std::cout << "TcpConnection: " << protocol->ToString() << std::endl;

            m_protocolHandler(shared_from_this(), protocol);
        }
        else
        {
            break;
        }
    }
    while (true);

    //m_recvBuf.ReedSeek(transferred);

    Recv();
}

void TcpConnection::Send(const char* data, size_t size)

{
    if (m_sendFrontBuf.Size() == 0)
    {
        m_sendFrontBuf.Reserve(size);
        m_sendFrontBuf.Write(data, size);

        auto byteToSend = m_sendFrontBuf.Size();

        boost::asio::async_write(m_socket,
                                boost::asio::buffer(m_sendFrontBuf.ReadPtr(), byteToSend),
                                boost::bind(&TcpConnection::OnSend,
                                            shared_from_this(),
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        m_sendBackBuf.Reserve(size);
        m_sendBackBuf.Write(data, size);
    }
}

void TcpConnection::OnSend(boost::system::error_code ec, const size_t transferred)
{
    if (ec)
    {
        OnClose();
        return;
    }

    if (transferred == 0)
    {
        OnClose();
        return;
    }

    m_sendFrontBuf.ReadSeek(transferred);

    if (m_sendFrontBuf.Size() > 0)
    {
        auto byteToSend = m_sendFrontBuf.Size();

        boost::asio::async_write(m_socket,
                                 boost::asio::buffer(m_sendFrontBuf.ReadPtr(), byteToSend),
                                 boost::bind(&TcpConnection::OnSend,
                                             shared_from_this(),
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));
    }
    else if (m_sendBackBuf.Size() > 0)
    {
        m_sendFrontBuf.Swap(m_sendBackBuf);

        auto byteToSend = m_sendFrontBuf.Size();

        boost::asio::async_write(m_socket,
                                 boost::asio::buffer(m_sendFrontBuf.ReadPtr(), byteToSend),
                                 boost::bind(&TcpConnection::OnSend,
                                             shared_from_this(),
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));
    }
}

void TcpConnection::OnClose()
{
    boost::system::error_code ec;

    m_socket.cancel(ec);

    m_socket.close(ec);

    //std::cout << "TcpConnection: Closed" << std::endl;
}

bool TcpConnection::IsClosed()
{
    return !m_socket.is_open();
}

TcpClient::TcpClient(IOServicePtr ioService, ConnectionHandler connectionHandler, ProtocolHandler protocolHandler)
        :m_ioService(ioService),
         m_socket(*ioService->GetImpl()),
         m_connectionHandler(connectionHandler),
         m_protocolHandler(protocolHandler)
{
}

TcpClient::~TcpClient()
{
}

void TcpClient::ConnectSync(const std::string& ip, int port)
{
    auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip), port);

    boost::system::error_code ec;
    m_socket.connect(endpoint, ec);
    if (!ec)
    {
        OnConnected();
    }
}

void TcpClient::ConnectAsync(const std::string& ip, int port)
{
    auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip), port);

    m_socket.async_connect(endpoint,
            [this](boost::system::error_code ec)
            {
                if (!ec)
                {
                    OnConnected();
                }
            });
}

void TcpClient::OnConnected()
{
    m_tcpConnection = std::make_shared<TcpConnection>(std::move(m_socket));

    m_tcpConnection->Open(m_connectionHandler, m_protocolHandler);
}

TcpAcceptor::~TcpAcceptor()
{
}

void TcpAcceptor::Start(int port)
{
    std::cout << "TcpAcceptor: Start accepting at " << port << "..." << std::endl;

    auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("0.0.0.0"), port);

    boost::system::error_code ec;
    m_acceptor->open(endpoint.protocol(), ec);
    if (ec)
    {
        throw;
    }

    m_acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
    if (ec)
    {
        throw;
    }

    m_acceptor->bind(endpoint, ec);
    if (ec)
    {
        throw;
    }

    m_acceptor->listen(boost::asio::socket_base::max_connections, ec);
    if (ec)
    {
        throw;
    }

    Accept();
}

void TcpAcceptor::Stop()
{
    m_acceptor->cancel();
    m_acceptor->close();
}

void TcpAcceptor::Accept()
{
    auto self = shared_from_this();

    m_acceptor->async_accept(m_socket,
            [this, self](boost::system::error_code ec)
            {
                if (!ec)
                {
                    OnAccept();
                }

                if (m_acceptor->is_open())
                {
                    Accept();
                }
            });
}

void TcpAcceptor::OnAccept()
{
    std::cout << "TcpAcceptor: New client connected." << std::endl;

    auto tcpConnection = std::make_shared<TcpConnection>(std::move(m_socket));

    m_tcpServer->OnConnected(tcpConnection);
}

void TcpServer::Start()
{
    m_tcpAcceptor = std::make_shared<TcpAcceptor>(m_ioService, shared_from_this());

    m_tcpAcceptor->Start(m_port);
}

void TcpServer::Stop()
{
    m_tcpAcceptor->Stop();
}

void TcpServer::OnConnected(TcpConnectionPtr tcpConnection)
{
    tcpConnection->Open(m_connectionHandler, m_protocolHandler);
}
