cmake_minimum_required(VERSION 3.20)
project(BankingFraudDetection LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- MariaDB Connector/C (installed through MSYS2) ----
set(MSYS2_PREFIX "C:/msys64/ucrt64" CACHE PATH "MSYS2 UCRT64 install prefix")

find_path(MARIADB_INCLUDE_DIR mysql.h
    HINTS ${MSYS2_PREFIX}/include
    PATH_SUFFIXES mariadb
    NO_DEFAULT_PATH)
find_library(MARIADB_LIBRARY NAMES mariadb libmariadb
    HINTS ${MSYS2_PREFIX}/lib
    NO_DEFAULT_PATH)

if(NOT MARIADB_INCLUDE_DIR OR NOT MARIADB_LIBRARY)
    message(FATAL_ERROR "MariaDB Connector/C not found under ${MSYS2_PREFIX}. "
                        "Install it in MSYS2: pacman -S mingw-w64-ucrt-x86_64-libmariadbclient")
endif()

# ---- Core library: everything except main() ----
set(CORE_SOURCES
    src/Config.cpp
    src/DatabaseManager.cpp
    src/Validation.cpp
    src/User.cpp
    src/Customer.cpp
    src/CustomerRepository.cpp
    src/Money.cpp
    src/Account.cpp
    src/AccountRepository.cpp
    src/Transaction.cpp
    src/TransactionRepository.cpp
    src/TransactionService.cpp
    src/HighAmountRule.cpp
    src/RapidTransactionRule.cpp
    src/FailedAttemptRule.cpp
    src/UnusualTransactionRule.cpp
    src/TransferChainRule.cpp
    src/FraudEngine.cpp
    src/Simulator.cpp
)

add_library(bankcore STATIC ${CORE_SOURCES})
target_include_directories(bankcore PUBLIC include ${MARIADB_INCLUDE_DIR})
target_link_libraries(bankcore PUBLIC ${MARIADB_LIBRARY})
target_compile_options(bankcore PRIVATE -Wall -Wextra)

# ---- Executables ----
add_executable(bank_fraud main.cpp)
target_link_libraries(bank_fraud PRIVATE bankcore)
target_compile_options(bank_fraud PRIVATE -Wall -Wextra)

add_executable(security_check tests/security_check.cpp)
target_link_libraries(security_check PRIVATE bankcore)
target_compile_options(security_check PRIVATE -Wall -Wextra)

add_executable(bank_tests tests/bank_tests.cpp)
target_link_libraries(bank_tests PRIVATE bankcore)
target_compile_options(bank_tests PRIVATE -Wall -Wextra)

add_executable(perf_check tests/perf_check.cpp)
target_link_libraries(perf_check PRIVATE bankcore)
target_compile_options(perf_check PRIVATE -Wall -Wextra)

# Use lld when it is installed (clearer link errors on Windows); otherwise the default linker.
find_program(LLD_LINKER NAMES ld.lld lld)
if(LLD_LINKER)
    target_link_options(bank_fraud PRIVATE -fuse-ld=lld)
    target_link_options(security_check PRIVATE -fuse-ld=lld)
    target_link_options(bank_tests PRIVATE -fuse-ld=lld)
    target_link_options(perf_check PRIVATE -fuse-ld=lld)
endif()