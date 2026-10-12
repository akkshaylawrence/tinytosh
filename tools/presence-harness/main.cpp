#include <cstdio>
#include <cstdlib>
#include <vector>

#include "PresenceEngine.h"

using namespace Presence;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (ok) return;
  failures++;
  printf("FAIL: %s\n", what);
}

struct Report { int8_t dx, dy, scroll; uint32_t at; };

struct Rig : Port {
  uint32_t seed;
  uint32_t now = 1000;
  bool linkUp = true;
  bool deliver = true;
  std::vector<Report> reports;
  Engine engine{*this};
  Command command;

  explicit Rig(uint32_t s = 1) : seed(s) { command.session = 7; }

  uint16_t random16() override {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 16;
  }
  bool emit(int8_t dx, int8_t dy, int8_t scroll) override {
    if (!deliver) return false;
    reports.push_back({dx, dy, scroll, now});
    return true;
  }

  void send() { engine.apply(command, now); }
  void lease(bool on, uint16_t ms = 5000) {
    command.lease = on;
    command.lease_ms = ms;
    send();
  }
  // With renew, the command is re-sent once a second the way the host does.
  void run(uint32_t ms, bool renew = false) {
    const uint32_t end = now + ms;
    uint32_t nextRenew = now;
    while (now < end) {
      if (renew && now >= nextRenew) {
        send();
        nextRenew = now + 1000;
      }
      uint32_t wait = engine.step(now, linkUp);
      now += wait ? wait : 1;
    }
  }
  Status status() { return engine.status(now); }
  void trigger() {
    command.trigger_seq++;
    send();
  }
};

void staysStillWithoutLease() {
  Rig r;
  r.send();
  r.run(600000);
  check(r.reports.empty(), "no lease, no motion");
}

void movesUnderLeaseAndStaysInBox() {
  for (uint8_t profile = PROFILE_STEALTH; profile <= PROFILE_AGGRESSIVE; profile++) {
    Rig r(profile * 97);
    r.command.profile = profile;
    r.lease(true);
    int x = 0, y = 0;
    size_t seen = 0;
    bool inside = true;
    for (int minute = 0; minute < 240; minute++) {
      r.run(60000, true);
      for (; seen < r.reports.size(); seen++) {
        x += r.reports[seen].dx;
        y += r.reports[seen].dy;
        // Tremor may overshoot a step by one pixel before the next step corrects it.
        if (abs(x) > BOX_LIMIT_X + 1 || abs(y) > BOX_LIMIT_Y + 1) inside = false;
      }
    }
    check(inside, "wander stays inside the box");
    check(r.status().count > 20, "gestures complete under a renewed lease");
    check(r.status().pos_x == x && r.status().pos_y == y, "reported offset equals the sum of reports");
  }
}

void freshLeaseWaitsBeforeMoving() {
  Rig r;
  r.lease(true);
  r.run(9900, true);
  check(r.reports.empty(), "nothing in the first ten seconds of a lease");
  r.run(16000, true);
  check(!r.reports.empty(), "first gesture within 25 seconds of a lease");
}

void leaseExpiryStopsAStroke() {
  Rig r;
  r.command.profile = PROFILE_AGGRESSIVE;
  r.lease(true);
  while (r.reports.empty()) {
    r.send();
    r.run(5);
  }
  check(r.status().moving, "stroke under way");
  r.lease(true, 20);
  const size_t before = r.reports.size();
  r.run(1000);
  check(!r.status().moving && !r.status().leased, "lease ran out");
  check(r.reports.size() <= before + 2, "stroke stopped within two steps of the lease ending");
  check(r.status().count == 0, "a cancelled gesture is not counted");
}

void revokeStopsOnlyLeaseBoundStrokes() {
  Rig r;
  r.send();
  r.trigger();
  r.run(60);
  r.lease(false);
  r.run(3000);
  check(r.status().count == 1, "a revoke leaves a manual gesture alone");
}

void leaseIsCapped() {
  Rig r;
  r.lease(true, 60000);
  r.run(MAX_LEASE_MS - 100);
  check(r.status().leased, "lease still live just under the cap");
  r.run(200);
  check(!r.status().leased, "lease ends at the cap whatever the host asked for");
}

void pauseStopsAStroke() {
  Rig r;
  r.send();
  r.trigger();
  r.run(60);
  check(r.status().moving, "manual gesture started");
  const size_t before = r.reports.size();
  r.command.paused = true;
  r.send();
  r.run(2000);
  check(r.reports.size() == before, "pause stops the stroke at once");
  check(r.status().paused && !r.status().moving, "paused and idle");

  r.lease(true);
  r.run(300000, true);
  check(r.reports.size() == before, "a paused device ignores its lease");
}

