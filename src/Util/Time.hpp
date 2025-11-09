#pragma once
#include <chrono>
namespace util
{
/**
 * \brief Helper struct to keep track of time passed.
 */
struct Timestamp
{
	using Seconds = std::chrono::seconds;
	using Milliseconds = std::chrono::milliseconds;
	using Clock = std::chrono::high_resolution_clock;
	using Point = Clock::time_point;

	Point m_point;

	Timestamp();

	/**
	 * \brief Returns the amount of milliseconds passed since construction.
	 */
    Milliseconds TimePassedMs();

	/**
     * \brief Returns the amount of seconds passed since construction.
     */
    Seconds TimePassed();
};
}