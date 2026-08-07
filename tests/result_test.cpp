#include <cassert>
#include <type_traits>

#include "src/fw/inc/Result.h"

namespace
{

    constexpr int ERROR_NOT_FOUND     = 0x101;
    constexpr int ERROR_INVALID_STATE = 0x102;
    constexpr int ERROR_NO_MEMORY     = 0x103;

    using ReadError = ErrorSet<
        ERROR_NOT_FOUND,
        ERROR_INVALID_STATE>;

    using ServiceError = ErrorSet<
        ERROR_NOT_FOUND,
        ERROR_INVALID_STATE,
        ERROR_NO_MEMORY>;

    using ReadResult    = Result<int, ReadError>;
    using ServiceResult = Result<int, ServiceError>;
    using VoidResult    = Result<void, ReadError>;
    using ServiceVoid   = Result<void, ServiceError>;

    static_assert(sizeof(ReadError) == sizeof(int));
    static_assert(std::is_trivially_copyable_v<ReadError>);
    static_assert(std::is_trivially_copyable_v<ReadResult>);
    static_assert(std::is_trivially_copyable_v<VoidResult>);
    static_assert(std::is_convertible_v<ReadError, ServiceError>);
    static_assert(!std::is_convertible_v<ServiceError, ReadError>);
    static_assert(std::is_constructible_v<ReadError, ErrorConstant<ERROR_NOT_FOUND>>);
    static_assert(!std::is_constructible_v<ReadError, ErrorConstant<ERROR_NO_MEMORY>>);

    constexpr ReadResult readValue(bool fail) {
        if (fail) {
            return Err<ERROR_NOT_FOUND>();
        }

        return Ok(42);
    }

    constexpr int classify(ReadError error) {
        return error.match(
            on<ERROR_NOT_FOUND>([] {
                return 1;
            }),
            on<ERROR_INVALID_STATE>([] {
                return 2;
            }));
    }

    constexpr int classifyWithNamedArms(ReadError error) {
        constexpr auto notFound     = on<ERROR_NOT_FOUND>([] {
            return 1;
        });
        constexpr auto invalidState = on<ERROR_INVALID_STATE>([] {
            return 2;
        });

        return error.match(notFound, invalidState);
    }

    constexpr int consume(ReadResult result) {
        return result.match(
            [](int value) {
                return value;
            },
            [](ReadError error) {
                return -classify(error);
            });
    }

    constexpr VoidResult initialize(bool fail) {
        if (fail) {
            return Err<ERROR_INVALID_STATE>();
        }

        return Ok();
    }

    constexpr int consumeInitialization(VoidResult result) {
        return result.match(
            [] {
                return 0;
            },
            [](ReadError error) {
                return classify(error);
            });
    }

    constexpr ServiceResult doubleValue(int value) {
        return Ok(value * 2);
    }

    constexpr ServiceResult failWithNoMemory(int) {
        return Err<ERROR_NO_MEMORY>();
    }

    constexpr ServiceResult recoverValue(ReadError) {
        return Ok(7);
    }

    constexpr Result<int, ServiceError> initializeValue() {
        return Ok(9);
    }

    constexpr ServiceVoid recoverInitialization(ReadError) {
        return Ok();
    }

    enum class DomainError : unsigned char
    {
        DISCONNECTED,
        BUSY,
    };

    enum class LegacyError : unsigned char
    {
        FAILED,
        INVALID_STATE,
    };

    using LegacyResult     = Result<int, LegacyError>;
    using LegacyVoidResult = Result<void, LegacyError>;

    constexpr LegacyResult legacyRead(bool fail) {
        return fail ? LegacyResult {Err(LegacyError::FAILED)}
                    : LegacyResult {Ok(11)};
    }

    constexpr LegacyVoidResult legacyInitialize(bool fail) {
        return fail ? LegacyVoidResult {Err(LegacyError::INVALID_STATE)}
                    : LegacyVoidResult {Ok()};
    }

