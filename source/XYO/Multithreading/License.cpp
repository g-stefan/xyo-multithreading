// Multithreading
// Copyright (c) 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/License.hpp>

namespace XYO::Multithreading::License {

	const char *license() {
		return XYO::ManagedMemory::License::license();
	};

	const char *shortLicense() {
		return XYO::ManagedMemory::License::shortLicense();
	};

};
