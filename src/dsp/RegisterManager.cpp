#include "RegisterManager.h"
#include "../utils/Debug.h"

using namespace ymulatorsynth;

void RegisterManager::writeRegister(int address, uint8_t data)
{
    CS_ASSERT(address >= 0 && address < REGISTER_COUNT);
    
    // Update cache with new value
    currentRegisters[static_cast<size_t>(address)] = data;
    
    CS_DBG("RegisterManager::writeRegister - addr: 0x" + 
           juce::String::toHexString(address) + ", data: 0x" + 
           juce::String::toHexString(data));
}

uint8_t RegisterManager::readCurrentRegister(int address) const
{
    CS_ASSERT(address >= 0 && address < REGISTER_COUNT);
    
    return currentRegisters[static_cast<size_t>(address)];
}

void RegisterManager::updateRegisterCache(uint8_t address, uint8_t value)
{
    CS_ASSERT(address < REGISTER_COUNT);
    
    currentRegisters[address] = value;
    
    CS_DBG("RegisterManager::updateRegisterCache - addr: 0x" + 
           juce::String::toHexString(address) + ", value: 0x" + 
           juce::String::toHexString(value));
}

void RegisterManager::reset()
{
    // Clear all register values
    currentRegisters.fill(0);
    
    CS_DBG("RegisterManager::reset - All registers cleared");
}