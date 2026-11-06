//! The sleep timers: the player stops by itself after a while.
//! Ported from `src/system/playback/sleeptimer.c`.
//!
//! One timer follows the listener through every kind of playback: local music,
//! streaming services, radio, podcasts and audiobooks. Switching sources does
//! not silently replace the countdown.
//!
//! Each countdown is measured against the monotonic clock rather than by adding
//! up ticks, so a slow tick with the screen dark loses nothing: the tick only
//! asks what time it is now.
//!
//! The clock is passed in rather than read here, which is what lets the
//! arithmetic of a stretch be tested without waiting a minute for one.

use std::sync::{Mutex, OnceLock};
use std::time::Instant;

/// A whole day, which is the most the two wheels can be set to and well past
/// anything anybody means by a sleep timer. The clamp is here so a hand-edited
/// config cannot arm something that never expires.
const MAX_MINUTES: i32 = 23 * 60 + 59;

/// The number of kinds. Every source shares the one countdown; the names in the
/// header all mean this one.
pub const COUNT: usize = 1;

pub const MUSIC: i32 = 0;

/// The name the log line uses.
const NAME: &str = "music";

#[derive(Clone, Copy, Default, Debug)]
struct Timer {
    enabled: bool,
    minutes: i32,
    counting: bool,
    remaining_ms: i64,
    last_ms: i64,
}

impl Timer {
    /// Armed means there is a stretch to count: switched on and longer than
    /// nothing.
    fn armed(&self) -> bool {
        self.enabled && self.minutes > 0
    }

    /// Starts a full stretch, or stops counting, from the state as it stands.
    /// Every change goes through here, so there is one place that decides and
    /// one place where `remaining_ms` is ever set.
    fn restart(&mut self, playing: bool, now: i64) {
        self.counting = playing && self.armed();
        self.remaining_ms = if self.counting {
            i64::from(self.minutes) * 60_000
        } else {
            0
        };
        self.last_ms = now;
    }

    /// One tick: true exactly once, on the tick the stretch runs out.
    fn tick(&mut self, now: i64) -> bool {
        if !self.counting {
            return false;
        }
        let mut elapsed = now - self.last_ms;
        self.last_ms = now;
        if elapsed < 0 {
            elapsed = 0; // the clock is monotonic, but do not trust a negative step
        }

        if elapsed >= self.remaining_ms {
            self.counting = false;
            self.remaining_ms = 0;
            return true;
        }
        self.remaining_ms -= elapsed;
        false
    }

    /// What is left, in seconds, or 0 when it is not counting.
    fn remaining_seconds(&self) -> i32 {
        if !self.counting {
            return 0;
        }
        ((self.remaining_ms + 999) / 1000) as i32
    }
}

/// Which kind is playing, kept apart from the countdowns: "something is
/// playing" and "a stretch is running" are not the same thing, and folding them
/// into one flag is how switching the option off and on again while the music
/// played armed nothing at all.
#[derive(Debug, Clone, Copy, Default)]
struct State {
    timers: [Timer; COUNT],
    playing: Option<usize>,
}

impl State {
    const fn new() -> Self {
        State {
            timers: [Timer {
                enabled: false,
                minutes: 0,
                counting: false,
                remaining_ms: 0,
                last_ms: 0,
            }; COUNT],
            playing: None,
        }
    }

    fn playing_index(kind: i32) -> Option<usize> {
        if kind >= 0 && (kind as usize) < COUNT {
            Some(kind as usize)
        } else {
            None
        }
    }
}

fn state() -> &'static Mutex<State> {
    static STATE: OnceLock<Mutex<State>> = OnceLock::new();
    STATE.get_or_init(|| Mutex::new(State::new()))
}

fn lock() -> std::sync::MutexGuard<'static, State> {
    state().lock().unwrap_or_else(|e| e.into_inner())
}

/// Milliseconds since the first question, which is all a difference needs.
fn now_ms() -> i64 {
    static BASE: OnceLock<Instant> = OnceLock::new();
    BASE.get_or_init(Instant::now).elapsed().as_millis() as i64
}

fn clamp_minutes(minutes: i32) -> i32 {
    if minutes < 0 {
        return 0;
    }
    minutes.min(MAX_MINUTES)
}

/// Arms the shared timer from the settings the caller has just read. The
/// clamp is applied here too, so a hand-edited config cannot arm something
/// that never expires.
pub fn init(enabled: bool, minutes: i32) {
    let mut state = lock();
    let timer = &mut state.timers[0];
    timer.enabled = enabled;
    timer.minutes = clamp_minutes(minutes);
    timer.counting = false;
    timer.remaining_ms = 0;
    state.playing = None;
}

/// The clamp `set_minutes` applies, for a caller that wants to store what the
/// timer would actually use.
pub fn clamp(minutes: i32) -> i32 {
    clamp_minutes(minutes)
}

pub fn enabled(kind: i32) -> bool {
    State::playing_index(kind).map_or(false, |i| lock().timers[i].enabled)
}

pub fn minutes(kind: i32) -> i32 {
    State::playing_index(kind).map_or(0, |i| lock().timers[i].minutes)
}

pub fn any_running() -> bool {
    lock().timers.iter().any(|t| t.counting)
}

pub fn running(kind: i32) -> bool {
    State::playing_index(kind).map_or(false, |i| lock().timers[i].counting)
}

