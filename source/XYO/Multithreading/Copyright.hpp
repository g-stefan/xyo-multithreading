// Multithreading
// Copyright (c) 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_COPYRIGHT_HPP
#define XYO_MULTITHREADING_COPYRIGHT_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

namespace XYO::Multithreading::Copyright {
	XYO_MULTITHREADING_EXPORT const char *copyright();
	XYO_MULTITHREADING_EXPORT const char *publisher();
	XYO_MULTITHREADING_EXPORT const char *company();
	XYO_MULTITHREADING_EXPORT const char *contact();
};

#endif
