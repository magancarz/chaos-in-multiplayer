// MIT License

// Copyright (c) 2025 Mateusz Gancarz

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Common/InputController.h"

#include <iostream>
#include <future>
#include <functional>

namespace chs::online
{
    InputController::InputController()
        : poll_input_thread{std::thread{&InputController::pollInput, this}} {}

    std::string waitForInput()
    {
        std::string input;
        std::getline(std::cin, input);
        return input;
    }

    void InputController::pollInput()
    {
        std::future<std::string> input_future = std::async(waitForInput);
        while (!exit_requested)
        {
            if (input_future.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
            {
                std::string input = input_future.get();
                addInput(input);
                std::future<std::string> input_future = std::async(waitForInput);
            }
        }
    }

    void InputController::addInput(std::string input)
    {
        std::lock_guard lock{input_queue_mutex};
        input_queue.emplace(std::move(input));
    }

    InputController::~InputController() noexcept
    {
        exit_requested = true;
    }

    bool InputController::inputAvailable() const
    {
        std::lock_guard lock{input_queue_mutex};
        return !input_queue.empty();
    }

    std::string InputController::readInput()
    {
        std::lock_guard lock{input_queue_mutex};
        std::string input = input_queue.front();
        input_queue.pop();
        return input;
    }
} // namespace chs::online
