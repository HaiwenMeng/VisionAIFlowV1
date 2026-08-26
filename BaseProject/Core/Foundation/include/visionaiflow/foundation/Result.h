#pragma once

#include "visionaiflow/foundation/Error.h"

#include <stdexcept>
#include <optional>
#include <utility>
#include <variant>

namespace visionaiflow::foundation
{
template<typename T>
class Result
{
public:
    static Result Success(T value)
    {
        return Result(std::move(value));
    }

    static Result Failure(Error error)
    {
        if (error.message.empty())
        {
            throw std::invalid_argument("Result failure requires a non-empty error message");
        }
        return Result(std::move(error));
    }

    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return std::holds_alternative<T>(m_value);
    }

    [[nodiscard]] const T &Value() const
    {
        if (!IsSuccess())
        {
            throw std::logic_error("Cannot access value of a failed Result");
        }
        return std::get<T>(m_value);
    }

    [[nodiscard]] T &Value()
    {
        if (!IsSuccess())
        {
            throw std::logic_error("Cannot access value of a failed Result");
        }
        return std::get<T>(m_value);
    }

    [[nodiscard]] const Error &Failure() const
    {
        if (IsSuccess())
        {
            throw std::logic_error("Cannot access error of a successful Result");
        }
        return std::get<Error>(m_value);
    }

private:
    explicit Result(T value) : m_value(std::move(value)) {}
    explicit Result(Error error) : m_value(std::move(error)) {}

    std::variant<T, Error> m_value;
};

template<>
class Result<void>
{
public:
    static Result Success()
    {
        return Result();
    }

    static Result Failure(Error error)
    {
        if (error.message.empty())
        {
            throw std::invalid_argument("Result failure requires a non-empty error message");
        }
        return Result(std::move(error));
    }

    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return m_success;
    }

    [[nodiscard]] const Error &Failure() const
    {
        if (m_success)
        {
            throw std::logic_error("Cannot access error of a successful Result");
        }
        return *m_error;
    }

private:
    Result() : m_success(true) {}
    explicit Result(Error error) : m_success(false), m_error(std::move(error)) {}

    bool m_success;
    std::optional<Error> m_error;
};
}
