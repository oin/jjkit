#pragma once
#include <cstdint>
#include <cstddef>

constexpr const float jjpi = 3.14159265359f;

template <typename T>
constexpr T jjabs(T v) {
	return v < T(0)? -v : v;
}

constexpr float jjlpfilter(float x, float xprev, float alpha) {
	return alpha * x + (1 - alpha) * xprev;
}

/**
 * A simple and efficient filter for smoothing interactive signals (https://gery.casiez.net/1euro/)
 *
 *
 * @note To minimize jitter and lag when tracking human motion, the two parameters (fcmin and beta) can be set using a simple two-step procedure. First beta is set to 0 and fcmin (mincutoff) to a reasonable middle-ground value such as 1 Hz. Then the body part is held steady or moved at a very low speed while fcmin is adjusted to remove jitter and preserve an acceptable lag during these slow movements (decreasing fcmin reduces jitter but increases lag, fcmin must be > 0). Next, the body part is moved quickly in different directions while beta is increased with a focus on minimizing lag. First find the right order of magnitude to tune beta, which depends on the kind of data you manipulate and their units: do not hesitate to start with values like 0.001 or 0.0001. You can first multiply and divide beta by factor 10 until you notice an effect on latency when moving quickly. Note that parameters fcmin and beta have clear conceptual relationships: if high speed lag is a problem, increase beta; if slow speed jitter is a problem, decrease fcmin.
 */
class jj1efilter {
public:
	/**
	 * The minimum cutoff frequency, in Hz.
	 * If slow speed jitter is a problem, decrease this.
	 */
	float fcmin = 1.f;
	/**
	 * The cutoff slope.
	 * If high speed lag is a problem, increase this.
	 */
	float beta = 0.f;

	/**
	 * @return The given value, filtered.
	 * @param x The new value to filter.
	 * @param t The current time, in milliseconds.
	 */
	float process(float x, uint32_t t) {
		if(!initialized) {
			initialized = true;
			dxfilt = 0.f;
			xfilt = x;
			last_time = t;
			return x;
		}
		if(t == last_time) {
			return xfilt;
		}

		const float dt = (t - last_time) * 0.001f;
		const float dx = (x - xfilt) / dt;
		last_time = t;

		dxfilt = jjlpfilter(dx, dxfilt, alpha(1.f, dt)); // dcutoff=1Hz
		const float fc = fcmin + beta * jjabs(dxfilt);
		xfilt = jjlpfilter(x, xfilt, alpha(fc, dt));
		return xfilt;
	}
private:
	static constexpr float alpha(float cutoff, float dt) {
		const float r = 2.f * jjpi * cutoff * dt;
		return r / (r + 1.f);
	}
	float xfilt = 0.f;
	float dxfilt = 0.f;
	uint32_t last_time = 0;
	bool initialized = false;
};
