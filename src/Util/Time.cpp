#include <Util/Time.hpp>

util::Timestamp::Timestamp()
    : m_point(Clock::now())
{

}

util::Timestamp::Milliseconds util::Timestamp::TimePassedMs() const
{
    return std::chrono::duration_cast<util::Timestamp::Milliseconds>(Clock::now() - m_point);
}

util::Timestamp::Seconds util::Timestamp::TimePassed() const
{
    return std::chrono::duration_cast<util::Timestamp::Seconds>(Clock::now() - m_point);
}