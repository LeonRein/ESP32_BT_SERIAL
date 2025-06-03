#include "MenuCLI.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>

#define MENUCLI_OUTPUT_BUFFER_SIZE 256
#define MENUCLI_DEFAULT_BUFFER_TIMEOUT_MS 30 // <-- Add this line

MenuCLI::MenuCLI() : _multiOutput() {}

void MenuCLI::attachOutput(Stream* out) {
    _multiOutput.addOutput(out);
}

void MenuCLI::begin()
{
    _lineLen = 0;
    _lineBuffer[0] = '\0';

    printHelp();
    printPrompt();
}

void MenuCLI::registerCommand(const String &cmd, const String &help, CommandHandler handler)
{
    _commands[cmd] = {help, handler};
}

void MenuCLI::handleInputChar(char c)
{
    if (c == '\r')
        return; // Ignore CR
    if (c == '\n')
    {
        bufferOutput("\n");
        processLine();
        _lineLen = 0;
        _lineBuffer[0] = '\0';
        return;
    }
    if (c == 8 || c == 127)
    { // Backspace or DEL
        if (_lineLen > 0)
        {
            _lineLen--;
            _lineBuffer[_lineLen] = '\0';
            bufferOutput("\b \b");
        }
        return;
    }
    if (isPrintable(c))
    {
        if (_lineLen < LINE_BUFFER_SIZE - 1)
        {
            _lineBuffer[_lineLen++] = c;
            _lineBuffer[_lineLen] = '\0';
            if (_echoEnabled)
                bufferOutputChar(c);
        }
    }
}

void MenuCLI::processLine()
{
    // Trim leading whitespace
    size_t start = 0;
    while (start < _lineLen && isspace(_lineBuffer[start])) start++;
    // Trim trailing whitespace
    size_t end = _lineLen;
    while (end > start && isspace(_lineBuffer[end - 1])) end--;
    if (end <= start) {
        printPrompt(); // Print prompt for empty input
        return;
    }

    _lineBuffer[end] = '\0'; // Null-terminate after last non-space

    const char* cmdline = _lineBuffer + start;

    String matchedCmd;
    String matchedArgs;
    auto matchedIt = _commands.end();

    for (auto it = _commands.begin(); it != _commands.end(); ++it) {
        const String &cmd = it->first;
        size_t cmdLen = cmd.length();
        if (strncmp(cmdline, cmd.c_str(), cmdLen) == 0) {
            if (cmdline[cmdLen] == '\0' || isspace(cmdline[cmdLen])) {
                if (cmdLen > matchedCmd.length()) {
                    matchedCmd = cmd;
                    matchedArgs = String(cmdline + cmdLen);
                    matchedArgs.trim();
                    matchedIt = it;
                }
            }
        }
    }

    if (matchedCmd.length() == 0) {
        if (strncmp(cmdline, "help", 4) == 0 && (cmdline[4] == '\0' || isspace(cmdline[4]))) {
            printHelp();
            printPrompt(); // Print prompt after help
            return;
        }
        if (strncmp(cmdline, "exit", 4) == 0 && (cmdline[4] == '\0' || isspace(cmdline[4]))) {
            bufferOutput("\nExiting menu, returning to idle mode.\n");
            if (_onExit) _onExit();
            // Do NOT print prompt after exit
            return;
        }
        bufferOutput("Unknown command. Type 'help' for a list.\n");
        printPrompt(); // Print prompt after unknown command
        return;
    }

    matchedIt->second.handler(matchedArgs, outputStream());
    printPrompt(); // Only print prompt after normal command
}

void MenuCLI::printPrompt()
{
    bufferOutput("> ");
}

void MenuCLI::printHelp()
{
    bufferOutput("Available commands:\n");
    for (const auto &kv : _commands)
    {
        bufferOutput("  " + kv.first + ": " + kv.second.help + "\n");
    }
    bufferOutput("  help: Show this help\n");
}

// Stream interface

int MenuCLI::available()
{
    return 0;
}

int MenuCLI::read()
{
    return -1;
}

int MenuCLI::peek()
{
    return -1;
}

void MenuCLI::flush()
{
    // No-op
}

void MenuCLI::bufferOutput(const String &s) {
    for (auto out : _multiOutput.outputs()) {
        out->print(s);
    }
}

void MenuCLI::bufferOutputChar(char c) {
    for (auto out : _multiOutput.outputs()) {
        out->write(c);
    }
}

size_t MenuCLI::write(uint8_t c) {
    handleInputChar((char)c);
    return 1;
}

size_t MenuCLI::write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    for (size_t i = 0; i < size; ++i) {
        handleInputChar((char)buffer[i]);
        ++n;
    }
    return n;
}

// MultiOutputStream implementation
size_t MenuCLI::MultiOutputStream::write(uint8_t c) {
    size_t total = 0;
    for (auto out : _outputs) {
        total += out->write(c);
    }
    return total;
}
size_t MenuCLI::MultiOutputStream::write(const uint8_t *buffer, size_t size) {
    size_t total = 0;
    for (auto out : _outputs) {
        total += out->write(buffer, size);
    }
    return total;
}