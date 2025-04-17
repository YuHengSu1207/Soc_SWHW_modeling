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

#include <chrono>

#include "ACALSim.hh"
#include "SOC.hh"
int main(int argc, char** argv) {
	acalsim::top = std::make_shared<SOC>("soc/configs.json");
	acalsim::top->init(argc, argv);
	auto start = std::chrono::high_resolution_clock::now();
	acalsim::top->run();
	auto stop = std::chrono::high_resolution_clock::now();

	auto diff = duration_cast<std::chrono::nanoseconds>(stop - start);

	acalsim::LogOStream(acalsim::LoggingSeverity::L_INFO, __FILE__, __LINE__, "Timer")
	    << "Time: " << (double)diff.count() / pow(10, 9) << " seconds.";

	acalsim::top->finish();
	return 0;
}
