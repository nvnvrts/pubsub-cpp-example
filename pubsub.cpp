#include "pubsub.h"

void MessageQueue::publish(const std::string& user, const std::string& message)
{
    const auto sequence = ++m_sequence;
    const auto now = std::chrono::system_clock::now();

    auto item = std::make_shared<MessageItem>(sequence, user, message, now);
    m_queue.push_back(item);

    for (auto subscriber : m_subscribers)
    {
        pushMessage(subscriber);
    }
}

bool MessageQueue::subscribe(SubscriberPtr subscriber)
{
    m_subscribers.push_back(subscriber);

    std::cout << m_topic << ": "
              << subscriber->GetUser() << " joined ("  << m_subscribers.size() << ")" << std::endl;

    pushMessage(subscriber);
}

void MessageQueue::pushMessage(SubscriberPtr subscriber)
{
    const auto now = std::chrono::system_clock::now();

    const int duration = 10 * 60; // 24 * 60 * 60;
    int count = 0;

    for (auto item : m_queue)
    {
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - item->Timestamp).count();
        if (diff > duration)
        {
            count++;
        }
        else
        {
            subscriber->SendMessage(m_topic, item->User, item->Message, item->Sequence);

            std::cout << m_topic << ": "
                      << item->User << " -> "
                      << subscriber->GetUser() << "," << item->Message << "," << item->Sequence
                      << std::endl;
        }
    }

    if (count > 0)
    {
        std::cout << m_topic << ": " << count << " message(s) deleted." << std::endl;

        m_queue.erase(m_queue.begin() + count - 1);
    }
}
