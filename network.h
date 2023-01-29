#pragma once

#include <iostream>

#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

class IOService : private boost::noncopyable
{
    boost::asio::io_service* m_ioService;

public:
    IOService()
    {
        m_ioService = new boost::asio::io_service();
    }

    virtual ~IOService()
    {
        delete m_ioService;
    }

    boost::asio::io_service* GetImpl()
    {
        return m_ioService;
    }

    void Run()
    {
        boost::system::error_code ec;

        do
        {
            m_ioService->run(ec);
        }
        while (!ec);
    }
};

typedef std::shared_ptr<IOService> IOServicePtr;

class ByteBuffer : private boost::noncopyable
{
    char* m_head;
    char* m_rd;
    char* m_wr;
    char* m_tail;

public:
    ByteBuffer()
    {
        int size = 512;
        m_head = new char[size];
        m_rd = m_head;
        m_wr = m_rd;
        m_tail = m_wr + size;
    }

    ~ByteBuffer()
    {
    }

    const char* ReadPtr() { return m_rd; }
    char* WritePtr() {return m_wr; }
    int Size() const { return m_wr - m_rd; }
    int Space() const { return m_rd - m_head; };
    int Capacity() const { return m_tail - m_wr; }

    void Read(char* data, int size)
    {
        std::memcpy(data, m_rd, size);
        m_rd += size;
    }

    void ReadSeek(int size)
    {
        m_rd += size;
    }

    void Write(const char* data, int size)
    {
        std::memcpy(m_wr, data, size);
        m_wr += size;
    }

    void WriteSeek(int size)
    {
        m_wr += size;
    }

    void Reserve(int capacity)
    {
        if (Capacity() < capacity)
        {
            if (Space() + Capacity() < capacity)
            {
                auto size = Size();
                auto newSize = size + capacity;
                auto head = new char[newSize];

                std::memcpy(head, m_rd, size);

                delete[] m_head;

                m_head = head;
                m_rd = m_head;
                m_wr = m_rd + size;
                m_tail = m_head + newSize;
            }
            else
            {
                auto size = Size();
                std::memmove(m_head, m_rd, size);
                m_rd = m_head;
                m_wr = m_rd + size;
            }
        }
    }

    void Swap(ByteBuffer& buffer)
    {
        std::swap(m_head, buffer.m_head);
        std::swap(m_rd, buffer.m_rd);
        std::swap(m_wr, buffer.m_wr);
        std::swap(m_tail, buffer.m_tail);
    }
};

class Protocol
{
public:
    const std::string User;
    const std::string Command;
    const std::string Topic;

    Protocol(const std::string user, const std::string command, const std::string topic)
            :User(user),
             Command(command),
             Topic(topic)
    {
    }

    enum Type
    {
        Publish,
        Subscribe,
        Push
    };

    virtual Type GetType() const = 0;
    virtual std::string ToString() const = 0;
};

typedef std::shared_ptr<Protocol> ProtocolPtr;

class PublishProtocol : public Protocol
{
public:
    const std::string Message;

    PublishProtocol(const std::string user,
                    const std::string command,
                    const std::string topic,
                    const std::string message)
            :Protocol(user, command, topic), Message(message)
    {
    }

    virtual Type GetType() const final;
    virtual std::string ToString() const final;
};

typedef std::shared_ptr<PublishProtocol> PublishProtocolPtr;

class SubscribeProtocol : public Protocol
{
public:
    SubscribeProtocol(const std::string user,
                      const std::string command,
                      const std::string topic)
            :Protocol(user, command, topic)
    {
    }

    virtual Type GetType() const final;
    virtual std::string ToString() const final;
};

typedef std::shared_ptr<SubscribeProtocol> SubscribeProtocolPtr;

class PushProtocol : public Protocol
{
public:
    const std::string Sender;
    const std::string Message;

    PushProtocol(const std::string user,
                 const std::string command,
                 const std::string topic,
                 const std::string sender,
                 const std::string message)
            :Protocol(user, command, topic), Sender(sender), Message(message)
    {
    }

    virtual Type GetType() const final;
    virtual std::string ToString() const final;
};

typedef std::shared_ptr<PushProtocol> PushProtocolPtr;

class Codec : public boost::noncopyable
{
public:
    ProtocolPtr Decode(const char* data, size_t size, size_t& result);
};

class TcpConnection;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

typedef std::function<void(TcpConnectionPtr)> ConnectionHandler;
typedef std::function<void(TcpConnectionPtr, ProtocolPtr)> ProtocolHandler;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
    boost::asio::ip::tcp::socket m_socket;
    ByteBuffer m_recvBuf;
    ByteBuffer m_sendFrontBuf;
    ByteBuffer m_sendBackBuf;
    Codec m_codec;
    ConnectionHandler m_connectionHandler;
    ProtocolHandler m_protocolHandler;

public:
    TcpConnection(boost::asio::ip::tcp::socket socket);
    virtual ~TcpConnection();

    void Open(ConnectionHandler connectionHandler, ProtocolHandler protocolHandler);

    void Recv();
    void OnRecv(boost::system::error_code ec, size_t transferred);

    void Send(const char* data, size_t size);
    void OnSend(boost::system::error_code ec, size_t transferred);

    void OnClose();

    bool IsClosed();
};

class TcpClient : private boost::noncopyable
{
    IOServicePtr m_ioService;
    boost::asio::ip::tcp::socket m_socket;
    ProtocolHandler m_handler;
    TcpConnectionPtr m_tcpConnection;
    ConnectionHandler m_connectionHandler;
    ProtocolHandler m_protocolHandler;

public:
    TcpClient(IOServicePtr ioService, ConnectionHandler connectionHandler, ProtocolHandler protocolHandler);
    virtual ~TcpClient();

    void ConnectSync(const std::string& ip, int port);
    void ConnectAsync(const std::string& ip, int port);

private:
    void OnConnected();
};

class TcpServer;
typedef std::shared_ptr<TcpServer> TcpServerPtr;

class TcpAcceptor : public std::enable_shared_from_this<TcpAcceptor>, private boost::noncopyable
{
    IOServicePtr m_ioService;
    TcpServerPtr m_tcpServer;
    boost::asio::ip::tcp::socket m_socket;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;

public:
    TcpAcceptor(IOServicePtr ioService, TcpServerPtr tcpServer)
            :m_ioService(ioService),
             m_tcpServer(tcpServer),
             m_socket(*ioService->GetImpl())
    {
        m_acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(*ioService->GetImpl());
    }

    virtual ~TcpAcceptor();

    void Start(int port);
    void Stop();

private:
    void Accept();
    void OnAccept();
};

typedef std::shared_ptr<TcpAcceptor> TcpAcceptorPtr;

class TcpServer : public std::enable_shared_from_this<TcpServer>, private boost::noncopyable
{
    IOServicePtr m_ioService;
    const int m_port;
    TcpAcceptorPtr m_tcpAcceptor;
    ConnectionHandler m_connectionHandler;
    ProtocolHandler m_protocolHandler;

public:
    TcpServer(IOServicePtr ioService, int port, ConnectionHandler connectionHandler, ProtocolHandler protocolHandler)
            :m_ioService(ioService),
             m_port(port),
             m_connectionHandler(connectionHandler),
             m_protocolHandler(protocolHandler)
    {
    }

    void Start();
    void Stop();

    void OnConnected(TcpConnectionPtr tcpConnection);
};
