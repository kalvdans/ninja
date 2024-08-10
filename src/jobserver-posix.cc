// Copyright 2024 Google Inc. All Rights Reserved.
//
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

#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <cstring>

#include "util.h"

Jobserver::Jobserver() {
  assert(!Enabled());

  // Return early if no makeflags are passed in the environment.
  char* makeflags = std::getenv("MAKEFLAGS");
  if (makeflags == nullptr || strlen(makeflags) == 0)
    return;

  std::string::size_type flag_char_ = 0;
  std::string flag_;
  std::vector<std::string> flags_;

  // Tokenize string to characters in flag_, then words in flags_.
  while (flag_char_ < strlen(makeflags)) {
    while (flag_char_ < strlen(makeflags) &&
           !isblank(static_cast<unsigned char>(makeflags[flag_char_]))) {
      flag_.push_back(static_cast<unsigned char>(makeflags[flag_char_]));
      flag_char_++;
    }

    if (!flag_.empty())
      flags_.push_back(flag_);

    flag_.clear();
    flag_char_++;
  }

  // --jobserver-auth=<val>
  for (size_t n = 0; n < flags_.size(); n++)
    if (flags_[n].find(AUTH_KEY) == 0)
      flag_ = flags_[n].substr(strlen(AUTH_KEY));

  // --jobserver-fds=<val>
  if (flag_.empty())
    for (size_t n = 0; n < flags_.size(); n++)
      if (flags_[n].find(FDS_KEY) == 0)
        flag_ = flags_[n].substr(strlen(FDS_KEY));

  // Return early if the flag's value is empty or flag is missing.
  if (flag_.empty())
    return;

  jobserver_name_.assign(flag_);
  const char* jobserver = jobserver_name_.c_str();

  // Check for fifo type first.
  if (jobserver_name_.find(FIFO_KEY) == 0)
    jobserver_fifo_ = true;

  // Return early if jobserver type is unknown (neither fifo nor pipe).
  if (!jobserver_fifo_ && sscanf(jobserver, "%d,%d", &rfd_, &wfd_) != 2) {
    Warning("invalid jobserver value: '%s'", jobserver);
    return;
  }

  // Open FDs to the pipe if needed.
  if (jobserver_fifo_) {
    rfd_ = open(jobserver + strlen(FIFO_KEY), O_RDONLY | O_NONBLOCK);
    wfd_ = open(jobserver + strlen(FIFO_KEY), O_WRONLY);
  }

  // Exit on failure to open FDs, build non-parallel for invalid passed FDs.
  if (rfd_ >= 0 && wfd_ >= 0)
    Info("using jobserver: %s", jobserver);
  else if (rfd_ == -1 || wfd_ == -1)
    Fatal("failed to open jobserver: %s: %s", jobserver,
          errno ? strerror(errno) : "No such file or directory");
  else
    jobserver_closed_ = true;

  // Signal that we have initialized but do not have a token yet.
  token_count_ = -1;
}

Jobserver::~Jobserver() {
  Clear();

  if (rfd_ >= 0)
    close(rfd_);
  if (wfd_ >= 0)
    close(wfd_);

  rfd_ = -1;
  wfd_ = -1;
}

bool Jobserver::Enabled() const {
  return rfd_ >= 0 && wfd_ >= 0;
}

bool Jobserver::Acquire() {
  // The first token is implicitly handed to a process.
  // Fallback to non-parallel building if pipe is closed.
  if (token_count_ <= 0 || jobserver_closed_) {
    token_count_ = 1;
    return true;
  }

  char token;
  int ret = read(rfd_, &token, 1);
  if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    jobserver_closed_ = true;
    if (!jobserver_fifo_)
      Warning("pipe closed: %d (mark the command as recursive)", rfd_);
    else
      Fatal("failed to read from jobserver: %d: %s", rfd_, strerror(errno));
  }

  if (ret > 0)
    token_count_++;

  return ret > 0;
}

void Jobserver::Release() {
  if (token_count_ < 0)
    token_count_ = 0;
  if (token_count_ > 0)
    token_count_--;

  // The first token is implicitly handed to a process.
  // Writing is not possible if the pipe is closed.
  if (token_count_ == 0 || jobserver_closed_)
    return;

  char token = '+';
  int ret = write(wfd_, &token, 1);
  if (ret != 1) {
    Fatal("failed to write to jobserver: %d: %s", wfd_, strerror(errno));
  }
}

void Jobserver::Clear() {
  while (token_count_)
    Release();
}
