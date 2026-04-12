#include "pch.h"
#include "ChatUI.h"
#include "SpriteBatch.h"
#include "BitmapFont.h"

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
  BitmapFont& _font,
  ShaderVisibleHeap& _srvHeap,
  UINT _screenWidth, UINT _screenHeight)
{
  // Chat panel is rendered at bottom-center of screen.
  constexpr LONG CHAT_WIDTH  = 400;
  constexpr LONG LINE_HEIGHT = 14;
  constexpr LONG MARGIN      = 20;
  constexpr LONG INPUT_HEIGHT = 20;

  LONG panelLeft = static_cast<LONG>(_screenWidth / 2) - CHAT_WIDTH / 2;
  LONG panelBottom = static_cast<LONG>(_screenHeight) - MARGIN;

  auto visible = GetVisibleMessages();

  // ── Background bars (solid-color pass) ──────────────────────────────
  _spriteBatch.Begin(_cmdList, _cbAlloc, _screenWidth, _screenHeight);

  LONG y = panelBottom - INPUT_HEIGHT - static_cast<LONG>(visible.size()) * LINE_HEIGHT;
  for (const auto* msg : visible)
  {
    // Semi-transparent background per message line
    XMVECTORF32 bgColor = { 0.0f, 0.0f, 0.0f, 0.35f };

    // Fade with age (when not typing)
    if (!m_isTyping && msg->Age > MESSAGE_FADE_TIME * 0.7f)
    {
      float fade = 1.0f - (msg->Age - MESSAGE_FADE_TIME * 0.7f) /
        (MESSAGE_FADE_TIME * 0.3f);
      fade = (std::max)(0.0f, fade);
      bgColor.f[3] *= fade;
    }

    RECT msgBar = { panelLeft, y, panelLeft + CHAT_WIDTH, y + LINE_HEIGHT - 2 };
    _spriteBatch.DrawRect(msgBar, bgColor);
    y += LINE_HEIGHT;
  }

  // Typing input bar background
  if (m_isTyping)
  {
    XMVECTORF32 inputBg = { 0.0f, 0.0f, 0.0f, 0.7f };
    RECT inputRect = { panelLeft, panelBottom - INPUT_HEIGHT,
                       panelLeft + CHAT_WIDTH, panelBottom };
    _spriteBatch.DrawRect(inputRect, inputBg);

    // Typing input bar border
    XMVECTORF32 borderColor = { 0.0f, 1.0f, 0.0f, 0.6f };
    RECT top = { panelLeft, panelBottom - INPUT_HEIGHT,
                 panelLeft + CHAT_WIDTH, panelBottom - INPUT_HEIGHT + 1 };
    _spriteBatch.DrawRect(top, borderColor);
  }

  _spriteBatch.End();

  // ── Text rendering (font pass) ──────────────────────────────────────
  if (!_font.IsLoaded())
    return;

  _font.BeginDraw(_cmdList, _cbAlloc, _srvHeap, _screenWidth, _screenHeight);

  // Render message text
  y = panelBottom - INPUT_HEIGHT - static_cast<LONG>(visible.size()) * LINE_HEIGHT;
  for (const auto* msg : visible)
  {
    // Channel color for sender name
    XMVECTORF32 senderColor;
    switch (msg->Channel)
    {
    case ChatChannel::Zone:    senderColor = { 0.9f, 0.9f, 0.9f, 1.0f }; break;
    case ChatChannel::Party:   senderColor = { 0.4f, 0.6f, 1.0f, 1.0f }; break;
    case ChatChannel::Whisper: senderColor = { 0.9f, 0.4f, 0.9f, 1.0f }; break;
    case ChatChannel::System:  senderColor = { 1.0f, 1.0f, 0.3f, 1.0f }; break;
    default:                   senderColor = { 0.7f, 0.7f, 0.7f, 1.0f }; break;
    }

    // Fade alpha with age
    float alpha = 1.0f;
    if (!m_isTyping && msg->Age > MESSAGE_FADE_TIME * 0.7f)
    {
      alpha = 1.0f - (msg->Age - MESSAGE_FADE_TIME * 0.7f) /
        (MESSAGE_FADE_TIME * 0.3f);
      alpha = (std::max)(0.0f, alpha);
    }

    constexpr float TEXT_SCALE = 0.75f; // 12px glyphs (16 * 0.75)
    float textX = static_cast<float>(panelLeft + 4);
    float textY = static_cast<float>(y) + 1.0f;

    // Draw "Sender: "
    std::string senderPrefix = msg->Sender + ": ";
    XMVECTOR sColor = XMVectorSetW(senderColor, alpha);
    _font.DrawString(textX, textY, senderPrefix, sColor, TEXT_SCALE);

    // Draw message text in white
    float msgX = textX + _font.MeasureString(senderPrefix, TEXT_SCALE);
    XMVECTOR textColor = XMVectorSet(0.9f, 0.9f, 0.9f, alpha);
    _font.DrawString(msgX, textY, msg->Text, textColor, TEXT_SCALE);

    y += LINE_HEIGHT;
  }

  // Render typing input text
  if (m_isTyping && !m_inputBuffer.empty())
  {
    constexpr float TEXT_SCALE = 0.75f;
    float inputX = static_cast<float>(panelLeft + 4);
    float inputY = static_cast<float>(panelBottom - INPUT_HEIGHT) + 4.0f;
    XMVECTOR inputColor = XMVectorSet(0.0f, 1.0f, 0.0f, 0.9f);
    _font.DrawString(inputX, inputY, m_inputBuffer, inputColor, TEXT_SCALE);

    // Blinking cursor after text
    float cursorX = inputX + _font.MeasureString(m_inputBuffer, TEXT_SCALE);
    _font.DrawString(cursorX, inputY, "_", inputColor, TEXT_SCALE);
  }

  _font.EndDraw();
}
