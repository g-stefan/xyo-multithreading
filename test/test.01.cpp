// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/MultiThreading.hpp>

using namespace XYO::Multithreading;

int mandelbrot(double x0, double y0) {
	double x = 0;
	double y = 0;
	double xTemp;
	int iteration = 0;
	int maxIteration = 1000;
	while ((x * x + y * y < 2 * 2) && (iteration < maxIteration)) {
		xTemp = x * x - y * y + x0;
		y = 2 * x * y + y0;
		x = xTemp;
		++iteration;
	};
	return iteration;
};

const int yLines = 10;
const int xLines = 40;
char screen[yLines * 2][xLines * 2 + 1];
char map[182];

struct ScreenToTop {
		bool dummy;
};

void screenRefresh(void *parameter = nullptr) {
	int y;
	for (y = -yLines; y < yLines; ++y) {
		printf("%s\r\n", screen[y + yLines]);
	};
	if (parameter) {
		printf("\x1B[%dF\r", yLines * 2);
	};
};

struct Parameter : public Object {
		double y;
};

TPointer<Parameter> parameterTransfer(Parameter &parameter) {
	TPointer<Parameter> retV;
	retV.newMemory();
	retV->y = parameter.y;
	return retV;
};

TPointer<Parameter> processProcedure(Parameter *parameter, TAtomic<bool> &requestToTerminate) {
	double y = parameter->y;
	double x;

	for (x = -xLines; x < xLines; ++x) {
		screen[(int)y + yLines][(int)x + xLines] = map[((mandelbrot((x - 15) / 25, y / 15) * (sizeof(map) - 1)) / 1000)];
		Thread::sleep(50 + ((50 * rand()) / RAND_MAX));
	};
	return parameter;
};

typedef TWorkerQueue<Parameter, Parameter, parameterTransfer, parameterTransfer, processProcedure> ProcessQueue;

void test() {
	double y;
	int z;

	map[0] = '.';
	for (z = 1; z < 181; ++z) {
		map[z] = ':';
	};
	map[181] = '#';

	memset(&screen[0][0], 0, sizeof(screen));

	WorkerQueue queue;
	TPointer<Thread> refresh;
	TPointer<Parameter> parameter;

	// queue.setNumberOfThreads(4);

	for (y = -yLines; y < yLines; ++y) {
		parameter.newMemory();
		parameter->y = y;
		ProcessQueue::add(queue, parameter);
	};

	printf("Queue length: %zd\r\n", queue.length());
	printf("Queue threads: %d\r\n", queue.getNumberOfThreads());

	TAtomic<bool> endRefresh = false;
	ScreenToTop screenToTop;
	refresh = Thread::setInterval(endRefresh, 100, screenRefresh, &screenToTop);

	queue.process();

	endRefresh = true;
	refresh->join();
	screenRefresh();

	printf("Done.\r\n");
};

int main(int cmdN, char *cmdS[]) {
	try {

		test();

		return 0;

	} catch (const std::exception &e) {
		printf("* Error: %s\n", e.what());
	} catch (...) {
		printf("* Error: Unknown\n");
	};

	return 1;
};
