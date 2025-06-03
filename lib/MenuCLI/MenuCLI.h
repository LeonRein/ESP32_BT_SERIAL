#pragma once
#include <Arduino.h>
#include <functional>
#include <map>
#include <vector>
#include <Stream.h>

class MenuCLI : public Stream {
public:
    using CommandHandler = std::function<void(const String& args, Stream& output)>;

    MenuCLI();

    void begin();
    void registerCommand(const String& cmd, const String& help, CommandHandler handler);

    // Stream interface
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    void setEcho(bool enabled) { _echoEnabled = enabled; }
    bool isEchoEnabled() const { return _echoEnabled; }

    void attachOutput(Stream* out);

    using OnExitCallback = std::function<void()>;
    void setOnExit(OnExitCallback cb) { _onExit = cb; }

private:
    static constexpr size_t LINE_BUFFER_SIZE = 128;
    char _lineBuffer[LINE_BUFFER_SIZE] = {0};
    size_t _lineLen = 0;
    bool _echoEnabled = true;

    struct CommandInfo {
        String help;
        CommandHandler handler;
    };
    std::map<String, CommandInfo> _commands;

    // Proxy stream that writes to all outputs
    class MultiOutputStream : public Stream {
    public:
        MultiOutputStream() = default;
        int available() override { return 0; }
        int read() override { return -1; }
        int peek() override { return -1; }
        void flush() override {}
        size_t write(uint8_t c) override;
        size_t write(const uint8_t *buffer, size_t size) override;
        void addOutput(Stream* out) { _outputs.push_back(out); }
        void clearOutputs() { _outputs.clear(); }
        std::vector<Stream*>& outputs() { return _outputs; }
    private:
        std::vector<Stream*> _outputs;
    } _multiOutput;

    MultiOutputStream& outputStream() { return _multiOutput; }

    void handleInputChar(char c);
    void processLine();
    void printPrompt();
    void printHelp();

    void bufferOutput(const String& s);
    void bufferOutputChar(char c);

    OnExitCallback _onExit = nullptr;
};