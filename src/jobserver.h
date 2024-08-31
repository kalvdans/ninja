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

#pragma once

#include <string>
#include <vector>

#define AUTH_KEY  "--jobserver-auth="
#define FDS_KEY   "--jobserver-fds="
#define FIFO_KEY  "fifo:"

std::tuple<std::string, int, int>
parse_makeflags(const char *makeflags);

/// The GNU jobserver limits parallelism by assigning a token from an external
/// pool for each command. On posix systems, the pool is a fifo or simple pipe
/// with N characters. On windows systems, the pool is a semaphore initialized
/// to N. When a command is finished, the acquired token is released by writing
/// it back to the fifo or pipe or by increasing the semaphore count.
///
/// The jobserver functionality is enabled by passing --jobserver-auth=<val>
/// (previously --jobserver-fds=<val> in older versions of Make) in the MAKEFLAGS
/// environment variable and creating the respective file descriptors or objects.
/// On posix systems, <val> is 'fifo:<name>' or '<read_fd>,<write_fd>' for pipes.
/// On windows systems, <val> is the name of the semaphore.
///
/// The class parses the MAKEFLAGS variable and opens the object handle if needed.
/// Once enabled, Acquire() must be called to acquire a token from the pool.
/// If a token is acquired, a new command can be started.
/// Once the command is completed, Release() must be called to return a token.
/// The token server does not care in which order a token is received.
struct Jobserver {
  /// Parse the MAKEFLAGS environment variable to receive the path / FDs / name
  /// of the token pool, and open the handle to the pool if it is an object.
  /// If a jobserver argument is found in the MAKEFLAGS environment variable,
  /// and the handle is successfully opened, later calls to Enable() return true.
  /// If a jobserver argument is found, but the handle fails to be opened,
  /// the ninja process is aborted with an error.
  Jobserver();

  /// Before exiting Ninja, ensure that tokens are returned and handles closed.
  ~Jobserver();

  /// Return true if jobserver functionality is enabled and initialized.
  bool Enabled() const;

  /// Return current token count or initialization signal if negative.
  int Tokens() const { return token_count_; }

  /// Implementation-specific method to acquire a token from the external pool
  /// which is called for all tokens but returns early for the first token.
  /// This method is called every time Ninja needs to start a command process.
  /// Return true on success (token acquired), and false on failure (no tokens
  /// available). First call always succeeds. Ninja is aborted on read errors.
  bool Acquire();

  /// Implementation-specific method to release a token to the external pool
  /// which is called for all tokens but returns early for the last token.
  /// Return a previously acquired token to the external token pool.
  /// It must be called for each successful call to Acquire() after the command
  /// even if subprocesses fail or in the case of errors causing Ninja to exit.
  /// Ninja is aborted on write errors, and otherwise calls always succeed.
  void Release();

  /// Loop through Release() to return all tokens. Called before Ninja exits.
  void Clear();

 private:
  /// The number of currently acquired tokens, or a status signal if negative.
  /// Used to verify that all acquired tokens have been released before exiting,
  /// and when the implicit (first) token has been acquired (initialization).
  /// -1: initialized without a token
  ///  0: uninitialized or disabled
  /// +n: number of tokens in use
  int token_count_ = 0;

  /// String of the parsed value of the jobserver flag passed to environment.
  std::string jobserver_name_;

  /// Whether a non-named pipe to the jobserver token pool is closed.
  bool jobserver_closed_ = false;

// TODO: jobserver client support for Windows
#ifdef _WIN32
#else
  /// Whether the type of jobserver pipe supplied to ninja is named.
  bool jobserver_fifo_ = false;

  /// File descriptors to communicate with upstream jobserver token pool.
  int rfd_ = -1;
  int wfd_ = -1;
#endif
};
