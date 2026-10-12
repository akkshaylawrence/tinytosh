#ifndef PRESENCE_ENGINE_H
#define PRESENCE_ENGINE_H

// Human-looking pointer gestures: minimum-jerk strokes that wander inside a small box.
//
// The engine holds no clock, radio or random source of its own. The caller passes the time in
// and supplies a Port, which is what lets tools/presence-harness run it on a desktop.
//
// It moves by itself only while the host holds a lease, and a lease runs out unless the host
// keeps renewing it. A host that sleeps, quits or loses the link therefore leaves it still.

#include <stdint.h>

namespace Presence {

const int BOX_LIMIT_X = 75;
const int BOX_LIMIT_Y = 50;
const uint16_t MAX_LEASE_MS = 15000;

enum Profile : uint8_t {
  PROFILE_STEALTH = 1,     // drifts of 2 to 8 px, 35 to 90 s apart
  PROFILE_BALANCED = 2,    // the four-gesture mix
  PROFILE_AGGRESSIVE = 3,  // navigation strokes only
};

enum Gesture : uint8_t { GESTURE_NONE, GESTURE_NAV, GESTURE_SCROLL, GESTURE_BURST, GESTURE_THINK, GESTURE_STEALTH };

// What the host wants. It sends the whole of it with every stats push, so nothing here is an
// event: a lost or repeated message changes nothing.
struct Command {
  uint16_t session = 0;      // changes when the host app restarts
  bool lease = false;
  uint16_t lease_ms = 0;
  bool paused = false;
  uint8_t profile = PROFILE_BALANCED;
  uint16_t trigger_seq = 0;  // a new value within one session asks for one gesture now
  uint16_t reset_seq = 0;    // a new value within one session moves the origin here
};

enum Link : uint8_t { LINK_OFF, LINK_ADVERTISING, LINK_CONNECTED };

struct Status {
  Link link = LINK_OFF;
  bool paused = false;
  bool leased = false;
  bool moving = false;
  uint8_t profile = PROFILE_BALANCED;
  uint8_t last_gesture = GESTURE_NONE;
  uint8_t next_sec = 0;
  int8_t pos_x = 0;
  int8_t pos_y = 0;
  uint16_t count = 0;
};

class Port {
public:
  virtual uint16_t random16() = 0;
  // False when the report could not be handed to the host.
  virtual bool emit(int8_t dx, int8_t dy, int8_t scroll) = 0;
};

class Engine {
public:
  explicit Engine(Port& port) : port_(port) {}

  void apply(const Command& command, uint32_t now);
  // Advances the gesture in progress or starts one that is due. Returns how many milliseconds
  // may pass before the next call.
  uint32_t step(uint32_t now, bool linkUp);
  Status status(uint32_t now) const;

private:
  enum OpKind : uint8_t { OP_MOVE, OP_WAIT, OP_SCROLL };
  struct Op {
    OpKind kind;
    int16_t arg;    // MOVE: longest step per axis, WAIT: milliseconds
    int8_t scroll;  // MOVE: wheel tick sent mid-stroke, SCROLL: the tick
  };
  static const int MAX_OPS = 3;
  static const uint32_t IDLE_POLL_MS = 20;

  Port& port_;

  bool paused_ = false;
  uint8_t profile_ = PROFILE_BALANCED;
  bool leased_ = false;
  uint32_t leaseAt_ = 0;
  uint16_t leaseMs_ = 0;

  bool sessionKnown_ = false;
  uint16_t session_ = 0;
  uint16_t triggerSeq_ = 0;
  uint16_t resetSeq_ = 0;
  bool pendingTrigger_ = false;

  bool linkUp_ = false;
  int16_t wanderX_ = 0;
  int16_t wanderY_ = 0;
  uint8_t lastGesture_ = GESTURE_NONE;
  uint32_t lastActionAt_ = 0;
  uint32_t intervalMs_ = 15000;
  uint32_t plannedIntervalMs_ = 0;
  uint16_t count_ = 0;

  bool moving_ = false;
  bool leaseBound_ = false;
  Op ops_[MAX_OPS];
  int opCount_ = 0;
  int opIndex_ = 0;
  bool opStarted_ = false;
  uint32_t wakeAt_ = 0;
  int strokeStep_ = 0;
  int16_t strokeDx_ = 0, strokeDy_ = 0;
  int16_t strokeX_ = 0, strokeY_ = 0;

  bool leaseValid(uint32_t now) const;
  int16_t randomRange(int16_t lo, int16_t hi);
  void boundedTarget(int16_t maxStep, int16_t& dx, int16_t& dy);
  void plan();
  void addOp(OpKind kind, int16_t arg, int8_t scroll = 0);
  bool emit(int8_t dx, int8_t dy, int8_t scroll);
  void cancel(uint32_t now);
  void finish(uint32_t now);
  bool runOp(uint32_t now);
};

}  // namespace Presence

#endif