void triggerNeedsNoLeaseAndFiresOnce() {
  Rig r;
  r.send();
  r.trigger();
  r.run(5000);
  check(r.status().count == 1, "trigger runs one gesture without a lease");
  const size_t after = r.reports.size();
  r.run(30000, true);
  check(r.reports.size() == after && r.status().count == 1, "a repeated trigger_seq fires once");

  r.command.paused = true;
  r.send();
  r.trigger();
  r.run(5000, true);
  check(r.status().count == 2, "trigger works while paused, and repeated pauses do not cancel it");
}

void newSessionAdoptsCounters() {
  Rig r;
  r.command.trigger_seq = 40;
  r.command.reset_seq = 9;
  r.send();
  r.run(5000);
  check(r.reports.empty(), "counters of the first message are adopted, not acted on");

  r.command.session = 8;
  r.command.trigger_seq = 0;
  r.send();
  r.run(5000);
  check(r.reports.empty(), "a restarted host does not trigger a gesture");
  r.trigger();
  r.run(5000);
  check(r.status().count == 1, "the new session's trigger works");
}

void resetZeroesTheOffset() {
  Rig r;
  r.command.profile = PROFILE_AGGRESSIVE;
  r.send();
  for (int i = 0; i < 5; i++) {
    r.trigger();
    r.run(3000);
  }
  check(r.status().pos_x != 0 || r.status().pos_y != 0, "offset moved off the origin");
  r.trigger();
  r.run(60);
  r.command.reset_seq++;
  r.send();
  check(r.status().pos_x == 0 && r.status().pos_y == 0 && !r.status().moving, "reset cancels and zeroes");
  const size_t before = r.reports.size();
  r.run(5000);
  check(r.reports.size() == before, "nothing trails a reset");
}

void lostLinkStopsMotion() {
  Rig r;
  r.send();
  r.trigger();
  r.run(60);
  r.linkUp = false;
  const size_t before = r.reports.size();
  r.run(2000);
  check(r.reports.size() == before && !r.status().moving, "link loss cancels the gesture");

  r.linkUp = true;
  r.deliver = false;
  r.trigger();
  r.run(2000);
  check(r.status().count == 0 && !r.status().moving, "an undelivered report cancels the gesture");
}

void profilesShapeTheGestures() {
  Rig stealth(5), aggressive(6), balanced(7);
  stealth.command.profile = PROFILE_STEALTH;
  aggressive.command.profile = PROFILE_AGGRESSIVE;
  stealth.send();
  aggressive.send();
  balanced.send();
  bool seen[6] = {};
  for (int i = 0; i < 200; i++) {
    stealth.trigger();
    stealth.run(3000);
    check(stealth.status().last_gesture == GESTURE_STEALTH, "stealth only drifts");
    aggressive.trigger();
    aggressive.run(3000);
    check(aggressive.status().last_gesture == GESTURE_NAV, "aggressive only navigates");
    balanced.trigger();
    balanced.run(3000);
    seen[balanced.status().last_gesture] = true;
  }
  check(seen[GESTURE_NAV] && seen[GESTURE_SCROLL] && seen[GESTURE_BURST] && seen[GESTURE_THINK], "balanced uses all four gestures");
  for (const Report& rep : stealth.reports) check(abs(rep.dx) <= 3 && abs(rep.dy) <= 3 && rep.scroll == 0, "stealth steps are tiny");
  bool scrolled = false;
  for (const Report& rep : balanced.reports) scrolled |= rep.scroll != 0;
  check(scrolled, "balanced scrolls the wheel");
}

void strokesStartAndEndAtRest() {
  Rig r(3);
  r.command.profile = PROFILE_AGGRESSIVE;
  r.send();
  r.trigger();
  r.run(3000);
  check(r.reports.size() == 16, "a navigation stroke is sixteen reports");
  if (r.reports.size() != 16) return;
  auto size = [&](int i) { return abs(r.reports[i].dx) + abs(r.reports[i].dy); };
  check(size(0) + size(15) <= 2, "a stroke starts and ends at rest");
  for (int i = 1; i < 16; i++) {
    const uint32_t gap = r.reports[i].at - r.reports[i - 1].at;
    check(gap >= 12 && gap <= 28, "steps are 12 to 28 ms apart");
  }
}

}  // namespace

int main() {
  staysStillWithoutLease();
  movesUnderLeaseAndStaysInBox();
  freshLeaseWaitsBeforeMoving();
  leaseExpiryStopsAStroke();
  revokeStopsOnlyLeaseBoundStrokes();
  leaseIsCapped();
  pauseStopsAStroke();
  triggerNeedsNoLeaseAndFiresOnce();
  newSessionAdoptsCounters();
  resetZeroesTheOffset();
  lostLinkStopsMotion();
  profilesShapeTheGestures();
  strokesStartAndEndAtRest();
  if (failures) {
    printf("%d presence checks failed.\n", failures);
    return 1;
  }
  printf("Presence engine checks passed.\n");
  return 0;
}
