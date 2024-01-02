// Multithreading
// Copyright (c) 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Copyright.hpp>
#include <XYO/Multithreading/Copyright.rh>

namespace XYO::Multithreading::Copyright {

	static const char *copyright_ = XYO_MULTITHREADING_COPYRIGHT;
	static const char *publisher_ = XYO_MULTITHREADING_PUBLISHER;
	static const char *company_ = XYO_MULTITHREADING_COMPANY;
	static const char *contact_ = XYO_MULTITHREADING_CONTACT;

	const char *copyright() {
		return copyright_;
	};

	const char *publisher() {
		return publisher_;
	};

	const char *company() {
		return company_;
	};

	const char *contact() {
		return contact_;
	};

};
