// Multithreading
// Copyright (c) 2016-2025 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2025 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_VERSION_HPP
#define XYO_MULTITHREADING_VERSION_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

namespace XYO::Multithreading::Version {

	XYO_MULTITHREADING_EXPORT const char *version();
	XYO_MULTITHREADING_EXPORT const char *build();
	XYO_MULTITHREADING_EXPORT const char *versionWithBuild();
	XYO_MULTITHREADING_EXPORT const char *datetime();

};

#endif
