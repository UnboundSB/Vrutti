# terminal (src/core/terminal/)

## Purpose
Native PTY (Pseudo-Terminal) broker for the integrated terminal. Uses ConPTY on Windows, forkpty on POSIX. Streams raw stdout/stderr bytes to the frontend via...

## Files
- **ReplProcess.cpp**: Implementation of the ReplProcess component.
- **ReplProcess.h**: Header for the ReplProcess component.
- **TerminalProcess.cpp**: Typedefs for dynamically loading ConPTY APIs
- **TerminalProcess.h**: Header for the TerminalProcess component.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **ReplProcess::ReplProcess()**
- **ReplProcess::~ReplProcess()**
- **bool ReplProcess::start(OutputCallback)**
- **ReplProcess::stop()**
- **ReplProcess::evaluate(const std::string&)**
- **TerminalProcess::TerminalProcess()**
- **TerminalProcess::~TerminalProcess()**
- **bool TerminalProcess::start(const std::string&, OutputCallback, std::string&)**
- **TerminalProcess::stop()**
- **TerminalProcess::writeInput(const std::string&)**
- **TerminalProcess::resize(int, int)**
