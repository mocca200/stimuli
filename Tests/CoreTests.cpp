#include "Engine/Core/SpscRingBuffer.h"
#include "Engine/Core/TripleBuffer.h"

#include <catch2/catch_all.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

using namespace mvt::core;

TEST_CASE ("SpscRingBuffer rounds capacity and preserves FIFO order", "[core][spsc]")
{
    SpscRingBuffer<int> ring (5);
    REQUIRE (ring.capacity() == 8);
    REQUIRE (ring.empty());
    REQUIRE (ring.availableToWrite() == 8);

    for (int i = 0; i < 8; ++i)
        REQUIRE (ring.push (i));

    REQUIRE (ring.full());
    REQUIRE_FALSE (ring.push (99));

    for (int i = 0; i < 8; ++i)
    {
        int value = -1;
        REQUIRE (ring.pop (value));
        REQUIRE (value == i);
    }

    REQUIRE (ring.empty());
}

TEST_CASE ("SpscRingBuffer block transfers wrap correctly", "[core][spsc]")
{
    SpscRingBuffer<int> ring (8);

    const std::vector<int> first { 0, 1, 2, 3, 4, 5 };
    REQUIRE (ring.pushBlock (first.data(), first.size()) == first.size());

    int scratch[4] {};
    REQUIRE (ring.popBlock (scratch, 4) == 4);

    const std::vector<int> second { 6, 7, 8, 9, 10, 11 };
    REQUIRE (ring.pushBlock (second.data(), second.size()) == second.size());

    std::vector<int> output (8);
    const auto read = ring.popBlock (output.data(), output.size());
    REQUIRE (read == 8);

    REQUIRE (output == std::vector<int> { 4, 5, 6, 7, 8, 9, 10, 11 });
}

TEST_CASE ("SpscRingBuffer is safe under one producer and one consumer", "[core][spsc][thread]")
{
    constexpr int total = 200'000;
    SpscRingBuffer<int> ring (1024);
    std::atomic<bool> producerDone { false };

    std::thread producer ([&]
    {
        for (int i = 0; i < total; ++i)
        {
            while (! ring.push (i))
                std::this_thread::yield();

            if ((i & 1023) == 0)
                std::this_thread::yield();
        }

        producerDone.store (true, std::memory_order_release);
    });

    int expected = 0;
    while (expected < total)
    {
        int value = -1;
        if (ring.pop (value))
        {
            REQUIRE (value == expected);
            ++expected;
        }
        else if (producerDone.load (std::memory_order_acquire) && ring.empty())
        {
            FAIL ("producer finished but consumer still has samples missing");
        }
        else
        {
            std::this_thread::yield();
        }
    }

    producer.join();
    REQUIRE (ring.empty());
}

TEST_CASE ("TripleBuffer never exposes a torn frame", "[core][triple]")
{
    struct Frame
    {
        std::uint64_t sequence = 0;
        std::uint64_t mirror = 0;
        std::uint32_t payload[64] {};
    };

    TripleBuffer<Frame> buffer;
    Frame initial;
    REQUIRE_FALSE (buffer.tryRead (initial));

    std::atomic<bool> done { false };
    std::atomic<std::uint64_t> published { 0 };

    std::thread writer ([&]
    {
        for (std::uint64_t sequence = 1; sequence <= 100'000; ++sequence)
        {
            Frame frame;
            frame.sequence = sequence;
            frame.mirror = sequence;
            for (auto& value : frame.payload)
                value = static_cast<std::uint32_t> (sequence);

            buffer.write (frame);
            published.store (sequence, std::memory_order_release);

            if ((sequence & 4095) == 0)
                std::this_thread::yield();
        }

        done.store (true, std::memory_order_release);
    });

    std::uint64_t lastSeen = 0;
    Frame frame;

    while (! done.load (std::memory_order_acquire) || lastSeen < published.load (std::memory_order_acquire))
    {
        if (buffer.tryRead (frame))
        {
            REQUIRE (frame.sequence == frame.mirror);
            REQUIRE (frame.sequence >= lastSeen);
            for (const auto value : frame.payload)
                REQUIRE (value == static_cast<std::uint32_t> (frame.sequence));

            lastSeen = frame.sequence;
        }
        else
        {
            std::this_thread::yield();
        }
    }

    writer.join();
    REQUIRE (lastSeen > 0);
}