    using DomainErrors = ErrorSet<
        DomainError::DISCONNECTED,
        DomainError::BUSY>;

    static_assert(legacyRead(false).is_ok());
    static_assert(legacyRead(true).is_err());
    static_assert(legacyRead(false).unwrap() == 11);
    static_assert(legacyRead(true).unwrap_err() == LegacyError::FAILED);
    static_assert(legacyRead(false).value() == 11);
    static_assert(legacyRead(true).error() == LegacyError::FAILED);
    static_assert(legacyRead(false).map([](int value) {
                                       return value + 1;
                                   })
                      .unwrap() == 12);
    static_assert(legacyRead(true).map_err([](LegacyError) {
                                      return 7;
                                  })
                      .unwrap_err() == 7);
    static_assert(legacyRead(false).map_or(-1, [](int value) {
        return value * 2;
    }) == 22);
    static_assert(legacyRead(true).map_err_or(0, [](LegacyError) {
        return 1;
    }) == 1);
    static_assert(legacyRead(false).and_then([](int value) {
                                       return LegacyResult {Ok(value + 2)};
                                   })
                      .unwrap() == 13);
    static_assert(legacyRead(true).or_else([](LegacyError) {
                                      return LegacyResult {Ok(9)};
                                  })
                      .unwrap() == 9);
    static_assert(legacyRead(false).match(
                      [](int value) {
                          return value;
                      },
                      [](LegacyError) {
                          return -1;
                      }) == 11);
    static_assert(legacyInitialize(false).is_ok());
    static_assert(legacyInitialize(true).unwrap_err() == LegacyError::INVALID_STATE);

    static_assert(readValue(false).is_ok());
    static_assert(readValue(true).is_err());
    static_assert(consume(readValue(false)) == 42);
    static_assert(consume(readValue(true)) == -1);
    static_assert(consumeInitialization(initialize(false)) == 0);
    static_assert(consumeInitialization(initialize(true)) == 2);
    static_assert(classify(ReadError::of<ERROR_INVALID_STATE>()) == 2);
    static_assert(classifyWithNamedArms(ReadError::of<ERROR_NOT_FOUND>()) == 1);
    static_assert(DomainErrors::of<DomainError::BUSY>().is<DomainError::BUSY>());
    static_assert(ReadError::contains(ERROR_NOT_FOUND));
    static_assert(!ReadError::contains(ERROR_NO_MEMORY));
    static_assert(ReadError::try_from(ERROR_INVALID_STATE).has_value());
    static_assert(!ReadError::try_from(ERROR_NO_MEMORY).has_value());
    static_assert(ReadError::narrow_or<ERROR_NOT_FOUND>(ERROR_NO_MEMORY).is<ERROR_NOT_FOUND>());
    static_assert(from_native<ReadError, ERROR_NOT_FOUND>(
                      0,
                      [](int) {})
                      .is_ok());
    static_assert(from_native<ReadError, ERROR_NOT_FOUND>(
                      ERROR_INVALID_STATE,
                      [](int) {})
                      .unwrap_err()
                      .is<ERROR_INVALID_STATE>());
    static_assert(from_native<ReadError, ERROR_NOT_FOUND>(
                      ERROR_NO_MEMORY,
                      [](int) {})
                      .unwrap_err()
                      .is<ERROR_NOT_FOUND>());

    constexpr auto mappedValue  = readValue(false).map([](int value) {
        return value + 1;
    });
    constexpr auto mappedError  = readValue(true).map([](int value) {
        return value + 1;
    });
    constexpr auto mappedToVoid = readValue(false).map([](int) {});
    constexpr auto failedToVoid = readValue(true).map([](int) {});

    static_assert(mappedValue.unwrap() == 43);
    static_assert(mappedError.unwrap_err().is<ERROR_NOT_FOUND>());
    static_assert(mappedToVoid.is_ok());
    static_assert(failedToVoid.unwrap_err().is<ERROR_NOT_FOUND>());

