/*
 * Copyright 2023-2024 Playlab/ACAL
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "BaseMemory.hh"

#include <cstdlib>
#include <cstring>

#include "ACALSim.hh"

BaseMemory::BaseMemory(size_t _size) : size(_size) { this->mem = std::calloc(this->size, 1); }

BaseMemory::~BaseMemory() { std::free(this->mem); }

size_t BaseMemory::getSize() const { return this->size; }

void* BaseMemory::readData(uint32_t _addr, size_t _size, bool _deep_copy) const {
	ASSERT_MSG(_addr + _size <= this->getSize(), "The memory region to be accessed is out of range.");
	if (_deep_copy) {
		size_t size = sizeof(uint8_t) * _size;
		void*  data = std::malloc(size);
		std::memcpy(data, (uint8_t*)this->mem + _addr, size);
		return data;
	} else {
		return (uint8_t*)this->mem + _addr;
	}
}

void BaseMemory::writeData(void* _data, uint32_t _addr, size_t _size) {
	ASSERT_MSG(_data, "The received argument `_data` is a nullptr.");
	ASSERT_MSG(_addr + _size <= this->getSize(), "The memory region to be accessed is out of range.");

	std::memcpy((uint8_t*)this->mem + _addr, _data, sizeof(uint8_t) * _size);
}
