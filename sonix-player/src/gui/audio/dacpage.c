#include "dacpage.h"

#include <stdio.h>
#include <string.h>

#include "src/gui/shell/confirm.h"
#include "src/gui/fonts/fonts.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/settingsrow.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/settings/musicsettings.h"
#include "src/gui/shell/theme.h"
#include "src/system/addons/addons.h"
#include "src/system/audio/eq.h"
#include "src/system/core/lang.h"
#include "src/system/audio/usbdac.h"

lv_obj_t *dacpage_screen;

static lv_obj_t *mode_switch;
static lv_obj_t *gain_pill;
static lv_obj_t *format_label;
static lv_obj_t *status_label;
static lv_obj_t *charge_btn;
static lv_obj_t *charge_glyph;
static lv_obj_t *eq_pill;
static lv_obj_t *peq_pill;
static lv_obj_t *peq_profile_pill;
static lv_obj_t *mseb_pill;
static lv_timer_t *poll_timer;
static unsigned last_serial = (unsigned)-1;
// The compact panel hides the rate line while there is no rate: see
// set_format() and dacpage_init().
static bool hide_empty_format;
static lv_color_t status_normal_color; // the theme's colour, captured at build time
static gui_config_t *dac_cfg;

// The pills follow the equalisers, not the finger: they mirror the control
// centre, including the selected PEQ profile, and either place may have changed
// them since this page was last looked at.
static void paint_equaliser_pills(void) {
	settingsrow_pill_active(eq_pill, eq_get_enabled());
	bool peq_enabled = peq_get_enabled();
	settingsrow_pill_active(peq_pill, peq_enabled);
	settingsrow_pill_active(mseb_pill, mseb_get_enabled());
	if (peq_profile_pill) {
		char profile[64];
		snprintf(profile, sizeof(profile), tr("peq_profile_name"), peq_get_profile());
		lv_obj_t *label = lv_obj_get_child(peq_profile_pill, 0);
		if (strcmp(lv_label_get_text(label), profile) != 0) {
			lv_label_set_text(label, profile);
		}
		settingsrow_pill_active(peq_profile_pill, peq_enabled);
		if (peq_enabled) {
			lv_obj_remove_state(peq_profile_pill, LV_STATE_DISABLED);
		} else {
			lv_obj_add_state(peq_profile_pill, LV_STATE_DISABLED);
		}
		lv_obj_set_style_opa(peq_profile_pill, peq_enabled ? LV_OPA_COVER : LV_OPA_50, 0);
	}
	bool high_gain = musicsettings_high_gain();
	settingsrow_pill_active(gain_pill, high_gain);
	if (gain_pill) {
		lv_label_set_text(lv_obj_get_child(gain_pill, 0),
						  tr(high_gain ? "musicsettings_high_gain" : "musicsettings_low_gain"));
	}
}

// ---------------------------------------------------------------------------
// the equalisers
//
// The control centre cannot be pulled down while DAC mode holds the screen, so
// the circles that switch the equaliser, PEQ and MSEB are out of reach exactly
// when they matter -- a curve set before the cable went in is heard, and a
// listener who wants to compare it should not have to unplug.
//
// Pills rather than switches, and that is the V1 talking: its panel is
// 240x320, so the page has 248 px under the heading. A separate row for each
// equaliser, the PEQ profile and gain would push the host's sample rate -- the
// one number this page exists to show -- off the screen. A titled card of
// wrapped pills keeps those controls compact; on the V1 the decorative mark
// below it goes too: that leaves
// the rate and the status on the same screen as the switches.
//
// The same equaliser and profile calls the circles make, so the settings pages
// and the control centre stay in step with a tap here.
// ---------------------------------------------------------------------------

static void eq_cb(lv_event_t *e) {
	(void)e;
	musicsettings_set_eq_enabled(!eq_get_enabled());
	paint_equaliser_pills();
}