    constexpr auto widenedError    = readValue(true).map_err([](ReadError error) {
        return ServiceError {error};
    });
    constexpr auto classifiedError = readValue(true).map_err([](ReadError error) {
        return classify(error);
    });

    static_assert(widenedError.unwrap_err().is<ERROR_NOT_FOUND>());
    static_assert(classifiedError.unwrap_err() == 1);
    static_assert(readValue(false).map_or(-1, [](int value) {
        return value * 2;
    }) == 84);
    static_assert(readValue(true).map_or(-1, [](int value) {
        return value * 2;
    }) == -1);
    static_assert(readValue(false).map_err_or(0, [](ReadError error) {
        return classify(error);
    }) == 0);
    static_assert(readValue(true).map_err_or(0, [](ReadError error) {
        return classify(error);
    }) == 1);

    constexpr auto chainedValue       = readValue(false).and_then(doubleValue);
    constexpr auto chainedInputError  = readValue(true).and_then(doubleValue);
    constexpr auto chainedOutputError = readValue(false).and_then(failWithNoMemory);

    static_assert(chainedValue.unwrap() == 84);
    static_assert(chainedInputError.unwrap_err().is<ERROR_NOT_FOUND>());
    static_assert(chainedOutputError.unwrap_err().is<ERROR_NO_MEMORY>());

    constexpr auto untouchedValue = readValue(false).or_else(recoverValue);
    constexpr auto recoveredValue = readValue(true).or_else(recoverValue);

    static_assert(untouchedValue.unwrap() == 42);
    static_assert(recoveredValue.unwrap() == 7);

    constexpr auto voidMappedValue  = initialize(false).map([] {
        return 5;
    });
    constexpr auto voidMappedError  = initialize(true).map([] {
        return 5;
    });
    constexpr auto voidMappedToVoid = initialize(false).map([] {});
    constexpr auto voidFailedToVoid = initialize(true).map([] {});
    constexpr auto voidWidenedError = initialize(true).map_err([](ReadError error) {
        return ServiceError {error};
    });

    static_assert(voidMappedValue.unwrap() == 5);
    static_assert(voidMappedError.unwrap_err().is<ERROR_INVALID_STATE>());
    static_assert(voidMappedToVoid.is_ok());
    static_assert(voidFailedToVoid.unwrap_err().is<ERROR_INVALID_STATE>());
    static_assert(voidWidenedError.unwrap_err().is<ERROR_INVALID_STATE>());
    static_assert(initialize(false).map_or(-1, [] {
        return 6;
    }) == 6);
    static_assert(initialize(true).map_or(-1, [] {
        return 6;
    }) == -1);
    static_assert(initialize(false).map_err_or(0, [](ReadError error) {
        return classify(error);
    }) == 0);
    static_assert(initialize(true).map_err_or(0, [](ReadError error) {
        return classify(error);
    }) == 2);

    constexpr auto voidChainedValue        = initialize(false).and_then(initializeValue);
    constexpr auto voidChainedError        = initialize(true).and_then(initializeValue);
    constexpr auto untouchedInitialization = initialize(false).or_else(recoverInitialization);
    constexpr auto recoveredInitialization = initialize(true).or_else(recoverInitialization);

    static_assert(voidChainedValue.unwrap() == 9);
    static_assert(voidChainedError.unwrap_err().is<ERROR_INVALID_STATE>());
    static_assert(untouchedInitialization.is_ok());
    static_assert(recoveredInitialization.is_ok());

} // namespace

int main() {
    const auto okResult  = readValue(false);
    const auto errResult = readValue(true);

    assert(okResult.unwrap() == 42);
    assert(errResult.unwrap_err().is<ERROR_NOT_FOUND>());
    assert(consume(okResult) == 42);
    assert(consume(errResult) == -1);

    const ServiceError widened = errResult.unwrap_err();
    assert(widened.is<ERROR_NOT_FOUND>());
}
