#pragma once

#include "Messages.h"
#include "BitmapFont.h"
#include "ShaderVisibleHeap.h"

namespace Neuron::Graphics { class ConstantBufferAllocator; class SpriteBatch; }

// ChatUI — chat message display and text input state machine.
// Stores a ring buffer of recent messages and manages typing mode.
// Pure logic methods are unit-testable without GPU resources.
class ChatUI
{
public:
  static constexpr size_t MAX_VISIBLE_MESSAGES = 8;
  static constexpr size_t MAX_STORED_MESSAGES  = 128;
  static constexpr size_t MAX_INPUT_LENGTH     = 200;
  static constexpr float  MESSAGE_FADE_TIME    = 10.0f; // Seconds before messages fade

  struct ChatEntry
  {
    Neuron::ChatChannel Channel;
    std::string Sender;
    std::string Text;
    float       Age = 0.0f; // Seconds since received
  };

  ChatUI() = default;

  // Add a received chat message.
  void AddMessage(Neuron::ChatChannel _channel,
    std::string_view _sender, std::string_view _text);

  // Update message ages (call each frame).
  void Update(float _deltaT);

  // Toggle typing mode (Enter key).
  void ToggleTyping() noexcept;

  // Whether the chat input is active (consuming keyboard input).
  [[nodiscard]] bool IsTyping() const noexcept { return m_isTyping; }

  // Type a character into the input buffer.
  void TypeChar(char _c);

  // Delete the last character from the input buffer.
  void Backspace();

  // Commit the input buffer: returns the typed text and clears the buffer.
  // Also exits typing mode.
  [[nodiscard]] std::string CommitInput();

  // Get the current input buffer text.
  [[nodiscard]] const std::string& GetInputBuffer() const noexcept { return m_inputBuffer; }

  // Get all stored messages.
  [[nodiscard]] const std::vector<ChatEntry>& GetMessages() const noexcept { return m_messages; }

  // Get messages visible on screen (recent, not yet faded, up to MAX_VISIBLE_MESSAGES).
  [[nodiscard]] std::vector<const ChatEntry*> GetVisibleMessages() const;

  // Get total message count.
  [[nodiscard]] size_t MessageCount() const noexcept { return m_messages.size(); }

  // Render the chat panel (message list + input bar).
  void Render(ID3D12GraphicsCommandList* _cmdList,
    Neuron::Graphics::ConstantBufferAllocator& _cbAlloc,
    Neuron::Graphics::SpriteBatch& _spriteBatch,
    Neuron::Graphics::BitmapFont& _font,
    Neuron::Graphics::ShaderVisibleHeap& _srvHeap,
    UINT _screenWidth, UINT _screenHeight);

  // Active channel for outgoing messages.
  Neuron::ChatChannel ActiveChannel = Neuron::ChatChannel::Zone;

private:
  std::vector<ChatEntry> m_messages;
  std::string m_inputBuffer;
  bool m_isTyping = false;
};
