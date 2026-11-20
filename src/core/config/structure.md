# config (src/core/config/)

## Purpose
Configuration handling: the settings model and the merge rules.

## Files
- **SettingsManager.cpp**: Implementation of the SettingsManager component.
- **SettingsManager.h**: Header for the SettingsManager component.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **SettingsManager& SettingsManager::getInstance()**
- **std::string SettingsManager::getSettingsJson()**
- **SettingsManager::updateSetting(const std::string&, const std::string&)**
- **SettingsManager::load()**
- **SettingsManager::saveAsync()**
- **SettingsManager::~SettingsManager()**