static void peq_cb(lv_event_t *e) {
	(void)e;
	peq_set_enabled(!peq_get_enabled());
	paint_equaliser_pills();
}

static void peq_profile_cb(lv_event_t *e) {
	(void)e;
	if (peq_get_enabled()) {
		peq_next_profile();
		paint_equaliser_pills();
	}
}

static void mseb_cb(lv_event_t *e) {
	(void)e;
	musicsettings_set_mseb_enabled(!mseb_get_enabled());
	paint_equaliser_pills();
}

static void gain_cb(lv_event_t *e) {
	(void)e;
	musicsettings_set_high_gain(!musicsettings_high_gain());
	paint_equaliser_pills();
}

// The host's rate, or nothing at all.
//
// Set, never removed, and the words only when they really changed:
// lv_label_set_text() re-lays the label out even for the same string, and the
// column around it with it. On the compact panel an empty rate is hidden rather
// than drawn as a blank line, so that it takes no space in the block the words
// are centred in (see dacpage_init).
// A transparent, growing nothing: what centres the words on the compact panel
// (see dacpage_init). Base 0 height, so flex grow is all it ever takes.
static void spacer(lv_obj_t *parent) {
	lv_obj_t *obj = lv_obj_create(parent);
	lv_obj_set_size(obj, lv_pct(100), 0);
	lv_obj_set_flex_grow(obj, 1);
	lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(obj, 0, 0);
	lv_obj_set_style_radius(obj, 0, 0);
	lv_obj_set_style_pad_all(obj, 0, 0);
	lv_obj_set_scrollable(obj, false);
	// Empty space has to behave like empty space: a press here belongs to the
	// page, which is what the swipe back -- and the scroll of a long page -- is
	// attached to. A clickable box on top of it would swallow both.
	lv_obj_set_clickable(obj, false);
}

static void set_format(const char *text) {
	// Hiding before the text: a label that starts empty is already equal to
	// "", so the comparison below cannot be what decides whether it is drawn.
	if (hide_empty_format) {
		lv_obj_set_hidden(format_label, text[0] == '\0');
	}
	if (strcmp(lv_label_get_text(format_label), text) != 0) {
		lv_label_set_text(format_label, text);
	}
}

static void refresh_charge_glyph(void) {
	if (!charge_glyph) {
		return;
	}
	// A lightning bolt when the cable is allowed to charge, the struck-through
	// one when it is not.
	lv_image_set_src(charge_glyph, usbdac_charging_enabled() ? &icon_zap : &icon_zap_off);
	if (dac_cfg) {
		settingsrow_scale_corner_icon(charge_glyph, dac_cfg);
	}
}

