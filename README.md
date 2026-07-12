# GUI-NEIRONKA

Local AI Chat with Ollama backend. Pure C++ WinAPI application.

## Files

- `gui_chat.cpp` - Main GUI application with streaming, model selection, Nikita preset
- `train_nikita.cpp` - Neural network trainer for persona optimization
- `chat.cpp` - Terminal chat client with streaming
- `neural.cpp` - Pure C++ neural network (backpropagation)

## Requirements

- Windows with MinGW g++
- Ollama running on localhost:11434

## Build

```bash
# GUI application
g++ -std=c++17 -O2 -mwindows -municode -o gui_chat.exe gui_chat.cpp -lws2_32 -lcomctl32 -lgdi32

# Terminal chat
g++ -std=c++17 -O2 -o chat.exe chat.cpp -lws2_32

# Trainer
g++ -std=c++17 -O2 -o train_nikita.exe train_nikita.cpp -lws2_32

# Neural network
g++ -std=c++17 -O2 -o neural.exe neural.cpp -lm
```

## Usage

1. Start Ollama: `ollama serve`
2. Pull a model: `ollama pull qwen2.5-coder:3b`
3. Run GUI: `gui_chat.exe`
4. Click "Nikita" button to load femboy persona preset
5. Start chatting in Russian!