pub fn remaining(kind: i32) -> i32 {
    State::playing_index(kind).map_or(0, |i| lock().timers[i].remaining_seconds())
}

/// Switches a kind on or off, and answers whether anything changed — the
/// caller saves the setting when it did.
pub fn set_enabled(kind: i32, on: bool) -> bool {
    let Some(i) = State::playing_index(kind) else {
        return false;
    };
    let mut state = lock();
    if on == state.timers[i].enabled {
        return false;
    }
    state.timers[i].enabled = on;
    // Switching it off in the middle of a stretch disarms it now, and switching
    // it on while something is already playing starts one: otherwise the
    // setting would only take effect at the next pause.
    let playing = state.playing;
    state.timers[i].restart(playing == Some(i), now_ms());
    true
}

/// Sets the length of a stretch, and answers the clamped value when it
/// changed — the caller saves that.
pub fn set_minutes(kind: i32, minutes: i32) -> Option<i32> {
    let Some(i) = State::playing_index(kind) else {
        return None;
    };
    let minutes = clamp_minutes(minutes);
    let mut state = lock();
    if minutes == state.timers[i].minutes {
        return None;
    }
    state.timers[i].minutes = minutes;
    // Moving the wheel is choosing a new stretch, not editing the one running:
    // whoever just set thirty minutes means thirty minutes from now.
    let playing = state.playing;
    state.timers[i].restart(playing == Some(i), now_ms());
    Some(minutes)
}

/// Tells the timer what is playing; `COUNT` (or anything out of range) means
/// nothing that any of them counts.
pub fn note_playing(kind: i32) {
    let index = State::playing_index(kind);
    let mut state = lock();
    if index == state.playing {
        return;
    }
    state.playing = index;
    // All of them, not only the one named: whatever stopped playing has a
    // countdown to put away, and going from an album to a podcast is both at
    // once.
    let now = now_ms();
    for i in 0..COUNT {
        state.timers[i].restart(index == Some(i), now);
        if state.timers[i].counting {
            eprintln!("sleeptimer: {NAME}, {} min", state.timers[i].minutes);
        }
    }
}

pub fn expired(kind: i32) -> bool {
    let Some(i) = State::playing_index(kind) else {
        return false;
    };
    let mut state = lock();
    let now = now_ms();
    if state.timers[i].tick(now) {
        eprintln!("sleeptimer: {NAME}, time is up; stopping");
        return true;
    }
    false
}

#[cfg(test)]
mod tests {
    use super::*;

    fn armed(minutes: i32) -> Timer {
        let mut timer = Timer {
            enabled: true,
            minutes,
            ..Timer::default()
        };
        timer.restart(true, 1_000);
        timer
    }

    #[test]
    fn a_fresh_stretch_counts_from_the_whole_span() {
        let timer = armed(30);
        assert!(timer.counting);
        assert_eq!(timer.remaining_ms, 30 * 60_000);
        assert_eq!(timer.remaining_seconds(), 1800);
    }

    #[test]
    fn a_switch_with_no_minutes_arms_nothing() {
        let mut timer = armed(0);
        assert!(!timer.counting);
        assert_eq!(timer.remaining_ms, 0);
        assert_eq!(timer.remaining_seconds(), 0);
        assert!(!timer.tick(9_999_999));
    }

    #[test]
    fn the_countdown_loses_only_what_passed() {
        let mut timer = armed(1);
        assert!(!timer.tick(1_000 + 20_000));
        assert_eq!(timer.remaining_ms, 40_000);
        assert_eq!(timer.remaining_seconds(), 40);
        assert!(!timer.tick(1_000 + 59_000));
        assert_eq!(timer.remaining_seconds(), 1);
    }

    #[test]
    fn it_expires_exactly_once() {
        let mut timer = armed(1);
        assert!(!timer.tick(1_000 + 59_999));
        assert!(timer.tick(1_000 + 60_000));
        assert!(!timer.counting);
        assert_eq!(timer.remaining_ms, 0);
        assert!(
            !timer.tick(1_000 + 90_000),
            "an expired timer does not re-fire"
        );
    }

    #[test]
    fn a_clock_that_steps_backwards_does_not_add_time() {
        let mut timer = armed(1);
        assert!(!timer.tick(900));
        assert_eq!(timer.remaining_ms, 60_000);
        assert!(!timer.tick(1_100));
        assert_eq!(timer.remaining_ms, 59_800);
    }

    #[test]
    fn only_the_playing_kind_counts() {
        let mut state = State::new();
        state.timers[0].enabled = true;
        state.timers[0].minutes = 5;

        state.timers[0].restart(false, 0);
        assert!(!state.timers[0].counting);

        state.playing = Some(0);
        state.timers[0].restart(true, 0);
        assert!(state.timers[0].counting);
        assert_eq!(state.timers[0].remaining_seconds(), 300);
    }

    #[test]
    fn minutes_are_clamped_to_a_day_and_never_negative() {
        assert_eq!(clamp_minutes(-1), 0);
        assert_eq!(clamp_minutes(0), 0);
        assert_eq!(clamp_minutes(1439), 1439);
        assert_eq!(clamp_minutes(1440), 1439);
        assert_eq!(clamp_minutes(100_000), 1439);
    }

    #[test]
    fn an_out_of_range_kind_answers_nothing() {
        assert_eq!(State::playing_index(-1), None);
        assert_eq!(State::playing_index(COUNT as i32), None);
        assert_eq!(State::playing_index(0), Some(0));
    }
}