static void refresh(void) {
	usbdac_state_t st;
	usbdac_get_state(&st);

	// The switch follows the mode rather than the finger: a start that failed
	// must not leave the toggle sitting proudly in the on position.
	if (st.active || st.starting) {
		lv_obj_add_state(mode_switch, LV_STATE_CHECKED);
	} else {
		lv_obj_remove_state(mode_switch, LV_STATE_CHECKED);
	}

	// Dimmed when the mode is off, but never LV_STATE_DISABLED: a disabled LVGL
	// object stops receiving presses altogether, so the button stays clickable
	// and explains itself instead of silently doing nothing.
	lv_obj_set_style_opa(charge_glyph, st.active ? LV_OPA_COVER : LV_OPA_40, 0);
	refresh_charge_glyph();
	paint_equaliser_pills();

	// Back to the theme's colour unless the checks below make it red. Set, never
	// removed: lv_obj_remove_style(obj, NULL, ...) drops every local style on
	// that part, width, font and alignment included. The colour to restore is
	// captured at build time, before anything local is set over it.
	lv_obj_set_style_text_color(status_label, status_normal_color, 0);

	if (st.error[0]) {
		set_format("");
		lv_label_set_text(status_label, st.error);
		lv_obj_set_style_text_color(status_label, lv_color_make(224, 27, 36), 0);
		return;
	}

	if (st.starting) {
		set_format("");
		lv_label_set_text(status_label, tr("dac_turning_on_2"));
		return;
	}

	if (!st.active) {
		set_format("");
		lv_label_set_text(status_label, tr("dac_note"));
		return;
	}

	if (!st.streaming) {
		set_format("");
		lv_label_set_text(status_label, tr("dac_waiting"));
		return;
	}

	// The numbers under the icon. The rate is the host's choice: the gadget
	// announces the whole range and the computer picks one.
	//
	// Sample rate only: the driver always delivers 32-bit words whatever the
	// host negotiated, so a bit depth printed here would always read 32.
	char rate_text[32];
	if (st.dsd) {
		snprintf(rate_text, sizeof(rate_text), "DSD %d Hz", st.sample_rate);
	} else if (st.sample_rate % 1000 == 0) {
		snprintf(rate_text, sizeof(rate_text), "%d kHz", st.sample_rate / 1000);
	} else {
		snprintf(rate_text, sizeof(rate_text), "%.1f kHz", st.sample_rate / 1000.0);
	}

	char text[64];
	if (st.dsd) {
		snprintf(text, sizeof(text), "%s", rate_text);
	} else if (st.bits > 0) {
		snprintf(text, sizeof(text), "%s · %d-bit", rate_text, st.bits);
	} else {
		snprintf(text, sizeof(text), "%s", rate_text);
	}
	set_format(text);
	lv_label_set_text(status_label, tr("dac_playing"));
}

static void poll_cb(lv_timer_t *timer) {
	(void)timer;
	if (lv_screen_active() != dacpage_screen) {
		return;
	}
	unsigned now = usbdac_serial();
	if (now == last_serial) {
		return;
	}
	last_serial = now;
	refresh();
}

// Applies whatever the switch currently says.
static void apply_mode_from_switch(void) {
	if (lv_obj_has_state(mode_switch, LV_STATE_CHECKED)) {
		usbdac_start(); // a failure lands in the state, and refresh() shows it
	} else {
		usbdac_stop();
	}
	last_serial = (unsigned)-1;
	refresh();
}

// The whole row toggles, not just the switch: a 68x36 switch at the far edge of
// the screen is a small target, and the row gives one the width of the page
// whatever else may sit over that corner.
static void mode_row_cb(lv_event_t *e) {
	(void)e;
	if (lv_obj_has_state(mode_switch, LV_STATE_CHECKED)) {
		lv_obj_remove_state(mode_switch, LV_STATE_CHECKED);
	} else {
		lv_obj_add_state(mode_switch, LV_STATE_CHECKED);
	}
	apply_mode_from_switch();
}

static void mode_cb(lv_event_t *e) {
	(void)e;
	// The stored switch rather than the event target, matching every other
	// toggle in the settings pages: the target of a VALUE_CHANGED can be the
	// row rather than the switch depending on how the row was built.
	apply_mode_from_switch();
}

static bool leaving;

// Leaves for real; the guard below lets the chevron through once `leaving` is
// set. Deferred by one turn of the event loop because really_leave() runs from
// inside the confirmation dialog's own handler, and navigating out from under a
// dialog that is still closing leaves the screen change undone.
static void leave_async(void *user) {
	(void)user;
	back_btn_cb(NULL);
	leaving = false;
}

static void really_leave(void *user) {
	(void)user;
	usbdac_stop();
	last_serial = (unsigned)-1;
	refresh();
	leaving = true;
	lv_async_call(leave_async, NULL);
}

// The back chevron and the swipe both come through here. Returning true means
// "handled, stay put": DAC mode is not something to wander out of by accident
// with a computer streaming through the cable. Once it is off the guard steps
// aside and back works as it does anywhere else.
static bool back_guard(void) {
	if (leaving || !usbdac_is_active()) {
		return false;
	}
	confirm_show("dac_leave_dac_mode",
				 "dac_leave_confirm_note", "leave",
				 really_leave, NULL);
	return true;
}

