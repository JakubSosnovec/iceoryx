#!/bin/bash

# Copyright (c) 2019-2020 by Robert Bosch GmbH. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This file runs all tests for Iceoryx

BASE_DIR=$PWD
TEST_SCOPE="all"
CONTINUE_ON_ERROR=false

set_sanitizer_options() {
    # This script runs from build folder
    cd ..
    local PROJECT_ROOT=$PWD
    cd build

    echo "Project root is PROJECT_ROOT"

    # new_delete_type_mismatch is disabled because of the below error
    # ==112203==ERROR: AddressSanitizer: new-delete-type-mismatch on 0x622000021100 in thread T0:
    #   object passed to delete has wrong type:
    #   size of the allocated type:   5120 bytes;
    #   size of the deallocated type: 496 bytes.
    #     #0 0x7fd36deac9d8 in operator delete(void*, unsigned long) (/usr/lib/x86_64-linux-gnu/libasan.so.4+0xe19d8)
    #     #1 0x55c8284bcc43 in ReceiverPort_test::~ReceiverPort_test() /home/pbt2kor/data/aos/repos/iceoryx_oss/iceoryx/iceoryx_posh/test/moduletests/test_posh_receiverport.cpp:49
    #     #2 0x55c8284c15d1 in ReceiverPort_test_newdata_Test::~ReceiverPort_test_newdata_Test() /home/pbt2kor/data/aos/repos/iceoryx_oss/iceoryx/iceoryx_posh/test/moduletests/test_posh_receiverport.cpp:137
    #     #3 0x55c8284c15ed in ReceiverPort_test_newdata_Test::~ReceiverPort_test_newdata_Test() /home/pbt2kor/data/aos/repos/iceoryx_oss/iceoryx/iceoryx_posh/test/moduletests/test_posh_receiverport.cpp:137
    #     #4 0x55c82857b2fb in testing::Test::DeleteSelf_() (/home/pbt2kor/data/aos/repos/iceoryx_oss/iceoryx/build/posh/test/posh_moduletests+0x3432fb)
    echo "OSTYPE is $OSTYPE"
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        ASAN_OPTIONS=detect_leaks=1
    else
        # other OS (Mac here)
        # ==23449==AddressSanitizer: detect_leaks is not supported on this platform.
        ASAN_OPTIONS=detect_leaks=0
    fi
    ASAN_OPTIONS=$ASAN_OPTIONS:detect_stack_use_after_return=1:detect_stack_use_after_scope=1:check_initialization_order=true:strict_init_order=true:new_delete_type_mismatch=0:suppressions=$BASE_DIR/sanitizer_blacklist/asan_runtime.txt
    export ASAN_OPTIONS
    export LSAN_OPTIONS=suppressions=$BASE_DIR/sanitizer_blacklist/lsan_runtime.txt

    echo "ASAN_OPTIONS : $ASAN_OPTIONS"
    echo "LSAN_OPTIONS : $LSAN_OPTIONS"
}

for arg in "$@"; do
    case "$arg" in
    "only-timing-tests")
        TEST_SCOPE="timingtest"
        ;;
    "continue-on-error")
        CONTINUE_ON_ERROR=true
        ;;
    "all" | "unit" | "integration")
        TEST_SCOPE="$arg"
        ;;
    *)
        echo ""
        echo "Test script for iceoryx."
        echo "By default tests on all levels are executed."
        echo ""
        echo "Usage: $0 [OPTIONS]"
        echo "Options:"
        echo "      disable-timing-tests        Disables all timing tests"
        echo "      only-timing-tests           Runs only timing tests"
        echo "      continue-on-error           Continue execution upon error"
        echo ""
        exit -1
        ;;
    esac
done

# check if this script is sourced by another script,
# if yes then exit properly, so the other script can use this
# scripts definitions
[[ "${#BASH_SOURCE[@]}" -gt "1" ]] && { return 0; }

if [ -z "$TEST_RESULTS_DIR" ]; then
    TEST_RESULTS_DIR="$(pwd)/testresults"
fi

mkdir -p "$TEST_RESULTS_DIR"

echo ">>>>>> Running Iceoryx Tests <<<<<<"

if [ $CONTINUE_ON_ERROR == true ]; then
    # Continue executing tests , when a test fails
    set +e
else
    # Stop executing tests , when a test fails
    set -e
fi

failed_tests=0

set_sanitizer_options

execute_test() {
    local test_scope=$1
    local test_binary=""

    case $test_scope in
    "all")
        echo "Executing all tests"
        make all_tests
        ;;
    "unit")
        echo "Executing unittests"
        make module_tests
        ;;
    "integration")
        echo "Executing integrationtests"
        make integration_tests
        ;;
    "timingtest")
        echo "Executing timingtests"
        make timing_tests
        ;;
    *)
        echo "Wrong scope $test_scope!"
        ;;
    esac
}

echo ""
echo "######################## executing tests for $COMPONENT ########################"

if [ $TEST_SCOPE == "unit" ]; then
    execute_test unit
elif [ $TEST_SCOPE == "integration" ]; then
    execute_test integration
elif [ $TEST_SCOPE == "all" ]; then
    execute_test all
fi

# do not start RouDi while the module and componenttests are running;
# they might do things which hurts RouDi, like in the roudi_shm test where named semaphores are opened and closed

if [ $failed_tests != 0 ]; then
    echo "$failed_tests tests failed!"
fi
echo ">>>>>> Finished Running Iceoryx Tests <<<<<<"
# set return code to indicate test execution status (code = number of failed tests)
# this return code should not be interpreted as standard unix return code
exit $failed_tests
