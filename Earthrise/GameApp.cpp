#include "pch.h"
#include "GameApp.h"

GameApp* g_app = nullptr;

namespace
{
  const std::vector<std::wstring> g_supportedLanguages =
  {
 L"ENG", L"FRA", L"ITA", L"DEU", L"ESP", L"RUS", L"POL", L"POR", L"JPN", L"KOR", L"CHS", L"CHT"
  };
}

GameApp::GameApp()
{
  g_app = this;


  // Determine default language if possible
  auto isoLang = Windows::System::UserProfile::GlobalizationPreferences::Languages().GetAt(0);
  auto lang = std::wstring(Windows::Globalization::Language(isoLang).AbbreviatedName());
  auto country = Windows::Globalization::GeographicRegion().DisplayName();
  m_isoLang = to_string(isoLang);
}

GameApp::~GameApp()
{
}