static void charge_cb(lv_event_t *e) {
	(void)e;
	if (!usbdac_is_active()) {
		gui_notify_popup("dac_turn_dac_mode_on_first");
		return;
	}
	// The choice is written to the config, so a player set never to charge in
	// DAC mode stays that way across reboots.
	bool now_on = !usbdac_charging_enabled();
	usbdac_set_charging(now_on);
	refresh_charge_glyph();

	// The two glyphs differ by one thin diagonal stroke, which at 34 px is not
	// enough to tell a state change from a button that did nothing. Say it in
	// words as well.
	gui_notify_popup(now_on ? "dac_usb_charging_on" : "dac_usb_charging_off");
}

bool dacpage_is_holding(void) {
	return usbdac_is_active() && lv_screen_active() == dacpage_screen;
}

// Same shape as the corner buttons on the music page.
static lv_obj_t *corner_button(gui_config_t *cfg, int slot, const lv_image_dsc_t *glyph, lv_obj_t **glyph_out) {
	lv_obj_t *button = lv_btn_create(dacpage_screen);
	settingsrow_place_corner_button(button, cfg, slot);
	lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(button, 0, 0);
	lv_obj_set_style_shadow_width(button, 0, 0);
	lv_obj_set_style_pad_all(button, 0, 0);

	lv_obj_t *icon = lv_image_create(button);
	lv_image_set_src(icon, glyph);
	lv_obj_add_style(icon, &theme_style_icon, 0);
	settingsrow_scale_corner_icon(icon, cfg);
	lv_obj_center(icon);
	if (glyph_out) {
		*glyph_out = icon;
	}
	return button;
}

static void loaded_cb(lv_event_t *e) {
	(void)e;
	last_serial = (unsigned)-1;
	refresh();
}

