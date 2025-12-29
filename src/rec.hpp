#pragma once

namespace rec {
	void init(void* dev);
	void cap(void* dev);
	void stop(void* dev);
	void rec_tick(void* dev);
}