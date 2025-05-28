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
    _lineBuffer = "";
    printPrompt();
}

void MenuCLI::registerCommand(const String &cmd, const String &help, CommandHandler handler)
{
    _commands[cmd] = {help, handler};
}

void MenuCLI::handleInputChar(char c)
{
    if (_echoEnabled)
    {
        bufferOutputChar(c);
    }
    if (c == '\r')
        return; // Ignore CR
    if (c == '\n')
    {
        bufferOutput("\n");
        processLine();
        _lineBuffer = "";
        printPrompt();
        return;
    }
    if (c == 8 || c == 127)
    { // Backspace or DEL
        if (_lineBuffer.length() > 0)
        {
            _lineBuffer.remove(_lineBuffer.length() - 1);
            bufferOutput("\b \b");
        }
        return;
    }
    if (isPrintable(c))
    {
        _lineBuffer += c;
        bufferOutputChar(c);
    }
}

void MenuCLI::processLine()
{
    String line = _lineBuffer;
    line.trim();
    if (line.length() == 0)
        return;

    int spaceIdx = line.indexOf(' ');
    String cmd = (spaceIdx == -1) ? line : line.substring(0, spaceIdx);
    String args = (spaceIdx == -1) ? "" : line.substring(spaceIdx + 1);

    cmd.toLowerCase();

    if (cmd == "help")
    {
        printHelp();
        return;
    }
    auto it = _commands.find(cmd);
    if (it != _commands.end())
    {
        it->second.handler(args, outputStream()); // Pass proxy stream
    }
    else
    {
        bufferOutput("Unknown command. Type 'help' for a list.\n");
    }
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
    bufferOutput("  exit: Exit menu\n");
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