#include "pch.h"
#include "ChatUI.h"
#include "SpriteBatch.h"

using namespace Neuron;
using namespace Neuron::Graphics;

void ChatUI::AddMessage(ChatChannel _channel,
  std::string_view _sender, std::string_view _text)
{
  ChatEntry entry;
  entry.Channel = _channel;
  entry.Sender  = std::string(_sender);
  entry.Text    = std::string(_text);
  entry.Age     = 0.0f;

  m_messages.push_back(std::move(entry));

  // Trim to max stored messages
  if (m_messages.size() > MAX_STORED_MESSAGES)
    m_messages.erase(m_messages.begin());
}

void ChatUI::Update(float _deltaT)
{
  for (auto& msg : m_messages)
    msg.Age += _deltaT;
}

void ChatUI::ToggleTyping() noexcept
{
  m_isTyping = !m_isTyping;
  if (!m_isTyping)
    m_inputBuffer.clear();
}

void ChatUI::TypeChar(char _c)
{
  if (!m_isTyping) return;
  if (_c < 32 || _c > 126) return; // Printable ASCII only
  if (m_inputBuffer.size() >= MAX_INPUT_LENGTH) return;
  m_inputBuffer += _c;
}

void ChatUI::Backspace()
{
  if (!m_isTyping || m_inputBuffer.empty()) return;
  m_inputBuffer.pop_back();
}

std::string ChatUI::CommitInput()
{
  std::string result = std::move(m_inputBuffer);
  m_inputBuffer.clear();
  m_isTyping = false;
  return result;
}

std::vector<const ChatUI::ChatEntry*> ChatUI::GetVisibleMessages() const
{
  std::vector<const ChatEntry*> visible;

  // Walk backwards from newest, collect up to MAX_VISIBLE_MESSAGES that haven't faded
  for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
  {
    if (visible.size() >= MAX_VISIBLE_MESSAGES)
      break;

    // Show messages when typing mode is active (ignore fade), or if still fresh
    if (m_isTyping || it->Age < MESSAGE_FADE_TIME)
      visible.push_back(&(*it));
  }

  // Reverse so oldest is first (top of display)
  std::reverse(visible.begin(), visible.end());
  return visible;
}

void ChatUI::Render(ID3D12GraphicsCommandList* _cmdList,
  ConstantBufferAllocator& _cbAlloc,
  SpriteBatch& _spriteBatch,
  UINT _screenWidth, UINT _screenHeight)
{
  // Chat panel is rendered at bottom-center of screen.
  // Without text rendering, we show colored bars as message indicators
  // and a typing bar when input is active.
  constexpr LONG CHAT_WIDTH  = 400;
  constexpr LONG LINE_HEIGHT = 14;
  constexpr LONG MARGIN      = 20;
  constexpr LONG INPUT_HEIGHT = 20;

  LONG panelLeft = static_cast<LONG>(_screenWidth / 2) - CHAT_WIDTH / 2;
  LONG panelBottom = static_cast<LONG>(_screenHeight) - MARGIN;

  _spriteBatch.Begin(_cmdList, _cbAlloc, _screenWidth, _screenHeight);

  auto visible = GetVisibleMessages();

  // Render message bars (colored line per message to indicate channel activity)
  LONG y = panelBottom - INPUT_HEIGHT - static_cast<LONG>(visible.size()) * LINE_HEIGHT;
  for (const auto* msg : visible)
  {
    // Channel color
    XMVECTORF32 barColor;
    switch (msg->Channel)
    {
    case ChatChannel::Zone:    barColor = { 0.8f, 0.8f, 0.8f, 0.5f }; break;
    case ChatChannel::Party:   barColor = { 0.3f, 0.5f, 1.0f, 0.5f }; break;
    case ChatChannel::Whisper: barColor = { 0.8f, 0.3f, 0.8f, 0.5f }; break;
    case ChatChannel::System:  barColor = { 1.0f, 1.0f, 0.3f, 0.5f }; break;
    default:                   barColor = { 0.5f, 0.5f, 0.5f, 0.5f }; break;
    }

    // Fade with age (when not typing)
    if (!m_isTyping && msg->Age > MESSAGE_FADE_TIME * 0.7f)
    {
      float fade = 1.0f - (msg->Age - MESSAGE_FADE_TIME * 0.7f) /
        (MESSAGE_FADE_TIME * 0.3f);
      fade = (std::max)(0.0f, fade);
      barColor.f[3] *= fade;
    }

    RECT msgBar = { panelLeft, y, panelLeft + CHAT_WIDTH, y + LINE_HEIGHT - 2 };
    _spriteBatch.DrawRect(msgBar, barColor);
    y += LINE_HEIGHT;
  }

  // Typing input bar
  if (m_isTyping)
  {
    XMVECTORF32 inputBg = { 0.0f, 0.0f, 0.0f, 0.7f };
    RECT inputRect = { panelLeft, panelBottom - INPUT_HEIGHT,
                       panelLeft + CHAT_WIDTH, panelBottom };
    _spriteBatch.DrawRect(inputRect, inputBg);

    // Cursor indicator
    LONG cursorX = panelLeft + 4 + static_cast<LONG>(m_inputBuffer.size()) * 6;
    if (cursorX > panelLeft + CHAT_WIDTH - 4)
      cursorX = panelLeft + CHAT_WIDTH - 4;
    XMVECTORF32 cursorColor = { 0.0f, 1.0f, 0.0f, 0.8f };
    RECT cursor = { cursorX, panelBottom - INPUT_HEIGHT + 2,
                    cursorX + 2, panelBottom - 2 };
    _spriteBatch.DrawRect(cursor, cursorColor);

    // Input bar border
    XMVECTORF32 borderColor = { 0.0f, 1.0f, 0.0f, 0.6f };
    RECT top = { panelLeft, panelBottom - INPUT_HEIGHT,
                 panelLeft + CHAT_WIDTH, panelBottom - INPUT_HEIGHT + 1 };
    _spriteBatch.DrawRect(top, borderColor);
  }

  _spriteBatch.End();
}
