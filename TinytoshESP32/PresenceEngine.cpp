#include "PresenceEngine.h"

namespace Presence {

namespace {

// Flash and Hogan's minimum-jerk profile, s(t) = 10t^3 - 15t^4 + 6t^5, in 16 steps scaled to 128.
const uint8_t MJ_LUT[17] = {0, 0, 2, 6, 13, 23, 35, 49, 64, 79, 93, 105, 115, 122, 126, 128, 128};
const int STROKE_STEPS = 16;

}  // namespace

bool Engine::leaseValid(uint32_t now) const {
  return leased_ && (uint32_t)(now - leaseAt_) < leaseMs_;
}

int16_t Engine::randomRange(int16_t lo, int16_t hi) {
  if (lo >= hi) return lo;
  return lo + port_.random16() % (uint16_t)(hi - lo + 1);
}

void Engine::apply(const Command& command, uint32_t now) {
  if (command.profile >= PROFILE_STEALTH && command.profile <= PROFILE_AGGRESSIVE) profile_ = command.profile;

  hostAsleep_ = command.host_asleep;

  if (command.paused && !paused_) {
    cancel(now);
    pendingTrigger_ = false;
  }
  paused_ = command.paused;

  if (command.lease) {
    if (!leaseValid(now)) {
      // A fresh lease waits out a short interval, so the pointer does not jump the instant
      // the display wakes or a watched app opens.
      lastActionAt_ = now;
      intervalMs_ = randomRange(10, 25) * 1000UL;
    }
    leased_ = true;
    leaseAt_ = now;
    leaseMs_ = command.lease_ms < MAX_LEASE_MS ? command.lease_ms : MAX_LEASE_MS;
  } else {
    leased_ = false;
    if (moving_ && leaseBound_) cancel(now);
  }

  // The counters of a host that has just started say nothing about what it wants now.
  if (!sessionKnown_ || command.session != session_) {
    sessionKnown_ = true;
    session_ = command.session;
    triggerSeq_ = command.trigger_seq;
    resetSeq_ = command.reset_seq;
    return;
  }
  if (command.reset_seq != resetSeq_) {
    resetSeq_ = command.reset_seq;
    cancel(now);
    pendingTrigger_ = false;
    wanderX_ = 0;
    wanderY_ = 0;
    lastActionAt_ = now;
  }
  if (command.trigger_seq != triggerSeq_) {
    triggerSeq_ = command.trigger_seq;
    pendingTrigger_ = true;
  }
}

void Engine::boundedTarget(int16_t maxStep, int16_t& dx, int16_t& dy) {
  int16_t minX = -maxStep, maxX = maxStep;
  int16_t minY = -maxStep, maxY = maxStep;

  // Past the half-way line a stroke may only head back toward the centre.
  if (wanderX_ > BOX_LIMIT_X / 2) maxX = 0;
  if (wanderX_ < -(BOX_LIMIT_X / 2)) minX = 0;
  if (wanderY_ > BOX_LIMIT_Y / 2) maxY = 0;
  if (wanderY_ < -(BOX_LIMIT_Y / 2)) minY = 0;

  // A full step taken from mid-range must still land inside the box.
  if (minX < -BOX_LIMIT_X - wanderX_) minX = -BOX_LIMIT_X - wanderX_;
  if (maxX > BOX_LIMIT_X - wanderX_) maxX = BOX_LIMIT_X - wanderX_;
  if (minY < -BOX_LIMIT_Y - wanderY_) minY = -BOX_LIMIT_Y - wanderY_;
  if (maxY > BOX_LIMIT_Y - wanderY_) maxY = BOX_LIMIT_Y - wanderY_;

  dx = randomRange(minX, maxX);
  dy = randomRange(minY, maxY);
}

void Engine::addOp(OpKind kind, int16_t arg, int8_t scroll) {
  ops_[opCount_++] = {kind, arg, scroll};
}

void Engine::plan() {
  opCount_ = 0;
  opIndex_ = 0;
  opStarted_ = false;

  if (profile_ == PROFILE_STEALTH) {
    lastGesture_ = GESTURE_STEALTH;
    addOp(OP_MOVE, randomRange(2, 8));
    plannedIntervalMs_ = randomRange(35, 90) * 1000UL;
    return;
  }

  const int roll = profile_ == PROFILE_AGGRESSIVE ? 0 : randomRange(0, 99);
  if (roll < 35) {
    lastGesture_ = GESTURE_NAV;
    addOp(OP_MOVE, randomRange(25, 60));
    plannedIntervalMs_ = randomRange(14, 32) * 1000UL;
  } else if (roll < 70) {
    lastGesture_ = GESTURE_SCROLL;
    const int8_t direction = randomRange(0, 1) == 0 ? -1 : 1;
    addOp(OP_MOVE, randomRange(4, 12), direction);
    addOp(OP_WAIT, randomRange(200, 400));
    addOp(OP_SCROLL, 0, direction);
    plannedIntervalMs_ = randomRange(24, 60) * 1000UL;
  } else if (roll < 88) {
    lastGesture_ = GESTURE_BURST;
    addOp(OP_MOVE, randomRange(10, 25));
    addOp(OP_WAIT, randomRange(150, 300));
    addOp(OP_MOVE, randomRange(10, 25));
    plannedIntervalMs_ = randomRange(8, 20) * 1000UL;
  } else {
    lastGesture_ = GESTURE_THINK;
    addOp(OP_MOVE, randomRange(1, 3));
    plannedIntervalMs_ = randomRange(90, 180) * 1000UL;
  }
}

bool Engine::emit(int8_t dx, int8_t dy, int8_t scroll) {
  if (!port_.emit(dx, dy, scroll)) return false;
  wanderX_ += dx;
  wanderY_ += dy;
  return true;
}

void Engine::cancel(uint32_t now) {
  if (!moving_) return;
  moving_ = false;
  lastActionAt_ = now;
}

void Engine::finish(uint32_t now) {
  moving_ = false;
  count_++;
  intervalMs_ = plannedIntervalMs_;
  lastActionAt_ = now;
}

// Runs the current op as far as it can go at this instant. False when the gesture was cancelled.
bool Engine::runOp(uint32_t now) {
  const Op& op = ops_[opIndex_];
  bool done = false;

  if (op.kind == OP_WAIT) {
    if (!opStarted_) {
      opStarted_ = true;
      wakeAt_ = now + op.arg;
      return true;
    }
    done = true;
  } else if (op.kind == OP_SCROLL) {
    if (!emit(0, 0, op.scroll)) return false;
    done = true;
  } else {
    if (!opStarted_) {
      opStarted_ = true;
      boundedTarget(op.arg, strokeDx_, strokeDy_);
      strokeX_ = strokeY_ = 0;
      strokeStep_ = 1;
    }
    const int step = strokeStep_;
    int8_t dx = ((strokeDx_ * (int16_t)MJ_LUT[step]) >> 7) - strokeX_;
    int8_t dy = ((strokeDy_ * (int16_t)MJ_LUT[step]) >> 7) - strokeY_;
    // Tremor. The next step aims at the same curve, so it is corrected, not accumulated.
    if (step > 3 && step < 13 && randomRange(0, 6) == 0) {
      dx += randomRange(-1, 1);
      dy += randomRange(-1, 1);
    }
    if (!emit(dx, dy, step == STROKE_STEPS / 2 ? op.scroll : 0)) return false;
    strokeX_ += dx;
    strokeY_ += dy;
    wakeAt_ = now + randomRange(12, 28);
    done = ++strokeStep_ > STROKE_STEPS;
  }

  if (done) {
    opIndex_++;
    opStarted_ = false;
  }
  return true;
}

uint32_t Engine::step(uint32_t now, bool linkUp) {
  if (linkUp && !linkUp_) lastActionAt_ = now;
  linkUp_ = linkUp;

  // Dropped outright, so a wrapped clock cannot bring an old lease back.
  if (leased_ && !leaseValid(now)) leased_ = false;

  if (!linkUp) {
    cancel(now);
    pendingTrigger_ = false;
    return IDLE_POLL_MS;
  }

  if (!moving_) {
    const bool due = !paused_ && leased_ && (uint32_t)(now - lastActionAt_) >= intervalMs_;
    if (!pendingTrigger_ && !due) return IDLE_POLL_MS;
    leaseBound_ = !pendingTrigger_;
    pendingTrigger_ = false;
    moving_ = true;
    wakeAt_ = now;
    plan();
  }

  // A gesture the lease started ends with it. A manual one needs no lease.
  if (leaseBound_ && !leased_) {
    cancel(now);
    return IDLE_POLL_MS;
  }

  while ((int32_t)(now - wakeAt_) >= 0) {
    if (opIndex_ >= opCount_) {
      finish(now);
      return IDLE_POLL_MS;
    }
    if (!runOp(now)) {
      cancel(now);
      return IDLE_POLL_MS;
    }
  }
  return wakeAt_ - now;
}

Status Engine::status(uint32_t now) const {
  Status s;
  s.paused = paused_;
  s.leased = leaseValid(now);
  s.moving = moving_;
  s.host_asleep = hostAsleep_;
  s.profile = profile_;
  s.last_gesture = lastGesture_;
  const uint32_t idle = now - lastActionAt_;
  const uint32_t left = idle < intervalMs_ ? (intervalMs_ - idle + 999) / 1000 : 0;
  s.next_sec = left > 255 ? 255 : left;
  s.pos_x = wanderX_;
  s.pos_y = wanderY_;
  s.count = count_;
  return s;
}

}  // namespace Presence
