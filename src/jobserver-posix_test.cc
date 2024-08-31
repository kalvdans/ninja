// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "jobserver.h"

#include "test.h"

TEST(JobserverPosixTest, EmptyString) {
    std::string fifo;
    int rfd;
    int wfd;
    std::tie(fifo, rfd, wfd) = parse_makeflags("");

    EXPECT_EQ(fifo, "");

    EXPECT_EQ(rfd, -1);
    EXPECT_EQ(wfd, -1);
}

TEST(JobserverPosixTest, NullString) {
    std::string fifo;
    int rfd;
    int wfd;
    std::tie(fifo, rfd, wfd) = parse_makeflags(NULL);

    EXPECT_EQ(fifo, "");

    EXPECT_EQ(rfd, -1);
    EXPECT_EQ(wfd, -1);
}


TEST(JobserverPosixTest, FIFO) {
    std::string fifo;
    int rfd;
    int wfd;
    std::tie(fifo, rfd, wfd) = parse_makeflags("--jobserver-auth=fifo:foo123");

    EXPECT_EQ(fifo, "foo123");

    EXPECT_EQ(rfd, -1);
    EXPECT_EQ(wfd, -1);
}

TEST(JobserverPosixTest, FDS) {
    std::string fifo;
    int rfd;
    int wfd;
    std::tie(fifo, rfd, wfd) = parse_makeflags("--jobserver-auth=18,66");

    EXPECT_EQ(fifo, "");

    EXPECT_EQ(rfd, 18);
    EXPECT_EQ(wfd, 66);
}
