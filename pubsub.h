#include <vector>
#include <unordered_map>
#include <chrono>

#include "network.h"

class Subscriber : private boost::noncopyable
{
    const std::string m_user;
    TcpConnectionPtr m_connection;
    unsigned long m_sequenceSent;

public:
    Subscriber(const std::string& user, TcpConnectionPtr connection)
            :m_user(user),
             m_connection(connection),
             m_sequenceSent(0)
    {
    }

    std::string GetUser() const
    {
        return m_user;
    }

    void SendMessage(const std::string& topic, const std::string& sender, const std::string& message, unsigned long sequence)
    {
        if (sequence > m_sequenceSent)
        {
            m_sequenceSent = sequence;

            std::stringstream ss;
            //ss << sender << ":" << message << "\n";
            ss << m_user << " push " << topic << " " << sender << " " << message << "\n";
            auto protocol = ss.str();

            m_connection->Send(protocol.data(), protocol.size());
        }
    }
};

typedef std::shared_ptr<Subscriber> SubscriberPtr;

class MessageItem
{
public:
    const unsigned long Sequence;
    const std::string User;
    const std::string Message;
    const std::chrono::time_point<std::chrono::system_clock> Timestamp;

    MessageItem(unsigned long sequence,
                const std::string& user,
                const std::string& message,
                const std::chrono::time_point<std::chrono::system_clock>& timestamp)
            :Sequence(sequence),
             User(user),
             Message(message),
             Timestamp(timestamp)
    {
    }
};

typedef std::shared_ptr<MessageItem> MessageItemPtr;

class MessageQueue
{
    const std::string m_topic;
    std::vector<MessageItemPtr> m_queue;
    std::vector<SubscriberPtr> m_subscribers;
    unsigned long m_sequence;

public:
    MessageQueue(const std::string& topic)
            :m_topic(topic),
             m_sequence(0)
    {
    }

    const std::string& GetTopic() const
    {
        return m_topic;
    }

    void publish(const std::string& user, const std::string& message);
    bool subscribe(SubscriberPtr subscriber);

private:
    void pushMessage(SubscriberPtr subscriber);
};

typedef std::shared_ptr<MessageQueue> MessageQueuePtr;

class MQContainer
{
    std::unordered_map<std::string, MessageQueuePtr> m_mqMap;

public:
    MessageQueuePtr GetOrCreate(const std::string& topic)
    {
        auto it = m_mqMap.find(topic);
        if (it != m_mqMap.end())
        {
            return it->second;
        }
        else
        {
            auto mq = std::make_shared<MessageQueue>(topic);
            m_mqMap[topic] = mq;
            return mq;
        }
    }
};
