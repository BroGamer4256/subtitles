#![feature(iter_advance_by)]
#[macro_use]
extern crate byte_strings;
use std::ffi::*;

struct Subtitle {
	duration: i32,
	subtitle: &'static CStr,
	name: &'static CStr,
}

static EMPTY_SUBTITLE: Subtitle = Subtitle {
	duration: 0,
	subtitle: c_str!(""),
	name: c_str!(""),
};

include!(concat!(env!("OUT_DIR"), "/codegen.rs"));

#[repr(C)]
pub struct SubtitleC {
	duration: c_int,
	subtitle: *const c_char,
	name: *const c_char,
}

unsafe fn subtitle_convert(subtitle: &Subtitle) -> SubtitleC {
	SubtitleC {
		duration: subtitle.duration,
		subtitle: subtitle.subtitle.as_ptr(),
		name: subtitle.name.as_ptr(),
	}
}

#[no_mangle]
pub unsafe extern "C" fn get_subtitle(name: *const c_char) -> SubtitleC {
	let name = CStr::from_ptr(name).to_str().unwrap();
	// Remove suffix marking radio info. e.g. AA054AA_E is a copy of AA054AA with a radio filter applied
	let name = if name.chars().nth(name.len() - 2) == Some('_') {
		let mut iter = name.chars().rev();
		_ = iter.advance_by(2);
		iter.rev().collect()
	} else {
		String::from(name)
	};
	let sub = SUBTITLES.get(name.as_str()).unwrap_or(&EMPTY_SUBTITLE);
	subtitle_convert(sub)
}
