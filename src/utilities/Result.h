#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tnp {

/// A recoverable failure. TNP does not throw across module boundaries; parsing,
/// serialization and validation all report failures with `Error` so callers are
/// forced to handle them.
struct Error {
    std::string message;
    std::string context;

    Error() = default;
    explicit Error(std::string msg, std::string ctx = {})
        : message(std::move(msg)), context(std::move(ctx)) {}

    /// Human readable one-line rendering, e.g. "topology.json: unexpected token".
    [[nodiscard]] std::string toString() const {
        return context.empty() ? message : context + ": " + message;
    }
};

/// Outcome of an operation that produces no value.
class Status {
public:
    Status() = default; ///< Success.

    static Status ok() { return Status{}; }
    static Status failure(Error error) {
        Status s;
        s.error_ = std::move(error);
        return s;
    }
    static Status failure(std::string message, std::string context = {}) {
        return failure(Error{std::move(message), std::move(context)});
    }

    [[nodiscard]] bool isOk() const { return !error_.has_value(); }
    explicit operator bool() const { return isOk(); }

    [[nodiscard]] const Error& error() const {
        if (!error_) throw std::logic_error("Status::error() called on a successful Status");
        return *error_;
    }

    /// Message of the failure, or an empty string when successful.
    [[nodiscard]] std::string message() const { return error_ ? error_->toString() : std::string{}; }

private:
    std::optional<Error> error_;
};

/// Outcome of an operation that produces a value of type `T`.
template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)) {} // NOLINT(google-explicit-constructor)

    static Result failure(Error error) {
        Result r;
        r.error_ = std::move(error);
        return r;
    }
    static Result failure(std::string message, std::string context = {}) {
        return failure(Error{std::move(message), std::move(context)});
    }

    [[nodiscard]] bool isOk() const { return value_.has_value(); }
    explicit operator bool() const { return isOk(); }

    [[nodiscard]] const T& value() const {
        if (!value_) throw std::logic_error("Result::value() called on a failed Result");
        return *value_;
    }
    [[nodiscard]] T& value() {
        if (!value_) throw std::logic_error("Result::value() called on a failed Result");
        return *value_;
    }
    [[nodiscard]] T valueOr(T fallback) const { return value_ ? *value_ : std::move(fallback); }

    [[nodiscard]] const Error& error() const {
        if (value_) throw std::logic_error("Result::error() called on a successful Result");
        return error_;
    }
    [[nodiscard]] std::string message() const { return value_ ? std::string{} : error_.toString(); }

    /// Drops the value, keeping only success/failure. Useful when a caller only
    /// cares whether a step worked.
    [[nodiscard]] Status status() const {
        return value_ ? Status::ok() : Status::failure(error_);
    }

private:
    Result() = default;

    std::optional<T> value_;
    Error error_;
};

} // namespace tnp
