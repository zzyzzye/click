#pragma once

#include <memory>

class ClickBackend;
class HotkeyService;

std::unique_ptr<ClickBackend> createClickBackend();
std::unique_ptr<HotkeyService> createHotkeyService();