void dacpage_init(gui_config_t *cfg) {
	dac_cfg = cfg;
	const bool compact = cfg->screen_width < 320;
	lv_obj_t *container = settingsrow_page(dacpage_screen, cfg, "dac");
	lv_obj_set_scrollable(container, false);

	// The mode is a switch, not a side effect of arriving: the page can be
	// looked at, and the charging button read, without the cable being taken
	// over the moment it opens.
	mode_switch = NULL;
	lv_obj_t *mode_row = settingsrow_toggle(container, "dac_mode", &mode_switch, mode_cb);
	lv_obj_set_clickable(mode_row, true);
	lv_obj_add_event_cb(mode_row, mode_row_cb, LV_EVENT_CLICKED, NULL);

	// The equalisers the control centre cannot be asked for here (see the block
	// above). Straight under the mode switch, ahead of the artwork, so on the
	// V1's 320 px panel they are on screen without a scroll.
	// Only the equalisers whose module is on: a module switched off in
	// Plugins has no page to open, so its pill would be a dead control on this
	// page long after its row left Music settings. With all three off the row
	// goes with them -- an "Equalisers" title over nothing is the same
	// evidence of a module the player does not have.
	bool eq_on = addons_enabled("eq");
	bool peq_on = addons_enabled("peq");
	bool mseb_on = addons_enabled("mseb");
	if (eq_on || peq_on || mseb_on) {
		lv_obj_t *equaliser_pills;
		settingsrow_pills(container, "dac_equalisers", &equaliser_pills);
		eq_pill = eq_on ? settingsrow_pill(equaliser_pills, "eq_short", 0, eq_cb) : NULL;
		peq_pill = peq_on ? settingsrow_pill(equaliser_pills, "peq", 1, peq_cb) : NULL;
		peq_profile_pill = peq_on ? settingsrow_pill_text(equaliser_pills, "", 0, peq_profile_cb) : NULL;
		mseb_pill = mseb_on ? settingsrow_pill(equaliser_pills, "mseb", 2, mseb_cb) : NULL;
		gain_pill = settingsrow_pill(equaliser_pills, "gain_short", 3, gain_cb);
		paint_equaliser_pills();
	}

	// Straight into the flex flow, with no alignment of its own: the container
	// settingsrow_page() builds already centres its children on the cross axis,
	// and an object positioned by alignment leaves the flow and is measured as
	// taking no space, so it would overlap the row above and steal its presses.
	//
	// No illustration: the V1's 240x320 panel has to hold the mode switch, the
	// equaliser card, the host's sample rate and the status copy in 248 px --
	// measured on the real layout engine, the last line of the note lands past
	// the foot of the screen with even a 48 px mark in between. The switch and
	// the status already say what the page is for, and a page whose status is
	// half cut looks broken rather than decorative. (The 200 px mark the regular
	// panels carried is not built: this fork draws the V1 only, and the block
	// that would have drawn it never ran.)

	// The words themselves: the host's rate, then the status copy.
	//
	// On this panel they are centred in whatever is left under the equalisers, by
	// a spacer on either side of them. With the mark gone there is about a
	// hundred pixels of nothing below the pills, and two lines hanging from the
	// top of it read as a page that stopped drawing halfway down. The rate line
	// is hidden while nothing streams, so the status sits in the middle when idle
	// and steps down one line when a stream starts -- rather than standing still
	// while a line appears over it, which is what it looked like before.
	//
	// Spacers and not a box around the two labels: a box would be a clipped
	// container, and a long error message is allowed to scroll the page. A
	// spacer shrinks to nothing instead, and the two labels stay direct children
	// of the page's own scrolling column.
	//
	// The regular panel keeps its stack under the mark: there the mark is what
	// fills the page, and the words belong directly beneath it.
	if (compact) {
		spacer(container);
		hide_empty_format = true;
	}

	format_label = lv_label_create(container);
	lv_obj_set_width(format_label, lv_pct(100));
	lv_label_set_long_mode(format_label, LV_LABEL_LONG_WRAP);
	// The theme's own text style, not the inherited default, which is black and
	// so invisible on the dark theme.
	lv_obj_add_style(format_label, &theme_style_text, 0);
	lv_obj_set_style_text_align(format_label, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(format_label, compact ? &font_ui_20_bold : &font_ui_24_bold, 0);
	lv_obj_set_style_margin_top(format_label, compact ? 0 : 20, 0);
	lv_label_set_text(format_label, "");

	status_label = lv_label_create(container);
	lv_obj_set_width(status_label, lv_pct(100));
	lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_add_style(status_label, &theme_style_text_dim, 0);
	lv_obj_set_style_text_font(status_label, compact ? &font_ui_14 : &font_ui_22, 0);
	lv_obj_set_style_margin_top(status_label, compact ? 0 : 10, 0);
	lv_label_set_text(status_label, "");
	status_normal_color = lv_obj_get_style_text_color(status_label, LV_PART_MAIN);
	if (compact) {
		spacer(container);
	}

	charge_btn = corner_button(cfg, 0, &icon_zap, &charge_glyph);
	lv_obj_add_event_cb(charge_btn, charge_cb, LV_EVENT_CLICKED, NULL);
	// settingsrow_title reserves CORNER_BTN_SIZE on the right, but the settings
	// container spans the whole screen and is built after this button, so the
	// button must come to the front to stay reachable.
	lv_obj_move_foreground(charge_btn);
	lv_obj_set_style_opa(charge_glyph, LV_OPA_40, 0);
	refresh_charge_glyph();

	lv_obj_add_event_cb(dacpage_screen, loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
	switcher_set_back_guard(dacpage_screen, back_guard);

	poll_timer = lv_timer_create(poll_cb, 500, NULL);
	(void)poll_timer;
}
