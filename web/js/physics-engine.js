/**
 * JavaScript port of WheelSmoother from src/wheel_smoother.cpp.
 * 1:1 replication of the C++ physics model for browser-based simulation.
 */
// eslint-disable-next-line no-unused-vars
const PhysicsEngine = (() => {

  const SMOOTH_MODE_SPEED = 0;
  const SMOOTH_MODE_DISTANCE = 1;
  const DRAG_VIEW_ACTIVATION_ALWAYS = 1;

  class WheelSmootherJS {
    constructor(options) {
      this.options = options;
      this.smoothMode = options.smooth_mode ?? SMOOTH_MODE_SPEED;
      this.wheelTickDistance = options.wheel_tick_distance ?? 120;
      this.reverseScrollIntentWindow = options.reverse_scroll_intent_window_microseconds ?? 200000;

      // Pre-computed constants (mirrors C++ constructor, wheel_smoother.cpp:13-21)
      this.tickInterval = options.tick_interval_microseconds / 1e6;
      this.invTickInterval = 1.0 / this.tickInterval;
      this.minDeltaDecreasePerTick = options.min_deceleration * this.tickInterval * this.tickInterval;
      this.maxDeltaDecreasePerTick = options.max_deceleration * this.tickInterval * this.tickInterval;
      this.initialDelta = options.initial_speed * this.tickInterval;
      this.alpha = Math.exp(-options.damping * this.tickInterval);
      this.maxDeltaChangeLowerbound = options.max_speed_change_lowerbound * this.tickInterval;
      this.minDeltaChangeUpperbound = options.min_speed_change_upperbound * this.tickInterval;
      this.curveLowSpeed = options.min_deceleration / options.damping;
      this.curveHighSpeedSquared = options.max_deceleration * options.max_deceleration /
        (options.damping * options.damping);
      this.curveLowDistance = this.curveLowSpeed * this.curveLowSpeed / (2 * options.min_deceleration);
      this.curveHighDistance = this.curveLowDistance +
        (options.max_deceleration - options.min_deceleration) / (options.damping * options.damping);

      // Build max_delta_braking_times_ (wheel_smoother.cpp:26-38)
      this.maxDeltaBrakingTimes = [];
      if (options.use_reverse_scroll_braking) {
        let maxDelta = this.initialDelta;
        this.maxDeltaBrakingTimes.push(maxDelta);
        for (let i = 0; i < options.max_reverse_scroll_braking_times; i++) {
          maxDelta += Math.max(maxDelta * options.max_speed_change_ratio, this.minDeltaChangeUpperbound);
          this.maxDeltaBrakingTimes.push(maxDelta);
        }
      }

      this.reset();
    }

    reset() {
      // State variables (wheel_smoother.h:117-131)
      this.eventIntervals = [];
      this.lastEventTime = 0;  // in microseconds
      this.nextTickTime = 0;
      this.lastBrakeStopTime = 0;
      this.dragViewPressTime = 0;
      this.positive_ = false;
      this.horizontal_ = false;
      this.delta_ = 0;
      this.distanceRemaining_ = 0;
      this.speed_ = 0;
      this.deviation_ = 0;
      this.totalDelta_ = 0;
      this.totalDeltaH_ = 0;
      this.brakingTimes_ = 0;
      this.relX_ = 0;
      this.relY_ = 0;
      this.freeSpin_ = false;
      this.dragView_ = false;
    }

    stopScroll() {
      this.delta_ = 0;
      this.distanceRemaining_ = 0;
      this.speed_ = 0;
    }

    scrollActive() {
      return this.delta_ !== 0 || this.distanceRemaining_ > 0;
    }

    // handleFreeSpinButton (wheel_smoother.cpp:50-68)
    handleFreeSpinButton(value) {
      if (this.scrollActive() && value === 1) {
        this.freeSpin_ = true;
        return true;
      }
      if (this.freeSpin_) {
        if (value === 0) {
          this.freeSpin_ = false;
        }
        return true;
      }
      return false;
    }

    // handleDragViewButton (wheel_smoother.cpp:70-90)
    handleDragViewButton(eventTimeUs, value) {
      if (!this.dragView_ && value === 1 &&
          (this.scrollActive() || Number(this.options.drag_view_activation_mode ?? 0) === DRAG_VIEW_ACTIVATION_ALWAYS)) {
        this.dragView_ = true;
        this.dragViewPressTime = eventTimeUs;
        this.stopScroll();
        return 'handled';
      }
      if (this.dragView_) {
        if (value === 0) {
          this.dragView_ = false;
          const clickTimeoutUs = (this.options.drag_view_click_timeout_milliseconds ?? 200) * 1000;
          const pressDurationUs = eventTimeUs - this.dragViewPressTime;
          if (pressDurationUs >= 0 && pressDurationUs < clickTimeoutUs) {
            return 'replay-click';
          }
        }
        return 'handled';
      }
      return 'passthrough';
    }

    // handleRelXEvent (wheel_smoother.cpp:317-327)
    handleRelXEvent(value) {
      if (this.dragView_) {
        return { code: 'REL_HWHEEL_HI_RES', value: this.options.drag_view_speed * value };
      }
      this.relX_ = value;
      return null;
    }

    // handleRelYEvent (wheel_smoother.cpp:329-339)
    handleRelYEvent(value) {
      if (this.dragView_) {
        return { code: 'REL_WHEEL_HI_RES', value: -this.options.drag_view_speed * value };
      }
      this.relY_ = value;
      return null;
    }

    // handleEvent (wheel_smoother.cpp:92-213)
    handleEvent(eventTimeUs, positive, horizontal) {
      if (this.dragView_) return null;

      if (this.smoothMode === SMOOTH_MODE_DISTANCE) {
        return this.handleDistanceEvent(eventTimeUs, positive, horizontal);
      }

      return this.handleSpeedEvent(eventTimeUs, positive, horizontal);
    }

    handleSpeedEvent(eventTimeUs, positive, horizontal) {
      if (this.horizontal_ !== horizontal) {
        this.stopScroll();
        this.brakingTimes_ = 0;
      }

      if (this.options.use_reverse_scroll_braking) {
        if (positive === this.positive_) {
          this.brakingTimes_ = 0;
        } else {
          if (this.scrollActive()) {
            // Reverse scroll stop
            this.eventIntervals = [];
            this.lastEventTime = eventTimeUs;
            this.lastBrakeStopTime = eventTimeUs;
            this.stopScroll();
            this.brakingTimes_ = 1;
            return null;
          }

          // delta_ == 0, in braking state
          if (this.brakingTimes_) {
            if (eventTimeUs < this.lastBrakeStopTime + this.options.max_reverse_scroll_braking_microseconds &&
                this.brakingTimes_ < this.options.max_reverse_scroll_braking_times) {
              // Braking dejitter
              this.eventIntervals.push(eventTimeUs - this.lastEventTime);
              this.lastEventTime = eventTimeUs;
              this.brakingTimes_++;
              return null;
            }

            // Resume after braking
            let delta = this.initialDelta;
            if (eventTimeUs <= this.lastBrakeStopTime + this.reverseScrollIntentWindow) {
              const speed = this.smoothSpeed(eventTimeUs - this.lastEventTime);
              const rawDelta = speed * this.tickInterval;
              delta = Math.max(this.initialDelta,
                Math.min(rawDelta, this.maxDeltaBrakingTimes[this.brakingTimes_] || this.initialDelta));
            } else {
              this.eventIntervals = [];
            }
            this.lastEventTime = eventTimeUs;
            this.nextTickTime = eventTimeUs + this.options.tick_interval_microseconds;
            this.positive_ = positive;
            this.delta_ = delta;
            this.speed_ = this.delta_ * this.invTickInterval;
            this.brakingTimes_ = 0;
            const roundDelta = Math.round(this.delta_);
            this.deviation_ = this.delta_ - roundDelta;
            this.totalDelta_ = roundDelta;
            return { emittedDelta: roundDelta, totalDelta: this.totalDelta_ };
          }
        }
      } else if (this.scrollActive() && positive !== this.positive_) {
        this.stopScroll();
        this.brakingTimes_ = 0;
      }

      // Initial startup (delta_ == 0)
      if (this.delta_ === 0) {
        this.eventIntervals = [];
        this.lastEventTime = eventTimeUs;
        this.nextTickTime = eventTimeUs + this.options.tick_interval_microseconds;
        this.positive_ = positive;
        this.horizontal_ = horizontal;
        this.delta_ = this.initialDelta;
        this.speed_ = this.delta_ * this.invTickInterval;
        const roundDelta = Math.round(this.delta_);
        this.deviation_ = this.delta_ - roundDelta;
        this.totalDelta_ = roundDelta;
        return { emittedDelta: roundDelta, totalDelta: this.totalDelta_ };
      }

      // Speed update for ongoing scroll
      const speed = this.smoothSpeed(eventTimeUs - this.lastEventTime);
      const minDeltaChange = Math.min(this.delta_ * this.options.min_speed_change_ratio, this.maxDeltaChangeLowerbound);
      const maxDeltaChange = Math.max(this.delta_ * this.options.max_speed_change_ratio, this.minDeltaChangeUpperbound);
      const rawDelta = Math.max(this.delta_ + minDeltaChange, Math.min(speed * this.tickInterval, this.delta_ + maxDeltaChange));

      this.lastEventTime = eventTimeUs;
      this.delta_ = rawDelta < this.initialDelta ? this.initialDelta : rawDelta;
      this.speed_ = this.delta_ * this.invTickInterval;
      return null;
    }

    handleDistanceEvent(eventTimeUs, positive, horizontal) {
      if (this.horizontal_ !== horizontal) {
        this.stopScroll();
        this.brakingTimes_ = 0;
      }

      const startDistanceScroll = (distanceTicks) => {
        this.lastEventTime = eventTimeUs;
        this.nextTickTime = eventTimeUs + this.options.tick_interval_microseconds;

        this.positive_ = positive;
        this.horizontal_ = horizontal;
        this.deviation_ = 0;
        this.totalDelta_ = 0;

        const initialDistance = this.wheelTickDistance * distanceTicks;
        this.distanceRemaining_ = initialDistance;
        this.speed_ = this.speedForDistance(this.distanceRemaining_);

        let desiredDelta = this.speed_ * this.tickInterval;
        if (this.freeSpin_) {
          const roundDelta = Math.round(desiredDelta);
          if (roundDelta <= 0) {
            return null;
          }

          this.totalDelta_ = roundDelta;
          return { emittedDelta: roundDelta, totalDelta: this.totalDelta_ };
        }

        desiredDelta = Math.min(desiredDelta, this.distanceRemaining_);
        const roundDelta = Math.min(Math.round(desiredDelta), initialDistance);

        this.deviation_ = desiredDelta - roundDelta;
        this.distanceRemaining_ -= desiredDelta;
        if (!this.scrollActive()) {
          this.speed_ = 0;
        }

        if (roundDelta <= 0) {
          return null;
        }

        this.totalDelta_ = roundDelta;
        return { emittedDelta: roundDelta, totalDelta: this.totalDelta_ };
      };

      if (this.options.use_reverse_scroll_braking) {
        if (positive === this.positive_) {
          this.brakingTimes_ = 0;
        } else {
          if (this.scrollActive()) {
            this.eventIntervals = [];
            this.lastEventTime = eventTimeUs;
            this.lastBrakeStopTime = eventTimeUs;
            this.stopScroll();
            this.brakingTimes_ = 1;
            return null;
          }

          if (this.brakingTimes_) {
            if (eventTimeUs < this.lastBrakeStopTime + this.options.max_reverse_scroll_braking_microseconds &&
                this.brakingTimes_ < this.options.max_reverse_scroll_braking_times) {
              this.lastEventTime = eventTimeUs;
              this.brakingTimes_++;
              return null;
            }

            let distanceTicks = 1;
            if (eventTimeUs <= this.lastBrakeStopTime + this.reverseScrollIntentWindow) {
              distanceTicks = this.brakingTimes_ + 1;
            }
            this.brakingTimes_ = 0;
            return startDistanceScroll(distanceTicks);
          }
        }
      } else if (this.scrollActive() && positive !== this.positive_) {
        this.stopScroll();
      }

      if (!this.scrollActive()) {
        return startDistanceScroll(1);
      }

      this.distanceRemaining_ += this.wheelTickDistance;
      this.speed_ = this.speedForDistance(this.distanceRemaining_);

      this.lastEventTime = eventTimeUs;
      return null;
    }

    // tick (wheel_smoother.cpp:215-274)
    tick() {
      if (this.smoothMode === SMOOTH_MODE_DISTANCE) {
        return this.tickDistance();
      }

      return this.tickSpeed();
    }

    tickSpeed() {
      if (this.delta_ === 0) return null;

      if (!this.freeSpin_) {
        const maxDelta = this.delta_ - this.minDeltaDecreasePerTick;
        const minDelta = this.delta_ - this.maxDeltaDecreasePerTick;
        this.delta_ *= this.alpha;

        if (this.delta_ > maxDelta) this.delta_ = maxDelta;
        if (this.delta_ < minDelta) this.delta_ = minDelta;

        if (this.delta_ < 0) {
          this.stopScroll();
          return null;
        }

        this.speed_ = this.delta_ * this.invTickInterval;
      }

      this.nextTickTime += this.options.tick_interval_microseconds;

      const roundDelta = Math.round(this.delta_ + this.deviation_);
      this.deviation_ = this.delta_ + this.deviation_ - roundDelta;

      if (roundDelta === 0) return null;

      this.totalDelta_ += roundDelta;
      return { emittedDelta: roundDelta, totalDelta: this.totalDelta_ };
    }

    tickDistance() {
      if (!this.scrollActive()) {
        this.stopScroll();
        return null;
      }

      this.nextTickTime += this.options.tick_interval_microseconds;

      this.speed_ = this.speedForDistance(this.distanceRemaining_);
      let desiredDelta = this.speed_ * this.tickInterval;
      if (this.freeSpin_) {
        const roundDelta = Math.round(desiredDelta);
        if (roundDelta <= 0) {
          return null;
        }

        this.totalDelta_ += roundDelta;
        return { emittedDelta: roundDelta, totalDelta: this.totalDelta_ };
      }

      desiredDelta = Math.min(desiredDelta, this.distanceRemaining_);

      const remainingDelta = Math.round(this.distanceRemaining_ + this.deviation_);
      const roundDelta = Math.max(
        0,
        Math.min(Math.round(desiredDelta + this.deviation_), remainingDelta)
      );

      this.deviation_ += desiredDelta - roundDelta;
      this.distanceRemaining_ -= desiredDelta;
      if (!this.scrollActive()) {
        this.speed_ = 0;
      }

      if (roundDelta <= 0) {
        if (this.distanceRemaining_ <= 0) {
          this.stopScroll();
        }
        return null;
      }

      this.totalDelta_ += roundDelta;
      return { emittedDelta: roundDelta, totalDelta: this.totalDelta_ };
    }

    speedForDistance(distance) {
      if (distance <= this.curveLowDistance) {
        return Math.sqrt(2 * this.options.min_deceleration * distance);
      }

      if (distance <= this.curveHighDistance) {
        return this.curveLowSpeed + this.options.damping * (distance - this.curveLowDistance);
      }

      return Math.sqrt(this.curveHighSpeedSquared +
        2 * this.options.max_deceleration * (distance - this.curveHighDistance));
    }

    // smoothSpeed (wheel_smoother.cpp:377-407)
    smoothSpeed(eventIntervalUs) {
      const speedSmoothWindow = this.options.speed_smooth_window_microseconds;
      let numEventIntervals = 1;
      let duration = eventIntervalUs;

      if (eventIntervalUs > speedSmoothWindow) {
        this.eventIntervals = [];
      } else {
        for (let i = this.eventIntervals.length - 1; i >= 0; i--) {
          if (this.eventIntervals[i] + duration > speedSmoothWindow) {
            numEventIntervals += (speedSmoothWindow - duration) / this.eventIntervals[i];
            duration = speedSmoothWindow;
            break;
          }
          duration += this.eventIntervals[i];
          numEventIntervals += 1;
        }
        this.eventIntervals.push(eventIntervalUs);
      }

      return this.options.speed_factor * numEventIntervals / (duration / 1e6);
    }
  }

  /**
   * Run a full simulation given a scenario.
   * Returns an array of data points for charting.
   */
  function simulate(options, scenario, featureOptions) {
    const smoother = new WheelSmootherJS(options);
    const tickInterval = options.tick_interval_microseconds;

    // Build unified event timeline
    const events = [];

    // Convert scenario scroll events
    for (const evt of scenario.events) {
      events.push({ timeUs: evt.timeMs * 1000, type: 'scroll', positive: evt.positive !== false });
    }

    // Add typed feature events
    if (featureOptions && featureOptions.events) {
      for (const evt of featureOptions.events) {
        events.push({ ...evt });
      }
    }

    // Backward compat: convert freeSpinStartTimeUs to a typed event
    if (featureOptions && featureOptions.freeSpinStartTimeUs != null && !featureOptions.events) {
      events.push({ timeUs: featureOptions.freeSpinStartTimeUs, type: 'free-spin', value: 1 });
    }

    // Sort events by time
    events.sort((a, b) => a.timeUs - b.timeUs);

    // Simulation: mirrors the C++ event loop (poll-based)
    const timeline = [];
    let eventIdx = 0;
    let maxTimeUs = (scenario.maxDurationMs || 3000) * 1000;
    const startTimeUs = events.length > 0 ? events[0].timeUs : 0;
    let currentTime = startTimeUs;
    let maxIter = 500000; // safety limit

    while (currentTime - startTimeUs < maxTimeUs && maxIter-- > 0) {
      // Process all events at or before currentTime
      while (eventIdx < events.length && events[eventIdx].timeUs <= currentTime) {
        const evt = events[eventIdx];
        switch (evt.type) {
          case 'scroll': {
            const result = smoother.handleEvent(evt.timeUs, evt.positive, false);
            if (result) {
              timeline.push({
                timeMs: (evt.timeUs - startTimeUs) / 1000,
                delta: smoother.delta_,
                speed: smoother.speed_,
                emittedDelta: result.emittedDelta,
                totalDelta: result.totalDelta,
                totalDeltaH: smoother.totalDeltaH_,
              });
            }
            break;
          }
          case 'free-spin':
            smoother.handleFreeSpinButton(evt.value);
            break;
          case 'drag-view':
            smoother.handleDragViewButton(evt.timeUs, evt.value);
            timeline.push({
              timeMs: (currentTime - startTimeUs) / 1000,
              delta: smoother.delta_,
              speed: smoother.speed_,
              emittedDelta: 0,
              totalDelta: smoother.totalDelta_,
              totalDeltaH: smoother.totalDeltaH_,
            });
            break;
          case 'rel-x': {
            const res = smoother.handleRelXEvent(evt.value);
            if (res) {
              const roundDelta = Math.round(res.value);
              if (res.code === 'REL_HWHEEL_HI_RES') {
                smoother.totalDeltaH_ += roundDelta;
                timeline.push({
                  timeMs: (currentTime - startTimeUs) / 1000,
                  delta: 0,
                  speed: 0,
                  emittedDelta: roundDelta,
                  emittedH: true,
                  totalDelta: smoother.totalDelta_,
                  totalDeltaH: smoother.totalDeltaH_,
                });
              }
            }
            break;
          }
          case 'rel-y': {
            const res = smoother.handleRelYEvent(evt.value);
            if (res) {
              const roundDelta = Math.round(res.value);
              if (res.code === 'REL_WHEEL_HI_RES') {
                smoother.totalDelta_ += roundDelta;
                timeline.push({
                  timeMs: (currentTime - startTimeUs) / 1000,
                  delta: 0,
                  speed: 0,
                  emittedDelta: roundDelta,
                  emittedV: true,
                  totalDelta: smoother.totalDelta_,
                  totalDeltaH: smoother.totalDeltaH_,
                });
              }
            }
            break;
          }
        }
        eventIdx++;
      }

      // Process tick if it's time
      if (smoother.scrollActive() && currentTime >= smoother.nextTickTime) {
        const result = smoother.tick();
        if (result) {
          timeline.push({
            timeMs: (currentTime - startTimeUs) / 1000,
            delta: smoother.delta_,
            speed: smoother.speed_,
            emittedDelta: result.emittedDelta,
            totalDelta: result.totalDelta,
            totalDeltaH: smoother.totalDeltaH_,
          });
        } else if (!smoother.scrollActive()) {
          break;
        }
      }

      // Determine next wake-up time: min of next event time and next tick time
      const nextEventTime = eventIdx < events.length ? events[eventIdx].timeUs : Infinity;
      const nextTickTime = smoother.scrollActive() ? smoother.nextTickTime : Infinity;

      if (!smoother.scrollActive() && eventIdx >= events.length) break;

      const nextTime = Math.min(nextEventTime, nextTickTime);
      if (nextTime === Infinity || nextTime <= currentTime) {
        currentTime += tickInterval; // fallback advance
      } else {
        currentTime = nextTime;
      }
    }

    const truncated = smoother.scrollActive();
    return { timeline, truncated };
  }

  return { simulate };
})();
